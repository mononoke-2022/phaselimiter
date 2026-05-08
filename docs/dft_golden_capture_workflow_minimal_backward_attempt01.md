# DFT Golden Capture Workflow Minimal Backward Attempt 01

## Scope

Updated the GitHub Actions DFT Golden Capture workflow so manual dispatch can select:

- `minimal`
- `forward_extended`
- `minimal_backward`

This attempt changes only:

- `.github/workflows/dft_golden_capture.yml`

Files intentionally not changed:

- `deps/bakuage/src/dft.cpp`
- `deps/bakuage/src/vector_math.cpp`
- `CMakeLists.txt`
- `src/tools`
- Existing DSP implementation
- `local_mastering_app`

No local build or test was run for this workflow-only change.

## Initial Status

Initial command:

```sh
git status --short --branch
```

Result:

```text
## mac-arm64-minimal-cli...origin/mac-arm64-minimal-cli
```

## Workflow Change

Added `minimal_backward` to the existing `workflow_dispatch.inputs.case_set.options` list.

The default remains:

```yaml
default: minimal
```

Existing workflow behavior is preserved:

- capture command passes the selected value to `dft_golden_capture --case-set`
- capture output directory includes the selected case set via `dft_golden_${{ inputs.case_set }}`
- artifact name includes the selected case set via `dft-golden-capture-${{ inputs.case_set }}-${{ github.sha }}`

For `minimal_backward`, the intended artifact name shape is:

```text
dft-golden-capture-minimal_backward-${{ github.sha }}
```

The intended capture directory is:

```text
dft_golden_minimal_backward
```

## Expected minimal_backward Artifact

`src/tools/dft_golden_capture.cpp` already supports:

```sh
dft_golden_capture \
  --output-dir dft_golden_minimal_backward \
  --case-set minimal_backward \
  --format json-binary
```

Expected output in Linux + IPP:

- manifest `case_set`: `minimal_backward`
- manifest `method`: `Backward`
- manifest `precision`: `float`
- manifest `input_kind`: `real_dft_spectrum`
- records: `52`
- input payload files: `52`
- output payload files: `52`

Apple/non-IPP local builds are expected to stop with the explicit Backward guard because production Backward is not implemented there yet.

## Notes

This workflow change only makes the case set selectable. It does not alter IPP installation, build flags, capture command structure, output paths, or artifact upload paths.
