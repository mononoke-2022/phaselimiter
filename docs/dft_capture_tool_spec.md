# PHASE_LIMITER_DFT_CAPTURE_TOOL_SPEC

## Scope

Capture current IPP `RealDft` golden outputs only. Do not prototype vDSP in this tool.

APIs to capture:

- `RealDft<float>::Forward`
- `RealDft<float>::Backward`
- `RealDft<float>::ForwardPerm`
- `RealDft<float>::BackwardPerm`
- Same for `double`

Capture both public overloads with internal work and explicit `work_size()` buffer where practical. Perm must capture out-of-place and in-place separately because `dft.cpp` switches implementation based on `input == output`.

## Recommended Capture Environment

First choice:

- GitHub Actions Linux x64 runner.

Reasons:

- No local Windows PC is required.
- The environment is more reproducible than an ad-hoc local machine.
- Golden outputs can be saved directly as CI artifacts.
- IPP capture can be rerun in CI from the same commit and workflow.

Windows remains a fallback option if a known-good Windows IPP build environment is already available.

Mac arm64 should not generate IPP golden outputs. Use Mac arm64 for the vDSP prototype and comparison side after Linux x64 CI has produced the IPP golden artifacts.

## Tool CLI

Suggested executable:

```sh
dft_golden_capture --output-dir <dir> --case-set minimal|extended --format json-binary
```

Required behavior:

- Create `<dir>/manifest.json`.
- Create one binary payload per captured output.
- Fail hard on any unsupported length or failed DFT; do not skip silently.

## JSON Manifest Schema

Top-level:

```json
{
  "schema": "phase_limiter.dft_golden.v1",
  "git_commit": "...",
  "platform": {"os": "...", "arch": "...", "compiler": "..."},
  "ipp": {"version": "...", "ipproot": "..."},
  "endianness": "little",
  "records": []
}
```

Record:

```json
{
  "id": "float_n16_impulse0_forward_oop",
  "precision": "float",
  "bytes_per_scalar": 4,
  "length": 16,
  "waveform": "impulse0",
  "method": "Forward",
  "direction": "forward",
  "layout": "ccs_public_complex_interleaved",
  "in_place": false,
  "uses_explicit_work": false,
  "input_payload": "inputs/float_n16_impulse0.bin",
  "output_payload": "outputs/float_n16_impulse0_forward_oop.bin",
  "input_scalar_count": 16,
  "output_scalar_count": 18,
  "normalization": "ipp_no_div_by_any",
  "roundtrip_gain": 16,
  "sha256": "..."
}
```

Layouts:

- `real_time_domain`
- `ccs_public_complex_interleaved`
- `perm_raw_scalar`

## Binary Payload Rules

- Little-endian IEEE-754 scalars.
- No text headers.
- `float` payload stores 32-bit floats.
- `double` payload stores 64-bit doubles.
- Scalar buffers only; complex is represented as raw interleaved scalar sequence.

Naming:

```text
inputs/{precision}_n{N}_{waveform}.bin
outputs/{precision}_n{N}_{waveform}_{method}_{place}_{work}.bin
```

Where:

- `{precision}`: `float` or `double`
- `{method}`: `forward`, `backward`, `forward_perm`, `backward_perm`, `roundtrip_forward`, `roundtrip_perm`
- `{place}`: `oop` or `ip`
- `{work}`: `internal_work` or `explicit_work`

Example:

```text
outputs/double_n12345_noise_seed305419896_forward_perm_oop_explicit_work.bin
```

## Test Case Sets

Minimal set:

- Lengths: `2,3,4,5,8,9,16,1000,1024,12345`
- Waveforms: zeros, impulse0, impulse1, impulse_last, constant1, ramp, hand_mixed, sine_bin1, cosine_bin1, sine_nonbin, noise_seed305419896
- Precision: float and double
- Methods: Forward, Backward via Forward output, ForwardPerm oop/ip, BackwardPerm via ForwardPerm oop/ip

Extended set:

- Lengths: `1,2,3,4,5,6,7,8,9,10,12,15,16,1000,1024,12345,16384`
- Add representative phase_limiter/audio_analyzer lengths once known.
- Add tiny_noise, sine_bin2 where valid, cosine_bin2 where valid, alternating_sign.
- Include explicit-work variants for all methods.

Note:

- If `N=1` fails in current IPP path, record as unsupported only if the tool can report a hard failure cleanly. Do not invent behavior.

## Deterministic Waveform Rules

Use identical generation on Windows/Linux/vDSP prototype:

- `zeros`: all `0`.
- `impulse0`: `x[0] = 1`, others `0`.
- `impulse1`: `x[1] = 1` if `N > 1`, otherwise unsupported.
- `impulse_last`: `x[N - 1] = 1`.
- `constant1`: all `1`.
- `ramp`: `x[i] = (2*i - (N - 1)) / max(1, N - 1)`.
- `hand_mixed`: repeat `[0.0, 1.0, -1.0, 0.5, -0.25, 2.0, -3.0, 0.125]`.
- `sine_bin1`: `sin(2*pi*i/N)`.
- `cosine_bin1`: `cos(2*pi*i/N)`.
- `sine_nonbin`: `sin(2*pi*(1.5)*i/N + 0.25)`.
- `alternating_sign`: `+1, -1, +1, -1...`.
- `tiny_noise`: deterministic noise scaled by `1e-20`.
- `noise_seed305419896`: deterministic uniform `[-1, 1]`.

Random rule:

- Use a local, specified PRNG, not `std::uniform_real_distribution`.
- Recommended: xorshift32 with seed `0x12345678`.
- Convert to double as `u / 4294967295.0`, then `2*u - 1`.
- Cast to float only when filling float input.

## Backward Input Rules

For `Backward`:

- Primary capture input is exactly the `Forward` output for the same input case.
- Output is raw time-domain scalar buffer before division by `N`.

For `BackwardPerm`:

- Primary capture input is exactly the `ForwardPerm` output for the same input case.
- Capture out-of-place and in-place separately.
- For in-place, initialize the shared buffer with the forward Perm payload, call `BackwardPerm(buffer, buffer)`, and capture final buffer.

## vDSP Prototype Reproduction

The vDSP prototype must:

- Read `manifest.json`.
- Regenerate input waveforms from metadata, or read `input_payload` directly.
- Prefer reading `input_payload` for exact parity.
- Produce output payloads using the same naming plus backend tag, e.g. `vdsp_outputs/...`.
- Compare scalar buffers against golden before applying complex interpretation.
- Treat layout mismatches as hard failures even when roundtrip looks good.

Comparison metrics:

- max absolute error
- RMS error
- max relative error for non-zero golden values
- DC/Nyquist index checks
- roundtrip normalized error
- spectral power and phase difference for `Forward`

## Implementation Notes For Later

- Keep capture tool outside production `dft.cpp`.
- If adding to repo, prefer a standalone `src/test` or `tools` target guarded so normal builds are unaffected.
- The capture artifact should include the exact git commit that produced it.
- Golden artifacts should be versioned or archived separately from source if large.
