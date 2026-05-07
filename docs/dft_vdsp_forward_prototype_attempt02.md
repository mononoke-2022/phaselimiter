# DFT vDSP Forward Prototype Attempt 02

## Scope

Implemented and tested the second Apple vDSP prototype for `RealDft<float>::Forward` candidate output generation.

Files changed:

- `src/tools/dft_vdsp_forward_prototype.cpp`
- `docs/dft_vdsp_forward_prototype_attempt02.md`

Production DSP files were not changed:

- `deps/bakuage/src/dft.cpp`: unchanged
- `deps/bakuage/src/vector_math.cpp`: unchanged
- Existing DSP implementation: unchanged
- `local_mastering_app`: unchanged

No production migration is made in this attempt.

## Goal

Attempt01 generated IPP-compatible candidate outputs for even lengths but left odd lengths unsupported because `vDSP_DFT_zrop_CreateSetup` requires an even real length.

Attempt02 tests the strategy from `docs/dft_odd_length_forward_strategy.md`:

- Keep the attempt01 zrop path for even lengths.
- For odd lengths, use an exact-length complex vDSP DFT of length `N`.
- Do not zero-pad.
- Do not change output scalar count.
- Do not apply the zrop `0.5f` scale correction to the complex path.

## Implementation

Even lengths:

- Same as attempt01.
- Uses `vDSP_DFT_zrop_CreateSetup(..., N, vDSP_DFT_FORWARD)`.
- Converts vDSP split real DFT output to public interleaved complex scalars.
- Applies `0.5f` scaling because zrop forward returns `2 * DFT`.

Odd lengths:

- Builds exact-length complex input:
  - real part = input samples
  - imaginary part = `0`
- First tries `vDSP_DFT_zop_CreateSetup` + `vDSP_DFT_Execute`.
- If that setup is unavailable, falls back to legacy `vDSP_DFT_CreateSetup` + `vDSP_DFT_zop`.
- Writes bins `0..floor(N/2)` as interleaved real/imag floats.
- Forces only DC imaginary to `0`.
- Does not force the final odd bin imaginary component to `0`.
- Does not apply zrop `0.5f` scaling.

Observed backend usage:

- `zrop_even`: `55`
- `zop_odd`: `0`
- `legacy_zop_odd`: `44`

The fast complex setup did not cover the current odd golden lengths; the legacy complex DFT path generated all odd records exactly at length `N`.

## Build

Dedicated build directory:

- `build_mac_arm64_vdsp_proto_attempt02`

Configure:

```sh
cmake -S . -B build_mac_arm64_vdsp_proto_attempt02 \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DDISABLE_TARGET_BENCH=ON \
  -DDISABLE_TARGET_TEST=ON \
  -DBAKUAGE_USE_IPP=OFF \
  -DENABLE_DFT_GOLDEN_COMPARE=ON \
  -DENABLE_DFT_VDSP_FORWARD_PROTO=ON
```

Build:

```sh
cmake --build build_mac_arm64_vdsp_proto_attempt02 \
  --target dft_vdsp_forward_prototype dft_compare_golden
```

Result:

- Configure succeeded.
- `dft_vdsp_forward_prototype` build succeeded.
- `dft_compare_golden` build succeeded.

## Generation

Command:

```sh
build_mac_arm64_vdsp_proto_attempt02/bin/dft_vdsp_forward_prototype \
  --golden-dir golden/dft_ipp_forward_float_attempt01 \
  --output-dir build_mac_arm64_vdsp_proto_attempt02/vdsp_candidate_outputs
```

Result:

- Manifest resolved to `golden/dft_ipp_forward_float_attempt01/dft-ipp-golden-b65b62a30707e3f4ff9fe30c87800f0c28f2ad25/dft_golden/manifest.json`.
- Generated candidate payloads: `99`.
- Unsupported records: `0`.
- Candidate output files: `99`.

## Comparison

Command:

```sh
build_mac_arm64_vdsp_proto_attempt02/bin/dft_compare_golden \
  --golden-dir golden/dft_ipp_forward_float_attempt01/dft-ipp-golden-b65b62a30707e3f4ff9fe30c87800f0c28f2ad25/dft_golden \
  --candidate-dir build_mac_arm64_vdsp_proto_attempt02/vdsp_candidate_outputs \
  --output-report build_mac_arm64_vdsp_proto_attempt02/vdsp_compare_report.json
```

Result:

- Compared records: `99`
- Passed records: `99`
- Failed records: `0`
- Global max abs error: `0.013671875`
- Global RMS error: `7.98397704e-05`
- NaN/Inf records: `0`
- Scalar count mismatch records: `0`
- Missing candidate records: `0`

Pass summary by length:

| Length | Passed | Max abs error | Max RMS error |
| --- | ---: | ---: | ---: |
| 2 | 11 | 0 | 0 |
| 3 | 11 | 2.980232239e-08 | 1.490116119e-08 |
| 4 | 11 | 0 | 0 |
| 5 | 11 | 1.192092896e-07 | 5.703162734e-08 |
| 8 | 11 | 2.384185791e-07 | 8.637533633e-08 |
| 9 | 11 | 4.768371582e-07 | 1.752550141e-07 |
| 16 | 11 | 3.135973117e-07 | 1.085113073e-07 |
| 1024 | 11 | 6.103515625e-05 | 3.041595506e-06 |
| 12345 | 11 | 0.013671875 | 0.0001703652743 |

Worst record:

- `float_n12345_sine_nonbin_forward_oop_internal_work`
- Max abs error: `0.013671875`
- RMS error: `0.0001605715517`
- Worst scalar: index `2`
  - IPP: `4569.09765625`
  - vDSP candidate: `4569.111328125`
  - diff: `0.013671875`

## Difference Analysis

Scalar count mismatch:

- Not observed.
- All 99 candidate files were generated with the manifest `output_scalar_count`.

Scaling mismatch:

- The odd complex path was compared without zrop `0.5f` scaling.
- For the worst odd record, normal scaling was clearly best:
  - normal max abs: `0.013671875`
  - candidate `0.5x` max abs: `2284.5419921875`
  - candidate `2x` max abs: `4569.125`
- This supports the expected conclusion that complex vDSP DFT is unnormalized and does not have zrop's factor `C=2`.

Sign mismatch:

- Not observed.
- For the worst odd record, negating all imaginary scalars produced max abs `2222.139892578125`, far worse than normal output.

Real/imag swap:

- Not observed.
- For the worst odd record, swapping each real/imag pair produced max abs `3791.365234375`, far worse than normal output.

DC mismatch:

- Not observed.
- DC imaginary diff was `0` for odd lengths `3`, `5`, `9`, and `12345` after explicitly writing DC imaginary as `0`.

Final odd bin mismatch:

- The final odd bin imaginary component was not forced to zero.
- Max final odd-bin scalar diff by length:
  - `N=3`: `2.9802322387695312e-08`
  - `N=5`: `5.960464477539063e-08`
  - `N=9`: `4.76837158203125e-07`
  - `N=12345`: `0.009765625`
- This is consistent with small numeric backend differences, not a final-bin layout error.

Layout mismatch:

- Not observed.
- Small odd lengths matched at near-float-noise levels.
- Large `N=12345` differences are concentrated in large-magnitude bins and do not match scaling, sign, or interleaving failure patterns.

## Conclusion

Attempt02 proves that the current IPP `RealDft<float>::Forward` golden set can be fully generated on Apple using:

- zrop real DFT path for even lengths, with attempt01 layout conversion and `0.5f` scaling.
- exact-length legacy complex vDSP DFT path for odd lengths, with no zero padding and no zrop scaling.

All 99 golden records compare successfully in observation mode, with no missing outputs, scalar count mismatch, or NaN/Inf.

This still does not authorize production `dft.cpp` changes. Before production migration, the legacy odd path needs performance evaluation, and `Backward`, `ForwardPerm`, and `BackwardPerm` still need their own IPP golden compatibility proof.
