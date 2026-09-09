#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "bakuage/dft.h"

namespace {

struct Options {
    std::vector<int> lengths;
    int repeats = 3;
    bool backward = false;
};

std::vector<int> ParseLengths(const std::string &text) {
    std::vector<int> lengths;
    std::stringstream ss(text);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (item.empty()) continue;
        const int length = std::atoi(item.c_str());
        if (length <= 0) {
            throw std::runtime_error("lengths must be positive");
        }
        lengths.push_back(length);
    }
    if (lengths.empty()) {
        throw std::runtime_error("at least one length is required");
    }
    return lengths;
}

Options ParseArgs(int argc, char **argv) {
    Options options;
    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        if (arg == "--lengths" && i + 1 < argc) {
            options.lengths = ParseLengths(argv[++i]);
        } else if (arg == "--repeats" && i + 1 < argc) {
            options.repeats = std::atoi(argv[++i]);
        } else if (arg == "--backward") {
            options.backward = true;
        } else {
            throw std::runtime_error("usage: dft_perf_probe --lengths n[,n...] [--repeats n] [--backward]");
        }
    }
    if (options.lengths.empty()) {
        throw std::runtime_error("usage: dft_perf_probe --lengths n[,n...] [--repeats n] [--backward]");
    }
    if (options.repeats <= 0) {
        throw std::runtime_error("repeats must be positive");
    }
    return options;
}

std::vector<float> MakeInput(int n) {
    std::vector<float> input(n, 0.0f);
    for (int i = 0; i < n; i++) {
        const double a = std::sin(2.0 * 3.14159265358979323846 * 1.5 * i / n);
        const double b = std::cos(2.0 * 3.14159265358979323846 * 7.25 * i / n);
        input[i] = static_cast<float>(0.6 * a + 0.4 * b);
    }
    return input;
}

double Median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

void ProbeLength(int length, int repeats, bool backward) {
    std::vector<float> input = MakeInput(length);
    std::vector<float> spectrum(2 * (length / 2 + 1), 0.0f);
    std::vector<float> output(length, 0.0f);
    bakuage::RealDft<float> dft(length);

    dft.Forward(input.data(), spectrum.data());

    std::vector<double> forward_ms;
    std::vector<double> backward_ms;
    for (int i = 0; i < repeats; i++) {
        const auto forward_begin = std::chrono::steady_clock::now();
        dft.Forward(input.data(), spectrum.data());
        const auto forward_end = std::chrono::steady_clock::now();
        forward_ms.push_back(std::chrono::duration<double, std::milli>(forward_end - forward_begin).count());

        if (backward) {
            const auto backward_begin = std::chrono::steady_clock::now();
            dft.Backward(spectrum.data(), output.data());
            const auto backward_end = std::chrono::steady_clock::now();
            backward_ms.push_back(std::chrono::duration<double, std::milli>(backward_end - backward_begin).count());
        }
    }

    std::cout << "length=" << length
              << " repeats=" << repeats
              << " work_size=" << dft.work_size()
              << " forward_ms_median=" << std::fixed << std::setprecision(3) << Median(forward_ms);
    if (backward) {
        std::cout << " backward_ms_median=" << std::fixed << std::setprecision(3) << Median(backward_ms);
    }
    std::cout << std::endl;
}

}  // namespace

int main(int argc, char **argv) {
    try {
        const Options options = ParseArgs(argc, argv);
        for (int length : options.lengths) {
            ProbeLength(length, options.repeats, options.backward);
        }
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "dft_perf_probe error: " << e.what() << std::endl;
        return 1;
    }
}
