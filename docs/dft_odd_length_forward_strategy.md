# DFT Odd-Length Forward Strategy

## Scope

Design only. No production code change is made here.

Target API:

- `RealDft<float>::Forward`
- Odd lengths that failed in vDSP Forward prototype attempt01:
  - `3`
  - `5`
  - `9`
  - `12345`

Non-goals for this document:

- Do not change `deps/bakuage/src/dft.cpp`.
- Do not change `deps/bakuage/src/vector_math.cpp`.
- Do not change existing DSP behavior.
- Do not change `local_mastering_app`.
- Do not modify the attempt01 prototype.
- Do not use zero padding as a compatibility shortcut.

The required compatibility target remains IPP golden output with the same input length, same output scalar count, same public scalar layout, and equivalent unnormalized scaling.

## Current State

`docs/dft_vdsp_forward_prototype_attempt01.md` established:

- vDSP `vDSP_DFT_zrop_CreateSetup(..., N, vDSP_DFT_FORWARD)` works for the tested even lengths.
- vDSP zrop forward has a factor of `2`, corrected by multiplying candidate output by `0.5f`.
- Even-length public layout can be converted as:
  - DC real from `Or[0]`, DC imaginary forced to `0`.
  - Bins `1 <= k < N/2` from `Or[k] + i * Oi[k]`.
  - Nyquist real from `Oi[0]`, Nyquist imaginary forced to `0`.
- Even-length generated records:
  - `55` compared.
  - `55` passed.
  - No scalar count mismatch.
  - No DC/Nyquist or real/imag interleaving mismatch trend.
- Odd-length records:
  - `44` unsupported/missing candidate outputs.
  - Cause: vDSP zrop real DFT requires even real length.

## IPP Behavior From Current Code

Relevant implementation:

- `deps/bakuage/src/dft.cpp`
- `deps/bakuage/include/bakuage/dft.h`

For `RealDft<float>::Forward`:

- `RealDft<float>::RealDft(int len, ...)` creates a `MyIppR2CDft32` for `len`.
- `MyIppR2CDft32` calls `ippsDFTGetSize_R_32f(len, IPP_FFT_NODIV_BY_ANY, ...)`.
- `MyIppR2CDft32` calls `ippsDFTInit_R_32f(len, IPP_FFT_NODIV_BY_ANY, ...)`.
- `RealDft<float>::Forward` calls `ippsDFTFwd_RToCCS_32f`.
- There is no odd-length special case in `Forward`.
- There is no zero padding in `Forward`.
- Scaling is unnormalized, matching `IPP_FFT_NODIV_BY_ANY`.

For `ForwardPerm`:

- Out-of-place `ForwardPerm` calls `ippsDFTFwd_RToPerm_32f` through `MyIppR2CDft32`, so it is DFT based.
- In-place `ForwardPerm` calls `ippsFFTFwd_RToPerm_32f_I` through `MyIppR2CFft32`.
- `MyIppR2CFft32` derives an FFT order from `IntLog2(len)`, so odd/non-power-of-two in-place Perm behavior should not be assumed safe from the `Forward` findings.
- This document does not choose a Perm strategy.

The golden capture confirms that IPP real DFT can produce odd-length `Forward` outputs for the current record set.

## Odd-Length Public Layout

The manifest output scalar count is:

```text
2 * (N / 2 + 1)
```

where `N / 2` is integer floor division.

For odd `N`:

- Stored bins are `0..floor(N/2)`.
- There is no Nyquist singleton.
- Bin `0` is DC and should have imaginary part zero.
- The final stored bin, `floor(N/2)`, is an ordinary complex bin and may have a non-zero imaginary part.
- A correct candidate must write exactly `2 * (floor(N/2) + 1)` floats.

Examples:

- `N=3`: bins `0,1`, output scalar count `4`.
- `N=5`: bins `0,1,2`, output scalar count `6`.
- `N=9`: bins `0,1,2,3,4`, output scalar count `10`.
- `N=12345`: bins `0..6172`, output scalar count `12346`.

Any strategy that changes the transform length changes the bin spacing and cannot be treated as IPP-compatible.

## vDSP API Constraints

From local SDK headers:

- `vDSP_DFT_zrop_CreateSetup` real-to-complex length must be even.
- zrop forward applies a factor `C=2`, already handled in attempt01 for even lengths.
- Complex DFT APIs do not have the real-length-even data layout, but they operate on full complex input/output and must be checked for length support.
- `vDSP_DFT_zop_CreateSetup` documents optimized/implemented length families, not arbitrary odd lengths.
- `vDSP_DFT_Interleaved_CreateSetup` also documents supported length families.

Therefore, a complex-DFT odd-length prototype is plausible but not proven. It must be tested against the actual odd golden records before any production decision.

## Candidate Comparison

| Candidate | IPP Golden Compatibility | Same Scalar Count | CCS / DC / Nyquist Handling | Scaling Risk | Audio Quality Risk | Implementation Difficulty | Complexity | Phase Limiter Practical Risk | Production Suitability |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| A. Odd-only naive DFT, O(N^2) | High mathematically if sign and accumulation are correct; must verify numeric tolerance against IPP | Yes | Straightforward: compute bins `0..floor(N/2)` directly; no Nyquist singleton for odd `N` | Low; direct DFT can use no normalization | Low for correctness, but floating differences must be measured | Low to medium | O(N * floor(N/2)); `N=12345` is about 76M bin/sample products per call | High if odd lengths appear in hot loops; could be severe CPU regression | Good as reference/prototype oracle; risky as production fallback except for very small odd `N` |
| B. Odd-only complex vDSP DFT | High if vDSP supports the exact length and sign matches IPP | Yes | Compute full complex DFT of real input with zero imaginary part, then copy bins `0..floor(N/2)`; no Nyquist singleton | Low to medium; complex vDSP DFT does not have the zrop `C=2` rule, but this must be confirmed by comparison | Low if all golden records pass; no length change | Medium | Expected better than naive for supported lengths; may be slow or unsupported for unfriendly lengths | Medium; setup support/performance for `12345` is unknown | Best next prototype candidate because it avoids new dependencies and preserves exact length |
| C. Odd-only FFTW/KissFFT-style backend | High if configured for unnormalized forward and exact length | Yes | Library full complex or real FFT output can be converted to bins `0..floor(N/2)` | Medium; library conventions must be verified | Low after golden comparison, but dependency integration creates risk | Medium to high | Usually O(N log N), including arbitrary composite lengths depending on library | Medium; dependency footprint, build portability, and licensing must be settled | Good long-term fallback if vDSP complex DFT fails; not the first change because it adds a dependency |
| D. Treat odd lengths as unsupported without IPP fallback | Exact when IPP is available and still used | Yes when falling back to IPP; otherwise no output | IPP preserves current layout | None when falling back to IPP | Lowest audio risk when IPP remains available; high functional risk without IPP | Low | Same as current IPP path when fallback is available | Medium; blocks full IPP removal and can fail on Mac arm64 IPP-free builds | Safe release gate, but not a migration solution |
| E. Zero pad odd length to even and use zrop | Not compatible; changes DFT length and bin spacing | No, unless truncating/hiding data, which is still not equivalent | DC may match for some inputs, but all non-DC bins have different frequencies; odd final bin is not preserved | High | High; spectral values no longer represent the requested transform | Low | O(padded N log N) | High; silent spectral changes are unacceptable | Reject |

## Recommended Strategy

Choose candidate B for the next prototype and as the provisional production direction:

> For odd `RealDft<float>::Forward`, try an exact-length complex vDSP DFT path, convert real input to complex input with zero imaginary part, execute the forward complex DFT, and export only the non-redundant bins `0..floor(N/2)` in the existing public interleaved scalar layout.

Reasons:

- It preserves the requested transform length exactly.
- It preserves the existing output scalar count.
- It does not invent a Nyquist singleton for odd lengths.
- It avoids zero padding.
- It avoids adding a third-party dependency before proving Apple-provided APIs are insufficient.
- It keeps the layout conversion simple and comparable against existing IPP golden files.

This recommendation is conditional:

- If complex vDSP setup is unsupported for any required odd golden length, do not silently fall back to zero padding.
- If complex vDSP is unsupported or too slow for `12345`, production should remain IPP-fallback-only for odd lengths until candidate C is prototyped.
- Candidate A should remain a diagnostic/reference option, not the preferred production implementation for large odd lengths.

## Attempt02 Prototype Proposal

Create a second prototype outside production `dft.cpp`.

Suggested source:

```text
src/tools/dft_vdsp_forward_prototype_attempt02.cpp
```

Suggested target:

```text
dft_vdsp_forward_prototype_attempt02
```

Scope:

- Still only `RealDft<float>::Forward`.
- Read the same `manifest.json` and `inputs/*.bin`.
- Write candidate payloads using each record's existing `output_file`.
- Compare with existing `dft_compare_golden`.

Behavior:

- Even lengths:
  - Keep the attempt01 zrop path and `0.5f` scaling.
- Odd lengths:
  - Build complex input of length `N`: real part from input, imaginary part zero.
  - Execute exact-length complex vDSP forward DFT.
  - Write bins `0..floor(N/2)` as interleaved real/imag floats.
  - Force only DC imaginary to zero if the backend produces tiny numerical noise there.
  - Do not force the final odd bin imaginary to zero.
  - Do not apply the zrop `0.5f` correction unless comparison proves the chosen complex API needs it.

Complex vDSP APIs to evaluate in order:

1. `vDSP_DFT_zop_CreateSetup` + `vDSP_DFT_Execute`, split-complex.
2. If needed for coverage, legacy `vDSP_DFT_CreateSetup` + `vDSP_DFT_zop`, with the same exact-length complex input.
3. Optionally `vDSP_DFT_Interleaved_CreateSetup` + `vDSP_DFT_Interleaved_Execute` for comparison, while documenting macOS version requirements.

Attempt02 must report, per length:

- Setup API used.
- Whether setup succeeded.
- Output scalar count.
- Max absolute error.
- RMS error.
- Worst scalar index.
- Whether the final odd bin imaginary component matches IPP golden.
- Whether any simple scaling factor, sign inversion, or real/imag swap explains a mismatch.

Acceptance for attempting production later:

- All 99 existing `Forward` records generate candidate outputs.
- No scalar count mismatch.
- No NaN/Inf.
- Odd lengths `3`, `5`, `9`, and `12345` compare with tolerances comparable to even attempt01.
- No layout mismatch for DC or the final odd bin.
- No production code change until the above is documented.

## Production Decision Gate

Do not update `deps/bakuage/src/dft.cpp` until one of these is true:

1. Candidate B passes all odd and even `RealDft<float>::Forward` golden records and has acceptable performance for representative odd lengths.
2. Candidate C is prototyped and passes all odd and even `Forward` records with acceptable dependency/licensing/build impact.
3. Production intentionally keeps IPP fallback for odd lengths and clearly fails when no IPP-compatible backend exists.

Candidate E should not be used for production or prototype pass claims.

## Open Risks

- Attempt01 only covered `Forward`; `Backward`, `ForwardPerm`, and `BackwardPerm` still need separate layout proof.
- In-place Perm remains especially risky because current IPP code switches to an FFT path when `input == output`.
- `N=12345` may be rare in phase limiter hot paths, but shared `RealDft` callers include analyzers and tests, so odd lengths cannot be ignored.
- Numeric tolerances must be calibrated after observing odd complex-vDSP differences, not guessed from even zrop alone.
