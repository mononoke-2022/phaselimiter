# DFT Backward Validation Strategy

## Scope

Design only. No code, build, test, or commit was performed for this document.

Target:

- Future validation and migration strategy for `bakuage::RealDft<float>::Backward`

Explicitly out of scope for this step:

- Changing `deps/bakuage/src/dft.cpp`
- Changing `deps/bakuage/src/vector_math.cpp`
- Changing `CMakeLists.txt`
- Changing `src/tools`
- Changing existing DSP implementation
- Changing `local_mastering_app`
- Implementing `Backward`
- Implementing `ForwardPerm`, `BackwardPerm`, Pack paths, or `double`

Initial repository check:

- `git status --short --branch` reported only:
  - `## mac-arm64-minimal-cli...origin/mac-arm64-minimal-cli`

## Current Forward Evidence

`RealDft<float>::Forward` Apple/non-IPP production path has passed:

- minimal IPP golden set: `99 / 99`
- `forward_extended` IPP golden set: `392 / 392`

Observed Forward validation status:

- failures: `0`
- missing candidate: `0`
- scalar count mismatch: `0`
- NaN/Inf: `0`

This proves the public Forward CCS/interleaved scalar layout for the tested cases. It does not prove inverse scaling, inverse sign, half-spectrum consumption, or roundtrip behavior.

## Current Implementation Relationship

In the IPP implementation branch of `deps/bakuage/src/dft.cpp`, `RealDft<float>` owns both:

- `MyIppR2CDft32`: arbitrary-length real DFT setup
- `MyIppR2CFft32`: real FFT setup used by in-place Perm and Pack paths

Method routing:

| Method | Current IPP route | In-place special case |
| --- | --- | --- |
| `Forward` | `MyIppR2CDft32::Execute` -> `ippsDFTFwd_RToCCS_32f` | no |
| `Backward` | `MyIppR2CDft32::ExecuteInv` -> `ippsDFTInv_CCSToR_32f` | no |
| `ForwardPerm` | out-of-place: `ippsDFTFwd_RToPerm_32f`; in-place: `ippsFFTFwd_RToPerm_32f_I` | yes |
| `BackwardPerm` | out-of-place: `ippsDFTInv_PermToR_32f`; in-place: `ippsFFTInv_PermToR_32f_I` | yes |
| `ForwardPack` / `BackwardPack` | DFT for out-of-place, FFT for in-place | yes |

The Apple/non-IPP production branch currently defines only the surface needed for Forward capture:

- `FftMemoryBuffer`
- `RealDft<float>::RealDft`
- `RealDft<float>::Forward`
- `RealDft<float>::work_size`

Therefore, Backward validation must be treated as a separate migration. Forward success must not be read as a roundtrip or inverse compatibility proof.

## IPP Backward Input Layout

Code path:

```cpp
void RealDft<float>::Backward(const Float *input, Float *output, void *work_data) const {
    const auto dft = (MyIppR2CDft32 *)dft_ptr_;
    dft->ExecuteInv(input, output, work_data);
}
```

`ExecuteInv` calls:

```cpp
ippsDFTInv_CCSToR_32f(src, dest, specPtr(), work)
```

From surrounding code and existing callers, `Backward` consumes the same public scalar layout that `Forward` produces:

- input scalar count: `2 * (N / 2 + 1)`
- can be stored as `std::complex<float>[N / 2 + 1]`
- bin `0`: DC real, DC imaginary expected to be zero
- bins `1..floor(N/2)`: interleaved real/imag complex bins
- for even `N`, bin `N/2` is a Nyquist singleton and its imaginary scalar is expected to be zero
- for odd `N`, final stored bin `floor(N/2)` is an ordinary complex bin and may have non-zero imaginary part

Hard compatibility rule:

- A vDSP `Backward` implementation must accept this exact public layout. It may internally convert to split-complex or full-complex storage, but it must not require callers to change their buffers.

## Scaling And Normalization

All current IPP DFT/FFT setups use:

```text
IPP_FFT_NODIV_BY_ANY
```

Consequences:

- Forward is unnormalized.
- Backward is unnormalized.
- `Backward(Forward(x))` returns approximately `N * x`, not `x`.
- Normalization is performed by callers when needed.

Evidence in callers:

- `src/audio_analyzer/test_dft.cpp` divides the inverse result by `width` before checking against the original signal.
- `src/phase_limiter/GradCalculator.h` divides by `memLen` after one Forward/Backward filtering path.
- `deps/bakuage/src/loudness_ebu_r128.cpp` applies `1 / fft_len` to frequency-domain weights before calling `Backward`, so the inverse output is pre-normalized by spectral scaling.
- Several filtering/convolution paths rely on manually chosen normalization constants around Forward/Backward use.

Migration requirement:

- Do not normalize inside `RealDft<float>::Backward`.
- Match IPP raw inverse output.
- Validate both raw output and normalized roundtrip, but only the raw output is the direct API compatibility target.

## Capture Case Families

Backward validation should be split into three complementary case sets.

### 1. `minimal_backward`

Goal:

- Capture direct `Backward` outputs for known synthetic CCS spectra.
- Prove inverse layout, DC/Nyquist handling, sign convention, and scaling without relying on the Forward path.

Suggested lengths:

- small: `1`, `2`, `3`, `4`, `5`, `6`, `7`, `8`
- non-power-of-two: `10`, `12`, `15`, `1000`, `12345`
- powers of two / phase_limiter: `16`, `256`, `1024`, `4096`, `16384`, `32768`

If `N=1` is unsupported by current IPP or vDSP, record that explicitly and do not invent behavior.

Suggested spectra:

- `zero_spectrum`
- `dc_only`
- `single_bin1_real`
- `single_bin1_imag`
- `single_bin_mid_real`
- `single_bin_mid_imag`
- `even_nyquist_real`
- `odd_final_bin_real`
- `odd_final_bin_imag`
- `conjugate_compatible_noise_seed305419896`
- `tiny_spectrum_1e-30`
- `large_spectrum_1e10`

Expected output:

- real time-domain buffer of `N` floats
- raw unnormalized IPP inverse output

### 2. `forward_backward_roundtrip`

Goal:

- Use known real time-domain inputs.
- Run IPP `Forward`, then IPP `Backward`.
- Capture raw inverse output and evaluate normalized roundtrip as `output / N` against the original input.

This proves how the current IPP pair behaves end-to-end and supplies an acceptance baseline for future vDSP pair behavior.

Use the current Forward waveform families:

- minimal 99-record waveform set
- selected `forward_extended` waveforms, especially non-bin sine/cosine, deterministic noise, tiny values, large safe values, and phase_limiter representative lengths

Record both:

- raw `Backward(Forward(x))` payload
- metadata field `roundtrip_gain = N`

Acceptance:

- raw candidate should match IPP raw payload
- normalized candidate should match original input within calibrated float tolerance

### 3. `backward_from_known_spectrum`

Goal:

- Test spectra that may not come from any currently captured Forward input but are still valid real-signal half-spectra.
- Stress caller-generated spectra, such as phase_limiter gradient/noise spectra.

This overlaps with `minimal_backward`, but should eventually become the larger extended set.

Important:

- All generated spectra must be conjugate-compatible with a real inverse.
- Do not include arbitrary non-Hermitian half-spectra in strict compatibility tests unless IPP behavior for invalid spectra is explicitly being characterized.

## Backward Spectrum Design

For a real length `N`, `Backward` input is the non-redundant positive-frequency spectrum:

```text
bins = 0..floor(N/2)
scalar_count = 2 * (N / 2 + 1)
```

### DC Only

Set:

- `H[0].real = A`
- `H[0].imag = 0`
- all other bins zero

Expected behavior:

- raw inverse is a constant signal scaled according to IPP's unnormalized inverse.
- This is the simplest scaling sentinel.

### Single Bin Real

For `1 <= k < N/2` on even lengths or `1 <= k <= floor(N/2)` on odd lengths:

- `H[k].real = A`
- `H[k].imag = 0`

Expected behavior:

- cosine-like time-domain signal.
- Sign convention is less sensitive than imaginary-only, but amplitude/scaling is sensitive.

### Single Bin Imaginary

For non-singleton bins:

- `H[k].real = 0`
- `H[k].imag = A`

Expected behavior:

- sine-like time-domain signal.
- This is the primary inverse sign-convention sentinel.

### Even Nyquist / Final Bin

Even `N`:

- bin `N/2` is a Nyquist singleton.
- its imaginary scalar must be zero.
- use `even_nyquist_real` to test alternating-sign output.
- do not set Nyquist imaginary in strict valid-spectrum cases.

Odd `N`:

- final stored bin `floor(N/2)` is ordinary complex.
- test both `odd_final_bin_real` and `odd_final_bin_imag`.
- never force odd final-bin imaginary to zero.

### Conjugate-Compatible Random Spectrum

Generate deterministic half-spectra:

- DC imaginary set to zero.
- even Nyquist imaginary set to zero.
- ordinary bins receive deterministic real and imaginary values.
- amplitudes should include normal, tiny, and large-but-safe variants.

The full negative-frequency half is implicit and must be reconstructed internally for complex-vDSP odd inverse paths.

### Forward Golden Output As Backward Input

Use existing Forward golden payloads as Backward inputs:

- minimal Forward golden outputs
- `forward_extended` Forward golden outputs

Advantages:

- guaranteed to be valid spectra under current IPP Forward.
- directly validates `Backward(Forward(x))` workflows.
- catches layout drift between the migrated Forward and future Backward.

Limit:

- Forward-generated spectra do not isolate every inverse layout case. Keep known synthetic spectra too.

## Even And Odd Length Risks

Even lengths:

- vDSP zrop inverse can be a natural candidate because it supports complex-to-real inverse with the same split layout as its Forward output.
- Input conversion must map public CCS/interleaved layout into zrop inverse split input:
  - `Ir[0] = H[0].real`
  - `Ii[0] = H[N/2].real`
  - for `1 <= k < N/2`, `Ir[k] = H[k].real`, `Ii[k] = H[k].imag`
- Output conversion must interleave zrop inverse output:
  - `out[2*j] = Or[j]`
  - `out[2*j+1] = Oi[j]`
- Even Nyquist imaginary scalar from the public buffer should be ignored only if IPP ignores it; this must be captured. Prefer hard-valid inputs with Nyquist imaginary zero.

Odd lengths:

- zrop is unavailable because real length must be even.
- The forward migration used exact-length complex vDSP for odd lengths.
- Backward should likely build a full complex spectrum of length `N` from public half-spectrum:
  - copy bins `0..floor(N/2)` from input
  - synthesize bins `floor(N/2)+1..N-1` as conjugates of positive bins
  - enforce DC imaginary zero internally to match public layout expectations
  - preserve odd final stored bin imaginary when constructing the conjugate pair
- Execute exact-length complex inverse.
- Copy the real part to the output buffer.
- The imaginary part of the inverse output should be near zero for valid spectra, but should be measured in the prototype as a diagnostic.

Odd final-bin handling is a hard gate. Treating the odd final bin as Nyquist and zeroing its imaginary component would be an audio-affecting layout bug.

## vDSP Candidate APIs

### Even: zrop inverse path

Candidate:

```text
vDSP_DFT_zrop_CreateSetup(..., N, vDSP_DFT_INVERSE)
vDSP_DFT_Execute(...)
```

From local SDK headers:

- `Length` is the number of real elements produced by inverse.
- `Length` must be even.
- For zrop, `C` is `2` for Forward and `1` for Inverse.
- `S` is `-1` for Forward and `+1` for Inverse.
- For Inverse, the input and output layouts are swapped relative to Forward.

Scaling implication:

- Forward needed a `0.5f` correction because zrop Forward has `C = 2`.
- zrop Inverse has `C = 1`; do not assume the Forward `0.5f` correction applies.
- Compare against IPP raw Backward golden before accepting any inverse scale.

### Odd: exact-length complex inverse path

Candidate order:

1. `vDSP_DFT_zop_CreateSetup(..., N, vDSP_DFT_INVERSE)` + `vDSP_DFT_Execute`
2. fallback: legacy `vDSP_DFT_CreateSetup` + `vDSP_DFT_zop(..., vDSP_DFT_INVERSE)`

This mirrors the odd Forward strategy.

Scaling implication:

- Complex vDSP DFT has no zrop `C = 2` factor.
- Prototype comparison must check whether raw inverse matches IPP's unnormalized inverse directly.
- Do not divide by `N` inside `Backward`.

### Rejected shortcut

Do not implement Backward by calling Forward with sign tricks or by zero-padding odd lengths. That would hide sign/layout issues and change spectral meaning.

## Prototype Before Production

Do not move directly into production `dft.cpp`.

Reason:

- Backward has independent risks from Forward:
  - inverse sign convention
  - raw `N` gain
  - even zrop inverse layout
  - odd half-spectrum to full-spectrum reconstruction
  - DC/Nyquist singleton handling
  - caller-generated spectra that did not come from Forward
- A wrong Backward can pass superficial roundtrip tests if both Forward and Backward share a compensating layout mistake.
- Existing Apple/non-IPP branch currently lacks the broader `RealDft` surface, so production migration should be the final step after prototype and golden proof.

Required order:

1. Extend or create IPP golden capture for Backward only.
2. Create a vDSP Backward prototype outside production `dft.cpp`.
3. Compare prototype outputs against IPP Backward golden.
4. Separately compare normalized roundtrip behavior.
5. Only then implement production `RealDft<float>::Backward`.

## Proposed Next Tools

### `dft_golden_capture` extension or new capture tool

Add case sets:

- `minimal_backward`
- `forward_backward_roundtrip`
- optionally `backward_known_spectrum_extended`

Suggested manifest additions:

- `method`: `Backward`
- `input_layout`: `ccs_public_complex_interleaved`
- `output_layout`: `real_time_domain`
- `input_scalar_count`: `2 * (N / 2 + 1)`
- `output_scalar_count`: `N`
- `roundtrip_gain`: `N` for roundtrip cases
- `spectrum_case`: for known synthetic spectra

Keep old Forward manifests readable by `dft_compare_golden`.

### vDSP Backward prototype

Suggested source:

```text
src/tools/dft_vdsp_backward_prototype.cpp
```

Responsibilities:

- read Backward golden manifest and input payloads
- generate vDSP candidate Backward outputs
- report backend used per length:
  - `zrop_even_inverse`
  - `zop_odd_inverse`
  - `legacy_zop_odd_inverse`
- write candidate payloads with the same `output_file` names
- optionally write diagnostics:
  - maximum inverse imaginary residue for odd complex path
  - scale probes: normal, `0.5x`, `2x`, `1/N`, `N`
  - sign probes for imaginary-bin cases

### Compare tool extension

`dft_compare_golden` currently assumes `float Forward` manifests. For Backward it should support:

- `method = Backward`
- output scalar count `N`
- real time-domain payloads
- max abs error
- RMS error
- max relative error
- worst scalar index and values
- normalized roundtrip error when source input is available

Hard failures:

- missing candidate
- scalar count mismatch
- NaN/Inf
- wrong method/precision
- unsupported length silently skipped

## Acceptance Gates Before Production Backward

Before touching production `dft.cpp`, require:

- IPP Backward golden artifact exists for `minimal_backward`.
- vDSP Backward prototype generates all candidate records.
- Compared records match expected count.
- Missing candidates: `0`
- Scalar count mismatches: `0`
- NaN/Inf: `0`
- Known spectrum cases pass:
  - DC only
  - real single-bin
  - imaginary single-bin
  - even Nyquist
  - odd final-bin real and imaginary
  - conjugate-compatible random spectra
- `Forward` golden outputs used as `Backward` inputs pass raw IPP comparison.
- Normalized `Backward(Forward(x)) / N` roundtrip is within calibrated tolerance.
- Odd final-bin imaginary handling is explicitly proven.
- No internal `1/N` normalization is introduced.
- No zero padding is used for odd lengths.
- Unsupported vDSP setup paths fail clearly or route to a proven exact-length fallback.
- Performance of odd legacy complex inverse is at least estimated for representative odd lengths such as `12345`.

## Practical Risk Notes

Backward migration has direct audio-risk because it is used by filtering, reconstruction, gradient, and analysis paths. A scale error changes loudness. A sign error changes phase. A final-bin or Nyquist mistake can create high-frequency artifacts. Any of these may be more audible than the small Forward numeric differences already observed.

Therefore, Backward should be treated as a separate compatibility project, not as a mechanical inverse of the Forward migration.
