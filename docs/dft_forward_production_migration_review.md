# DFT Forward Production Migration Review

## Scope

Review only. No production code is changed by this document.

Target under review:

- `RealDft<float>::Forward`
- Future Apple/non-IPP vDSP migration path only

Out of scope for the first production migration:

- `RealDft<float>::Backward`
- `RealDft<float>::ForwardPerm`
- `RealDft<float>::BackwardPerm`
- `RealDft<double>` paths
- `ForwardPack` / `BackwardPack`
- Complex `Dft<T>`, `Dft2D`, and `Dct2D`

Files intentionally not changed in this review:

- `deps/bakuage/src/dft.cpp`
- `deps/bakuage/src/vector_math.cpp`
- `CMakeLists.txt`
- Existing DSP implementation
- `local_mastering_app`

## Prototype Evidence

### Attempt01

Reference:

- `docs/dft_vdsp_forward_prototype_attempt01.md`

Result summary:

- Even lengths generated with vDSP zrop real DFT.
- Even records compared: `55`.
- Even records passed: `55`.
- Odd records unsupported/missing: `44`.
- Unsupported odd lengths:
  - `3`
  - `5`
  - `9`
  - `12345`
- Cause:
  - `vDSP_DFT_zrop_CreateSetup` requires an even real length.

Confirmed even-length mapping:

- vDSP zrop input layout:
  - even time samples in `Ir`
  - odd time samples in `Ii`
- vDSP zrop output layout:
  - DC real in `Or[0]`
  - Nyquist real in `Oi[0]`
  - normal bins in `Or[k] + i * Oi[k]`
- Public output layout:
  - interleaved complex scalars
  - DC imaginary forced to `0`
  - even Nyquist imaginary forced to `0`
- Scaling:
  - zrop forward returns `2 * DFT`
  - candidate applies `0.5f`

Attempt01 did not prove full `Forward` migration because odd lengths remained unsupported.

### Attempt02

Reference:

- `docs/dft_vdsp_forward_prototype_attempt02.md`

Result summary:

- Total records: `99`
- Passed records: `99`
- Failures: `0`
- Scalar count mismatch: `0`
- NaN/Inf: `0`
- Missing candidate: `0`
- Global max abs error: `0.013671875`
- Global RMS error: `7.98397704e-05`

Backend usage:

- Even lengths:
  - `zrop_even = 55`
  - zrop real DFT path from attempt01
  - `0.5f` scaling retained
- Odd lengths:
  - `zop_odd = 0`
  - `legacy_zop_odd = 44`
  - exact-length legacy complex vDSP DFT
  - no zero padding
  - no zrop `0.5f` scaling

Confirmed odd-length mapping:

- Complex input length is exactly `N`.
- Real part is input samples.
- Imaginary part is zero.
- Output bins `0..floor(N/2)` are copied to public interleaved complex scalars.
- DC imaginary is forced to `0`.
- Final odd bin imaginary is preserved, not forced to `0`.
- Scaling/sign/interleaving diagnostics did not indicate a mismatch:
  - `0.5x`, `2x`, imaginary sign flip, and real/imag swap were all much worse than normal output.

Attempt02 is the first evidence that the full current `RealDft<float>::Forward` golden set can be reproduced on Apple without IPP.

## Current Production Implementation

References:

- `deps/bakuage/src/dft.cpp`
- `deps/bakuage/include/bakuage/dft.h`

Current `RealDft<float>::Forward` behavior:

- `RealDft<float>::RealDft(int len, ...)` creates:
  - `MyIppR2CDft32` for real DFT paths
  - `MyIppR2CFft32` for FFT-based in-place Perm/Pack paths
- `RealDft<float>::Forward` always calls:
  - `MyIppR2CDft32::Execute`
  - `ippsDFTFwd_RToCCS_32f`
- IPP setup uses:
  - `ippsDFTGetSize_R_32f(len, IPP_FFT_NODIV_BY_ANY, ...)`
  - `ippsDFTInit_R_32f(len, IPP_FFT_NODIV_BY_ANY, ...)`
- There is no Forward-specific odd-length branch.
- There is no zero padding.
- Scaling is unnormalized.

Production migration point:

- The behavioral replacement point is `RealDft<float>::Forward(const Float *input, Float *output, void *work_data) const`.
- The setup/lifetime replacement point is the `RealDft<float>` constructor and private state used by that method.
- The public overload `Forward(const Float *input, Float *output)` in the header should keep delegating to the work-buffer overload.

The migration should not alter `ForwardPerm`, `ForwardPack`, `Backward`, `BackwardPerm`, or `BackwardPack`.

## Prototype-to-Production Differences

The prototype is a standalone tool. Production code has additional constraints:

- Prototype allocates temporary `std::vector<float>` buffers per record.
- Production must avoid avoidable allocations in hot paths, especially phase limiter loops.
- Prototype creates and destroys vDSP setups during generation.
- Production should cache setup objects per length, as the current IPP path does through `MyIppDftLibrary`.
- Prototype ignores `work_data`.
- Production API already exposes `work_size()` and explicit work buffers; the Apple/non-IPP path must either use that memory deliberately or document that vDSP temporaries/setup are owned elsewhere.
- Prototype reports backend counts.
- Production should not print or log in the audio path.
- Prototype only handles `float Forward`.
- Production must preserve existing symbols and class shape enough for all callers.

Potential build integration difference:

- The tool target links `Accelerate.framework` directly.
- A future production path may require linking `bakuage` or the relevant executable targets with Accelerate on Apple/non-IPP builds.
- That should be handled in a dedicated migration change, not in this review.

## Required Compatibility Conditions

The production migration must preserve these conditions exactly.

Output scalar count:

- Output scalar count must remain `2 * (N / 2 + 1)`.
- No candidate path may write fewer or more floats.
- The output must remain readable as `std::complex<float>[N / 2 + 1]` by existing callers.

CCS public layout:

- Public output must be interleaved:
  - `output[2 * k + 0] = real(H[k])`
  - `output[2 * k + 1] = imag(H[k])`
- Scalar order must match IPP golden, not merely equivalent complex magnitudes.

DC handling:

- `output[0]` is DC real.
- `output[1]` must be `0`.
- Tiny numerical imaginary noise at DC should not leak to callers.

Even Nyquist handling:

- For even `N`, bin `N/2` is the Nyquist singleton.
- `output[N]` must be Nyquist real.
- `output[N + 1]` must be `0`.
- vDSP zrop `Oi[0]` maps to this real Nyquist scalar.

Odd final bin handling:

- For odd `N`, there is no Nyquist singleton.
- Final stored bin is `floor(N/2)`.
- Both real and imaginary scalars of the final stored odd bin must be preserved.
- Do not force the final odd bin imaginary scalar to zero.

Scaling:

- Forward output must remain unnormalized, matching IPP `IPP_FFT_NODIV_BY_ANY`.
- Even zrop path must apply `0.5f`.
- Odd complex path must not apply zrop `0.5f`.
- Any future API substitution must be compared against golden before accepting its scale.

Zero padding:

- Zero padding odd lengths is forbidden for compatibility.
- Transform length must be exactly the requested `N`.
- Changing the transform length changes bin spacing and is not IPP-compatible.

Numerical health:

- No NaN/Inf may appear in generated output.
- No missing output path or unsupported length may be silently accepted.
- Unsupported vDSP setup must fail clearly or route to a proven exact-length compatible backend.

Roundtrip:

- `Forward` golden comparison is necessary but not sufficient for full DFT migration.
- `Backward(Forward(x)) / N` has not been proven for the vDSP production path yet.
- Roundtrip validation is required before replacing Backward or claiming end-to-end DFT equivalence.

## Can Forward Be Migrated First?

Conditional answer: yes, but only as a tightly scoped Apple/non-IPP `RealDft<float>::Forward` migration behind the existing backend selection, and only if the migration immediately reruns the golden comparison against the production implementation.

Reasons it is reasonable to migrate Forward first:

- `RealDft<float>::Forward` has direct golden coverage for the current 99 records.
- The prototype reproduced both even and odd lengths with matching scalar layout.
- The safe implementation order in `docs/dft_comparison_test_plan.md` explicitly allows `Forward` before `Backward` and Perm paths.
- `Forward` is out-of-place in current production code and does not have the in-place Perm branch complexity.

Limits of this approval:

- This is not approval to remove IPP for all DFT paths.
- This is not approval to change `double`.
- This is not approval to change `Backward`, `ForwardPerm`, or `BackwardPerm`.
- This is not approval to change normalization rules.
- This is not approval to use zero padding for any unsupported case.

## Unverified Backward and Perm Risks

Backward:

- Current `Backward` consumes the public CCS layout and returns raw time-domain output with gain `N`.
- vDSP inverse scaling/sign/layout has not been proven against IPP golden.
- Forward-only success does not guarantee `Backward(Forward(x)) / N` parity.

ForwardPerm:

- Current out-of-place `ForwardPerm` uses IPP real DFT Perm layout.
- Current in-place `ForwardPerm` uses IPP real FFT Perm layout when `input == output`.
- Perm layout is not the same as public CCS layout.
- Phase limiter and FIR paths are sensitive to Perm placement and raw scalar layout.

BackwardPerm:

- `GradCore` contains special handling around `spec[1] = spec[len]` before `BackwardPerm`.
- That behavior is a layout dependency and must be proven before touching Perm.
- In-place BackwardPerm is especially risky because current code switches backend based on pointer equality.

Double:

- `RealDft<double>` has separate IPP APIs and tighter numeric expectations.
- Float Forward evidence does not cover double precision.

## Minimal Production Migration Plan

Step 1: Add only `RealDft<float>::Forward` Apple/non-IPP path.

- Guard it so existing IPP builds keep current behavior.
- Do not change non-Apple behavior.
- Do not change `RealDft<double>`.
- Do not change Backward or Perm paths.
- Preserve current public API.
- Preserve exact output scalar layout.
- Cache vDSP setup objects by length.
- Use exact-length legacy complex vDSP for odd lengths if fast complex setup is unavailable.
- Keep zero-padding impossible by construction.

Step 2: Recompare production output against IPP golden.

- Build the production path in a dedicated Apple/non-IPP build directory.
- Generate candidate outputs through the production `RealDft<float>::Forward`, not the prototype helper.
- Compare with `dft_compare_golden`.
- Require all 99 records to generate and compare successfully.

Step 3: Keep other DFT methods untouched.

- Do not implement `Backward`.
- Do not implement `ForwardPerm`.
- Do not implement `BackwardPerm`.
- Do not implement double.
- Record remaining gaps before moving to the next method.

Step 4: Review performance before broad use.

- Measure odd `N=12345` legacy complex vDSP path.
- Compare with current IPP baseline where available.
- Decide whether the legacy path is acceptable for real workloads or needs a future alternative.

## Required Post-Migration Verification

Must run immediately after a production migration:

- Existing IPP golden comparison for all 99 `RealDft<float>::Forward` records.
- Explicit even-length validation:
  - `2`, `4`, `8`, `16`, `1024`
  - DC and Nyquist placement
  - zrop `0.5f` scaling
- Explicit odd-length validation:
  - `3`, `5`, `9`, `12345`
  - no zero padding
  - no final-bin imaginary zeroing
  - no complex-path scaling correction
- NaN/Inf scan.
- Scalar count and byte count check.
- Worst-difference report with scalar index.

Future required verification before further migration:

- `Backward(Forward(x)) / N` roundtrip tests.
- `ForwardPerm` and `BackwardPerm` golden captures and comparisons.
- In-place and out-of-place Perm comparisons.
- Representative phase limiter and audio analyzer lengths beyond the current 99-record minimal set.
- Downstream smoke tests for spectrogram, loudness, GradCore, and FIR convolution users.

## Conditions That Should Block Migration

Do not migrate `RealDft<float>::Forward` yet if any of these are true:

- Production implementation cannot generate all 99 candidate outputs.
- Any generated output has scalar count mismatch.
- Any output contains NaN/Inf.
- Any odd length is handled by zero padding.
- The final stored odd bin imaginary scalar is forced to zero.
- Even zrop path omits the `0.5f` correction.
- Odd complex path applies the zrop `0.5f` correction without new evidence.
- Setup caching or temporary allocation design would allocate excessively in hot audio paths.
- Accelerate linkage is not cleanly limited to Apple/non-IPP builds.
- Golden comparison is not run against the production path after migration.
- The change accidentally modifies Backward, Perm, double, `vector_math.cpp`, or app code.

## Recommendation

Proceed to a narrowly scoped production migration only after accepting this checklist:

- Implement `RealDft<float>::Forward` Apple/non-IPP path only.
- Preserve IPP behavior for IPP builds.
- Preserve exact scalar layout from the prototype.
- Keep even and odd paths separate.
- Re-run golden comparison through production code immediately.
- Treat Backward and Perm as separate future migrations.

The prototype evidence is strong enough to justify the next production experiment for Forward only. It is not strong enough to justify a broader DFT backend replacement.
