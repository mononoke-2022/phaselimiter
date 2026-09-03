#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "bakuage/dft.h"

namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;

struct Options {
    std::string case_set = "minimal";
    double abs_tolerance = 1.0e-3;
    double rel_tolerance = 2.0e-5;
    bool verbose = false;
};

struct TestCase {
    int length;
    std::string waveform;
};

struct CaseResult {
    TestCase test_case;
    bool passed = true;
    bool nan_or_inf = false;
    double max_raw_abs_error = 0.0;
    double max_normalized_abs_error = 0.0;
    double max_relative_error = 0.0;
    double rms_normalized_error = 0.0;
};

Options ParseArgs(int argc, char **argv) {
    Options options;
    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        if (arg == "--case-set" && i + 1 < argc) {
            options.case_set = argv[++i];
        } else if (arg == "--abs-tolerance" && i + 1 < argc) {
            options.abs_tolerance = std::atof(argv[++i]);
        } else if (arg == "--rel-tolerance" && i + 1 < argc) {
            options.rel_tolerance = std::atof(argv[++i]);
        } else if (arg == "--verbose") {
            options.verbose = true;
        } else {
            throw std::runtime_error("usage: dft_roundtrip_validate [--case-set minimal|forward_extended] [--abs-tolerance value] [--rel-tolerance value] [--verbose]");
        }
    }
    if (options.case_set != "minimal" && options.case_set != "forward_extended") {
        throw std::runtime_error("only --case-set minimal or forward_extended is supported");
    }
    if (!(options.abs_tolerance >= 0.0) || !(options.rel_tolerance >= 0.0)) {
        throw std::runtime_error("tolerances must be non-negative");
    }
    return options;
}

std::uint32_t XorShift32(std::uint32_t *state) {
    std::uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

std::vector<float> GenerateNoise(int n, std::uint32_t seed, double scale) {
    std::vector<float> x(n, 0.0f);
    std::uint32_t state = seed;
    for (int i = 0; i < n; i++) {
        const double u = static_cast<double>(XorShift32(&state)) / 4294967295.0;
        x[i] = static_cast<float>((2.0 * u - 1.0) * scale);
    }
    return x;
}

std::vector<float> GenerateInput(int n, const std::string &waveform) {
    std::vector<float> x(n, 0.0f);
    if (waveform == "zeros") return x;
    if (waveform == "impulse0") {
        x[0] = 1.0f;
        return x;
    }
    if (waveform == "impulse1") {
        if (n <= 1) throw std::runtime_error("impulse1 requires length > 1");
        x[1] = 1.0f;
        return x;
    }
    if (waveform == "impulse_last") {
        x[n - 1] = 1.0f;
        return x;
    }
    if (waveform == "constant1") {
        std::fill(x.begin(), x.end(), 1.0f);
        return x;
    }
    if (waveform == "ramp") {
        const double denom = std::max(1, n - 1);
        for (int i = 0; i < n; i++) {
            x[i] = static_cast<float>((2.0 * i - (n - 1)) / denom);
        }
        return x;
    }
    if (waveform == "hand_mixed") {
        const float pattern[] = {0.0f, 1.0f, -1.0f, 0.5f, -0.25f, 2.0f, -3.0f, 0.125f};
        for (int i = 0; i < n; i++) {
            x[i] = pattern[i % (sizeof(pattern) / sizeof(pattern[0]))];
        }
        return x;
    }
    if (waveform == "sine_bin1") {
        for (int i = 0; i < n; i++) x[i] = static_cast<float>(std::sin(2.0 * kPi * i / n));
        return x;
    }
    if (waveform == "cosine_bin1") {
        for (int i = 0; i < n; i++) x[i] = static_cast<float>(std::cos(2.0 * kPi * i / n));
        return x;
    }
    if (waveform == "sine_nonbin" || waveform == "sine_nonbin_1p5") {
        for (int i = 0; i < n; i++) x[i] = static_cast<float>(std::sin(2.0 * kPi * 1.5 * i / n + 0.25));
        return x;
    }
    if (waveform == "sine_nonbin_7p25") {
        for (int i = 0; i < n; i++) x[i] = static_cast<float>(std::sin(2.0 * kPi * 7.25 * i / n + 0.125));
        return x;
    }
    if (waveform == "sine_nonbin_high") {
        const double frequency = 0.5 * n - 1.25;
        for (int i = 0; i < n; i++) x[i] = static_cast<float>(std::sin(2.0 * kPi * frequency * i / n + 0.375));
        return x;
    }
    if (waveform == "cosine_nonbin_1p5") {
        for (int i = 0; i < n; i++) x[i] = static_cast<float>(std::cos(2.0 * kPi * 1.5 * i / n + 0.25));
        return x;
    }
    if (waveform == "two_tone_nonbin") {
        for (int i = 0; i < n; i++) {
            const double a = std::sin(2.0 * kPi * 1.5 * i / n + 0.25);
            const double b = std::cos(2.0 * kPi * 7.25 * i / n + 0.125);
            x[i] = static_cast<float>(0.6 * a + 0.4 * b);
        }
        return x;
    }
    if (waveform == "noise_seed305419896") return GenerateNoise(n, 0x12345678u, 1.0);
    if (waveform == "noise_seed1") return GenerateNoise(n, 1u, 1.0);
    if (waveform == "noise_seed2") return GenerateNoise(n, 2u, 1.0);
    if (waveform == "noise_seed3735928559") return GenerateNoise(n, 0xDEADBEEFu, 1.0);
    if (waveform == "noise_seed3237998081") return GenerateNoise(n, 0xC0FFEE01u, 1.0);
    if (waveform == "alternating_sign") {
        for (int i = 0; i < n; i++) x[i] = (i % 2 == 0) ? 1.0f : -1.0f;
        return x;
    }
    if (waveform == "sparse_impulses") {
        x[0] += 1.0f;
        x[n / 3] += -0.75f;
        x[n / 2] += 0.5f;
        x[n - 1] += -0.25f;
        return x;
    }
    if (waveform == "step_half") {
        for (int i = 0; i < n; i++) x[i] = i < n / 2 ? 1.0f : -1.0f;
        return x;
    }
    if (waveform == "near_cancellation_pairs") {
        for (int i = 0; i < n; i++) x[i] = (i % 2 == 0) ? 1.0f : -0.999999f;
        return x;
    }
    if (waveform == "tiny_noise_1e-30") return GenerateNoise(n, 0x12345678u, 1.0e-30);
    if (waveform == "tiny_constant_1e-38") {
        std::fill(x.begin(), x.end(), static_cast<float>(1.0e-38));
        return x;
    }
    if (waveform == "subnormal_pattern") {
        const float pattern[] = {
            0.0f,
            static_cast<float>(1.0e-45),
            static_cast<float>(-1.0e-45),
            static_cast<float>(1.0e-40),
            static_cast<float>(-1.0e-40),
        };
        for (int i = 0; i < n; i++) x[i] = pattern[i % (sizeof(pattern) / sizeof(pattern[0]))];
        return x;
    }
    if (waveform == "large_sine_nonbin_1e10") {
        for (int i = 0; i < n; i++) x[i] = static_cast<float>(1.0e10 * std::sin(2.0 * kPi * 1.5 * i / n + 0.25));
        return x;
    }
    if (waveform == "large_noise_1e10") return GenerateNoise(n, 0x12345678u, 1.0e10);
    throw std::runtime_error("unknown waveform: " + waveform);
}

int SpectrumScalarCount(int n) {
    return 2 * (n / 2 + 1);
}

std::vector<TestCase> MinimalCases() {
    const int lengths[] = {2, 3, 4, 5, 8, 9, 16, 1024, 12345};
    const char *waveforms[] = {
        "zeros", "impulse0", "impulse1", "impulse_last", "constant1", "ramp",
        "hand_mixed", "sine_bin1", "cosine_bin1", "sine_nonbin", "noise_seed305419896",
    };
    std::vector<TestCase> cases;
    for (int length : lengths) {
        for (const char *waveform : waveforms) cases.push_back(TestCase{length, waveform});
    }
    return cases;
}

std::vector<TestCase> ForwardExtendedCases() {
    const int lengths[] = {
        31, 32, 33, 63, 64, 65, 255, 256, 257, 4095, 4096, 4097, 16384, 32768,
    };
    const char *waveforms[] = {
        "zeros", "impulse0", "impulse1", "impulse_last", "constant1", "ramp",
        "hand_mixed", "sine_bin1", "cosine_bin1", "noise_seed1", "noise_seed2",
        "noise_seed305419896", "noise_seed3735928559", "noise_seed3237998081",
        "sine_nonbin_1p5", "sine_nonbin_7p25", "sine_nonbin_high",
        "cosine_nonbin_1p5", "two_tone_nonbin", "alternating_sign",
        "sparse_impulses", "step_half", "near_cancellation_pairs",
        "tiny_noise_1e-30", "tiny_constant_1e-38", "subnormal_pattern",
        "large_sine_nonbin_1e10", "large_noise_1e10",
    };
    std::vector<TestCase> cases;
    for (int length : lengths) {
        for (const char *waveform : waveforms) cases.push_back(TestCase{length, waveform});
    }
    return cases;
}

std::vector<TestCase> CasesForSet(const std::string &case_set) {
    if (case_set == "minimal") return MinimalCases();
    if (case_set == "forward_extended") return ForwardExtendedCases();
    throw std::runtime_error("unsupported case set: " + case_set);
}

CaseResult ValidateCase(const Options &options, const TestCase &test_case) {
    CaseResult result;
    result.test_case = test_case;

    const std::vector<float> input = GenerateInput(test_case.length, test_case.waveform);
    std::vector<float> spectrum(SpectrumScalarCount(test_case.length), 0.0f);
    std::vector<float> output(test_case.length, 0.0f);

    bakuage::RealDft<float> dft(test_case.length);
    dft.Forward(input.data(), spectrum.data());
    dft.Backward(spectrum.data(), output.data());

    double sum_square_normalized_error = 0.0;
    double case_scale = 1.0;
    for (int i = 0; i < test_case.length; i++) {
        const double expected_raw = static_cast<double>(test_case.length) * static_cast<double>(input[i]);
        const double actual_raw = static_cast<double>(output[i]);
        case_scale = std::max(case_scale, std::abs(expected_raw));
        case_scale = std::max(case_scale, std::abs(actual_raw));
    }

    for (int i = 0; i < test_case.length; i++) {
        const double expected_raw = static_cast<double>(test_case.length) * static_cast<double>(input[i]);
        const double actual_raw = static_cast<double>(output[i]);
        const double raw_abs_error = std::abs(actual_raw - expected_raw);
        const double normalized = actual_raw / static_cast<double>(test_case.length);
        const double normalized_abs_error = std::abs(normalized - static_cast<double>(input[i]));
        const double relative_error = raw_abs_error / case_scale;

        if (!std::isfinite(expected_raw) || !std::isfinite(actual_raw) || !std::isfinite(normalized)) {
            result.nan_or_inf = true;
        }
        result.max_raw_abs_error = std::max(result.max_raw_abs_error, raw_abs_error);
        result.max_normalized_abs_error = std::max(result.max_normalized_abs_error, normalized_abs_error);
        result.max_relative_error = std::max(result.max_relative_error, relative_error);
        sum_square_normalized_error += normalized_abs_error * normalized_abs_error;
    }

    result.rms_normalized_error = std::sqrt(sum_square_normalized_error / static_cast<double>(test_case.length));
    result.passed = !result.nan_or_inf &&
        (result.max_normalized_abs_error <= options.abs_tolerance ||
         result.max_relative_error <= options.rel_tolerance);
    return result;
}

std::string CaseName(const TestCase &test_case) {
    return "n" + std::to_string(test_case.length) + "/" + test_case.waveform;
}

}  // namespace

int main(int argc, char **argv) {
    try {
        const Options options = ParseArgs(argc, argv);
        const std::vector<TestCase> cases = CasesForSet(options.case_set);

        int passed_count = 0;
        int failed_count = 0;
        int nan_or_inf_count = 0;
        CaseResult worst_abs;
        CaseResult worst_rel;
        bool have_result = false;

        for (const TestCase &test_case : cases) {
            const CaseResult result = ValidateCase(options, test_case);
            if (result.passed) {
                passed_count++;
            } else {
                failed_count++;
            }
            if (result.nan_or_inf) nan_or_inf_count++;
            if (!have_result || result.max_normalized_abs_error > worst_abs.max_normalized_abs_error) {
                worst_abs = result;
            }
            if (!have_result || result.max_relative_error > worst_rel.max_relative_error) {
                worst_rel = result;
            }
            have_result = true;

            if (options.verbose || !result.passed) {
                std::cout << (result.passed ? "pass " : "FAIL ")
                          << CaseName(test_case)
                          << " max_norm_abs=" << std::setprecision(10) << result.max_normalized_abs_error
                          << " max_raw_abs=" << result.max_raw_abs_error
                          << " max_rel=" << result.max_relative_error
                          << " rms_norm=" << result.rms_normalized_error
                          << (result.nan_or_inf ? " nan_or_inf" : "")
                          << "\n";
            }
        }

        std::cout << "roundtrip case_set=" << options.case_set
                  << " records=" << cases.size()
                  << " passed=" << passed_count
                  << " failed=" << failed_count
                  << " nan_or_inf=" << nan_or_inf_count
                  << "\n";
        if (have_result) {
            std::cout << "worst_normalized_abs " << CaseName(worst_abs.test_case)
                      << " value=" << std::setprecision(10) << worst_abs.max_normalized_abs_error << "\n";
            std::cout << "worst_relative " << CaseName(worst_rel.test_case)
                      << " value=" << std::setprecision(10) << worst_rel.max_relative_error << "\n";
        }
        std::cout << "expected raw API contract: Backward(Forward(x)) ~= N*x\n";
        return failed_count == 0 ? 0 : 1;
    } catch (const std::exception &e) {
        std::cerr << "dft_roundtrip_validate error: " << e.what() << std::endl;
        return 1;
    }
}
