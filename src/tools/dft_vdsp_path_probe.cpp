#include <Accelerate/Accelerate.h>

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Options {
    std::vector<int> lengths;
};

std::vector<int> ParseLengths(const std::string &text) {
    std::vector<int> lengths;
    std::stringstream ss(text);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (item.empty()) continue;
        const int length = std::atoi(item.c_str());
        if (length <= 0) throw std::runtime_error("lengths must be positive");
        lengths.push_back(length);
    }
    if (lengths.empty()) throw std::runtime_error("at least one length is required");
    return lengths;
}

Options ParseArgs(int argc, char **argv) {
    Options options;
    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        if (arg == "--lengths" && i + 1 < argc) {
            options.lengths = ParseLengths(argv[++i]);
        } else {
            throw std::runtime_error("usage: dft_vdsp_path_probe --lengths n[,n...]");
        }
    }
    if (options.lengths.empty()) {
        throw std::runtime_error("usage: dft_vdsp_path_probe --lengths n[,n...]");
    }
    return options;
}

bool CanCreateZrop(int length, vDSP_DFT_Direction direction) {
    vDSP_DFT_Setup setup = vDSP_DFT_zrop_CreateSetup(nullptr, static_cast<vDSP_Length>(length), direction);
    if (setup) vDSP_DFT_DestroySetup(setup);
    return setup != nullptr;
}

bool CanCreateZop(int length, vDSP_DFT_Direction direction) {
    vDSP_DFT_Setup setup = vDSP_DFT_zop_CreateSetup(nullptr, static_cast<vDSP_Length>(length), direction);
    if (setup) vDSP_DFT_DestroySetup(setup);
    return setup != nullptr;
}

bool CanCreateLegacy(int length) {
    vDSP_DFT_Setup setup = vDSP_DFT_CreateSetup(nullptr, static_cast<vDSP_Length>(length));
    if (setup) vDSP_DFT_DestroySetup(setup);
    return setup != nullptr;
}

std::string ProductionForwardPath(int length, bool zrop, bool zop, bool legacy) {
    if (length % 2 == 0 && zrop) return "real_even_zrop";
    if (zop) return "complex_zop";
    if (legacy) return "legacy_complex";
    return "unsupported";
}

void ProbeLength(int length) {
    const bool forward_zrop = (length % 2 == 0) && CanCreateZrop(length, vDSP_DFT_FORWARD);
    const bool forward_zop = CanCreateZop(length, vDSP_DFT_FORWARD);
    const bool forward_legacy = CanCreateLegacy(length);
    const bool inverse_zrop = (length % 2 == 0) && CanCreateZrop(length, vDSP_DFT_INVERSE);
    const bool inverse_zop = CanCreateZop(length, vDSP_DFT_INVERSE);

    std::cout << "length=" << length
              << " forward_zrop=" << (forward_zrop ? "yes" : "no")
              << " forward_zop=" << (forward_zop ? "yes" : "no")
              << " legacy=" << (forward_legacy ? "yes" : "no")
              << " inverse_zrop=" << (inverse_zrop ? "yes" : "no")
              << " inverse_zop=" << (inverse_zop ? "yes" : "no")
              << " production_forward_path=" << ProductionForwardPath(length, forward_zrop, forward_zop, forward_legacy)
              << std::endl;
}

}  // namespace

int main(int argc, char **argv) {
    try {
        const Options options = ParseArgs(argc, argv);
        for (int length : options.lengths) ProbeLength(length);
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "dft_vdsp_path_probe error: " << e.what() << std::endl;
        return 1;
    }
}
