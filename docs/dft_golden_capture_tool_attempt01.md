# DFT Golden Capture Tool Attempt 01

## Scope

- Added first minimal capture tool structure.
- Added only a guarded CMake target.
- Did not modify `dft.cpp`, `vector_math.cpp`, or other DSP implementation files.
- Did not add vDSP code.
- Did not add GitHub Actions workflow.
- Did not commit.

## Added Tool

Source:

- `src/tools/dft_golden_capture.cpp`

Target:

- `dft_golden_capture`

CMake option:

- `ENABLE_DFT_GOLDEN_CAPTURE`, default `OFF`

## First Capture Scope

Implemented only:

- `RealDft<float>::Forward`
- public overload with internal work
- `--case-set minimal`
- `--format json-binary`

Not implemented yet:

- `double`
- `Backward`
- `ForwardPerm`
- `BackwardPerm`
- explicit-work overloads
- vDSP prototype

## CLI

```sh
dft_golden_capture --output-dir dft_golden --case-set minimal
```

Optional `--format json-binary` is accepted.

## Output

The tool writes:

- `manifest.json`
- `inputs/*.bin`
- `outputs/*.bin`

Binary payloads are raw little-endian IEEE-754 `float` scalar buffers.

## Minimal Cases

Lengths:

- `2, 3, 4, 5, 8, 9, 16, 1024, 12345`

Waveforms:

- `zeros`
- `impulse0`
- `impulse1`
- `impulse_last`
- `constant1`
- `ramp`
- `hand_mixed`
- `sine_bin1`
- `cosine_bin1`
- `sine_nonbin`
- `noise_seed305419896`

## Build

Build was not run in this attempt.

Reason:

- This local machine is Mac arm64 without IPP.
- The capture target is intended for future Linux x64 / IPP CI capture.
- Running a local build would hit the known `dft.cpp` IPP dependency before validating the intended CI path.

## Next Step

Run configure/build on Linux x64 with IPP and `-DENABLE_DFT_GOLDEN_CAPTURE=ON`, building only the `dft_golden_capture` target.
