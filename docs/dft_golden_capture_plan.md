# PHASE_LIMITER_DFT_GOLDEN_CAPTURE_PLAN

## Goal

Capture IPP `RealDft` golden outputs before designing the vDSP replacement.

Target APIs:

- `RealDft<float>::Forward`
- `RealDft<float>::Backward`
- `RealDft<float>::ForwardPerm`
- `RealDft<float>::BackwardPerm`
- Same for `double`

## Capture Environment Options

### A. x64/Rosetta Mac Build With IPP

Merit:

- Closest to the target Apple platform and compiler behavior.
- Useful for comparing Apple-side file paths and CMake behavior.

Difficulty:

- Likely high on Apple Silicon because IPP install, x64 Homebrew/Boost/libsndfile, and Rosetta CMake toolchain must all line up.
- Current arm64 path has no IPP, so this is not immediately available.

Audio verification trust:

- High if it builds with the same `dft.cpp` and IPP library family.
- Risk is environment friction, not golden quality.

### B. Windows Build / Windows Environment

Merit:

- Existing CMake has explicit Windows IPP libraries: `ippimt.lib`, `ippsmt.lib`, `ippcoremt.lib`, `ippvmmt.lib`.
- Prebuilt Windows dependency layout appears already expected by the project.

Difficulty:

- Medium if a known Windows dev environment already exists.
- Lower risk than reconstructing x64/Rosetta Mac if the historical Windows build is known-good.

Audio verification trust:

- High for IPP layout capture because it exercises the current IPP code path.
- Need to record IPP version and compiler/runtime, but DFT layout should be the key invariant.

### C. Linux x64 IPP Build

Merit:

- CMake already has Linux IPP library/link-directory assumptions.
- Usually easier to automate in a clean VM/container if Intel oneAPI IPP is installable.

Difficulty:

- Medium to high depending on dependency availability.
- Linux block still uses `-std=c++11`; Boost 1.90 may require a separate C++14 adjustment unless older Boost is used.

Audio verification trust:

- High if using the same IPP major version and current `dft.cpp`.
- Good choice for reproducible CI-like golden generation once dependencies are pinned.

## Recommendation

Prefer B or C.

- If a known Windows build machine exists, use B first because project CMake already encodes Windows IPP libs.
- If automation/repeatability matters more, use C in a pinned Linux x64 VM/container.
- Use A only if an x64/Rosetta IPP environment is already working; otherwise the setup cost is too high for first capture.

## Capture Tool Minimum Spec

Inputs:

- Lengths: `1,2,3,4,5,6,7,8,9,10,12,15,16,1024,1000,12345,16384`
- Add phase_limiter/audio_analyzer representative lengths once known.
- Waveforms: zeros, impulse at `0`, impulse at `1`, impulse at `N-1`, constant one, ramp, small hand-checkable mixed-sign array, exact-bin sine, exact-bin cosine, non-bin sine, deterministic white noise, tiny-amplitude noise.
- Precision: `float` and `double`.

Methods:

- `Forward`
- `Backward`
- `ForwardPerm`
- `BackwardPerm`
- Out-of-place Perm and in-place Perm must be captured separately.
- Roundtrip capture: `Backward(Forward(x))`, `BackwardPerm(ForwardPerm(x))`, raw unnormalized output.

Output format:

- Primary: JSON metadata + binary payload files.
- JSON should include: IPP version if available, OS, compiler, git commit, precision, length, waveform, method, in-place flag, scalar count, normalization convention, payload filename, checksum.
- Binary payload should store little-endian IEEE floats/doubles exactly as scalar buffers.
- Optional CSV only for small hand-checkable lengths; do not rely on CSV for full precision golden data.

## vDSP Comparison Flow

1. Capture IPP golden outputs on B or C.
2. Copy golden artifacts to Apple Silicon workspace.
3. Build a standalone vDSP prototype outside production `dft.cpp`.
4. Generate vDSP outputs for the same metadata matrix.
5. Compare scalar buffers first, not only complex views.
6. Validate layout hard requirements: DC, Nyquist, CCS, Perm, in-place Perm final buffer.
7. Validate math metrics: max absolute error, RMS error, relative error, spectral power difference, phase difference, roundtrip gain.
8. Only after all `Forward` cases pass, design production `RealDft::Forward` under `BAKUAGE_USE_IPP=OFF`.
9. Delay `BackwardPerm` until the `GradCore` `spec[1] = spec[len]` behavior is proven equivalent.

## Risk Notes

- Do not generate golden outputs from a reimplemented reference DFT; golden must come from the current IPP `dft.cpp`.
- Do not treat FFTW/vDSP/math-definition agreement as sufficient. The public buffer layout is the compatibility contract.
- Unsupported vDSP lengths must be explicit failures or routed through a verified exact-layout fallback, never approximated.
