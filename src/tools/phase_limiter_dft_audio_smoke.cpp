#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "bakuage/dft.h"
#include "sndfile.h"

namespace {

struct Options {
    std::string input;
    std::string output;
    int channels = 2;
    int sample_rate = 0;
    int max_frames = 0;
    double abs_tolerance = 1.0e-3;
    double rel_tolerance = 2.0e-5;
};

Options ParseArgs(int argc, char **argv) {
    Options options;
    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        if (arg == "--input" && i + 1 < argc) {
            options.input = argv[++i];
        } else if (arg == "--output" && i + 1 < argc) {
            options.output = argv[++i];
        } else if (arg == "--channels" && i + 1 < argc) {
            options.channels = std::atoi(argv[++i]);
        } else if (arg == "--sample-rate" && i + 1 < argc) {
            options.sample_rate = std::atoi(argv[++i]);
        } else if (arg == "--max-frames" && i + 1 < argc) {
            options.max_frames = std::atoi(argv[++i]);
        } else if (arg == "--abs-tolerance" && i + 1 < argc) {
            options.abs_tolerance = std::atof(argv[++i]);
        } else if (arg == "--rel-tolerance" && i + 1 < argc) {
            options.rel_tolerance = std::atof(argv[++i]);
        } else {
            throw std::runtime_error("usage: phase_limiter_dft_audio_smoke --input <wav> --output <wav> [--channels 2] [--sample-rate 44100] [--max-frames n] [--abs-tolerance value] [--rel-tolerance value]");
        }
    }
    if (options.input.empty()) throw std::runtime_error("--input is required");
    if (options.output.empty()) throw std::runtime_error("--output is required");
    if (options.channels <= 0) throw std::runtime_error("--channels must be positive");
    if (options.sample_rate < 0) throw std::runtime_error("--sample-rate must be non-negative");
    if (options.max_frames < 0) throw std::runtime_error("--max-frames must be non-negative");
    if (!(options.abs_tolerance >= 0.0) || !(options.rel_tolerance >= 0.0)) {
        throw std::runtime_error("tolerances must be non-negative");
    }
    return options;
}

int SpectrumScalarCount(int n) {
    return 2 * (n / 2 + 1);
}

std::vector<float> LoadWave(const std::string &path, int expected_channels, int *sample_rate) {
    SF_INFO sfinfo = {};
    SNDFILE *file = sf_open(path.c_str(), SFM_READ, &sfinfo);
    if (!file) {
        throw std::runtime_error(std::string("failed to open input: ") + path + ": " + sf_strerror(nullptr));
    }
    if (sfinfo.channels != expected_channels) {
        sf_close(file);
        std::ostringstream ss;
        ss << "expected " << expected_channels << " channels, got " << sfinfo.channels;
        throw std::runtime_error(ss.str());
    }
    if ((sfinfo.format & SF_FORMAT_TYPEMASK) != SF_FORMAT_WAV &&
        (sfinfo.format & SF_FORMAT_TYPEMASK) != SF_FORMAT_WAVEX) {
        sf_close(file);
        throw std::runtime_error("only WAV/WAVEX input is supported");
    }

    std::vector<float> wave(static_cast<std::size_t>(sfinfo.frames * sfinfo.channels));
    const sf_count_t read_frames = sf_readf_float(file, wave.data(), sfinfo.frames);
    sf_close(file);
    if (read_frames != sfinfo.frames) {
        throw std::runtime_error("failed to read all input frames");
    }
    *sample_rate = sfinfo.samplerate;
    return wave;
}

void SaveWave(const std::string &path, const std::vector<float> &wave, int channels, int sample_rate) {
    const sf_count_t expected_frames = static_cast<sf_count_t>(wave.size() / channels);
    SF_INFO sfinfo = {};
    sfinfo.channels = channels;
    sfinfo.frames = expected_frames;
    sfinfo.samplerate = sample_rate;
    sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;

    SNDFILE *file = sf_open(path.c_str(), SFM_WRITE, &sfinfo);
    if (!file) {
        throw std::runtime_error(std::string("failed to open output: ") + path + ": " + sf_strerror(nullptr));
    }

    std::vector<float> sanitized = wave;
    for (float &x : sanitized) {
        if (!std::isfinite(x)) x = 0.0f;
        x = std::max(-1.0e7f, std::min(1.0e7f, x));
    }

    const sf_count_t written_frames = sf_writef_float(file, sanitized.data(), expected_frames);
    sf_close(file);
    if (written_frames != expected_frames) {
        throw std::runtime_error("failed to write all output frames");
    }
}

}  // namespace

int main(int argc, char **argv) {
    try {
        Options options = ParseArgs(argc, argv);

        int input_sample_rate = 0;
        std::vector<float> wave = LoadWave(options.input, options.channels, &input_sample_rate);
        if (options.sample_rate == 0) {
            options.sample_rate = input_sample_rate;
        }
        if (wave.size() % options.channels != 0) {
            throw std::runtime_error("input sample count is not divisible by channel count");
        }

        int frames = static_cast<int>(wave.size() / options.channels);
        if (options.max_frames > 0 && options.max_frames < frames) {
            frames = options.max_frames;
            wave.resize(static_cast<std::size_t>(frames * options.channels));
        }
        if (frames <= 0) throw std::runtime_error("input has no frames");

        std::vector<float> output(wave.size(), 0.0f);
        double max_abs_error = 0.0;
        double max_relative_error = 0.0;
        double sum_square_error = 0.0;
        bool nan_or_inf = false;

        for (int ch = 0; ch < options.channels; ch++) {
            std::vector<float> time(frames, 0.0f);
            std::vector<float> restored(frames, 0.0f);
            std::vector<float> spectrum(SpectrumScalarCount(frames), 0.0f);

            for (int i = 0; i < frames; i++) {
                time[i] = wave[options.channels * i + ch];
            }

            bakuage::RealDft<float> dft(frames);
            dft.Forward(time.data(), spectrum.data());
            dft.Backward(spectrum.data(), restored.data());

            double case_scale = 1.0;
            for (int i = 0; i < frames; i++) {
                case_scale = std::max(case_scale, std::abs(static_cast<double>(time[i])));
                case_scale = std::max(case_scale, std::abs(static_cast<double>(restored[i]) / frames));
            }

            for (int i = 0; i < frames; i++) {
                const double normalized = static_cast<double>(restored[i]) / frames;
                const double expected = static_cast<double>(time[i]);
                const double abs_error = std::abs(normalized - expected);
                const double relative_error = abs_error / case_scale;
                if (!std::isfinite(normalized) || !std::isfinite(expected)) {
                    nan_or_inf = true;
                }
                max_abs_error = std::max(max_abs_error, abs_error);
                max_relative_error = std::max(max_relative_error, relative_error);
                sum_square_error += abs_error * abs_error;
                output[options.channels * i + ch] = static_cast<float>(normalized);
            }
        }

        const double rms_error = std::sqrt(sum_square_error / static_cast<double>(wave.size()));
        const bool passed = !nan_or_inf &&
            (max_abs_error <= options.abs_tolerance || max_relative_error <= options.rel_tolerance);

        SaveWave(options.output, output, options.channels, options.sample_rate);

        std::cout << "phase_limiter_dft_audio_smoke"
                  << " frames=" << frames
                  << " channels=" << options.channels
                  << " max_abs_error=" << max_abs_error
                  << " max_relative_error=" << max_relative_error
                  << " rms_error=" << rms_error
                  << " nan_or_inf=" << (nan_or_inf ? 1 : 0)
                  << " status=" << (passed ? "pass" : "fail")
                  << std::endl;
        return passed ? 0 : 1;
    } catch (const std::exception &e) {
        std::cerr << "phase_limiter_dft_audio_smoke error: " << e.what() << std::endl;
        return 1;
    }
}
