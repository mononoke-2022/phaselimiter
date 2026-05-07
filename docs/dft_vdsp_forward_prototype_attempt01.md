# DFT vDSP Forward Prototype Attempt 01

## Scope

Implemented an Apple-only prototype output generator for comparing vDSP `RealDft<float>::Forward` candidate payloads against captured IPP golden payloads.

Files changed:

- `src/tools/dft_vdsp_forward_prototype.cpp`
- `CMakeLists.txt`
- `docs/dft_vdsp_forward_prototype_attempt01.md`

Production DSP files were not changed:

- `deps/bakuage/src/dft.cpp`: unchanged
- `deps/bakuage/src/vector_math.cpp`: unchanged
- `local_mastering_app`: unchanged

## Tool

Target:

- `dft_vdsp_forward_prototype`

CMake option:

- `ENABLE_DFT_VDSP_FORWARD_PROTO`, default `OFF`
- The target is added only when `APPLE` is true.
- The target links `Accelerate.framework`.

CLI:

```sh
dft_vdsp_forward_prototype \
  --golden-dir <path-to-dft_golden-or-parent> \
  --output-dir <path-to-candidate-root>
```

Input and output layout:

- Reads `<golden-dir>/manifest.json` and each `record.input_file`.
- Writes candidate payloads to `<output-dir>/<record.output_file>`.
- This matches `dft_compare_golden --candidate-dir <output-dir>`.

The tool can resolve the current nested artifact shape when `--golden-dir` points at `golden/dft_ipp_forward_float_attempt01`.

## vDSP Mapping

The prototype uses `vDSP_DFT_zrop_CreateSetup(..., N, vDSP_DFT_FORWARD)` and `vDSP_DFT_Execute`.

Important layout and scaling choices are explicitly documented in code:

- vDSP real input split layout stores even time samples in `Ir[j]` and odd time samples in `Ii[j]`.
- vDSP forward zrop output stores DC real in `Or[0]`.
- vDSP forward zrop output stores even-length Nyquist real in `Oi[0]`.
- Bins `1 <= k < N/2` are split as `Or[k] + i * Oi[k]`.
- Current public candidate output is interleaved complex scalars:
  - `output[2 * k + 0] = real(H[k])`
  - `output[2 * k + 1] = imag(H[k])`
  - `imag(DC)` and `imag(Nyquist)` are forced to zero.
- vDSP zrop forward returns `2 * DFT`; IPP golden was captured with no division, so the prototype multiplies all vDSP outputs by `0.5f`.

Unsupported length handling:

- `vDSP_DFT_zrop_CreateSetup` requires an even real length.
- Odd golden records are left unsupported and no approximate fallback is used.
- This is deliberate; unsupported lengths must not be silently approximated before touching production `dft.cpp`.

## Build

Dedicated build directory:

- `build_mac_arm64_vdsp_proto_attempt01`

Configure:

```sh
cmake -S . -B build_mac_arm64_vdsp_proto_attempt01 \
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
cmake --build build_mac_arm64_vdsp_proto_attempt01 \
  --target dft_vdsp_forward_prototype dft_compare_golden
```

Result:

- Configure succeeded.
- `dft_vdsp_forward_prototype` build succeeded.
- `dft_compare_golden` build succeeded.

## Generation

Command:

```sh
build_mac_arm64_vdsp_proto_attempt01/bin/dft_vdsp_forward_prototype \
  --golden-dir golden/dft_ipp_forward_float_attempt01 \
  --output-dir build_mac_arm64_vdsp_proto_attempt01/vdsp_candidate_outputs
```

Result:

- Manifest resolved to `golden/dft_ipp_forward_float_attempt01/dft-ipp-golden-b65b62a30707e3f4ff9fe30c87800f0c28f2ad25/dft_golden/manifest.json`.
- Generated candidate payloads: `55`.
- Unsupported records: `44`.
- Unsupported lengths: `3`, `5`, `9`, `12345`, all 11 cases each.

## Comparison

Command:

```sh
build_mac_arm64_vdsp_proto_attempt01/bin/dft_compare_golden \
  --golden-dir golden/dft_ipp_forward_float_attempt01/dft-ipp-golden-b65b62a30707e3f4ff9fe30c87800f0c28f2ad25/dft_golden \
  --candidate-dir build_mac_arm64_vdsp_proto_attempt01/vdsp_candidate_outputs \
  --output-report build_mac_arm64_vdsp_proto_attempt01/vdsp_compare_report.json
```

Result:

- Compared records: `99`
- Passed records: `55`
- Failed records: `44`
- Global max abs error: `6.103515625e-05`
- Global RMS error: `5.373500719e-07`
- NaN/Inf records: `0`
- Scalar count mismatch records: `0`
- Missing candidate records: `44`

Pass summary by even length:

| Length | Passed | Max abs error | Max RMS error |
| --- | ---: | ---: | ---: |
| 2 | 11 | 0 | 0 |
| 4 | 11 | 0 | 0 |
| 8 | 11 | 2.384185791e-07 | 8.637533633e-08 |
| 16 | 11 | 3.135973117e-07 | 1.085113073e-07 |
| 1024 | 11 | 6.103515625e-05 | 3.041595506e-06 |

Worst passing record:

- `float_n1024_cosine_bin1_forward_oop_internal_work`
- Max abs error: `6.103515625e-05`
- RMS error: `3.041595506e-06`
- Worst scalar: index `2`, IPP `512.0`, vDSP candidate `511.99993896484375`
- DC and Nyquist scalar positions matched for this record.

## Difference Analysis

Scalar count mismatch:

- Not observed for generated even-length candidate payloads.

Scaling mismatch:

- No obvious 2x scaling mismatch remains after the explicit `0.5f` correction.
- Constant and impulse records pass for generated even lengths, including exact matches for `N=2` and `N=4`.

DC/Nyquist mismatch:

- Not observed for generated even-length records.
- The prototype maps `DC = Or[0]` and `Nyquist = Oi[0]`, with imaginary singleton slots set to zero.

Real/imag interleaving mismatch:

- Not observed for generated even-length records.
- Sine/cosine and impulse records show small numeric differences rather than bin-wide swaps or sign-inverted imaginary components.

Unsupported length mismatch:

- The only record-level failures are missing candidate outputs for odd lengths.
- This is caused by `vDSP_DFT_zrop_CreateSetup` requiring even real lengths.
- A production replacement cannot use this API alone for the existing odd-length behavior.

## Conclusion

For even lengths present in the IPP golden set, the vDSP zrop prototype with `0.5f` scaling and explicit DC/Nyquist conversion produces IPP-compatible candidate layout with small float-level differences.

This attempt does not prove full `RealDft<float>::Forward` compatibility because odd lengths remain unsupported by the chosen vDSP real DFT API. Do not change production `dft.cpp` until odd-length strategy and broader method coverage are resolved.
