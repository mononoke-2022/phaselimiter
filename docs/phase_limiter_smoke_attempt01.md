# Phase Limiter Apple Non-IPP Smoke Attempt 01

## Scope

Next checkpoint after `RealDft<float>` Forward/Backward roundtrip validation.

Goal:

- Start a real `phase_limiter` Apple/non-IPP smoke test.
- Do not change production DSP behavior.
- Identify the first full-CLI build/runtime blocker before modifying implementation code.

Files changed:

- `CMakeLists.txt`
- `.gitignore`
- `docs/phase_limiter_smoke_attempt01.md`

Files intentionally not changed:

- `deps/bakuage/src/dft.cpp`
- `deps/bakuage/src/vector_math.cpp`
- `ForwardPerm`, `BackwardPerm`, Pack paths, or `RealDft<double>`
- `local_mastering_app`

## Build Configuration

Existing Apple/non-IPP build directory:

```text
build_mac_arm64_dft_roundtrip_attempt01
```

Configuration used:

```sh
cmake -S . -B build_mac_arm64_dft_roundtrip_attempt01 \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DDISABLE_TARGET_BENCH=ON \
  -DDISABLE_TARGET_TEST=ON \
  -DBAKUAGE_USE_IPP=OFF \
  -DENABLE_DFT_ROUNDTRIP_VALIDATE=ON
```

## CMake Library Path Fix

The local machine has arm64 Homebrew libraries under `/opt/homebrew`, including:

```text
/opt/homebrew/lib/libsndfile.dylib: Mach-O 64-bit dynamically linked shared library arm64
```

Before this attempt, CMake was finding `/usr/local/lib` x86_64 dylibs during arm64 links and `snd_file_info` failed with unresolved `libsndfile` symbols.

The Apple CMake branch now adds:

```text
${LOCAL_PREFIX}/lib
${LOCAL_PREFIX}/include
```

before the existing Boost and Conda search paths. This is build-system-only plumbing and does not change DSP behavior.

## Audio I/O Smoke

Target:

```sh
cmake --build build_mac_arm64_dft_roundtrip_attempt01 --target snd_file_info
```

Result:

```text
Built target snd_file_info
```

Input check:

```sh
build_mac_arm64_dft_roundtrip_attempt01/bin/snd_file_info --input test_data/test5.wav
build_mac_arm64_dft_roundtrip_attempt01/bin/snd_file_info --input test_data/test2.wav
```

Result:

```text
test_data/test5.wav: channels=2 frames=10534 samplerate=44100
test_data/test2.wav: channels=2 frames=280049 samplerate=44100
```

This confirms the local arm64 non-IPP build can link `libsndfile` and read existing WAV test inputs.

## DFT Regression Check

After the CMake path fix:

```text
roundtrip case_set=minimal records=99 passed=99 failed=0 nan_or_inf=0
```

The existing Apple/non-IPP `RealDft<float>` roundtrip checkpoint remains valid.

## Full `phase_limiter` CLI Build

Target:

```sh
cmake --build build_mac_arm64_dft_roundtrip_attempt01 --target phase_limiter
```

Result:

```text
Failed while compiling src/phase_limiter/auto_mastering2.cpp.
```

Primary error:

```text
deps/bakuage/include/bakuage/fir_filter.h includes <immintrin.h>
The AppleClang arm64 header reports:
"This header is only meant to be used on x86 and x64 architecture"
```

Related existing x86 assumptions:

- `deps/bakuage/include/bakuage/fir_filter.h` includes `<immintrin.h>` unconditionally.
- `src/phase_limiter/GradCalculator.h` and `src/phase_limiter/GradCore.h` also include `<immintrin.h>`.
- `CMakeLists.txt` still defines x86 SIMDPP architecture macros globally.
- `src/phase_limiter/main.cpp` calls `ippInit()` and `ippGetLibVersion()` unconditionally, which will be another non-IPP blocker once arm64 SIMD includes are resolved.

## Conclusion

The real `phase_limiter` Apple/non-IPP smoke test has started but cannot yet reach runtime.

Current status:

- DFT roundtrip validation remains green.
- arm64 audio I/O smoke is green.
- full `phase_limiter` CLI build is blocked by existing x86 SIMD assumptions outside the DFT production path.

Next safe step:

- make a narrow Apple/non-IPP CLI portability plan for `phase_limiter` build blockers before touching `GradCore`, `GradCalculator`, `fir_filter`, or `main.cpp`.
