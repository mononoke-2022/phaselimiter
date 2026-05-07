# DFT GitHub Actions Capture Workflow Plan

## Goal

Generate IPP `RealDft` golden outputs on a reproducible Linux x64 CI environment, then use the artifact on Mac arm64 for vDSP comparison.

Do not generate IPP golden outputs on Mac arm64.

## Inputs

Reference docs:

- `docs/dft_capture_tool_spec.md`
- `docs/dft_comparison_test_plan.md`
- `docs/dft_golden_capture_plan.md`
- `docs/dft_ipp_analysis.md`

Capture target:

- `RealDft<float/double>::Forward`
- `RealDft<float/double>::Backward`
- `RealDft<float/double>::ForwardPerm`
- `RealDft<float/double>::BackwardPerm`

## Runner

Recommended first target:

- GitHub Actions `ubuntu-latest` or a pinned Ubuntu x64 image.

Prefer pinning once confirmed:

- Example: `ubuntu-24.04` or `ubuntu-22.04`

Reason:

- No local Windows PC needed.
- Reproducible enough for rerunning golden capture from the same commit.
- Easy artifact upload.
- Linux x64 can install Intel oneAPI IPP from Intel's APT repository.

## Dependencies

Likely required:

- `cmake`
- `ninja-build` or GNU Make
- `gcc` / `g++` or `clang`
- `boost` development packages
- `tbb` development/runtime packages
- `libsndfile` development package
- `libarmadillo-dev`
- `libpng-dev`
- `zlib1g-dev`
- Intel oneAPI IPP development package

Notes:

- The capture tool should be as small as possible to avoid needing full `phase_limiter` dependencies.
- If the capture target links only `bakuage` plus IPP, many app-level dependencies may become unnecessary.
- Existing Linux CMake path assumes IPP-style libraries and may need environment variables such as `IPPROOT`.

## Intel oneAPI IPP Install Candidates

Candidate A, preferred:

- Add Intel oneAPI APT repository.
- Install `intel-oneapi-ipp-devel`.
- Source oneAPI environment if needed, or set `IPPROOT` explicitly.

Why:

- Installs IPP headers and libraries without requiring the full oneAPI toolkit.
- Official Intel IPP download docs list `intel-oneapi-ipp-devel` for APT.

Candidate B:

- Install `intel-oneapi-toolkit`.

Why:

- Broadest official package.
- Higher install time and cache/artifact pressure.

Candidate C:

- Install conda packages from Intel/conda-forge channels, such as `ipp-devel` or `ipp-static`.

Why:

- May be easier to isolate.
- Must verify library names and CMake discovery against this repository.

Candidate D:

- Use static IPP package if dynamic linking is painful.

Why:

- Easier artifact/runtime behavior.
- Link command may need explicit static libraries and extra system deps.

Official references to verify before implementation:

- Intel oneAPI Linux APT install guide
- Intel IPP download page and package list

## Job Shape

Single job initially:

```yaml
name: Capture DFT IPP Golden

on:
  workflow_dispatch:

jobs:
  capture-dft-golden:
    runs-on: ubuntu-24.04
```

Steps:

1. `checkout`
2. Submodule init/update if needed.
3. Install system dependencies.
4. Add Intel oneAPI APT repo.
5. Install `intel-oneapi-ipp-devel`.
6. Print environment diagnostics: compiler, CMake, Boost, IPP paths.
7. Configure only what is needed for capture.
8. Build only the capture tool.
9. Run capture:

```sh
dft_golden_capture --output-dir dft_golden --case-set minimal --format json-binary
```

10. Upload artifact.

Later split, if useful:

- `prepare-deps`
- `build-capture-tool`
- `run-capture`
- `upload-artifact`

But start with one job to keep debugging simple.

## Configure Strategy

Preferred:

- Add a future dedicated capture target that does not build all executables.
- Keep it guarded so normal builds are unaffected.

Possible configure flags:

- `-DBAKUAGE_USE_IPP=ON`
- `-DDISABLE_TARGET_BENCH=ON`
- `-DDISABLE_TARGET_TEST=ON`
- Any future flag like `-DENABLE_DFT_GOLDEN_CAPTURE=ON`

Avoid:

- Building `phase_limiter`, `audio_analyzer`, GUI, or unrelated tools.
- Depending on Mac arm64 non-IPP build settings.

## Artifact Contents

Artifact name:

- `dft-ipp-golden-${{ github.sha }}`

Contents:

- `manifest.json`
- `inputs/*.bin`
- `outputs/*.bin`
- capture stdout/stderr log
- configure log
- build log
- environment summary:
  - OS image
  - compiler version
  - CMake version
  - IPP package/version if available
  - `IPPROOT`
  - git SHA

Retention:

- Use GitHub Actions artifact retention for exploratory runs.
- Promote known-good artifacts manually if they become baseline references.

## Failure Risks

- Intel oneAPI package names or repo setup may change.
- GitHub-hosted runner image may change, so pin Ubuntu version after first success.
- Full project configure may pull unnecessary dependencies; a narrow capture target is safer.
- Existing Linux CMake still uses C++11, which may conflict with modern Boost if full `bakuage` is built.
- IPP library names/link directories may differ between old project assumptions and oneAPI packages.
- `optim` external project and space-containing paths may cause unrelated failures if full project is configured.
- Golden artifacts can become large for the extended case set.

## Recommended First Implementation Pass

1. Implement the capture tool as a narrow target.
2. Add a manual-only GitHub Actions workflow using Ubuntu x64.
3. Install `intel-oneapi-ipp-devel` from Intel APT repo.
4. Run only the minimal case set.
5. Upload artifact and inspect `manifest.json` plus a few small binary payloads.

Do not start with the extended case set or production vDSP changes.
