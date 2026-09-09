# Mac arm64 phase_limiter longer smoke attempt 01

Date: 2026-09-10

Branch: `mac-arm64-minimal-cli`

## Goal

Validate that the Apple/non-IPP `GradCalculator` memLen fix continues to work beyond the short 0.25s and 1s smoke cases.

## Scope

No production code was changed in this attempt. This is a validation-only record after commit `3496d2f`.

## Validation

### `test2.wav`, 3 seconds, default-ish phase

Command shape:

- input: `test_data/test2.wav`
- output: `build_mac_arm64_dft_roundtrip_attempt01/test2_3s_defaultish_phase_iter1_fixed.wav`
- limiting mode: `phase`
- default low/high cut enabled
- default pre-compression enabled
- `max_iter1=1`
- `max_iter2=1`
- `end_at=3`

Result:

- Passed.
- Completed in about 3.7 seconds.
- Output WAV info:
  - channels: 2
  - frames: 132300
  - samplerate: 44100
  - format: `0x00010002`

### `test2.wav`, full length, default-ish phase

Command shape:

- input: `test_data/test2.wav`
- output: `build_mac_arm64_dft_roundtrip_attempt01/test2_full_defaultish_phase_iter1_fixed.wav`
- limiting mode: `phase`
- default low/high cut enabled
- default pre-compression enabled
- `max_iter1=1`
- `max_iter2=1`

Result:

- Passed.
- Completed in about 6.2 seconds.
- Output WAV info:
  - channels: 2
  - frames: 280049
  - samplerate: 44100
  - format: `0x00010002`

## Interpretation

The previous slow path is no longer blocking practical short-file validation. The fixed Apple/non-IPP build can process `test2.wav` full length with default-ish preprocessing and phase limiting when optimization iterations are intentionally capped for smoke testing.

## Suggested next step

Run a fuller quality-oriented smoke with larger `max_iter1` / `max_iter2` values on a short segment, then reconnect this CLI path to the Mac Electron app.
