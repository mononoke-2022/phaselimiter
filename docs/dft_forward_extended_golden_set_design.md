# DFT Forward Extended Golden Set Design

## Scope

Design only. No code, build, test, or commit was performed for this document.

Target:

- Extended IPP golden set for `bakuage::RealDft<float>::Forward`
- Apple/non-IPP production candidate comparison for the already migrated Forward path

Out of scope:

- `deps/bakuage/src/dft.cpp`
- `deps/bakuage/src/vector_math.cpp`
- `CMakeLists.txt`
- Existing DSP implementation
- `local_mastering_app`
- `Backward`, `ForwardPerm`, `BackwardPerm`, Pack paths, and `double`

Initial repository check:

- `git status --short --branch` reported only:
  - `## mac-arm64-minimal-cli...origin/mac-arm64-minimal-cli`

## Current Minimal Set

Current `src/tools/dft_golden_capture.cpp` supports only:

```text
--case-set minimal
```

Current minimal lengths:

- `2`, `3`, `4`, `5`, `8`, `9`, `16`, `1024`, `12345`

Current minimal waveforms:

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

Current record count:

```text
9 lengths * 11 waveforms = 99 records
```

The production candidate already passed all 99 records against the IPP golden artifact. The extended set should not replace this baseline; it should be a separate artifact and case-set.

## Case-Set Name

Use:

```text
forward_extended
```

Reasons:

- It is explicit that this set is still Forward-only.
- It avoids overloading the older generic `extended` name from `docs/dft_capture_tool_spec.md`, which includes future Backward, Perm, and double scope.
- It lets `minimal` remain stable for regression continuity.

## Artifact Location

IPP golden artifact root:

```text
golden/dft_ipp_forward_float_extended_attempt01/
```

Expected expanded artifact shape:

```text
golden/dft_ipp_forward_float_extended_attempt01/
  dft-ipp-forward-float-extended-<commit-or-run-id>/
    dft_golden/
      manifest.json
      inputs/*.bin
      outputs/*.bin
```

Mac production candidate root:

```text
build_mac_arm64_dft_forward_prod_extended_attempt01/prod_forward_candidate_extended/
```

Comparison report:

```text
build_mac_arm64_dft_forward_prod_extended_attempt01/prod_forward_extended_compare_report.json
```

Keep generated golden artifacts out of Git, matching the existing local `golden/` convention.

## Length Set

Use these 14 lengths for `forward_extended` attempt01:

| Group | Lengths | Purpose |
| --- | --- | --- |
| small odd/even boundary | `31`, `32`, `33` | odd complex path vs even zrop path around a power of two |
| medium odd/even boundary | `63`, `64`, `65` | same boundary check at a larger size |
| phase_limiter minimum boundary | `255`, `256`, `257` | `256` is the 44.1k phase_limiter minimum window length |
| large boundary | `4095`, `4096`, `4097` | practical mid-size stress around a common FFT size |
| phase_limiter max | `16384` | `PL_FFT_MAX_LEN` at 44.1k |
| oversample representative | `32768` | `2 * PL_FFT_MAX_LEN`, used by oversample filter length at 44.1k |

Do not include `12345` in this first extended artifact. It remains covered by the existing 99-record minimal golden set and should continue to be treated as a named regression sentinel. A later `forward_extended_attempt02` can add `12345` neighbors if the attempt01 extended report shows odd-length concerns.

## Waveform Set

Use these 28 waveforms for `forward_extended` attempt01.

### Baseline Layout Waveforms

Keep enough of the minimal set to preserve scalar layout diagnostics:

- `zeros`
- `impulse0`
- `impulse1`
- `impulse_last`
- `constant1`
- `ramp`
- `hand_mixed`
- `sine_bin1`
- `cosine_bin1`

Notes:

- `impulse1` is valid for all proposed lengths.
- These waveforms make DC, Nyquist, final odd bin, and real/imag interleaving issues easy to identify.

### Deterministic Noise Seeds

Add multiple deterministic uniform noise cases:

- `noise_seed1`
- `noise_seed2`
- `noise_seed305419896`
- `noise_seed3735928559`
- `noise_seed3237998081`

Generation rule:

- Use the same local xorshift32 style as the existing capture tool.
- Convert the generated `uint32` to `u / 4294967295.0`.
- Store `2 * u - 1` as `float`.
- Do not use `std::uniform_real_distribution`, because implementation details can vary.

### Non-Bin Trigonometric Cases

Add leakage-sensitive cases:

- `sine_nonbin_1p5`
- `sine_nonbin_7p25`
- `sine_nonbin_high`
- `cosine_nonbin_1p5`
- `two_tone_nonbin`

Suggested formulas:

- `sine_nonbin_1p5`: `sin(2*pi*1.5*i/N + 0.25)`
- `sine_nonbin_7p25`: `sin(2*pi*7.25*i/N + 0.125)`
- `sine_nonbin_high`: `sin(2*pi*(N/2 - 1.25)*i/N + 0.375)` for even and odd lengths, using floating frequency
- `cosine_nonbin_1p5`: `cos(2*pi*1.5*i/N + 0.25)`
- `two_tone_nonbin`: `0.6*sin(2*pi*1.5*i/N + 0.25) + 0.4*cos(2*pi*7.25*i/N + 0.125)`

Rationale:

- Non-bin sine/cosine inputs exercise spectral leakage, phase consistency, and large-bin absolute error behavior better than exact-bin tones.

### Pathological Finite Cases

Add:

- `alternating_sign`
- `sparse_impulses`
- `step_half`
- `near_cancellation_pairs`

Suggested formulas:

- `alternating_sign`: `+1, -1, +1, -1...`
- `sparse_impulses`: mostly `0`, with deterministic impulses at `0`, `N/3`, `N/2`, and `N-1`, using mixed signs and amplitudes
- `step_half`: first half `1`, second half `-1`
- `near_cancellation_pairs`: adjacent pairs such as `1.0` and `-1.0 + 1e-6`, repeated

Rationale:

- These stress high-frequency concentration, cancellation, and sparse-bin behavior.

### Very Small, Very Large, And Denormal-Adjacent Cases

Add:

- `tiny_noise_1e-30`
- `tiny_constant_1e-38`
- `subnormal_pattern`
- `large_sine_nonbin_1e10`
- `large_noise_1e10`

Suggested formulas:

- `tiny_noise_1e-30`: deterministic `noise_seed305419896 * 1e-30`
- `tiny_constant_1e-38`: all samples `1e-38`
- `subnormal_pattern`: repeat values near `0`, `1e-45`, `-1e-45`, `1e-40`, `-1e-40`
- `large_sine_nonbin_1e10`: `sine_nonbin_1p5 * 1e10`
- `large_noise_1e10`: `noise_seed305419896 * 1e10`

Rationale:

- Tiny and subnormal-adjacent cases check flush-to-zero and underflow-adjacent behavior without introducing NaN or Inf.
- `1e10` is large enough to expose absolute-error scaling while still far from float overflow after DFT summation at the proposed lengths.

Do not include NaN or Inf inputs in this artifact. They are useful diagnostics, but they are not part of the current Forward compatibility contract.

## Expected Record Count

Attempt01 extended count:

```text
14 lengths * 28 waveforms = 392 records
```

Approximate payload size:

- Sum of input scalar counts across the 14 lengths: about `62,496` floats per waveform.
- Sum of output scalar counts across the 14 lengths: about `62,516` floats per waveform.
- Input plus output payload per waveform: about `125,012` floats, or about `500 KiB`.
- For 28 waveforms: about `14 MiB` of binary payload, plus JSON and artifact overhead.

This is small enough for CI artifacts and local Mac comparison while being much broader than the current 99-record set.

## Manifest Requirements

The current manifest shape is sufficient for `dft_compare_golden`:

- top-level `precision = "float"`
- top-level `method = "Forward"`
- per-record `id`
- per-record `case_name`
- per-record `length`
- per-record `input_file`
- per-record `output_file`
- per-record `input_scalar_count`
- per-record `output_scalar_count`

Recommended additions when implementing `forward_extended`:

- `case_set`: `forward_extended`
- `bytes_per_scalar`: `4`
- `layout`: `ccs_public_complex_interleaved`
- `normalization`: `ipp_no_div_by_any`
- `generation_rule_version`: stable string for waveform definitions

Compatibility rule:

- Do not require these new fields in the first compare-tool update unless needed. Keep old minimal manifests readable.

## CI IPP Golden Generation

Use an IPP-capable Linux x64 CI environment, consistent with `docs/dft_capture_tool_spec.md`.

Intended CI steps:

1. Check out the exact source commit to validate.
2. Configure a dedicated IPP build with golden capture enabled.
3. Build only the narrow target needed for capture.
4. Run:

```sh
dft_golden_capture \
  --output-dir dft_golden \
  --case-set forward_extended \
  --format json-binary
```

5. Package `dft_golden/` as a CI artifact named like:

```text
dft-ipp-forward-float-extended-<commit>
```

6. Store or copy the downloaded artifact locally under:

```text
golden/dft_ipp_forward_float_extended_attempt01/
```

CI acceptance for golden generation:

- The tool writes exactly `392` records.
- No unsupported lengths are skipped.
- Every output payload byte size equals `output_scalar_count * sizeof(float)`.
- The manifest records `precision = "float"` and `method = "Forward"`.
- The CI job does not build or exercise Backward, Perm, Pack, double, or app targets unless required by existing build linkage.

## Mac Production Candidate Generation

On Mac arm64 Apple/non-IPP production build, generate the candidate through production `RealDft<float>::Forward`, not through a standalone prototype.

Intended command shape:

```sh
build_mac_arm64_dft_forward_prod_extended_attempt01/bin/dft_golden_capture \
  --output-dir build_mac_arm64_dft_forward_prod_extended_attempt01/prod_forward_candidate_extended \
  --case-set forward_extended \
  --format json-binary
```

Candidate acceptance before comparison:

- The tool writes exactly `392` records.
- No output has NaN or Inf.
- No output payload has a scalar count mismatch.
- Candidate record IDs and `output_file` paths match the IPP golden manifest.

## Comparison Procedure

Compare the IPP golden artifact to the Mac production candidate:

```sh
build_mac_arm64_dft_forward_prod_extended_attempt01/bin/dft_compare_golden \
  --golden-dir golden/dft_ipp_forward_float_extended_attempt01/dft-ipp-forward-float-extended-<commit>/dft_golden \
  --candidate-dir build_mac_arm64_dft_forward_prod_extended_attempt01/prod_forward_candidate_extended \
  --output-report build_mac_arm64_dft_forward_prod_extended_attempt01/prod_forward_extended_compare_report.json
```

Required pass/fail checks:

- Compared records: `392`
- Missing candidate records: `0`
- Scalar count mismatch records: `0`
- NaN/Inf records: `0`
- Failed records from file/format errors: `0`

Numerical reporting should include at least:

- global max abs error
- global RMS error
- worst record by max abs error
- worst record by RMS error
- per-length summary
- per-waveform summary

Recommended compare-tool enhancement before relying on this set as a gate:

- max relative error for non-zero golden scalars
- worst scalar index
- golden/candidate scalar values at the worst index
- worst complex bin
- phase error for bins above a magnitude floor
- spectral power relative error

## Acceptance Guidance

Hard failures:

- Missing candidate output
- Payload byte-size mismatch
- Manifest record mismatch
- NaN or Inf for finite inputs
- Any even length with incorrect DC or Nyquist singleton placement
- Any odd length with final stored bin imaginary scalar dropped or forced to zero
- Any evidence of zero padding for odd lengths

Numeric acceptance should remain observation-first for attempt01:

- Do not set strict thresholds until the first extended report exists.
- Use the 99-record minimal result as the current reference shape.
- Investigate any systematic odd/even split, especially around `31/32/33`, `255/256/257`, and `4095/4096/4097`.
- Investigate any high relative error in normal-magnitude bins, even when absolute error is small.
- Treat tiny/subnormal cases separately from normal audio-range cases.

## Implementation Notes For Later

When code changes are allowed, implement only the capture/compare tooling needed for this design:

- Add `ForwardExtendedCases()` beside `MinimalCases()`.
- Update `ParseArgs()` to allow `--case-set forward_extended`.
- Keep `minimal` behavior byte-for-byte stable where practical.
- Keep record naming compatible with the current compare tool:

```text
outputs/float_n{N}_{waveform}_forward_oop_internal_work.bin
```

- Preserve `output_scalar_count = 2 * (N / 2 + 1)`.
- Continue using `RealDft<float>::Forward(input, output)` or the current internal-work path.
- Do not add Backward, Perm, Pack, or double in this case-set.

## Recommended Next Step

The next implementation task should be tooling-only:

1. Add `forward_extended` support to `src/tools/dft_golden_capture.cpp`.
2. If needed, extend `src/tools/dft_compare_golden.cpp` with relative and worst-index reporting.
3. Generate the IPP extended golden artifact in CI.
4. Generate the Mac Apple/non-IPP production candidate.
5. Compare and document the extended result before starting Backward.
