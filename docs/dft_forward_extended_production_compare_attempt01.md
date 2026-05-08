# DFT Forward Extended Production Compare Attempt 01

## Scope

Compared the Apple/non-IPP production `RealDft<float>::Forward` path against the `forward_extended` IPP golden artifact.

No production DSP code was changed.

Files intentionally not changed:

- `deps/bakuage/src/dft.cpp`
- `deps/bakuage/src/vector_math.cpp`
- `CMakeLists.txt`
- `src/tools`
- Existing DSP implementation
- `local_mastering_app`

No commit was made for this attempt.

## Initial Status

Command:

```sh
git status --short --branch
```

Result:

```text
## mac-arm64-minimal-cli...origin/mac-arm64-minimal-cli
```

## IPP Golden

Golden root:

```text
golden/dft_ipp_forward_float_attempt01/dft-golden-capture-forward_extended-49be3451641a25e8c78cdeb1c50b552a70618bb2/dft_golden_forward_extended
```

Manifest summary:

- `case_set`: `forward_extended`
- `precision`: `float`
- `method`: `Forward`
- Records: `392`

## Build

Dedicated build directory:

```text
build_mac_arm64_forward_extended_compare_attempt01
```

Configure command:

```sh
cmake -S . -B build_mac_arm64_forward_extended_compare_attempt01 \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DDISABLE_TARGET_BENCH=ON \
  -DDISABLE_TARGET_TEST=ON \
  -DBAKUAGE_USE_IPP=OFF \
  -DENABLE_DFT_GOLDEN_CAPTURE=ON \
  -DENABLE_DFT_GOLDEN_COMPARE=ON
```

Configure result:

- Succeeded.

Build command:

```sh
cmake --build build_mac_arm64_forward_extended_compare_attempt01 \
  --target dft_golden_capture dft_compare_golden
```

Build result:

- Succeeded.
- Link emitted warnings about ignoring `/usr/local/lib` x86_64 dylibs while building arm64.
- Both requested targets were built successfully.

## Candidate Generation

Command:

```sh
build_mac_arm64_forward_extended_compare_attempt01/bin/dft_golden_capture \
  --output-dir build_mac_arm64_forward_extended_compare_attempt01/prod_forward_candidate_extended \
  --case-set forward_extended \
  --format json-binary
```

Result:

```text
wrote 392 DFT golden capture records to build_mac_arm64_forward_extended_compare_attempt01/prod_forward_candidate_extended
```

Candidate manifest/output checks:

- `case_set`: `forward_extended`
- Records: `392`
- Inputs: `392`
- Outputs: `392`

This candidate was generated through production `dft.cpp` in an Apple/non-IPP build.

## Comparison

Command:

```sh
build_mac_arm64_forward_extended_compare_attempt01/bin/dft_compare_golden \
  --golden-dir golden/dft_ipp_forward_float_attempt01/dft-golden-capture-forward_extended-49be3451641a25e8c78cdeb1c50b552a70618bb2/dft_golden_forward_extended \
  --candidate-dir build_mac_arm64_forward_extended_compare_attempt01/prod_forward_candidate_extended \
  --output-report build_mac_arm64_forward_extended_compare_attempt01/prod_forward_extended_compare_report.json
```

Result:

```text
compared 392 records, failures=0
```

Report summary:

- Compared records: `392`
- Passed records: `392`
- Failed records: `0`
- Missing candidate records: `0`
- Scalar count mismatch records: `0`
- NaN/Inf records: `0`
- Global max abs error: `24117248`
- Global RMS error: `55404.42544`

Worst max-abs and RMS record:

- Record: `float_n4097_large_sine_nonbin_1e10_forward_oop_internal_work`
- Length: `4097`
- Case: `large_sine_nonbin_1e10`
- Max abs error: `24117248`
- RMS error: `495167.15`
- Status: `pass`
- NaN/Inf: `false`

Worst scalar details for that record:

- Scalar count: `4098`
- Worst scalar index: `4`
- IPP golden: `-1.0828161e+13`
- Apple/non-IPP candidate: `-1.08281368e+13`
- Difference: `24117248`
- Relative error at worst scalar: about `2.22727092e-06`

## Per-Length Summary

All 28 records for every tested length passed.

| Length | Records | Passed | Max abs error | Max RMS error |
| ---: | ---: | --- | ---: | ---: |
| `31` | 28 | true | `16384` | `3546.373923` |
| `32` | 28 | true | `6144` | `2177.549621` |
| `33` | 28 | true | `16384` | `4282.73074` |
| `63` | 28 | true | `36864` | `8531.676096` |
| `64` | 28 | true | `16384` | `4413.336043` |
| `65` | 28 | true | `65536` | `9834.951403` |
| `255` | 28 | true | `393216` | `35240.50457` |
| `256` | 28 | true | `131072` | `12707.18848` |
| `257` | 28 | true | `327680` | `30913.31385` |
| `4095` | 28 | true | `18874368` | `439367.3743` |
| `4096` | 28 | true | `1048576` | `65616.09012` |
| `4097` | 28 | true | `24117248` | `495167.15` |
| `16384` | 28 | true | `4194304` | `133979.3793` |
| `32768` | 28 | true | `12582912` | `202544.6355` |

## Assessment

The Apple/non-IPP production `RealDft<float>::Forward` path reproduced the `forward_extended` IPP golden set structurally for all `392` records:

- no missing candidates
- no scalar count mismatch
- no NaN/Inf
- no comparison failures

The largest absolute differences are dominated by the deliberately large-amplitude `1e10` waveforms. The worst observed scalar has relative error around `2.23e-6`, which is consistent with a numeric backend difference rather than a layout, scaling, or missing-output failure.

This validates Forward only. It does not validate `Backward`, `ForwardPerm`, `BackwardPerm`, Pack paths, or `double`.

## Generated Files

Generated build and candidate files are under:

```text
build_mac_arm64_forward_extended_compare_attempt01/
```

These are local verification artifacts and should not be committed.
