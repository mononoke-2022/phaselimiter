# DFT Golden Capture Workflow Attempt 01

## Scope

- Added a manual GitHub Actions workflow for Linux x64 IPP capture.
- Did not modify `dft.cpp`, `vector_math.cpp`, or other DSP implementation files.
- Did not add vDSP code.
- Did not expand `dft_golden_capture` beyond `RealDft<float>::Forward`.
- Did not run the workflow locally.
- Did not commit.

## Workflow

File:

- `.github/workflows/dft_golden_capture.yml`

Trigger:

- `workflow_dispatch`

Runner:

- `ubuntu-24.04`

Target:

- `dft_golden_capture`

Configure flags:

- `-DBAKUAGE_USE_IPP=ON`
- `-DENABLE_DFT_GOLDEN_CAPTURE=ON`
- `-DDISABLE_TARGET_BENCH=ON`
- `-DDISABLE_TARGET_TEST=ON`

Build command:

```sh
cmake --build "${BUILD_DIR}" --target dft_golden_capture --verbose
```

Capture command:

```sh
"${BUILD_DIR}/bin/dft_golden_capture" \
  --output-dir "${CAPTURE_DIR}" \
  --case-set minimal \
  --format json-binary
```

## Dependencies

System packages:

- `cmake`
- `gcc` / `g++`
- `ninja-build`
- `libboost-all-dev`
- `libtbb-dev`
- `libsndfile1-dev`
- `libarmadillo-dev`
- `libpng-dev`
- `zlib1g-dev`

Intel IPP:

- Adds Intel oneAPI APT repository.
- Installs `intel-oneapi-ipp-devel`.
- Sets `IPPROOT=/opt/intel/oneapi/ipp/latest`.
- Creates `/opt/intel/ipp` symlink for the repository's existing Linux CMake assumptions.

## Artifacts

Uploaded artifact:

- `dft-ipp-golden-${{ github.sha }}`

Contents:

- `dft_golden/manifest.json`
- `dft_golden/inputs/*.bin`
- `dft_golden/outputs/*.bin`
- `env_summary.log`
- `configure_dft_golden_capture.log`
- `build_dft_golden_capture.log`
- `run_dft_golden_capture.log`

## Unverified Points

- Intel oneAPI APT install has not been run in this repository's workflow yet.
- `intel-oneapi-ipp-devel` package layout must be verified on the first run.
- Existing Linux CMake expects `/opt/intel/ipp/lib/intel64`; oneAPI currently installs under `/opt/intel/oneapi/ipp/latest`, so the workflow creates a compatibility symlink.
- Link library names such as `libipps.a`, `libippcore.a`, and `libippvm.a` may differ by oneAPI package/version.
- Full CMake configure still defines many non-capture targets, even though the build step targets only `dft_golden_capture`.
- Linux branch still uses `-std=c++11`; this may become a blocker if CI Boost is too new.
- `optim` external project may still run because `bakuage` depends on it outside Windows.

## Next Check

Run the workflow manually from GitHub Actions.

If it fails, inspect in this order:

1. Intel APT repository setup.
2. IPP header/library paths.
3. CMake configure log.
4. `optim` external project behavior.
5. `dft_golden_capture` link command.
6. Capture runtime output and uploaded artifact contents.

Keep fixes small and prefer workflow-only changes before touching CMake again.
