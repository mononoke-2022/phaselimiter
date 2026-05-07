# PHASE_LIMITER_DFT_COMPARISON_TEST_PLAN

## Goal

Design comparison tests before replacing `RealDft<float/double>` IPP paths with vDSP.

Absolute rule:

- No stub.
- No approximate DSP shortcut.
- No production `dft.cpp` change until IPP and vDSP output layouts are proven compatible.

## Scope

Compare only:

- `RealDft<float>::Forward`
- `RealDft<float>::Backward`
- `RealDft<float>::ForwardPerm`
- `RealDft<float>::BackwardPerm`
- Same four paths for `double`

Out of initial scope:

- `ForwardPack` / `BackwardPack`
- complex `Dft<T>`
- `Dft2D`
- `Dct2D`

## Test Lengths

Use lengths that expose layout and backend support differences:

- Small hand-checkable: `1`, `2`, `3`, `4`, `5`, `6`, `7`, `8`
- Even non-power-of-two: `10`, `12`, `1000`
- Odd non-power-of-two: `9`, `15`, `12345`
- Power-of-two: `16`, `1024`, `16384`
- Phase limiter representative: `PL_FFT_MAX_LEN` and common oversampled variants if available
- Audio analyzer representative: widths derived from 44.1k and 48k paths

If vDSP cannot support a length, record it explicitly and do not silently approximate.

## Test Inputs

For each length and precision:

- All zeros
- Unit impulse at `0`
- Unit impulse at `1`
- Unit impulse at `N - 1`
- Constant one
- Ramp, e.g. `i / N`
- Small hand-checkable arrays with mixed signs
- Sine at exact bin frequencies
- Cosine at exact bin frequencies
- Non-bin sine to expose leakage consistency
- Deterministic white noise with fixed seed
- Very small amplitude noise to catch denormal/precision issues

## IPP Output Capture

Step 1 is to capture current IPP outputs as golden references:

- Capture raw `Forward` output buffer as both scalar sequence and complex-bin view.
- Capture raw `ForwardPerm` output buffer as scalar sequence.
- Capture `Backward(Forward(x))` raw inverse output before normalization.
- Capture `BackwardPerm(ForwardPerm(x))` raw inverse output before normalization.
- Capture out-of-place and in-place `ForwardPerm` / `BackwardPerm` separately because current implementation switches IPP DFT vs IPP FFT based on pointer equality.

Store metadata with each record:

- precision
- length
- input type
- method name
- in-place or out-of-place
- buffer scalar length
- expected normalization factor

## Comparison Items

Layout:

- DC placement
- Nyquist placement for even `N`
- Whether odd `N` has no Nyquist singleton
- CCS public layout for `Forward`
- Perm layout for `ForwardPerm`
- Whether `Forward` output is safely readable as `std::complex<T>[N/2 + 1]`
- Whether `ForwardPerm` output is safely usable as `std::complex<T>[N/2]`

Math:

- Forward/inverse sign convention
- Scaling: IPP currently uses no division by any
- Forward then backward gain should be `N`
- Max absolute error
- RMS error
- Relative error where magnitude is non-zero
- Spectral power difference per bin
- Phase difference per bin for non-zero bins

Roundtrip:

- `Backward(Forward(x)) / N` vs input
- `BackwardPerm(ForwardPerm(x)) / N` vs input
- In-place Perm roundtrip vs out-of-place Perm roundtrip

Downstream-sensitive checks:

- `GradCore` expectation around `spec[1] = spec[len]` before `BackwardPerm`
- `FirFilter2` convolution behavior with `ForwardPerm` and `BackwardPerm`
- `mfcc.h` expectation that DFT input is `(real, image, real, image, ...)`

## Acceptance Thresholds

Suggested initial thresholds:

- `float`: max abs error <= `2e-5 * max(1, N)` for raw inverse, tighter for forward where reasonable
- `double`: max abs error <= `1e-10 * max(1, N)`
- Roundtrip normalized RMS error should be near existing IPP baseline, not merely below a loose absolute threshold
- Any DC/Nyquist or Perm index mismatch is a hard failure, regardless of numeric error

Thresholds must be calibrated from IPP-vs-IPP repeatability and vDSP prototype behavior before production migration.

## vDSP Compatibility Conditions

Before touching production `dft.cpp`, prove:

- vDSP forward sign matches IPP forward sign or is converted exactly.
- vDSP inverse sign matches IPP inverse sign or is converted exactly.
- vDSP scaling is corrected so forward/inverse matches IPP `IPP_FFT_NODIV_BY_ANY`.
- vDSP split-complex or interleaved output can be converted to the current public CCS layout.
- Perm layout can be reproduced exactly, including DC/Nyquist placement.
- In-place Perm behavior is either exactly reproduced or safely emulated with temporary buffers and identical final output.
- Unsupported lengths are handled deliberately, not by falling back to a fake approximation.
- `float` and `double` paths both pass.

## Safe Implementation Order

1. IPP output capture test.
2. vDSP prototype outside production `dft.cpp`.
3. Layout conversion verification against captured IPP outputs.
4. Implement `RealDft::Forward` only behind `BAKUAGE_USE_IPP=OFF`.
5. Add `Backward`, then `ForwardPerm`, then `BackwardPerm` only after layout proof.

Do not start with Perm. It is the most layout-sensitive and phase_limiter-critical path.

## Recommended Next Task

Create a small standalone capture tool or test file that runs only the current IPP `RealDft` implementation and writes/prints golden outputs for the selected small lengths. Keep it outside production DSP code until the layout is fully understood.
