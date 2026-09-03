# Phase Limiter DFT Audio Smoke Attempt 01

## Scope

Adds a narrow real-audio smoke tool for the current Apple/non-IPP checkpoint.

The tool intentionally exercises only:

- WAV load through `libsndfile`
- `RealDft<float>::Forward`
- `RealDft<float>::Backward`
- normalization by `1 / N`
- WAV save through `libsndfile`

Files changed:

- `CMakeLists.txt`
- `src/tools/phase_limiter_dft_audio_smoke.cpp`
- `docs/phase_limiter_dft_audio_smoke_attempt01.md`

Files intentionally not changed:

- `deps/bakuage/src/dft.cpp`
- `deps/bakuage/src/vector_math.cpp`
- `ForwardPerm`, `BackwardPerm`, Pack paths, or `RealDft<double>`
- `src/phase_limiter/main.cpp`
- `src/phase_limiter/GradCore.h`
- `src/phase_limiter/GradCalculator.h`
- `local_mastering_app`

## Why This Tool

The full `phase_limiter` CLI is currently blocked before runtime by existing arm64 portability issues:

- x86-only `<immintrin.h>` includes in phase limiter headers
- x86 SIMDPP architecture assumptions
- unconditional IPP initialization in `src/phase_limiter/main.cpp`

`CutLowAndHighFreq`, resampling, and the full limiter path are not used here because they pull in `ForwardPerm` / `BackwardPerm` and `RealDft<double>`, which are intentionally outside the current migration scope.

This tool provides the next useful smoke level without broadening the DSP surface. It uses local `libsndfile` I/O instead of `phase_limiter::SaveFloatWave` because the existing save helper currently pulls in non-IPP `vector_math` functions that remain outside this step's scope.

## Build

Configure:

```sh
cmake -S . -B build_mac_arm64_dft_roundtrip_attempt01 \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DDISABLE_TARGET_BENCH=ON \
  -DDISABLE_TARGET_TEST=ON \
  -DBAKUAGE_USE_IPP=OFF \
  -DENABLE_DFT_ROUNDTRIP_VALIDATE=ON \
  -DENABLE_PHASE_LIMITER_DFT_AUDIO_SMOKE=ON
```

Build:

```sh
cmake --build build_mac_arm64_dft_roundtrip_attempt01 --target phase_limiter_dft_audio_smoke
```

Run:

```sh
build_mac_arm64_dft_roundtrip_attempt01/bin/phase_limiter_dft_audio_smoke \
  --input test_data/test5.wav \
  --output build_mac_arm64_dft_roundtrip_attempt01/test5_dft_audio_smoke.wav
```

Expected contract:

```text
Backward(Forward(channel)) / N ~= channel
```

## Attempt 01 Result

Build:

```text
Built target phase_limiter_dft_audio_smoke
```

Initial full-input probe:

```sh
build_mac_arm64_dft_roundtrip_attempt01/bin/phase_limiter_dft_audio_smoke \
  --input test_data/test5.wav \
  --output build_mac_arm64_dft_roundtrip_attempt01/test5_dft_audio_smoke.wav
```

Result:

```text
phase_limiter_dft_audio_smoke error: failed to create vDSP RealDft<float>::Forward setup
```

`test5.wav` has `10534` frames. The existing synthetic roundtrip suite still covers arbitrary lengths including `12345`, so this is recorded as a real-audio length probe rather than a production code change trigger.

Power-of-two smoke checks:

```text
phase_limiter_dft_audio_smoke frames=8192 channels=2 max_abs_error=1.2666e-07 max_relative_error=1.2666e-07 rms_error=2.83957e-08 nan_or_inf=0 status=pass
```

Output file check:

```text
test5_dft_audio_smoke_8192.wav: channels=2 frames=8192 samplerate=44100
```

Longer input smoke:

```text
phase_limiter_dft_audio_smoke frames=32768 channels=2 max_abs_error=2.08616e-07 max_relative_error=2.08616e-07 rms_error=3.78412e-08 nan_or_inf=0 status=pass
```

Output file check:

```text
test2_dft_audio_smoke_32768.wav: channels=2 frames=32768 samplerate=44100
```

Regression:

```text
roundtrip case_set=forward_extended records=392 passed=392 failed=0 nan_or_inf=0
```

## Length Probe

Follow-up probing used `test_data/test2.wav` with `--max-frames` to isolate setup behavior without changing production DSP code.

Results:

```text
1000  fail: failed to create vDSP RealDft<float>::Forward setup
1001  pass: max_abs_error=3.04871e-07 rms_error=5.53299e-08
1024  pass: max_abs_error=5.96046e-08 rms_error=1.36841e-08
10534 fail: failed to create vDSP RealDft<float>::Forward setup
10535 pass: max_abs_error=1.9814e-06 rms_error=3.05321e-07
10536 fail: failed to create vDSP RealDft<float>::Forward setup
12344 fail: failed to create vDSP RealDft<float>::Forward setup
12345 pass: max_abs_error=3.09007e-06 rms_error=4.0346e-07
12346 fail: failed to create vDSP RealDft<float>::Forward setup
16384 pass: max_abs_error=1.63913e-07 rms_error=3.43159e-08
32768 pass: max_abs_error=2.08616e-07 rms_error=3.78412e-08
```

This points to the current Apple/non-IPP even-length real DFT setup path accepting power-of-two frame counts but rejecting representative even non-power-of-two audio lengths. Odd non-power-of-two lengths continue to roundtrip through the existing complex DFT fallback path.

## Conclusion

The current Apple/non-IPP `RealDft<float>::Forward` and `Backward` production paths successfully roundtrip real WAV input at representative power-of-two audio frame lengths and write valid WAV output.

At the end of this validation checkpoint, the unresolved real-audio issue was narrowed to even non-power-of-two lengths such as `N=10534` failing vDSP setup on the Apple/non-IPP path, while odd non-power-of-two lengths and power-of-two lengths passed.

Follow-up resolution is documented in `docs/dft_even_length_complex_fallback_attempt01.md`.
