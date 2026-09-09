# Mac arm64 phase_limiter memLen fix attempt 01

Date: 2026-09-10

Branch: `mac-arm64-minimal-cli`

## Goal

Avoid the very slow Apple/non-IPP `RealDft<float>` legacy complex vDSP path hit by default-ish `phase_limiter --limiting_mode=phase` runs around the 0.25s and 1s smoke cases.

## Change

`phase_limiter::GradCalculator` now rounds its internal `memLen` to the next power of two only when building on Apple without IPP.

The previous Apple/non-IPP `memLen` for the problematic 0.25s case could become `57344`. The vDSP path probe showed that this length cannot create either:

- `vDSP_DFT_zrop_CreateSetup`
- `vDSP_DFT_zop_CreateSetup`

The current `RealDft<float>` implementation therefore fell back to the legacy complex vDSP API for that length, which was observed taking more than 60 seconds in a direct DFT probe.

Rounding the phase limiter internal buffer to a power-of-two length makes these RealDft calls land on vDSP's fast real-even setup path. This change is scoped to Apple/non-IPP builds; IPP and non-Apple builds keep the existing sizing.

## Validation

Build:

```txt
cmake --build build_mac_arm64_dft_roundtrip_attempt01 --target phase_limiter dft_roundtrip_validate dft_vdsp_path_probe
```

Result:

- Passed.

DFT roundtrip validation:

```txt
dft_roundtrip_validate --case-set forward_extended
dft_roundtrip_validate --case-set apple_length_probe
```

Results:

```txt
roundtrip case_set=forward_extended records=392 passed=392 failed=0 nan_or_inf=0
roundtrip case_set=apple_length_probe records=80 passed=80 failed=0 nan_or_inf=0
```

vDSP path probe:

```txt
dft_vdsp_path_probe --lengths 49152,57344,65536
```

Result:

```txt
length=49152 forward_zrop=yes forward_zop=yes legacy=yes inverse_zrop=yes inverse_zop=yes production_forward_path=real_even_zrop
length=57344 forward_zrop=no forward_zop=no legacy=yes inverse_zrop=no inverse_zop=no production_forward_path=legacy_complex
length=65536 forward_zrop=yes forward_zop=yes legacy=yes inverse_zrop=yes inverse_zop=yes production_forward_path=real_even_zrop
```

This confirms that avoiding `57344` and rounding to `65536` moves the DFT setup back onto the fast path.

## Audio smoke

Previously slow case:

```txt
phase_limiter --input=test_data/test2.wav --output=build_mac_arm64_dft_roundtrip_attempt01/test2_025s_defaultish_phase_iter1_fixed.wav --disable_input_encode=true --end_at=0.25 --limiting_mode=phase --max_iter1=1 --max_iter2=1 --quick_exit=false
```

Result:

- Passed.
- Completed in about 1.4 seconds.
- Output WAV info:
  - channels: 2
  - frames: 11025
  - samplerate: 44100
  - format: `0x00010002`

Previously blocked longer case:

```txt
phase_limiter --input=test_data/test2.wav --output=build_mac_arm64_dft_roundtrip_attempt01/test2_1s_defaultish_phase_iter1_fixed.wav --disable_input_encode=true --end_at=1 --limiting_mode=phase --max_iter1=1 --max_iter2=1 --quick_exit=false
```

Result:

- Passed.
- Completed in about 2.0 seconds.
- Output WAV info:
  - channels: 2
  - frames: 44100
  - samplerate: 44100
  - format: `0x00010002`

## Notes

This is a pragmatic Apple/non-IPP performance fix. It changes the amount of internal zero-padded context used by the phase limiter on Mac arm64, so exact numerical parity with the previous unsupported slow path is not expected. The tested user-facing output length and WAV format remain correct.
