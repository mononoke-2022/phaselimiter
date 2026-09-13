# Rejected line-search output guard

The optimizer wrote each trial into `waveProx` before checking whether the line
search accepted it. When `max_iter2` was exhausted after a rejection, it returned
that rejected waveform with a successful CLI exit status. A real-audio 0.5-second
fixture with `max_iter1=1,max_iter2=1` reproduced nearly total waveform saturation.

`GradCalculator` now throws if a line search exits without an accepted candidate.
It also rejects nonpositive iteration limits when optimization is invoked. The
CLI's existing exception handling returns failure before the final WAV save.
Previously existing destination files remain unchanged on this failure.
Accepted iterations and their numerical operations are unchanged. This guard
does not certify convergence, audio quality, or catch every possible DSP error.

## Validation (Mac arm64, non-IPP, 2026-09-13)

Build target: `phase_limiter`, build directory
`build_mac_arm64_dft_roundtrip_attempt01`.

Run the portable CLI regression with:

```sh
python3 src/tools/limiter_exhaustion_validate.py \
  --binary build_mac_arm64_dft_roundtrip_attempt01/bin/phase_limiter \
  --ffmpeg /opt/homebrew/bin/ffmpeg
```

The tool generates a short sine fixture. `--input` can supply a short stereo
PCM16 WAV instead. All outputs live in an isolated temporary directory.

- Exhausted 1/1 search fails without creating a destination.
- The same failure preserves a pre-existing destination byte-for-byte.
- Zero outer and negative inner limits fail, both with and without a destination.
- A 1/400 search successfully saves a WAV as a positive control.
- All seven checks passed for synthesized audio and the 30.0-30.5 second excerpt
  of the user's real audio.
- Default 100/400 output on that real excerpt matches the previous binary's PCM
  byte-for-byte. Listening quality and full-song runtime were not revalidated.

The guarded executable was installed in the existing `aimastering-local-cli.app`
test copy with app-relative library references and ad-hoc signing. The previous
binary is retained as `phase_limiter.before-exhaustion-guard`. The app itself was
not launched and its wrapper/settings were not changed. Windows/IPP validation
has not been performed.
