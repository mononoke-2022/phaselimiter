"""CLI regression: rejected optimizer candidates must never be saved as success."""
import argparse
import math
import pathlib
import struct
import subprocess
import tempfile
import wave


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', required=True, type=pathlib.Path)
    parser.add_argument('--ffmpeg', required=True, type=pathlib.Path)
    parser.add_argument('--input', type=pathlib.Path, help='Optional short stereo PCM16 fixture')
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='limiter-exhaustion-') as directory:
        root = pathlib.Path(directory)
        source = args.input.resolve() if args.input else root / 'input.wav'
        if not args.input:
            with wave.open(str(source), 'wb') as wav:
                wav.setparams((2, 2, 44100, 0, 'NONE', 'not compressed'))
                samples = [int(18000 * math.sin(2 * math.pi * 997 * i / 44100)) for i in range(22050)]
                wav.writeframes(b''.join(struct.pack('<hh', x, x) for x in samples))
        # The existing CLI shell invocation does not quote the executable path.
        ffmpeg = root / 'ffmpeg'
        ffmpeg.symlink_to(args.ffmpeg.resolve())
        common = [str(args.binary.resolve()), '--input=' + str(source),
                  '--ffmpeg=' + str(ffmpeg), '--tmp=' + str(root / 'temp'),
                  '--disable_input_encode=true', '--quick_exit=false',
                  '--reference_mode=peak', '--reference=6', '--ceiling=-1',
                  '--pre_compression=false', '--output_format=wav']
        cases = [
            ('exhausted', ['--max_iter1=1', '--max_iter2=1'], 'line search exhausted'),
            ('zero_outer', ['--max_iter1=0'], 'iteration limits must be positive'),
            ('negative_inner', ['--max_iter2=-1'], 'iteration limits must be positive'),
            ('accepted', ['--max_iter1=1', '--max_iter2=400'], None),
        ]
        for name, flags, error in cases:
            for existing in ([False, True] if error else [False]):
                output = root / (name + '.wav')
                sentinel = b'previous output must survive failed processing'
                if existing:
                    output.write_bytes(sentinel)
                run = subprocess.run(common + ['--output=' + str(output)] + flags,
                                     capture_output=True, text=True, timeout=90)
                if error:
                    assert run.returncode != 0, (name, run.stderr)
                    assert error in run.stderr, (name, run.stderr)
                    assert output.read_bytes() == sentinel if existing else not output.exists(), name
                else:
                    assert run.returncode == 0, run.stderr
                    with wave.open(str(output), 'rb') as wav:
                        assert wav.getnframes() > 0 and wav.getnchannels() == 2
                print('PASS', name, 'existing_output=' + str(existing), flush=True)


if __name__ == '__main__':
    main()
