#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

#include "bakuage/dft.h"

#ifndef BAKUAGE_USE_IPP
#define BAKUAGE_USE_IPP 1
#endif

namespace {

constexpr const char *kSchema = "phase_limiter.dft_golden.v1";
constexpr const char *kToolName = "dft_golden_capture";
constexpr double kPi = 3.141592653589793238462643383279502884;

struct Options {
    std::string output_dir;
    std::string case_set = "minimal";
};

struct TestCase {
    int length;
    std::string waveform;
};

struct Record {
    std::string id;
    int length;
    std::string waveform;
    std::string input_file;
    std::string output_file;
    int input_scalar_count;
    int output_scalar_count;
};

std::string JoinPath(const std::string &a, const std::string &b) {
    if (a.empty()) return b;
    const char last = a[a.size() - 1];
    if (last == '/' || last == '\\') return a + b;
    return a + "/" + b;
}

void MakeDir(const std::string &path) {
    if (path.empty()) return;
#if defined(_WIN32)
    const int result = _mkdir(path.c_str());
#else
    const int result = mkdir(path.c_str(), 0755);
#endif
    if (result != 0 && errno != EEXIST) {
        std::ostringstream ss;
        ss << "failed to create directory: " << path << " errno=" << errno;
        throw std::runtime_error(ss.str());
    }
}

bool IsLittleEndian() {
    const std::uint16_t value = 1;
    return *reinterpret_cast<const std::uint8_t *>(&value) == 1;
}

std::string JsonEscape(const std::string &s) {
    std::ostringstream out;
    for (char c : s) {
        switch (c) {
            case '\\': out << "\\\\"; break;
            case '"': out << "\\\""; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default: out << c; break;
        }
    }
    return out.str();
}

std::string RunCommandCaptureFirstLine(const char *command) {
#if defined(_WIN32)
    FILE *pipe = _popen(command, "r");
#else
    FILE *pipe = popen(command, "r");
#endif
    if (!pipe) return "";

    char buffer[256] = {};
    std::string result;
    if (std::fgets(buffer, sizeof(buffer), pipe)) {
        result = buffer;
        while (!result.empty() && (result[result.size() - 1] == '\n' || result[result.size() - 1] == '\r')) {
            result.resize(result.size() - 1);
        }
    }

#if defined(_WIN32)
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return result;
}

Options ParseArgs(int argc, char **argv) {
    Options options;
    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        if (arg == "--output-dir" && i + 1 < argc) {
            options.output_dir = argv[++i];
        } else if (arg == "--case-set" && i + 1 < argc) {
            options.case_set = argv[++i];
        } else if (arg == "--format" && i + 1 < argc) {
            const std::string format = argv[++i];
            if (format != "json-binary") {
                throw std::runtime_error("only --format json-binary is supported");
            }
        } else {
            throw std::runtime_error("usage: dft_golden_capture --output-dir <dir> [--case-set minimal|forward_extended|minimal_backward] [--format json-binary]");
        }
    }
    if (options.output_dir.empty()) {
        throw std::runtime_error("--output-dir is required");
    }
    if (options.case_set != "minimal" &&
        options.case_set != "forward_extended" &&
        options.case_set != "minimal_backward") {
        throw std::runtime_error("only --case-set minimal, forward_extended, or minimal_backward is supported");
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
    if (waveform == "zeros") {
        return x;
    }
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
        for (int i = 0; i < n; i++) {
            x[i] = static_cast<float>(std::sin(2.0 * kPi * i / n));
        }
        return x;
    }
    if (waveform == "cosine_bin1") {
        for (int i = 0; i < n; i++) {
            x[i] = static_cast<float>(std::cos(2.0 * kPi * i / n));
        }
        return x;
    }
    if (waveform == "sine_nonbin") {
        for (int i = 0; i < n; i++) {
            x[i] = static_cast<float>(std::sin(2.0 * kPi * 1.5 * i / n + 0.25));
        }
        return x;
    }
    if (waveform == "sine_nonbin_1p5") {
        for (int i = 0; i < n; i++) {
            x[i] = static_cast<float>(std::sin(2.0 * kPi * 1.5 * i / n + 0.25));
        }
        return x;
    }
    if (waveform == "sine_nonbin_7p25") {
        for (int i = 0; i < n; i++) {
            x[i] = static_cast<float>(std::sin(2.0 * kPi * 7.25 * i / n + 0.125));
        }
        return x;
    }
    if (waveform == "sine_nonbin_high") {
        const double frequency = 0.5 * n - 1.25;
        for (int i = 0; i < n; i++) {
            x[i] = static_cast<float>(std::sin(2.0 * kPi * frequency * i / n + 0.375));
        }
        return x;
    }
    if (waveform == "cosine_nonbin_1p5") {
        for (int i = 0; i < n; i++) {
            x[i] = static_cast<float>(std::cos(2.0 * kPi * 1.5 * i / n + 0.25));
        }
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
    if (waveform == "noise_seed305419896") {
        return GenerateNoise(n, 0x12345678u, 1.0);
    }
    if (waveform == "noise_seed1") {
        return GenerateNoise(n, 1u, 1.0);
    }
    if (waveform == "noise_seed2") {
        return GenerateNoise(n, 2u, 1.0);
    }
    if (waveform == "noise_seed3735928559") {
        return GenerateNoise(n, 0xDEADBEEFu, 1.0);
    }
    if (waveform == "noise_seed3237998081") {
        return GenerateNoise(n, 0xC0FFEE01u, 1.0);
    }
    if (waveform == "alternating_sign") {
        for (int i = 0; i < n; i++) {
            x[i] = (i % 2 == 0) ? 1.0f : -1.0f;
        }
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
        for (int i = 0; i < n; i++) {
            x[i] = i < n / 2 ? 1.0f : -1.0f;
        }
        return x;
    }
    if (waveform == "near_cancellation_pairs") {
        for (int i = 0; i < n; i++) {
            x[i] = (i % 2 == 0) ? 1.0f : -0.999999f;
        }
        return x;
    }
    if (waveform == "tiny_noise_1e-30") {
        return GenerateNoise(n, 0x12345678u, 1.0e-30);
    }
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
        for (int i = 0; i < n; i++) {
            x[i] = pattern[i % (sizeof(pattern) / sizeof(pattern[0]))];
        }
        return x;
    }
    if (waveform == "large_sine_nonbin_1e10") {
        for (int i = 0; i < n; i++) {
            x[i] = static_cast<float>(1.0e10 * std::sin(2.0 * kPi * 1.5 * i / n + 0.25));
        }
        return x;
    }
    if (waveform == "large_noise_1e10") {
        return GenerateNoise(n, 0x12345678u, 1.0e10);
    }

    throw std::runtime_error("unknown waveform: " + waveform);
}

int SpectrumScalarCount(int n) {
    return 2 * (n / 2 + 1);
}

bool HasOrdinaryComplexBin(int n) {
    return n >= 3;
}

int FirstOrdinaryComplexBin(int n) {
    if (!HasOrdinaryComplexBin(n)) {
        throw std::runtime_error("no ordinary complex bin for length: " + std::to_string(n));
    }
    return 1;
}

std::vector<float> GenerateBackwardSpectrum(int n, const std::string &case_name) {
    std::vector<float> spectrum(SpectrumScalarCount(n), 0.0f);
    const int half = n / 2;
    const bool even = (n % 2) == 0;

    if (case_name == "dc_only") {
        spectrum[0] = 1.0f;
        return spectrum;
    }
    if (case_name == "single_bin_real") {
        const int bin = FirstOrdinaryComplexBin(n);
        spectrum[2 * bin] = 1.0f;
        return spectrum;
    }
    if (case_name == "single_bin_imag") {
        const int bin = FirstOrdinaryComplexBin(n);
        spectrum[2 * bin + 1] = 1.0f;
        return spectrum;
    }
    if (case_name == "final_even_nyquist_real") {
        if (!even) throw std::runtime_error("final_even_nyquist_real requires even length");
        spectrum[2 * half] = 1.0f;
        return spectrum;
    }
    if (case_name == "final_odd_bin_complex") {
        if (even) throw std::runtime_error("final_odd_bin_complex requires odd length");
        spectrum[2 * half] = 0.5f;
        spectrum[2 * half + 1] = -0.75f;
        return spectrum;
    }
    if (case_name == "conjugate_safe_noise") {
        std::uint32_t state = 0xBADC0DEu ^ static_cast<std::uint32_t>(n * 2654435761u);
        spectrum[0] = static_cast<float>((2.0 * (static_cast<double>(XorShift32(&state)) / 4294967295.0) - 1.0) * 0.5);
        spectrum[1] = 0.0f;
        for (int bin = 1; bin <= half; bin++) {
            const double real_u = static_cast<double>(XorShift32(&state)) / 4294967295.0;
            const double imag_u = static_cast<double>(XorShift32(&state)) / 4294967295.0;
            spectrum[2 * bin] = static_cast<float>((2.0 * real_u - 1.0) * 0.5);
            spectrum[2 * bin + 1] = (even && bin == half) ? 0.0f : static_cast<float>((2.0 * imag_u - 1.0) * 0.5);
        }
        return spectrum;
    }
    if (case_name == "forward_output_from_time_cases") {
        const std::vector<float> input = GenerateInput(n, "hand_mixed");
        bakuage::RealDft<float> dft(n);
        dft.Forward(input.data(), spectrum.data());
        return spectrum;
    }

    throw std::runtime_error("unknown backward spectrum case: " + case_name);
}

void WriteFloatBinary(const std::string &path, const std::vector<float> &values) {
    std::ofstream out(path.c_str(), std::ios::binary);
    if (!out) throw std::runtime_error("failed to open output: " + path);
    out.write(reinterpret_cast<const char *>(values.data()), static_cast<std::streamsize>(values.size() * sizeof(float)));
    if (!out) throw std::runtime_error("failed to write output: " + path);
}

std::vector<TestCase> MinimalCases() {
    const int lengths[] = {2, 3, 4, 5, 8, 9, 16, 1024, 12345};
    const char *waveforms[] = {
        "zeros",
        "impulse0",
        "impulse1",
        "impulse_last",
        "constant1",
        "ramp",
        "hand_mixed",
        "sine_bin1",
        "cosine_bin1",
        "sine_nonbin",
        "noise_seed305419896",
    };
    std::vector<TestCase> cases;
    for (int length : lengths) {
        for (const char *waveform : waveforms) {
            cases.push_back(TestCase{length, waveform});
        }
    }
    return cases;
}

std::vector<TestCase> ForwardExtendedCases() {
    const int lengths[] = {
        31, 32, 33,
        63, 64, 65,
        255, 256, 257,
        4095, 4096, 4097,
        16384, 32768,
    };
    const char *waveforms[] = {
        "zeros",
        "impulse0",
        "impulse1",
        "impulse_last",
        "constant1",
        "ramp",
        "hand_mixed",
        "sine_bin1",
        "cosine_bin1",
        "noise_seed1",
        "noise_seed2",
        "noise_seed305419896",
        "noise_seed3735928559",
        "noise_seed3237998081",
        "sine_nonbin_1p5",
        "sine_nonbin_7p25",
        "sine_nonbin_high",
        "cosine_nonbin_1p5",
        "two_tone_nonbin",
        "alternating_sign",
        "sparse_impulses",
        "step_half",
        "near_cancellation_pairs",
        "tiny_noise_1e-30",
        "tiny_constant_1e-38",
        "subnormal_pattern",
        "large_sine_nonbin_1e10",
        "large_noise_1e10",
    };
    std::vector<TestCase> cases;
    for (int length : lengths) {
        for (const char *waveform : waveforms) {
            cases.push_back(TestCase{length, waveform});
        }
    }
    return cases;
}

std::vector<TestCase> MinimalBackwardCases() {
    const int lengths[] = {2, 3, 4, 5, 8, 9, 16, 1024, 12345};
    std::vector<TestCase> cases;
    for (int length : lengths) {
        cases.push_back(TestCase{length, "dc_only"});
        if (HasOrdinaryComplexBin(length)) {
            cases.push_back(TestCase{length, "single_bin_real"});
            cases.push_back(TestCase{length, "single_bin_imag"});
        }
        if (length % 2 == 0) {
            cases.push_back(TestCase{length, "final_even_nyquist_real"});
        } else {
            cases.push_back(TestCase{length, "final_odd_bin_complex"});
        }
        cases.push_back(TestCase{length, "conjugate_safe_noise"});
        cases.push_back(TestCase{length, "forward_output_from_time_cases"});
    }
    return cases;
}

std::vector<TestCase> CasesForSet(const std::string &case_set) {
    if (case_set == "minimal") return MinimalCases();
    if (case_set == "forward_extended") return ForwardExtendedCases();
    if (case_set == "minimal_backward") return MinimalBackwardCases();
    throw std::runtime_error("unsupported case set: " + case_set);
}

std::string MethodForCaseSet(const std::string &case_set) {
    if (case_set == "minimal_backward") return "Backward";
    return "Forward";
}

std::string InputKindForCaseSet(const std::string &case_set) {
    if (case_set == "minimal_backward") return "real_dft_spectrum";
    return "real_time_domain";
}

std::string MethodFileToken(const std::string &method) {
    std::string token = method;
    std::transform(token.begin(), token.end(), token.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return token;
}

std::string CaseId(const TestCase &test_case, const std::string &method) {
    std::ostringstream ss;
    ss << "float_n" << test_case.length << "_" << test_case.waveform << "_" << MethodFileToken(method) << "_oop_internal_work";
    return ss.str();
}

void RunForward(int length, const std::vector<float> &input, std::vector<float> *output) {
    bakuage::RealDft<float> dft(length);
    dft.Forward(input.data(), output->data());
}

void RunBackward(int length, const std::vector<float> &input, std::vector<float> *output) {
    bakuage::RealDft<float> dft(length);
    dft.Backward(input.data(), output->data());
}

void WriteManifest(
        const std::string &path,
        const std::string &case_set,
        const std::string &method,
        const std::string &input_kind,
        const std::vector<Record> &records) {
    std::ofstream out(path.c_str());
    if (!out) throw std::runtime_error("failed to open manifest: " + path);

    const std::string git_commit = RunCommandCaptureFirstLine("git rev-parse HEAD 2>/dev/null");
    out << "{\n";
    out << "  \"schema\": \"" << kSchema << "\",\n";
    out << "  \"tool_name\": \"" << kToolName << "\",\n";
    out << "  \"git_commit\": \"" << JsonEscape(git_commit) << "\",\n";
    out << "  \"case_set\": \"" << JsonEscape(case_set) << "\",\n";
    out << "  \"precision\": \"float\",\n";
    out << "  \"method\": \"" << JsonEscape(method) << "\",\n";
    out << "  \"input_kind\": \"" << JsonEscape(input_kind) << "\",\n";
    out << "  \"endianness\": \"little\",\n";
    out << "  \"records\": [\n";
    for (std::size_t i = 0; i < records.size(); i++) {
        const Record &r = records[i];
        out << "    {\n";
        out << "      \"id\": \"" << JsonEscape(r.id) << "\",\n";
        out << "      \"precision\": \"float\",\n";
        out << "      \"method\": \"" << JsonEscape(method) << "\",\n";
        out << "      \"input_kind\": \"" << JsonEscape(input_kind) << "\",\n";
        out << "      \"case_name\": \"" << JsonEscape(r.waveform) << "\",\n";
        out << "      \"length\": " << r.length << ",\n";
        out << "      \"input_file\": \"" << JsonEscape(r.input_file) << "\",\n";
        out << "      \"output_file\": \"" << JsonEscape(r.output_file) << "\",\n";
        out << "      \"input_scalar_count\": " << r.input_scalar_count << ",\n";
        out << "      \"output_scalar_count\": " << r.output_scalar_count << "\n";
        out << "    }" << (i + 1 == records.size() ? "\n" : ",\n");
    }
    out << "  ]\n";
    out << "}\n";
}

}  // namespace

int main(int argc, char **argv) {
    try {
        if (!IsLittleEndian()) {
            throw std::runtime_error("this tool writes little-endian payloads and requires a little-endian host");
        }

        const Options options = ParseArgs(argc, argv);
        const std::string input_dir = JoinPath(options.output_dir, "inputs");
        const std::string output_dir = JoinPath(options.output_dir, "outputs");
        MakeDir(options.output_dir);
        MakeDir(input_dir);
        MakeDir(output_dir);

        const std::string method = MethodForCaseSet(options.case_set);
        const std::string input_kind = InputKindForCaseSet(options.case_set);
        std::vector<Record> records;
        for (const TestCase &test_case : CasesForSet(options.case_set)) {
            const std::string id = CaseId(test_case, method);
            const std::string input_file = "inputs/float_n" + std::to_string(test_case.length) + "_" + test_case.waveform + ".bin";
            const std::string output_file = "outputs/" + id + ".bin";

            std::vector<float> input;
            std::vector<float> output;
            if (method == "Forward") {
                input = GenerateInput(test_case.length, test_case.waveform);
                output.assign(SpectrumScalarCount(test_case.length), 0.0f);
                RunForward(test_case.length, input, &output);
            } else if (method == "Backward") {
                input = GenerateBackwardSpectrum(test_case.length, test_case.waveform);
                output.assign(test_case.length, 0.0f);
                RunBackward(test_case.length, input, &output);
            } else {
                throw std::runtime_error("unsupported method: " + method);
            }

            WriteFloatBinary(JoinPath(options.output_dir, input_file), input);
            WriteFloatBinary(JoinPath(options.output_dir, output_file), output);

            records.push_back(Record{
                id,
                test_case.length,
                test_case.waveform,
                input_file,
                output_file,
                static_cast<int>(input.size()),
                static_cast<int>(output.size()),
            });
        }

        WriteManifest(JoinPath(options.output_dir, "manifest.json"), options.case_set, method, input_kind, records);
        std::cout << "wrote " << records.size() << " DFT golden capture records to " << options.output_dir << std::endl;
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "dft_golden_capture error: " << e.what() << std::endl;
        return 1;
    }
}
