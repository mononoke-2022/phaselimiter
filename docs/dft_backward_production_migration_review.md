# DFT Backward Production Migration Review

## Scope

This is a design/review document for a future production migration of:

- `bakuage::RealDft<float>::Backward`
- Apple / non-IPP build path
- IPP-compatible public CCS/interleaved spectrum layout

This step intentionally did not change:

- `deps/bakuage/src/dft.cpp`
- `deps/bakuage/src/vector_math.cpp`
- `CMakeLists.txt`
- `src/tools`
- existing DSP implementation
- `local_mastering_app`

No build, test, or commit was performed for this review.

Repository status checked with:

```sh
git status --short --branch
```

Result:

```text
## mac-arm64-minimal-cli...origin/mac-arm64-minimal-cli
```

## Current Evidence

Known production state:

- `RealDft<float>::Forward` Apple/non-IPP production path is implemented.
- Forward minimal IPP golden compare passed: `99 / 99`.
- Forward `forward_extended` IPP golden compare passed: `392 / 392`.
- `RealDft<float>::Backward` Apple/non-IPP production path is not implemented yet.
- `ForwardPerm`, `BackwardPerm`, Pack paths, and `RealDft<double>` remain production-unmodified.

Backward IPP golden state:

- Case set: `minimal_backward`
- Records: `52`
- Method: `Backward`
- Precision: `float`
- Input kind: `real_dft_spectrum`
- Input scalar count per record: `2 * (floor(N / 2) + 1)`
- Output scalar count per record: `N`

Backward vDSP prototype attempt01 result:

- Generated: `52`
- Unsupported: `0`
- Compared: `52`
- Passed: `52`
- Failures: `0`
- Scalar count mismatch: `0`
- Missing candidate: `0`
- NaN/Inf: `0`
- Backend counts: `zrop_even=28`, `zop_odd=0`, `legacy_zop_odd=24`

This is strong evidence that the prototype mapping can reproduce IPP `RealDft<float>::Backward` for the current minimal set. It is not yet evidence that production `Forward` plus production `Backward` is safe as an application-level roundtrip.

## Current Production Shape

IPP branch behavior in `deps/bakuage/src/dft.cpp`:

- `RealDft<float>::Backward` calls `MyIppR2CDft32::ExecuteInv`.
- `ExecuteInv` calls `ippsDFTInv_CCSToR_32f`.
- IPP setup uses `IPP_FFT_NODIV_BY_ANY`, so inverse output is unnormalized.

Current Apple/non-IPP branch behavior:

- Defines only `FftMemoryBuffer`, `RealDft<float>::RealDft`, `RealDft<float>::Forward`, and `RealDft<float>::work_size`.
- Uses `MyVdspR2CForwardDft32`.
- `dft_ptr_` currently points to the Forward vDSP object.
- `fft_ptr_` is `nullptr`.
- `work_size()` currently reflects Forward-only work needs.

Production migration must therefore add Backward without accidentally changing the already validated Forward behavior or the public `RealDft<float>` ABI.

## Prototype-To-Production Differences

The prototype proves math/layout in a standalone tool. Production migration still has several mechanical differences that must be handled deliberately:

- Setup lifetime: the prototype creates/destroys vDSP setup per record; production should cache setup through the existing `MyVdspDftLibrary` pattern.
- Work memory: the prototype uses local `std::vector<float>` allocations; production must fit the conversion buffers into `work_data` or a safe fallback allocation when `work_data == nullptr`.
- `work_size()` contract: production must return enough memory for both Forward and Backward paths after Backward is added.
- Constructor ownership: the current Apple/non-IPP constructor stores only a Forward object in `dft_ptr_`; adding Backward may require a combined object or a second cached object while preserving the existing public fields.
- Error behavior: unsupported vDSP setup creation should fail explicitly, not fall back to padding, fake lengths, or approximate transforms.
- Forward preservation: the existing Forward code path, scale constants, and odd/even layout writes must remain byte-for-byte equivalent unless a separate Forward review approves changes.

## Migration Invariants

These conditions must hold for a production `RealDft<float>::Backward` Apple/non-IPP path:

- Transform length must remain exactly `N`.
- Zero padding is forbidden.
- Input scalar count is exactly `2 * (floor(N / 2) + 1)`.
- Output scalar count is exactly `N`.
- Public input layout is the same layout produced by `RealDft<float>::Forward`: interleaved complex scalars for bins `0..floor(N/2)`.
- DC imaginary scalar must not affect output; strict valid inputs should keep it zero.
- Even length bin `N/2` is a Nyquist singleton; use its real scalar and ignore/require zero for its imaginary scalar.
- Odd length final stored bin `floor(N/2)` is an ordinary complex bin; preserve both real and imaginary values when constructing the full conjugate-symmetric spectrum.
- No NaN or Inf may be introduced.
- Backward must not perform `1 / N` normalization internally.
- `Backward(Forward(x))` must remain approximately `N * x`; callers keep responsibility for normalization.

## vDSP Backward Mapping

Even lengths:

- Use `vDSP_DFT_zrop_CreateSetup(..., N, vDSP_DFT_INVERSE)`.
- Convert public CCS/interleaved spectrum to zrop split input:
  - `in_real[0] = spectrum[0]` for DC real.
  - `in_imag[0] = spectrum[2 * half]` for even Nyquist real.
  - bins `1..half-1` map to split real/imag arrays.
- Execute inverse zrop.
- Interleave returned even/odd time samples into output length `N`.
- Do not apply extra `2x` input scaling.

Odd lengths:

- Build exact-length full complex spectrum of length `N`.
- Set DC imaginary to zero.
- For bins `1..floor(N/2)`, copy the public positive bin and mirror it with conjugation.
- Use `vDSP_DFT_zop_CreateSetup(..., N, vDSP_DFT_INVERSE)` when available.
- Fall back to `vDSP_DFT_CreateSetup` plus `vDSP_DFT_zop(..., vDSP_DFT_INVERSE)` for exact-length odd transforms.
- Return the real part as the time-domain output.
- Do not use an even-length transform, zero padding, or truncation for odd lengths.

## Scaling Review

IPP uses `IPP_FFT_NODIV_BY_ANY` for both Forward and Backward. The current API contract is therefore:

- Forward is unnormalized.
- Backward is unnormalized.
- `Backward(Forward(x))` produces approximately `N * x`.
- Normalization belongs at call sites or in spectral pre-scaling, not inside `RealDft<float>::Backward`.

Prototype attempt01 confirms this for Backward:

- An initial even zrop trial with extra `2x` scaling produced a clear mismatch.
- Removing that extra scaling made the 52-record `minimal_backward` set pass.
- Therefore the production Backward path should not apply `1 / N`, `0.5`, `2x`, or any other global scaling unless a new golden comparison proves otherwise.

The Forward production path has its own validated even-length `0.5f` correction for vDSP zrop forward output. That Forward correction must not be mirrored blindly into Backward.

## Forward And Backward Together

Using the existing production Forward path and a new production Backward path together introduces risks not covered by the Backward-only golden compare:

- Forward and Backward may each match IPP within tolerance independently, while their combined numeric residual differs from IPP roundtrip behavior.
- Forward even zrop applies a validated `0.5f` correction; Backward even zrop must not add a compensating correction without roundtrip evidence.
- Large unnormalized outputs can magnify float backend differences by `N`, especially for large amplitudes and large lengths.
- Roundtrip normalization expectations are caller-owned; silently changing Backward normalization would alter application gain and audio quality.
- Existing callers use both direct `Backward` and `Forward`-generated spectra, so both direct golden compare and `Forward -> Backward` roundtrip validation are required.

Conclusion: Backward-only golden success is necessary but not sufficient for an audio-safe production migration.

## Perm And Pack Boundary

`ForwardPerm` and `BackwardPerm` remain a separate, higher-risk migration:

- Current IPP code uses DFT out-of-place and FFT in-place paths depending on pointer equality.
- The Perm layout is not the public CCS/interleaved layout used by `Forward` and `Backward`.
- `GradCore` has layout-sensitive code around `spec[1] = spec[len]` before `BackwardPerm`.
- `FirFilter2` and related filtering paths use Perm heavily.

A CCS `Backward` implementation must not be treated as evidence that Perm is compatible. Production Backward migration should leave `ForwardPerm`, `BackwardPerm`, `ForwardPack`, and `BackwardPack` untouched.

## Double Boundary

`RealDft<double>` remains unvalidated for Apple/non-IPP:

- It currently has IPP production implementations for Forward, Backward, Perm, and Pack.
- There is no matching vDSP double prototype/golden result in the current evidence set.
- Double precision has different tolerance expectations and should not inherit float thresholds.
- Some filtering code owns both float and double `RealDft` instances.

A float Backward migration should not imply double support. Any accidental link-time or runtime exposure of Apple/non-IPP `RealDft<double>` would be a release blocker.

## Can Backward Be Productionized Alone?

Yes, with a narrow interpretation:

- It is reasonable to implement only `RealDft<float>::Backward` for Apple/non-IPP as the next production step.
- It should be limited to the same public CCS/interleaved layout validated by `minimal_backward`.
- It should not include Perm, Pack, double, complex DFT, vector math, or application changes.
- It should not be considered audio-release-ready until the required post-migration validations pass.

Do not productionize Backward yet if any of these are true:

- The production implementation cannot preserve exact transform length for all target lengths.
- The implementation needs zero padding to support odd or unsupported lengths.
- `work_size()` cannot safely cover both Forward and Backward buffers.
- Even Nyquist, odd final-bin complex, or DC imaginary handling differs from the prototype.
- The Backward golden compare has any failure, missing candidate, scalar-count mismatch, or NaN/Inf.
- Forward minimal or Forward extended regression starts failing.
- A `Forward -> Backward` roundtrip validation tool/check is not available before release qualification.

## Proposed Minimal Migration Steps

Step 1: Add only `RealDft<float>::Backward` to the Apple/non-IPP production path.

- Reuse the prototype mapping.
- Preserve the existing Forward implementation.
- Update Apple/non-IPP `work_size()` only as needed for Backward work buffers.
- Keep `ForwardPerm`, `BackwardPerm`, Pack paths, and `double` unimplemented/unmodified.

Step 2: Run `minimal_backward` IPP golden compare.

- Expected records: `52`.
- Required result: `52 / 52` pass.
- Hard failures: missing candidate, scalar count mismatch, NaN/Inf, unsupported length, or broad scaling/sign/layout mismatch.

Step 3: Add Forward/Backward roundtrip verification.

- Use production Apple/non-IPP Forward followed by production Apple/non-IPP Backward.
- Check raw output against the expected `N` gain.
- Check normalized output `y / N` against the original input.
- Include even, odd, power-of-two, non-power-of-two, large-length, tiny-value, and large-safe-value cases.

Step 4: Keep Perm and double out of this migration.

- Document them as intentionally unsupported/unmodified.
- Add separate prototype and golden work before touching them.

## Required Validation After Migration

Before considering the migration safe, run at minimum:

- `minimal_backward` golden compare: `52 / 52` must pass.
- Forward minimal regression: `99 / 99` must still pass.
- Forward `forward_extended` regression: `392 / 392` must still pass.
- Forward/Backward roundtrip validation with normalized `output / N` comparison.
- Explicit even/odd validation:
  - even DC-only
  - even Nyquist real
  - ordinary bin real/imag
  - odd final bin complex
  - length `12345`
- Structural checks:
  - no missing outputs
  - no scalar count mismatch
  - no NaN/Inf
  - no unsupported target length

Recommended additional validation before audio-facing release:

- Phase limiter representative lengths and amplitudes.
- Downstream checks for callers that use direct `Backward`.
- A guard that Apple/non-IPP builds still do not expose unsupported Perm/double paths as if they were implemented.

## Review Conclusion

The vDSP Backward prototype result is good enough to justify a narrowly scoped production migration attempt for `RealDft<float>::Backward` on Apple/non-IPP builds.

The migration should preserve the prototype's exact-length strategy:

- even: `zrop_even`
- odd: exact-length complex inverse, with `legacy_zop_odd` fallback when needed

The highest-risk details are scaling, final-bin layout, and production work-buffer integration. The most important rule is that Backward remains an unnormalized IPP-compatible inverse: `Forward -> Backward` should produce `N * x`, and any `1 / N` normalization must stay outside `RealDft<float>::Backward`.
