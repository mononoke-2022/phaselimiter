# DFT Forward Additional Validation Plan

## Scope

Design only. No code, build, test, or commit was performed for this plan.

Target:

- Apple/non-IPP production path for `bakuage::RealDft<float>::Forward`

Explicitly out of scope for this step:

- Changing `deps/bakuage/src/dft.cpp`
- Changing `deps/bakuage/src/vector_math.cpp`
- Changing `CMakeLists.txt`
- Changing existing DSP implementations
- Changing `local_mastering_app`
- Implementing or validating `Backward`, `ForwardPerm`, `BackwardPerm`, Pack paths, or `double`

Initial repository check:

- `git status --short --branch` reported only:
  - `## mac-arm64-minimal-cli...origin/mac-arm64-minimal-cli`

## Current Forward Result

Reference:

- `docs/dft_forward_production_attempt01.md`

Current evidence:

- `RealDft<float>::Forward` Apple/non-IPP production candidate was compared against the IPP golden artifact.
- Compared records: `99`
- Passed records: `99`
- Failed records: `0`
- Missing candidate records: `0`
- Scalar count mismatch records: `0`
- NaN/Inf records: `0`
- Global max abs error: `0.013671875`
- Global RMS error: `7.98397704e-05`

Even and odd split:

| Parity | Count | Passed | Failed | Max abs error | Max RMS error |
| --- | ---: | ---: | ---: | ---: | ---: |
| Even | 55 | 55 | 0 | `6.103515625e-05` | `3.041595506e-06` |
| Odd | 44 | 44 | 0 | `0.013671875` | `0.0001703652743` |

Worst max-abs record:

- `float_n12345_sine_nonbin_forward_oop_internal_work`
- Max abs error: `0.013671875`
- Worst scalar from prototype analysis:
  - scalar index `2`
  - IPP: `4569.09765625`
  - vDSP candidate: `4569.111328125`
  - diff: `0.013671875`

What this result already supports:

- Current minimal scalar layout is compatible with IPP golden for `Forward`.
- Even zrop conversion, including DC, Nyquist, interleaving, and `0.5f` scale, is likely correct.
- Odd exact-length complex vDSP conversion, including no zero padding and final odd-bin imaginary preservation, is likely correct for the currently tested odd lengths.
- Current comparison found no missing outputs, byte-size mismatches, scalar count mismatches, NaN, or Inf.

What this result does not yet prove:

- It does not cover `N=1`, `6`, `7`, `10`, `12`, `15`, `1000`, `16384`, `32768`, or larger stress lengths.
- It has only one random seed.
- It has only moderate input amplitude variety.
- It does not stress subnormal, denormal-adjacent, near-overflow, cancellation-heavy, or non-finite-rejection behavior.
- It does not validate phase_limiter representative lengths beyond the current overlap with `1024`.
- It does not validate any inverse path or roundtrip behavior.

## Additional Validation Axes

### 1. Length Coverage

Add a new extended Forward golden set with at least:

- Small hand-checkable: `1`, `2`, `3`, `4`, `5`, `6`, `7`, `8`
- Even non-power-of-two: `10`, `12`, `1000`, `1536`, `3000`
- Odd non-power-of-two: `9`, `15`, `999`, `1001`, `12345`
- Powers of two: `16`, `256`, `512`, `1024`, `2048`, `4096`, `8192`, `16384`, `32768`
- Larger stress lengths: `65536`, and one odd/composite length larger than `12345` if runtime remains practical

Rationale:

- Current production phase_limiter code creates window lengths from `fft_min_len()` to `fft_max_len()` by powers of two.
- `PL_FFT_MAX_LEN` is `1 << 14`, so `16384` is a direct phase_limiter length.
- `oversample_filter_fft_len()` is `2 * fft_max_len()`, so `32768` is representative when oversampling paths are exercised.
- Analyzer and utility paths also use arbitrary widths, including the existing `12345` test width.

For every length, require:

- Candidate output file exists.
- Output scalar count is exactly `2 * (N / 2 + 1)`.
- DC imaginary scalar is `0` or exactly matches the IPP public layout.
- Even Nyquist imaginary scalar is `0`.
- Odd final stored bin imaginary scalar is preserved and compared, not forced to zero.

### 2. Random Seeds

Current coverage has only `noise_seed305419896`.

Add deterministic noise cases:

- `noise_seed1`
- `noise_seed2`
- `noise_seed305419896`
- `noise_seed3735928559`
- `noise_seed3237998081`

Use the same local PRNG rule as the current capture tool style, not platform-dependent distributions.

Add amplitude variants:

- Unit noise in `[-1, 1]`
- Low amplitude noise scaled by `1e-12`
- Very low amplitude noise scaled by `1e-30`
- High amplitude noise scaled by `1e10`

Rationale:

- Additional seeds reduce the chance that a layout or scaling issue hides behind one favorable spectrum.
- Amplitude scaling helps separate absolute-error growth from relative-error or cancellation problems.

### 3. Pathological Inputs

Add deterministic waveforms:

- `alternating_sign`: `+1, -1, +1, -1...`
- `single_large_impulse`: one large finite scalar surrounded by zero
- `two_impulses_opposite`: impulses at distant indices with opposite signs
- `sparse_random`: mostly zero with deterministic non-zero samples
- `step_half`: first half `1`, second half `-1`
- `linear_large_ramp`: ramp scaled to a large finite range
- `near_cancellation_pairs`: adjacent samples designed to nearly cancel

Rationale:

- These expose Nyquist handling, high-frequency concentration, cancellation, and bin placement errors better than smooth sine-only inputs.

Non-finite inputs should not be part of the first strict golden set:

- NaN and Inf input behavior is not documented as a public contract here.
- If tested later, keep it in a separate diagnostic set and do not mix it with production compatibility pass/fail.

### 4. Denormal And Subnormal Region

Add cases around the float underflow boundary:

- `tiny_constant_1e-30`
- `tiny_constant_1e-38`
- `tiny_noise_1e-30`
- `tiny_noise_1e-38`
- `subnormal_pattern` using values around `1e-45` to `1e-40`

Report separately:

- max abs error
- RMS error
- number of non-zero candidate scalars where IPP has zero
- number of zero candidate scalars where IPP has non-zero

Interpretation should be careful:

- Hardware and math libraries may flush subnormals differently.
- Subnormal differences are important for stability and NaN avoidance, but they should not automatically block Forward if all normal audio-range cases pass and no NaN/Inf is produced.
- Any denormal-induced performance risk should be handled by measurement later, not inferred only from numeric payloads.

### 5. Very Small And Very Large Values

Add finite amplitude sweeps:

- `1e-20`
- `1e-12`
- `1e-6`
- `1`
- `1e6`
- `1e12`
- `1e20`

Use multiple shapes at those scales:

- constant
- impulse
- sine non-bin
- deterministic noise

Acceptance should include relative metrics, not only max absolute error:

- `max_relative_error` for `abs(golden) > relative_floor`
- RMS error normalized by signal RMS or spectrum RMS
- no NaN/Inf

Rationale:

- The current global max abs error is dominated by a large-magnitude bin. Larger amplitudes will naturally increase absolute differences, so the report must distinguish acceptable float-level drift from a backend mistake.

### 6. Sine/Cosine Non-Bin Cases

Current `sine_nonbin` uses one fractional bin pattern.

Add:

- `sine_nonbin_0p5`
- `sine_nonbin_1p5`
- `sine_nonbin_7p25`
- `sine_nonbin_high`
- `cosine_nonbin_1p5`
- `two_tone_nonbin`
- `chirp_linear`

Rationale:

- Non-bin tones spread energy across many bins and make small phase, sign, and leakage differences visible.
- High-frequency non-bin cases are useful for even Nyquist-adjacent behavior and phase_limiter spectral weighting.

### 7. Even/Odd Boundary Coverage

Add pairs around important lengths:

- `255`, `256`, `257`
- `511`, `512`, `513`
- `1023`, `1024`, `1025`
- `16383`, `16384`, `16385`
- `32767`, `32768`, `32769`

Rationale:

- The production path uses different vDSP APIs for even and odd lengths.
- Adjacent length pairs can expose off-by-one output sizing, final-bin handling, setup support gaps, or accidental zero padding.

### 8. Phase Limiter Representative Lengths

From the current phase_limiter code:

- `PL_FFT_MAX_LEN = 16384`
- `fft_min_len() = 256 * (sample_rate / 44100)`
- `fft_max_len() = 16384 * (sample_rate / 44100)`
- `oversample_filter_fft_len() = 2 * fft_max_len()`
- The constructor requires `sample_rate == 44100 * CeilPowerOf2(sample_rate / 44100)`, so practical tested rates should include `44100`, `88200`, and `176400` if those modes are expected.

Forward validation should include the window lengths actually created by GradCalculator for representative sample rates:

| Sample rate | Forward window lengths |
| ---: | --- |
| `44100` | `256`, `512`, `1024`, `2048`, `4096`, `8192`, `16384` |
| `88200` | `512`, `1024`, `2048`, `4096`, `8192`, `16384`, `32768` |
| `176400` | `1024`, `2048`, `4096`, `8192`, `16384`, `32768`, `65536` |

Oversample-specific Forward lengths should include:

- `32768` for `44100`
- `65536` for `88200`
- `131072` only if runtime and memory are acceptable and that mode is a real target

For phase_limiter-like input shapes, add:

- sqrt-Hanning-windowed noise
- sqrt-Hanning-windowed sine non-bin
- zero-padded segment with active center region
- clipped/prox-like waveform with hard-limited peaks
- lowpass FIR kernel shape used in oversample initialization

## Judgment On `n=12345 sine_nonbin`

The current `n=12345 sine_nonbin` max abs error of `0.013671875` needs additional validation, but it is not by itself evidence of a layout or scaling bug.

Reasons it is probably acceptable as preliminary Forward evidence:

- The same record passed the comparison framework.
- RMS error for the record was `0.0001605715517`, much smaller than the worst absolute bin.
- The worst scalar is a large real bin near `4569`, so the relative error is about `3e-6`.
- Prototype diagnostics showed that `0.5x`, `2x`, imaginary sign flip, and real/imag swap were all far worse than the normal candidate.
- Small odd lengths matched near float-noise levels, which argues against a general odd-layout mistake.

Reasons it still needs follow-up:

- It is the global max-abs worst case.
- It is from the odd legacy complex vDSP path, which is different from the even zrop path.
- Only one large odd length and one non-bin sine family have been tested.
- Audio workflows can be phase and leakage sensitive even when scalar relative error is small.

Recommended follow-up decision:

- Keep `n=12345 sine_nonbin` as a named regression sentinel.
- Add neighboring odd lengths and non-bin frequencies.
- Add per-record relative error and worst-bin metadata to the compare report.
- Compare normalized spectral power and phase error for bins with meaningful magnitude.
- Do not block Forward on this value unless extended cases show a systematic odd-length error trend, NaN/Inf, or phase/power outliers.

## Forward-Only Production Risk While Backward Is Not Migrated

The current Apple/non-IPP production branch defines only the `RealDft<float>::Forward` surface needed by the capture path. This is a narrow compatibility state, not a complete DFT backend.

Risks:

- Any Apple/non-IPP target that links code calling `RealDft<float>::Backward`, `ForwardPerm`, `BackwardPerm`, Pack paths, `RealDft<double>`, complex `Dft<T>`, `Dft2D`, or `Dct2D` may fail to link or fail functionally depending on the build surface.
- Roundtrip behavior is not proven. `Backward(Forward(x)) / N` cannot be claimed for the vDSP production path yet.
- phase_limiter oversample code uses `Forward` together with `Backward`; enabling those runtime paths before inverse support is validated is risky.
- Existing tests such as `src/audio_analyzer/test_dft.cpp` expect `Forward` followed by `Backward` at width `12345`; Forward success alone does not satisfy that contract.
- Perm paths are layout-sensitive and phase_limiter-critical. Forward CCS compatibility does not imply Perm compatibility.
- Mixed states can produce false confidence: a Forward-only golden pass proves capture-tool behavior, not whole-application audio equivalence.

Mitigation until Backward and Perm are ready:

- Treat the current state as Forward-only.
- Keep application builds or code paths that need inverse/Perm outside the acceptance claim.
- Make every validation report state which methods are implemented and which remain absent.
- Before enabling broader app targets, add link-level or smoke-test gates that exercise all referenced `RealDft` methods.

## Proposed Next Tooling

Recommended next document-to-code step, when code changes are allowed:

1. Extend `dft_golden_capture` with a new case set, for example `--case-set forward_extended`.
2. Keep the existing `minimal` set unchanged for reproducibility.
3. Add manifest fields or sidecar report fields for:
   - backend tag
   - explicit/internal work mode
   - output scalar count
   - bytes per scalar
   - input amplitude scale
   - waveform seed
4. Extend `dft_compare_golden` to report:
   - max relative error
   - worst scalar index
   - golden and candidate values at worst index
   - worst complex bin
   - phase error for bins above a magnitude floor
   - spectral power relative error
   - per-length and per-waveform summaries
5. Add a strict mode only after observation data is collected:
   - hard fail for missing candidate, scalar count mismatch, NaN/Inf, DC/Nyquist layout mismatch
   - calibrated numeric thresholds by length and waveform family

Suggested artifact layout:

```text
golden/dft_ipp_forward_float_attempt02/
  .../dft_golden/
    manifest.json
    inputs/*.bin
    outputs/*.bin
```

Suggested candidate layout:

```text
build_.../prod_forward_candidate_extended/
  manifest.json
  inputs/*.bin
  outputs/*.bin
```

Do not replace the original 99-record artifact. Keep it as a stable minimal regression set.

## Mandatory Gates Before Moving To Backward

Before starting the Backward production migration, require:

- The existing 99-record Forward production comparison remains `99/99` pass.
- A new extended Forward golden set is captured from IPP and compared against Apple/non-IPP production output.
- Extended set includes `16384`, `32768`, and other phase_limiter representative lengths.
- Extended set includes both even and odd adjacent boundary lengths.
- Extended set includes multiple random seeds.
- Extended set includes subnormal/denormal-adjacent diagnostics.
- Extended set includes very small and very large finite values.
- Extended set includes multiple non-bin sine/cosine cases.
- No missing candidate records.
- No scalar count or byte count mismatch.
- No NaN/Inf in any normal finite-input case.
- DC and even Nyquist imaginary scalars remain compatible with IPP public layout.
- Odd final-bin imaginary scalars remain compared and preserved.
- `n=12345 sine_nonbin` remains within calibrated relative, RMS, phase, and power expectations.
- Compare report includes enough worst-case metadata to debug failures without manually opening binary payloads.
- Performance risk for large odd legacy complex vDSP lengths is at least estimated, especially if large odd lengths appear in real workloads.
- Documentation clearly states that Backward, Perm, Pack, and double remain separate migrations.

Only after these gates are satisfied should the next task shift to `RealDft<float>::Backward` design/prototype/golden comparison.

## Recommended Next Action

Create the extended Forward capture/compare design as the next implementation task, but keep it outside production DSP code:

- Update or add tooling for `forward_extended` golden generation.
- Generate IPP golden artifacts in the IPP-capable environment.
- Generate Apple/non-IPP production candidates on Mac arm64.
- Compare and document results.

Do not proceed to Backward or Perm until the extended Forward report is reviewed.
