# Mac arm64 phase_limiter CLI smoke attempt 02

Date: 2026-09-10

Branch: `mac-arm64-minimal-cli`

Goal: broaden the Apple arm64 / non-IPP CLI smoke validation from short proof-of-build runs into more realistic audio paths, without changing production DSP code.

## Results

### `test5.wav` full length, simple limiting

Command shape:

- input: `test_data/test5.wav`
- output: `build_mac_arm64_dft_roundtrip_attempt01/test5_full_simple.wav`
- limiting mode: `simple`
- low/high cut disabled
- pre-compression disabled

Result:

- Passed.
- Output WAV info:
  - channels: 2
  - frames: 10534
  - samplerate: 44100
  - format: `0x00010002`

### `test5.wav` full length, phase limiting, minimal iterations

Command shape:

- input: `test_data/test5.wav`
- output: `build_mac_arm64_dft_roundtrip_attempt01/test5_full_phase_iter1.wav`
- limiting mode: `phase`
- low/high cut disabled
- pre-compression disabled
- `max_iter1=1`
- `max_iter2=1`

Result:

- Passed.
- Output WAV info:
  - channels: 2
  - frames: 10534
  - samplerate: 44100
  - format: `0x00010002`

### `test2.wav` default-ish path, phase limiting, 0.05 seconds

Command shape:

- input: `test_data/test2.wav`
- output: `build_mac_arm64_dft_roundtrip_attempt01/test2_005s_defaultish_phase_iter1.wav`
- limiting mode: `phase`
- default low/high cut path enabled
- default pre-compression path enabled
- `max_iter1=1`
- `max_iter2=1`
- `end_at=0.05`

Result:

- Passed.
- Output WAV info:
  - channels: 2
  - frames: 2205
  - samplerate: 44100
  - format: `0x00010002`

### `test2.wav` default-ish path, phase limiting, 0.1 seconds

Command shape:

- input: `test_data/test2.wav`
- output: `build_mac_arm64_dft_roundtrip_attempt01/test2_01s_defaultish_phase_iter1.wav`
- limiting mode: `phase`
- default low/high cut path enabled
- default pre-compression path enabled
- `max_iter1=1`
- `max_iter2=1`
- `end_at=0.1`

Result:

- Passed.
- Output WAV info:
  - channels: 2
  - frames: 4410
  - samplerate: 44100
  - format: `0x00010002`

### `test2.wav` default-ish path, phase limiting, longer probes

Command shape:

- input: `test_data/test2.wav`
- limiting mode: `phase`
- default low/high cut path enabled
- default pre-compression path enabled
- `max_iter1=1`
- `max_iter2=1`

Results:

- `end_at=0.25`: stopped manually after about 60 seconds without optimizer progress output.
- `end_at=1`: stopped manually after more than 2 minutes without optimizer progress output.

### `test2.wav` default-ish path, simple limiting, 1 second

Command shape:

- input: `test_data/test2.wav`
- output: `build_mac_arm64_dft_roundtrip_attempt01/test2_1s_defaultish_simple.wav`
- limiting mode: `simple`
- default low/high cut path enabled
- default pre-compression path enabled
- `end_at=1`

Result:

- Passed.
- Output WAV info:
  - channels: 2
  - frames: 44100
  - samplerate: 44100
  - format: `0x00010002`

## Interpretation

The Mac arm64 / non-IPP CLI now passes:

- full-length short WAV processing in `simple` mode
- full-length short WAV processing in `phase` mode with low/high cut and pre-compression disabled
- default-ish low/high cut and pre-compression paths for short `phase` runs
- default-ish low/high cut and pre-compression paths for a 1 second `simple` run

The current blocker is not the basic CLI build, audio I/O, DFT roundtrip path, low/high cut, pre-compression, or WAV writing. The remaining issue is practical runtime for default-ish `phase` mode as the processed duration grows beyond about 0.1 seconds on the current Apple/non-IPP scalar/vDSP path.

## Suggested next step

Add a lightweight timing/instrumentation probe around the `phase` optimization path, then decide whether the slowdown is caused by:

- an expected algorithmic cost increase from larger buffers
- a scalar Apple/non-IPP fallback in a hot vector math function
- a DFT size/path choice that becomes expensive for the 0.25s and 1s cases

Production DSP behavior should remain unchanged until the hot section is identified.
