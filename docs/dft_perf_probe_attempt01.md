# DFT performance probe attempt 01

Date: 2026-09-10

Branch: `mac-arm64-minimal-cli`

Goal: identify the default-ish `phase_limiter --limiting_mode=phase` slowdown observed when `test2.wav` is processed beyond the short 0.05s and 0.1s smoke cases.

## Scope

No production DSP behavior was changed in this attempt.

Added a small tool target:

- CMake option: `ENABLE_DFT_PERF_PROBE`
- executable: `dft_perf_probe`
- source: `src/tools/dft_perf_probe.cpp`

The tool measures `bakuage::RealDft<float>` Forward timing for selected lengths, with optional Backward timing.

## Process sampling result

The slow case was reproduced with:

- input: `test_data/test2.wav`
- `end_at=0.25`
- `limiting_mode=phase`
- default low/high cut enabled
- default pre-compression enabled
- `max_iter1=1`
- `max_iter2=1`

The process was sampled externally for 5 seconds while it was stalled after:

- `upsampled`
- `GradCalculator initialized`

The sample showed the main thread inside:

- `PhaseLimitInplace<float>`
- `GradCalculator::outputUnitEval("src_with_cut")`
- `RealDft<float>::Forward`
- `MyVdspR2CForwardDft32::ExecuteComplex`
- `libvDSP` with heavy `__sincos_stret` activity

Interpretation: the slowdown is happening before the optimizer loop starts. The hot path is the RealDft forward call used while computing the unit evaluation signal, and it is using the complex vDSP fallback path rather than the fast real-even path.

## DFT timing probe

Successful probe command shape:

- lengths: `32768,49152,57344,65536`
- repeats: 3
- Backward timing enabled

Observed output before stopping the slow length:

```txt
length=32768 repeats=3 work_size=524288 forward_ms_median=0.086 backward_ms_median=0.084
length=49152 repeats=3 work_size=786432 forward_ms_median=0.104 backward_ms_median=0.113
```

The run was manually stopped while measuring length `57344`, after more than 60 seconds without completing that length.

## vDSP setup path probe

Added a second non-production diagnostic target:

- CMake option: `ENABLE_DFT_VDSP_PATH_PROBE`
- executable: `dft_vdsp_path_probe`
- source: `src/tools/dft_vdsp_path_probe.cpp`

This tool checks which vDSP setup APIs can be created for each length, then reports the path the current Apple/non-IPP `RealDft<float>::Forward` implementation would select.

Probe command:

```txt
dft_vdsp_path_probe --lengths 32768,49152,57344,65536
```

Observed output:

```txt
length=32768 forward_zrop=yes forward_zop=yes legacy=yes inverse_zrop=yes inverse_zop=yes production_forward_path=real_even_zrop
length=49152 forward_zrop=yes forward_zop=yes legacy=yes inverse_zrop=yes inverse_zop=yes production_forward_path=real_even_zrop
length=57344 forward_zrop=no forward_zop=no legacy=yes inverse_zrop=no inverse_zop=no production_forward_path=legacy_complex
length=65536 forward_zrop=yes forward_zop=yes legacy=yes inverse_zrop=yes inverse_zop=yes production_forward_path=real_even_zrop
```

Additional nearby probe:

```txt
length=49152 forward_zrop=yes forward_zop=yes legacy=yes inverse_zrop=yes inverse_zop=yes production_forward_path=real_even_zrop
length=57344 forward_zrop=no forward_zop=no legacy=yes inverse_zrop=no inverse_zop=no production_forward_path=legacy_complex
length=65536 forward_zrop=yes forward_zop=yes legacy=yes inverse_zrop=yes inverse_zop=yes production_forward_path=real_even_zrop
length=81920 forward_zrop=yes forward_zop=yes legacy=yes inverse_zrop=yes inverse_zop=yes production_forward_path=real_even_zrop
length=98304 forward_zrop=yes forward_zop=yes legacy=yes inverse_zrop=yes inverse_zop=yes production_forward_path=real_even_zrop
```

## Interpretation

The slowdown is length-sensitive. Some large non-power-of-two lengths are still fast on the current Apple/non-IPP vDSP path, but length `57344` is extremely slow.

This lines up with the `phase_limiter` smoke result:

- `end_at=0.1` default-ish `phase` completed quickly.
- `end_at=0.25` default-ish `phase` did not reach optimizer progress within about 60 seconds.

The difference is not general WAV I/O, pre-compression, low/high cut, or simple limiting. It is a RealDft<float> length/path issue hit by `GradCalculator::outputUnitEval("src_with_cut")`.

The vDSP path probe confirms that length `57344` cannot create the fast `zrop` real-even setup or the regular `zop` complex setup on this system. The current implementation therefore falls to the legacy complex vDSP API, which matches the process sample showing heavy `__sincos_stret` activity inside libvDSP.

## Suggested next step

The likely production fix is to avoid the slow complex fallback for problematic lengths, either by choosing supported fast DFT lengths in the phase limiter buffer sizing or by adding a faster Apple real-DFT fallback strategy for those lengths. Keep production DSP unchanged until that behavior change is explicitly approved.
