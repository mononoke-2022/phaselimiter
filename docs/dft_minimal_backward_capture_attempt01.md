# DFT Minimal Backward Capture Attempt 01

## Scope

Implemented a capture-tool-only case-set extension for:

- `src/tools/dft_golden_capture.cpp`
- `--case-set minimal_backward`

This attempt does not change production DSP implementation.

Files intentionally not changed:

- `deps/bakuage/src/dft.cpp`
- `deps/bakuage/src/vector_math.cpp`
- `CMakeLists.txt`
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

## Implementation

`dft_golden_capture` now accepts:

```text
--case-set minimal
--case-set forward_extended
--case-set minimal_backward
```

`minimal` and `forward_extended` remain Forward case sets.

`minimal_backward` is a Backward capture set:

- method: `Backward`
- precision: `float`
- input kind: `real_dft_spectrum`
- input payload: public `RealDft<float>` CCS/interleaved spectrum, scalar count `2 * (N / 2 + 1)`
- output payload: raw unnormalized Backward time-domain result, scalar count `N`

The manifest now includes top-level and per-record:

```json
"input_kind": "real_dft_spectrum"
```

For Forward case sets this field is:

```json
"input_kind": "real_time_domain"
```

## Lengths

`minimal_backward` uses the same length range as the current minimal Forward set:

- `2`
- `3`
- `4`
- `5`
- `8`
- `9`
- `16`
- `1024`
- `12345`

## Spectrum Cases

Implemented spectrum cases:

- `dc_only`
- `single_bin_real`
- `single_bin_imag`
- `final_even_nyquist_real`
- `final_odd_bin_complex`
- `conjugate_safe_noise`
- `forward_output_from_time_cases`

Applicability rules:

- `single_bin_real` and `single_bin_imag` require an ordinary complex bin, so length `2` is skipped for those cases.
- `final_even_nyquist_real` is emitted only for even lengths.
- `final_odd_bin_complex` is emitted only for odd lengths.
- `conjugate_safe_noise` keeps DC imaginary and even-Nyquist imaginary scalars at zero.
- `forward_output_from_time_cases` uses `RealDft<float>::Forward` on the existing `hand_mixed` time-domain waveform and stores that spectrum as Backward input.

Expected record count:

```text
length 2: 4 records
other 8 lengths: 6 records each
total: 52 records
```

## Apple/non-IPP Local Behavior

The current Apple/non-IPP production implementation defines `RealDft<float>::Forward` only.

To keep local Forward capture builds linkable while Backward production is still unimplemented, the Backward execution call is guarded:

- IPP-capable builds can call `RealDft<float>::Backward` and generate the golden payloads.
- Apple/non-IPP builds fail at runtime for `--case-set minimal_backward` with an explicit message instead of silently producing unsupported data.

This keeps the capture tool ready for IPP CI while avoiding changes to `deps/bakuage/src/dft.cpp`.

## Build

Dedicated build directory:

```text
build_mac_arm64_minimal_backward_capture_attempt01
```

Configure command:

```sh
cmake -S . -B build_mac_arm64_minimal_backward_capture_attempt01 \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DDISABLE_TARGET_BENCH=ON \
  -DDISABLE_TARGET_TEST=ON \
  -DBAKUAGE_USE_IPP=OFF \
  -DENABLE_DFT_GOLDEN_CAPTURE=ON
```

Configure result:

- Succeeded.

Build command:

```sh
cmake --build build_mac_arm64_minimal_backward_capture_attempt01 \
  --target dft_golden_capture
```

Build result:

- Succeeded.
- Link emitted warnings about ignoring `/usr/local/lib` x86_64 dylibs while building arm64.
- The requested `dft_golden_capture` target was built successfully.

## Local Checks

Because this Mac build uses `BAKUAGE_USE_IPP=OFF`, it cannot generate Backward IPP golden payloads.

### minimal Forward smoke

Command:

```sh
build_mac_arm64_minimal_backward_capture_attempt01/bin/dft_golden_capture \
  --output-dir build_mac_arm64_minimal_backward_capture_attempt01/prod_forward_candidate_minimal_smoke \
  --case-set minimal \
  --format json-binary
```

Result:

```text
wrote 99 DFT golden capture records to build_mac_arm64_minimal_backward_capture_attempt01/prod_forward_candidate_minimal_smoke
```

### minimal_backward Apple/non-IPP guard

Command:

```sh
build_mac_arm64_minimal_backward_capture_attempt01/bin/dft_golden_capture \
  --output-dir build_mac_arm64_minimal_backward_capture_attempt01/minimal_backward_apple_non_ipp_guard_smoke \
  --case-set minimal_backward \
  --format json-binary
```

Result:

```text
dft_golden_capture error: --case-set minimal_backward requires RealDft<float>::Backward; Apple/non-IPP production build currently has Forward only
```

## Notes For CI

IPP golden generation should be done in an IPP-capable CI environment.

Intended command:

```sh
dft_golden_capture \
  --output-dir dft_golden_minimal_backward \
  --case-set minimal_backward \
  --format json-binary
```

CI acceptance for this case-set:

- manifest `case_set`: `minimal_backward`
- manifest `method`: `Backward`
- manifest `precision`: `float`
- manifest `input_kind`: `real_dft_spectrum`
- record count: `52`
- input payload files: `52`
- output payload files: `52`
- each input scalar count: `2 * (N / 2 + 1)`
- each output scalar count: `N`

The GitHub Actions workflow input currently needs a follow-up option addition before `minimal_backward` can be selected from the manual dispatch dropdown.
