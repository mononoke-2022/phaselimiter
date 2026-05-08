# DFT Golden Capture Workflow Case-Set Input Attempt 01

## Scope

Updated the GitHub Actions DFT golden capture workflow so manual runs can choose the capture case set.

Changed:

- `.github/workflows/dft_golden_capture.yml`

Files intentionally not changed:

- `deps/bakuage/src/dft.cpp`
- `deps/bakuage/src/vector_math.cpp`
- `CMakeLists.txt`
- `src/tools`
- Existing DSP implementation
- `local_mastering_app`

No local build or test was run for this workflow-only attempt.

## Initial Status

Initial command:

```sh
git status --short --branch
```

Result:

```text
## mac-arm64-minimal-cli...origin/mac-arm64-minimal-cli
```

## Workflow Changes

Added a `workflow_dispatch` input:

```yaml
case_set:
  description: DFT golden capture case set
  required: true
  default: minimal
  type: choice
  options:
    - minimal
    - forward_extended
```

The capture output directory now includes the selected case set:

```yaml
CAPTURE_DIR: dft_golden_${{ inputs.case_set }}
```

The capture command now passes the selected case set:

```sh
"${BUILD_DIR}/bin/dft_golden_capture" \
  --output-dir "${CAPTURE_DIR}" \
  --case-set "${{ inputs.case_set }}" \
  --format json-binary
```

The uploaded artifact name now includes the selected case set:

```yaml
name: dft-golden-capture-${{ inputs.case_set }}-${{ github.sha }}
```

## Expected Behavior

Manual workflow runs can now select:

- `minimal`
- `forward_extended`

Expected record counts:

- `minimal`: `99`
- `forward_extended`: `392`

The default remains `minimal`, so the existing manual workflow behavior remains the default path.

## Notes

This workflow depends on the already implemented `dft_golden_capture` support for:

```text
--case-set minimal
--case-set forward_extended
```

The workflow still builds and runs only the capture target. It does not modify or validate Backward, Perm, Pack, double, or app targets.
