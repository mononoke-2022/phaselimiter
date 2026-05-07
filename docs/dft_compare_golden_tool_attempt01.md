# DFT Compare Golden Tool Attempt 01

## Scope

Implemented the first minimal comparison tool for checking candidate `RealDft<float>::Forward` output payloads against IPP golden capture payloads.

Files changed:

- `src/tools/dft_compare_golden.cpp`
- `CMakeLists.txt`
- `docs/dft_compare_golden_tool_attempt01.md`

Production DSP files were not changed.

## Tool

Target:

- `dft_compare_golden`

CMake option:

- `ENABLE_DFT_GOLDEN_COMPARE`, default `OFF`

CLI:

```sh
dft_compare_golden \
  --golden-dir <path-to-dft_golden> \
  --candidate-dir <path-to-candidate-root> \
  --output-report <path>
```

Expected layout:

- Golden manifest: `<golden-dir>/manifest.json`
- Golden outputs: `<golden-dir>/<record.output_file>`
- Candidate outputs: `<candidate-dir>/<record.output_file>`

The candidate directory should therefore contain an `outputs/` directory with files named the same as the golden output payloads.

## Implemented Checks

- Manifest parse using `picojson`
- Supports only `precision=float` and `method=Forward`
- Iterates manifest `records`
- Verifies golden/candidate payload byte size equals `output_scalar_count * sizeof(float)`
- Reports missing candidate files clearly
- Computes per-record max absolute error
- Computes per-record RMS error
- Detects NaN/Inf in either golden or candidate payload
- Writes JSON summary report

## Failure Behavior

The tool returns:

- `0` when all records are readable and finite
- `1` for CLI/manifest/report-level errors
- `2` when any record-level comparison fails

Numeric threshold failure is not enabled yet. This first version is observation-oriented; it records numeric differences without deciding acceptable vDSP tolerance.

## Build

Run locally with dedicated build directory:

- Build directory: `build_mac_arm64_compare_tool_attempt01`
- Configure: succeeded
- Build target: `dft_compare_golden` succeeded
- Self-compare using the IPP golden directory as both golden and candidate: succeeded

```sh
cmake -S . -B build_mac_arm64_compare_tool_attempt01 \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DDISABLE_TARGET_BENCH=ON \
  -DDISABLE_TARGET_TEST=ON \
  -DBAKUAGE_USE_IPP=OFF \
  -DENABLE_DFT_GOLDEN_COMPARE=ON

cmake --build build_mac_arm64_compare_tool_attempt01 --target dft_compare_golden

build_mac_arm64_compare_tool_attempt01/bin/dft_compare_golden \
  --golden-dir golden/dft_ipp_forward_float_attempt01/dft-ipp-golden-b65b62a30707e3f4ff9fe30c87800f0c28f2ad25/dft_golden \
  --candidate-dir golden/dft_ipp_forward_float_attempt01/dft-ipp-golden-b65b62a30707e3f4ff9fe30c87800f0c28f2ad25/dft_golden \
  --output-report build_mac_arm64_compare_tool_attempt01/self_compare_report.json
```

Self-compare result:

```text
compared 99 records, failures=0
```

## Next Step

Generate the first vDSP prototype payloads outside production `dft.cpp`, then compare them against the same IPP golden directory with this tool.
