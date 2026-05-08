# DFT Forward Extended Capture Attempt 01

## Scope

Implemented a capture-tool-only case-set extension for:

- `src/tools/dft_golden_capture.cpp`
- `--case-set forward_extended`

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
```

`minimal` remains the existing 99-record case set.

`forward_extended` follows:

- `docs/dft_forward_extended_golden_set_design.md`
- `docs/dft_forward_additional_validation_plan.md`

Top-level manifest output now includes:

```json
"case_set": "<case-set-name>"
```

The existing manifest fields used by `dft_compare_golden` remain present:

- `precision`
- `method`
- per-record `id`
- per-record `case_name`
- per-record `length`
- per-record `input_file`
- per-record `output_file`
- per-record `input_scalar_count`
- per-record `output_scalar_count`

## Extended Lengths

`forward_extended` uses 14 lengths:

- `31`, `32`, `33`
- `63`, `64`, `65`
- `255`, `256`, `257`
- `4095`, `4096`, `4097`
- `16384`
- `32768`

## Extended Waveforms

`forward_extended` uses 28 waveforms:

- `zeros`
- `impulse0`
- `impulse1`
- `impulse_last`
- `constant1`
- `ramp`
- `hand_mixed`
- `sine_bin1`
- `cosine_bin1`
- `noise_seed1`
- `noise_seed2`
- `noise_seed305419896`
- `noise_seed3735928559`
- `noise_seed3237998081`
- `sine_nonbin_1p5`
- `sine_nonbin_7p25`
- `sine_nonbin_high`
- `cosine_nonbin_1p5`
- `two_tone_nonbin`
- `alternating_sign`
- `sparse_impulses`
- `step_half`
- `near_cancellation_pairs`
- `tiny_noise_1e-30`
- `tiny_constant_1e-38`
- `subnormal_pattern`
- `large_sine_nonbin_1e10`
- `large_noise_1e10`

Expected record count:

```text
14 lengths * 28 waveforms = 392 records
```

## Build

Dedicated build directory:

```text
build_mac_arm64_forward_extended_capture_attempt01
```

Configure command:

```sh
cmake -S . -B build_mac_arm64_forward_extended_capture_attempt01 \
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
cmake --build build_mac_arm64_forward_extended_capture_attempt01 \
  --target dft_golden_capture
```

Build result:

- Succeeded.
- Link emitted warnings about ignoring `/usr/local/lib` x86_64 dylibs while building arm64.
- The requested `dft_golden_capture` target was built successfully.

## Local Generation Checks

Because this Mac build uses `BAKUAGE_USE_IPP=OFF`, the local generation below is a production-candidate smoke check, not IPP golden generation.

### forward_extended

Command:

```sh
build_mac_arm64_forward_extended_capture_attempt01/bin/dft_golden_capture \
  --output-dir build_mac_arm64_forward_extended_capture_attempt01/prod_forward_candidate_extended \
  --case-set forward_extended \
  --format json-binary
```

Result:

```text
wrote 392 DFT golden capture records to build_mac_arm64_forward_extended_capture_attempt01/prod_forward_candidate_extended
```

Manifest/output checks:

- Manifest `case_set`: `forward_extended`
- Manifest record IDs: `392`
- Output payload files: `392`
- Input payload files: `392`

### minimal smoke

Command:

```sh
build_mac_arm64_forward_extended_capture_attempt01/bin/dft_golden_capture \
  --output-dir build_mac_arm64_forward_extended_capture_attempt01/prod_forward_candidate_minimal_smoke \
  --case-set minimal \
  --format json-binary
```

Result:

```text
wrote 99 DFT golden capture records to build_mac_arm64_forward_extended_capture_attempt01/prod_forward_candidate_minimal_smoke
```

Manifest check:

- Manifest `case_set`: `minimal`
- Manifest record IDs: `99`

## Notes For CI

IPP golden generation should still be done in an IPP-capable CI environment, not from this local Mac Apple/non-IPP build.

Intended CI command:

```sh
dft_golden_capture \
  --output-dir dft_golden \
  --case-set forward_extended \
  --format json-binary
```

CI acceptance for this case-set:

- `392` records are written.
- No unsupported lengths are skipped.
- `manifest.json` contains `case_set: forward_extended`.
- `precision` remains `float`.
- `method` remains `Forward`.
- All output payload sizes match `output_scalar_count * sizeof(float)`.

## Current Limit

This attempt only extends the capture tool case set. It does not generate an IPP extended golden artifact and does not compare Mac production output against IPP extended golden, because the local build was Apple/non-IPP.
