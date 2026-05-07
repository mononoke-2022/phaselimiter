# DFT Compare Golden Tool Design

## Goal

Design a small comparison tool for checking future vDSP prototype outputs against captured IPP `RealDft<float>::Forward` golden outputs.

This is design only. Do not modify production `dft.cpp`, `vector_math.cpp`, or CMake behavior yet.

## Current Golden Artifact Shape

Observed artifact root:

```text
golden/dft_ipp_forward_float_attempt01/
  dft-ipp-golden-b65b62a30707e3f4ff9fe30c87800f0c28f2ad25/
    dft_golden/
      manifest.json
      inputs/*.bin
      outputs/*.bin
```

`golden/` is local generated data and must stay out of Git.

Manifest top-level fields currently present:

- `schema`
- `tool_name`
- `git_commit`
- `precision`
- `method`
- `endianness`
- `records`

Record fields currently present:

- `id`
- `precision`
- `method`
- `case_name`
- `length`
- `input_file`
- `output_file`
- `input_scalar_count`
- `output_scalar_count`

Current record set:

- Records: `99`
- Precision: `float`
- Method: `Forward`
- Lengths: `2,3,4,5,8,9,16,1024,12345`
- Cases: `zeros`, `impulse0`, `impulse1`, `impulse_last`, `constant1`, `ramp`, `hand_mixed`, `sine_bin1`, `cosine_bin1`, `sine_nonbin`, `noise_seed305419896`

## Payload Naming

Input payloads:

```text
inputs/float_n{N}_{case}.bin
```

Output payloads:

```text
outputs/float_n{N}_{case}_forward_oop_internal_work.bin
```

Payload format:

- Raw little-endian IEEE-754 `float`
- No text header
- Scalar buffer only
- `input_scalar_count == N`
- `output_scalar_count == 2 * (N / 2 + 1)`

## Tool Purpose

The comparison tool should answer one narrow question:

Can a candidate backend output, initially from a vDSP prototype outside production `dft.cpp`, reproduce the IPP golden scalar output layout and values for `RealDft<float>::Forward`?

It must detect:

- Missing candidate output files
- Scalar count mismatch
- Byte size mismatch
- NaN or Inf in golden or candidate output
- Max absolute error
- RMS error
- Max relative error for non-zero golden scalars
- Likely layout mismatch, especially DC/Nyquist placement and even/odd CCS shape

## Initial Scope

Include only:

- `RealDft<float>::Forward`
- Out-of-place public output layout
- Internal-work capture records
- Existing 99-record artifact shape

Do not include initially:

- `double`
- `Backward`
- `ForwardPerm`
- `BackwardPerm`
- In-place paths
- Explicit-work variants
- Automatic vDSP execution

The first implementation should compare already-written binary candidate outputs, not call DFT code directly.

## CLI Proposal

Recommended source path:

```text
src/tools/dft_compare_golden.cpp
```

Recommended CMake target:

```text
dft_compare_golden
```

Suggested CLI:

```sh
dft_compare_golden \
  --golden-manifest golden/.../dft_golden/manifest.json \
  --candidate-output-dir vdsp_outputs \
  --summary-json compare_summary.json \
  --text-report compare_report.txt
```

Candidate lookup rule:

- Read each golden record from `manifest.json`.
- Use `record.output_file` basename by default.
- Look for the candidate at:

```text
<candidate-output-dir>/<record.output_file>
```

Example:

```text
vdsp_outputs/outputs/float_n16_impulse0_forward_oop_internal_work.bin
```

Optional later:

- `--candidate-manifest`
- `--fail-on-threshold`
- `--case-filter`
- `--length-filter`
- `--dump-first-difference`

## Output Files

Summary JSON:

```json
{
  "schema": "phase_limiter.dft_compare.v1",
  "golden_manifest": "...",
  "candidate_output_dir": "...",
  "precision": "float",
  "method": "Forward",
  "record_count": 99,
  "passed_count": 99,
  "failed_count": 0,
  "max_abs_error_global": 0.0,
  "rms_error_global": 0.0,
  "records": []
}
```

Per-record metrics:

```json
{
  "id": "float_n16_impulse0_forward_oop_internal_work",
  "length": 16,
  "case_name": "impulse0",
  "golden_file": "outputs/float_n16_impulse0_forward_oop_internal_work.bin",
  "candidate_file": "outputs/float_n16_impulse0_forward_oop_internal_work.bin",
  "scalar_count": 18,
  "max_abs_error": 0.0,
  "rms_error": 0.0,
  "max_relative_error": 0.0,
  "nan_or_inf": false,
  "layout_check": "ok",
  "status": "pass"
}
```

Text report:

- One short global summary
- Worst 10 records by max absolute error
- Worst 10 records by RMS error
- Missing or size-mismatched records first
- Layout mismatch suspects first if detected

## Numeric Metrics

For each scalar index `i`:

- `diff = candidate[i] - golden[i]`
- `abs_error = abs(diff)`
- `rms_error = sqrt(sum(diff * diff) / scalar_count)`
- `relative_error = abs(diff) / max(abs(golden[i]), relative_floor)`

Suggested `relative_floor`:

- `1e-20f` for float

NaN/Inf:

- Any NaN or Inf in either buffer is a hard failure.

Scalar count:

- Candidate byte size must equal `record.output_scalar_count * 4`.
- Golden byte size must equal `record.output_scalar_count * 4`.

## Initial Tolerance Proposal

Use two modes:

- Observation mode: always exits success if files are readable, writes metrics, and does not enforce numeric thresholds.
- Strict mode: exits failure on threshold violations.

Initial strict thresholds for `RealDft<float>::Forward`:

- `max_abs_error <= 2e-4 * max(1, sqrt(N))`
- `rms_error <= 2e-5 * max(1, sqrt(N))`
- `max_relative_error <= 2e-4` where `abs(golden) > 1e-6`

These thresholds are provisional. Before using strict mode for production migration, calibrate with:

- IPP artifact rerun from the same commit.
- Small hand-checkable cases.
- Known vDSP prototype output differences.

Any scalar count mismatch, missing file, NaN/Inf, or DC/Nyquist placement mismatch is a hard failure regardless of numeric thresholds.

## Layout Checks

For the current `Forward` CCS public output:

- Treat the output as raw interleaved scalar sequence first.
- Do not reinterpret layout to hide scalar mismatches.
- Check even lengths for DC/Nyquist singleton placement expectations.
- Check odd lengths do not assume a separate Nyquist singleton.
- Compare scalar-by-scalar before any complex-bin derived metrics.

Initial layout mismatch detection:

- If numeric error is tiny after shifting by one scalar or swapping adjacent real/imag scalars, report `layout_suspect`.
- If DC-like energy appears at a non-zero expected index for `constant1`, report `dc_suspect`.
- If even-length Nyquist-like value differs while neighboring bins match, report `nyquist_suspect`.

These are diagnostics only. The source of truth remains exact scalar order matching the IPP golden payload.

## Implementation Dependencies

Prefer standard C++ only:

- `fstream`
- `vector`
- `cmath`
- `cstdint`
- `cstring`
- `limits`
- simple JSON reader/writer strategy

JSON parsing options:

- Reuse existing lightweight JSON dependency if already available to tool targets.
- Otherwise implement a minimal manifest reader only if the manifest shape remains fixed.

Avoid:

- IPP
- Accelerate/vDSP
- libsndfile
- Boost
- app-level dependencies

The comparison tool should be runnable on Mac arm64, because the candidate vDSP side is expected there.

## Risks Before Implementation

- Current manifest is intentionally smaller than the full capture spec; future fields such as `layout`, `bytes_per_scalar`, and `sha256` are absent.
- The artifact path has an extra GitHub artifact-name directory, so CLI should accept explicit `manifest.json` rather than assuming a fixed root.
- Candidate output naming must match golden `output_file` paths exactly, or the tool needs a backend-tag mapping rule.
- Tolerances are not final; strict pass/fail should wait until observation data exists.
- Layout heuristics can identify suspicious differences but must not rewrite or normalize candidate output silently.

## Recommended Next Step

Implement `dft_compare_golden` as a read-only file comparison tool for existing binary payloads, starting with observation mode for the 99 `RealDft<float>::Forward` records. Keep it independent from production `dft.cpp` and from any vDSP implementation.
