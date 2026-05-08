# DFT vDSP Backward Prototype Attempt 01

## Scope

Implemented an Apple-only prototype tool for generating `RealDft<float>::Backward` candidate outputs from the downloaded IPP `minimal_backward` golden inputs.

Changed files:

- `src/tools/dft_vdsp_backward_prototype.cpp`
- `src/tools/dft_compare_golden.cpp`
- `CMakeLists.txt`

Production DSP files intentionally not changed:

- `deps/bakuage/src/dft.cpp`
- `deps/bakuage/src/vector_math.cpp`
- Existing DSP implementation
- `local_mastering_app`

No commit was made for this attempt.

## Initial Status

Initial command:

```sh
git status --short --branch
```

Result:

```text
## mac-arm64-minimal-cli...origin/mac-arm64-minimal-cli
```

## Prototype Tool

Added:

```text
src/tools/dft_vdsp_backward_prototype.cpp
```

Target:

```text
dft_vdsp_backward_prototype
```

The target is available only on Apple and links `Accelerate.framework`.

CLI:

```sh
dft_vdsp_backward_prototype \
  --golden-dir <IPP minimal_backward golden dir> \
  --output-dir <candidate output dir>
```

Input:

- reads `manifest.json`
- requires `precision: float`
- requires `method: Backward`
- requires `input_kind: real_dft_spectrum`
- reads each `inputs/*.bin` spectrum payload

Output:

- writes candidate `outputs/*.bin`
- output scalar count is always `N`
- no production code is called for Backward

## vDSP Paths

Even lengths:

- uses `vDSP_DFT_zrop_CreateSetup(..., N, vDSP_DFT_INVERSE)`
- maps public IPP-compatible scalar layout to zrop split layout
- keeps transform length exactly `N`
- no zero padding
- no extra `2x` input scaling

Odd lengths:

- builds the exact-length conjugate-symmetric complex spectrum
- uses `vDSP_DFT_zop_CreateSetup(..., N, vDSP_DFT_INVERSE)` when available
- falls back to `vDSP_DFT_CreateSetup` + `vDSP_DFT_zop` for exact-length odd transforms
- keeps the final odd bin real/imag values
- keeps transform length exactly `N`
- no zero padding

`attempt01` result on this machine used:

```text
zrop_even=28
zop_odd=0
legacy_zop_odd=24
```

## Compare Tool Note

`dft_compare_golden` previously accepted only `method: Forward` manifests.

For this prototype comparison, it was minimally extended to accept either:

- `Forward`
- `Backward`

The comparison behavior remains otherwise unchanged:

- missing candidate output causes failure
- scalar count mismatch causes failure
- NaN/Inf causes failure
- numeric errors are reported as metrics

## Build

Dedicated build directory:

```text
build_mac_arm64_vdsp_backward_proto_attempt01
```

Configure command:

```sh
cmake -S . -B build_mac_arm64_vdsp_backward_proto_attempt01 \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DDISABLE_TARGET_BENCH=ON \
  -DDISABLE_TARGET_TEST=ON \
  -DBAKUAGE_USE_IPP=OFF \
  -DENABLE_DFT_GOLDEN_COMPARE=ON \
  -DENABLE_DFT_VDSP_BACKWARD_PROTO=ON
```

Configure result:

- Succeeded.

Build command:

```sh
cmake --build build_mac_arm64_vdsp_backward_proto_attempt01 \
  --target dft_vdsp_backward_prototype dft_compare_golden
```

Build result:

- `dft_vdsp_backward_prototype` succeeded.
- `dft_compare_golden` succeeded.

## Candidate Generation

Golden root:

```text
golden/dft_ipp_backward_float_minimal_attempt01/dft-golden-capture-minimal_backward-459edd2988af6dff041ee1665edf9750feb0b723/dft_golden_minimal_backward
```

Command:

```sh
build_mac_arm64_vdsp_backward_proto_attempt01/bin/dft_vdsp_backward_prototype \
  --golden-dir golden/dft_ipp_backward_float_minimal_attempt01/dft-golden-capture-minimal_backward-459edd2988af6dff041ee1665edf9750feb0b723/dft_golden_minimal_backward \
  --output-dir build_mac_arm64_vdsp_backward_proto_attempt01/vdsp_backward_candidate_minimal
```

Result:

```text
generated 52 vDSP RealDft<float>::Backward candidate outputs, unsupported=0
backend counts: zrop_even=28, zop_odd=0, legacy_zop_odd=24
```

Candidate output files:

```text
52
```

## Comparison

Command:

```sh
build_mac_arm64_vdsp_backward_proto_attempt01/bin/dft_compare_golden \
  --golden-dir golden/dft_ipp_backward_float_minimal_attempt01/dft-golden-capture-minimal_backward-459edd2988af6dff041ee1665edf9750feb0b723/dft_golden_minimal_backward \
  --candidate-dir build_mac_arm64_vdsp_backward_proto_attempt01/vdsp_backward_candidate_minimal \
  --output-report build_mac_arm64_vdsp_backward_proto_attempt01/vdsp_backward_compare_report.json
```

Console result:

```text
compared 52 records, failures=0
```

Report summary:

```json
{
  "method": "Backward",
  "record_count": 52,
  "passed_count": 52,
  "failed_count": 0,
  "max_abs_error_global": 0.23046875,
  "rms_error_global": 0.01302288188
}
```

Worst record:

```json
{
  "id": "float_n12345_forward_output_from_time_cases_backward_oop_internal_work",
  "length": 12345,
  "case_name": "forward_output_from_time_cases",
  "max_abs_error": 0.23046875,
  "rms_error": 0.03325344261,
  "status": "pass"
}
```

Top residuals:

```text
12345  forward_output_from_time_cases  0.23046875       0.03325344261
12345  conjugate_safe_noise            0.0006790161133  0.00008138388831
1024   conjugate_safe_noise            0.0000114440918  0.00000225788173
9      forward_output_from_time_cases  0.00000125169754 0.0000004621228913
```

## Attempt01 Classification

Initial even zrop trial used `2x` spectrum input scaling and produced an obvious even-only scaling mismatch. Example:

- `n=4 dc_only` IPP: `[1, 1, 1, 1]`
- `n=4 dc_only` initial candidate: `[2, 2, 2, 2]`

Removing the extra `2x` zrop inverse input scaling fixed the broad even-length mismatch.

Final attempt01 classification:

- scaling mismatch: not present after the even zrop correction
- sign mismatch: not observed
- real/imag swap: not observed
- DC/final bin mismatch: not observed as a broad pattern
- even/odd difference: no broad failure pattern after correction
- remaining residual: largest on `n=12345 forward_output_from_time_cases`, likely implementation-level floating point difference on large unnormalized inverse output

## Current Conclusion

The prototype generated all 52 `minimal_backward` candidate outputs and compared them against IPP golden with:

- compared records: `52`
- failures: `0`
- missing candidate: `0`
- scalar count mismatch: `0`
- NaN/Inf: `0`

This supports continuing Backward investigation with the prototype path, but this attempt does not make any production migration decision and does not modify `deps/bakuage/src/dft.cpp`.
