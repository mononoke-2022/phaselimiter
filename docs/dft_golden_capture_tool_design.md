# DFT Golden Capture Tool Design

## Goal

Define the first minimal C++ target for IPP `RealDft` golden capture.

This is design only. Do not implement yet.

## RealDft API Summary

Public classes:

- `bakuage::RealDft<float>`
- `bakuage::RealDft<double>`

Relevant methods:

- `Forward(input, output)`
- `Backward(input, output)`
- `ForwardPerm(input, output)`
- `BackwardPerm(input, output)`
- Explicit-work overloads exist for the same methods.
- `work_size()` exposes required external work buffer size.

Important behavior:

- `ForwardPerm` / `BackwardPerm` switch implementation when `input == output`, so in-place and out-of-place must be captured separately later.
- `Forward` / `Backward` are the safest first target because they avoid the Perm layout risk.

## First Target Scope

Start with:

- `RealDft<float>::Forward` only.
- Public overload with internal work only.
- Minimal case set only.
- JSON manifest + binary input/output payloads.

Do not include initially:

- `double`
- `Backward`
- `ForwardPerm`
- `BackwardPerm`
- explicit-work overloads
- extended case set

Reason:

- `Forward` validates the public CCS/interleaved complex layout first.
- `float` is the main phase_limiter/audio_analyzer path.
- Perm is the highest-risk layout path and should wait until file format, waveform generation, and artifact upload are proven.

Second pass:

- Add `double Forward`.
- Add `Backward` roundtrip.
- Add `ForwardPerm` out-of-place.
- Add `ForwardPerm` in-place.
- Add `BackwardPerm` only after Perm layout is understood.

## Minimal Case Set

Use a reduced minimal set for the first CI run:

- Lengths: `2,3,4,5,8,9,16,1024,12345`
- Waveforms: `zeros`, `impulse0`, `impulse1`, `impulse_last`, `constant1`, `ramp`, `hand_mixed`, `sine_bin1`, `cosine_bin1`, `sine_nonbin`, `noise_seed305419896`
- Precision: `float`
- Method: `Forward`

Defer:

- `1000`
- `double`
- explicit work
- tiny noise
- extended representative app lengths

## Source Location

Recommended path:

- `src/tools/dft_golden_capture.cpp`

Why:

- It is a tool, not a unit test.
- It should not be mixed into production DSP source.
- It can be guarded by a dedicated CMake option later.

Alternative:

- `src/test/dft_golden_capture.cpp`

Use only if the build system already has easier isolated test-target support.

## CMake Target

Recommended target name:

- `dft_golden_capture`

Future CMake option:

- `ENABLE_DFT_GOLDEN_CAPTURE`

Desired behavior:

- Default `OFF`.
- When `ON`, build only this capture executable and required `bakuage` objects/libraries.
- Avoid building `phase_limiter`, `audio_analyzer`, GUI, bench, or unrelated tools.

## Dependencies

Direct code dependencies:

- `bakuage/dft.h`
- `bakuage/memory.h` if aligned buffers are used
- standard C++ filesystem or simple POSIX directory creation
- JSON writing via a tiny local writer or existing lightweight dependency

Link dependencies:

- `bakuage`
- IPP libraries required by `dft.cpp`: at least `ipps`, `ippcore`, possibly `ippvm`

Avoid if possible:

- gflags
- Boost
- libsndfile
- TBB
- Armadillo
- CImg
- app-level dependencies

If existing `bakuage` static library forces extra deps, later CMake isolation may compile only the small required source subset instead.

## Output Format

Directory layout:

```text
<output-dir>/
  manifest.json
  inputs/
    float_n16_impulse0.bin
  outputs/
    float_n16_impulse0_forward_oop_internal_work.bin
```

Manifest:

- schema: `phase_limiter.dft_golden.v1`
- git commit
- platform summary
- IPP summary if available
- one record per output payload

Payload:

- little-endian IEEE-754 scalar buffers
- input payload scalar count: `N`
- output payload scalar count for `Forward`: `2 * (N / 2 + 1)`
- complex bins represented as raw interleaved scalars

## CLI

Minimal CLI:

```sh
dft_golden_capture --output-dir dft_golden --case-set minimal
```

Optional later:

- `--precision float|double|all`
- `--method forward|all`
- `--format json-binary`

For the first pass, hard-code:

- `case-set=minimal`
- `precision=float`
- `method=Forward`

But keep CLI shape compatible with the full spec.

## Implementation Risks

- Full project `bakuage` target may pull unrelated Linux dependencies.
- Linux CMake still has C++11 in the non-Apple branch, which may conflict with modern Boost if the full project is configured.
- IPP oneAPI library names may not match legacy CMake assumptions.
- `N=12345` may expose IPP/vDSP support differences; keep it because it is valuable.
- Binary endianness and exact float serialization must be stable.
- JSON should not round-trip floating values; binaries are the source of truth.

## Recommended Next Step

Implement only `src/tools/dft_golden_capture.cpp` plus the smallest guarded CMake target needed to build it on Linux x64 with IPP. Keep the first capture to `float Forward` minimal cases.
