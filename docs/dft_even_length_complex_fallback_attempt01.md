# DFT Even Length Complex Fallback Attempt 01

## Scope

This checkpoint narrows the Apple/non-IPP production change to:

- `RealDft<float>::Forward`
- `RealDft<float>::Backward`
- even lengths where vDSP `zrop` setup fails

Files intentionally not changed:

- `ForwardPerm`, `BackwardPerm`, or Pack paths
- `RealDft<double>`
- `vector_math`
- `local_mastering_app`
- full `phase_limiter` CLI portability blockers

## Change

The Apple/non-IPP `RealDft<float>` path keeps the existing vDSP real even-length path when `vDSP_DFT_zrop_CreateSetup` succeeds.

If `zrop` setup fails for an even length, it now falls back to an exact-length complex DFT setup, matching the odd-length fallback strategy. The public CCS layout is preserved:

- DC is stored as a real singleton
- positive-frequency bins remain interleaved real/imag scalars
- even-length Nyquist is stored as a real singleton with imaginary value forced to zero

## Validation

Build:

```text
Built target dft_roundtrip_validate
Built target phase_limiter_dft_audio_smoke
```

Existing synthetic regression:

```text
roundtrip case_set=forward_extended records=392 passed=392 failed=0 nan_or_inf=0
```

New Apple length probe:

```text
roundtrip case_set=apple_length_probe records=80 passed=80 failed=0 nan_or_inf=0
```

Real WAV smoke, full `test5.wav` length `10534`:

```text
phase_limiter_dft_audio_smoke frames=10534 channels=2 max_abs_error=1.72664e-06 max_relative_error=1.72664e-06 rms_error=2.60233e-07 nan_or_inf=0 status=pass
```

Real WAV smoke, `test2.wav --max-frames 10534`:

```text
phase_limiter_dft_audio_smoke frames=10534 channels=2 max_abs_error=1.77879e-06 max_relative_error=1.77879e-06 rms_error=2.63809e-07 nan_or_inf=0 status=pass
```

## Notes

An exploratory run that mixed arbitrary non-power-of-two lengths into the full `forward_extended` stress set exposed a tolerance miss only for a huge-amplitude non-power-of-two case. The regression suite now keeps `forward_extended` unchanged and adds the focused `apple_length_probe` set for this production gap.

The full `phase_limiter` CLI remains blocked by unrelated arm64 portability issues documented in `docs/phase_limiter_smoke_attempt01.md`.
