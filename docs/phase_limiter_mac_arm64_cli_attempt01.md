# Phase Limiter Mac arm64 CLI Attempt 01

## Scope

This checkpoint resumes the full `phase_limiter` CLI Apple arm64/non-IPP build after the `RealDft<float>` Forward/Backward audio smoke work.

The migration now includes the minimum additional surfaces required by the CLI:

- x86-only include guards for Apple arm64
- non-IPP `main.cpp` startup
- oneTBB scheduler compatibility
- scalar non-IPP `vector_math` specializations required by the CLI
- Apple/non-IPP `RealDft<double>`
- Apple/non-IPP `RealDft<float/double>` `ForwardPerm` and `BackwardPerm`
- required OptimLib source files for `AutoMastering5`

Still intentionally not implemented:

- Pack paths beyond explicit runtime errors on Apple/non-IPP
- local_mastering_app integration
- audio parity against the original IPP/x86 output

## Build Result

The full CLI now builds:

```text
Built target phase_limiter
```

## Validation

Existing DFT regression:

```text
roundtrip case_set=forward_extended records=392 passed=392 failed=0 nan_or_inf=0
```

Apple length probe:

```text
roundtrip case_set=apple_length_probe records=80 passed=80 failed=0 nan_or_inf=0
```

CLI help starts successfully and reports `IPP disabled`.

Simple limiter smoke:

```text
phase_limiter --input=test_data/test5.wav --output=build_mac_arm64_dft_roundtrip_attempt01/test5_phase_limiter_simple.wav --disable_input_encode=true --end_at=0.05 --limiting_mode=simple --low_cut_freq=0 --high_cut_freq=0 --pre_compression=false --quick_exit=false
```

Output:

```text
channels=2 frames=2205 samplerate=44100 format=0x00010002
```

Phase limiter smoke:

```text
phase_limiter --input=test_data/test5.wav --output=build_mac_arm64_dft_roundtrip_attempt01/test5_phase_limiter_phase.wav --disable_input_encode=true --end_at=0.05 --limiting_mode=phase --low_cut_freq=0 --high_cut_freq=0 --pre_compression=false --max_iter1=1 --max_iter2=1 --quick_exit=false
```

Output:

```text
channels=2 frames=2205 samplerate=44100 format=0x00010002
```

## Notes

`deps/hnsw` is a submodule. The Apple arm64 header fix is stored in the parent repository as `cmake/patches/hnswlib-apple-arm64.patch`, and CMake applies it when configuring an Apple/non-IPP build if needed. This avoids publishing a parent commit that depends on unpublished submodule commits.

This checkpoint proves that the full CLI can compile and run short WAV smoke tests on Apple arm64/non-IPP. It does not prove production audio parity.
