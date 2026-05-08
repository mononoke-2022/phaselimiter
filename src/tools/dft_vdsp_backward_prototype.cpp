#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>

#include <Accelerate/Accelerate.h>

#include "picojson.h"

namespace {

struct Options {
    std::string golden_dir;
    std::string output_dir;
};

struct Record {
    std::string id;
    std::string precision;
    std::string method;
    std::string input_kind;
    std::string case_name;
    std::string input_file;
    std::string output_file;
    int length = 0;
    int input_scalar_count = 0;
    int output_scalar_count = 0;
};

struct BackwardOutput {
    std::vector<float> values;
    std::string backend;
};

struct GenerateResult {
    Record record;
    std::string status;
    std::string message;
    std::string backend;
};

std::string JoinPath(const std::string &a, const std::string &b) {
    if (a.empty()) return b;
    const char last = a[a.size() - 1];
    if (last == '/' || last == '\\') return a + b;
    return a + "/" + b;
}

std::string DirName(const std::string &path) {
    const std::string::size_type pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return ".";
    if (pos == 0) return path.substr(0, 1);
    return path.substr(0, pos);
}

bool PathExists(const std::string &path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

bool IsDirectory(const std::string &path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

void MakeDir(const std::string &path) {
    if (path.empty() || PathExists(path)) return;
    if (mkdir(path.c_str(), 0755) != 0 && errno != EEXIST) {
        std::ostringstream ss;
        ss << "failed to create directory: " << path << " errno=" << errno;
        throw std::runtime_error(ss.str());
    }
}

void MakeParentDirs(const std::string &path) {
    const std::string parent = DirName(path);
    if (parent.empty() || parent == "." || parent == "/") return;
    if (PathExists(parent)) return;
    MakeParentDirs(parent);
    MakeDir(parent);
}

bool IsLittleEndian() {
    const std::uint16_t value = 1;
    return *reinterpret_cast<const std::uint8_t *>(&value) == 1;
}

Options ParseArgs(int argc, char **argv) {
    Options options;
    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        if (arg == "--golden-dir" && i + 1 < argc) {
            options.golden_dir = argv[++i];
        } else if (arg == "--output-dir" && i + 1 < argc) {
            options.output_dir = argv[++i];
        } else {
            throw std::runtime_error("usage: dft_vdsp_backward_prototype --golden-dir <path> --output-dir <path>");
        }
    }
    if (options.golden_dir.empty()) throw std::runtime_error("--golden-dir is required");
    if (options.output_dir.empty()) throw std::runtime_error("--output-dir is required");
    return options;
}

void FindManifestFiles(const std::string &dir, int depth, std::vector<std::string> *paths) {
    if (depth < 0 || !IsDirectory(dir)) return;

    const std::string manifest = JoinPath(dir, "manifest.json");
    if (PathExists(manifest)) {
        paths->push_back(manifest);
        return;
    }

    DIR *handle = opendir(dir.c_str());
    if (!handle) return;
    while (dirent *entry = readdir(handle)) {
        const std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        const std::string child = JoinPath(dir, name);
        if (IsDirectory(child)) {
            FindManifestFiles(child, depth - 1, paths);
        }
    }
    closedir(handle);
}

std::string ResolveGoldenDir(const std::string &golden_dir) {
    if (PathExists(JoinPath(golden_dir, "manifest.json"))) {
        return golden_dir;
    }

    std::vector<std::string> manifests;
    FindManifestFiles(golden_dir, 4, &manifests);
    std::sort(manifests.begin(), manifests.end());
    if (manifests.empty()) {
        throw std::runtime_error("failed to find manifest.json under --golden-dir: " + golden_dir);
    }
    if (manifests.size() != 1) {
        throw std::runtime_error("multiple manifest.json files found under --golden-dir; pass the exact dft_golden directory");
    }
    return DirName(manifests[0]);
}

const picojson::value &RequireField(const picojson::object &object, const std::string &key) {
    const picojson::object::const_iterator it = object.find(key);
    if (it == object.end()) throw std::runtime_error("manifest is missing field: " + key);
    return it->second;
}

std::string RequireString(const picojson::object &object, const std::string &key) {
    const picojson::value &value = RequireField(object, key);
    if (!value.is<std::string>()) throw std::runtime_error("manifest field must be string: " + key);
    return value.get<std::string>();
}

std::string OptionalString(const picojson::object &object, const std::string &key) {
    const picojson::object::const_iterator it = object.find(key);
    if (it == object.end()) return "";
    if (!it->second.is<std::string>()) throw std::runtime_error("manifest field must be string: " + key);
    return it->second.get<std::string>();
}

int RequireInt(const picojson::object &object, const std::string &key) {
    const picojson::value &value = RequireField(object, key);
    if (!value.is<double>()) throw std::runtime_error("manifest field must be number: " + key);
    const double number = value.get<double>();
    if (number < 0 || number > static_cast<double>(std::numeric_limits<int>::max()) || std::floor(number) != number) {
        throw std::runtime_error("manifest field must be non-negative integer: " + key);
    }
    return static_cast<int>(number);
}

picojson::value ReadJsonFile(const std::string &path) {
    std::ifstream in(path.c_str());
    if (!in) throw std::runtime_error("failed to open manifest: " + path);

    picojson::value value;
    const std::string error = picojson::parse(value, in);
    if (!error.empty()) throw std::runtime_error("failed to parse manifest: " + error);
    if (!value.is<picojson::object>()) throw std::runtime_error("manifest root must be object");
    return value;
}

std::vector<Record> ReadRecords(const std::string &manifest_path) {
    const picojson::value manifest = ReadJsonFile(manifest_path);
    const picojson::object &root = manifest.get<picojson::object>();
    const std::string precision = RequireString(root, "precision");
    const std::string method = RequireString(root, "method");
    const std::string input_kind = OptionalString(root, "input_kind");
    if (precision != "float" || method != "Backward" || input_kind != "real_dft_spectrum") {
        throw std::runtime_error("only RealDft<float>::Backward real_dft_spectrum manifests are supported");
    }

    const picojson::value &records_value = RequireField(root, "records");
    if (!records_value.is<picojson::array>()) throw std::runtime_error("manifest records must be array");

    std::vector<Record> records;
    for (const picojson::value &record_value : records_value.get<picojson::array>()) {
        if (!record_value.is<picojson::object>()) throw std::runtime_error("record must be object");
        const picojson::object &record_object = record_value.get<picojson::object>();
        Record record;
        record.id = RequireString(record_object, "id");
        record.precision = RequireString(record_object, "precision");
        record.method = RequireString(record_object, "method");
        record.input_kind = OptionalString(record_object, "input_kind");
        record.case_name = RequireString(record_object, "case_name");
        record.input_file = RequireString(record_object, "input_file");
        record.output_file = RequireString(record_object, "output_file");
        record.length = RequireInt(record_object, "length");
        record.input_scalar_count = RequireInt(record_object, "input_scalar_count");
        record.output_scalar_count = RequireInt(record_object, "output_scalar_count");
        if (record.precision != "float" || record.method != "Backward" || record.input_kind != "real_dft_spectrum") {
            throw std::runtime_error("only float Backward real_dft_spectrum records are supported: " + record.id);
        }
        if (record.input_scalar_count != 2 * (record.length / 2 + 1)) {
            throw std::runtime_error("unexpected Backward spectrum scalar count: " + record.id);
        }
        if (record.output_scalar_count != record.length) {
            throw std::runtime_error("unexpected Backward output scalar count: " + record.id);
        }
        records.push_back(record);
    }
    return records;
}

std::vector<float> ReadFloatFile(const std::string &path, int expected_scalar_count) {
    std::ifstream in(path.c_str(), std::ios::binary | std::ios::ate);
    if (!in) throw std::runtime_error("missing file: " + path);
    const std::ifstream::pos_type end_pos = in.tellg();
    if (end_pos < 0) throw std::runtime_error("failed to get file size: " + path);

    const std::uint64_t expected_bytes = static_cast<std::uint64_t>(expected_scalar_count) * sizeof(float);
    const std::uint64_t actual_bytes = static_cast<std::uint64_t>(end_pos);
    if (actual_bytes != expected_bytes) {
        std::ostringstream ss;
        ss << "scalar count mismatch for " << path << ": expected " << expected_scalar_count
           << " floats (" << expected_bytes << " bytes), got " << actual_bytes << " bytes";
        throw std::runtime_error(ss.str());
    }

    std::vector<float> values(static_cast<std::size_t>(expected_scalar_count));
    in.seekg(0, std::ios::beg);
    in.read(reinterpret_cast<char *>(values.data()), static_cast<std::streamsize>(expected_bytes));
    if (!in) throw std::runtime_error("failed to read file: " + path);
    return values;
}

void WriteFloatFile(const std::string &path, const std::vector<float> &values) {
    MakeParentDirs(path);
    std::ofstream out(path.c_str(), std::ios::binary);
    if (!out) throw std::runtime_error("failed to open output: " + path);
    out.write(reinterpret_cast<const char *>(values.data()), static_cast<std::streamsize>(values.size() * sizeof(float)));
    if (!out) throw std::runtime_error("failed to write output: " + path);
}

BackwardOutput BackwardEvenZropFromIppPublicLayout(const std::vector<float> &spectrum, int n) {
    const int half = n / 2;
    if (n % 2 != 0) throw std::runtime_error("even zrop path requires an even real length");
    if (static_cast<int>(spectrum.size()) != 2 * (half + 1)) throw std::runtime_error("spectrum length mismatch");

    std::vector<float> in_real(static_cast<std::size_t>(half), 0.0f);
    std::vector<float> in_imag(static_cast<std::size_t>(half), 0.0f);
    in_real[0] = spectrum[0];
    in_imag[0] = spectrum[static_cast<std::size_t>(2 * half)];
    for (int k = 1; k < half; k++) {
        in_real[static_cast<std::size_t>(k)] = spectrum[static_cast<std::size_t>(2 * k + 0)];
        in_imag[static_cast<std::size_t>(k)] = spectrum[static_cast<std::size_t>(2 * k + 1)];
    }

    vDSP_DFT_Setup setup = vDSP_DFT_zrop_CreateSetup(nullptr, static_cast<vDSP_Length>(n), vDSP_DFT_INVERSE);
    if (!setup) {
        throw std::runtime_error("vDSP_DFT_zrop_CreateSetup returned null for this even length");
    }

    std::vector<float> even(static_cast<std::size_t>(half), 0.0f);
    std::vector<float> odd(static_cast<std::size_t>(half), 0.0f);
    vDSP_DFT_Execute(setup, in_real.data(), in_imag.data(), even.data(), odd.data());
    vDSP_DFT_DestroySetup(setup);

    std::vector<float> output(static_cast<std::size_t>(n), 0.0f);
    for (int j = 0; j < half; j++) {
        output[static_cast<std::size_t>(2 * j)] = even[static_cast<std::size_t>(j)];
        output[static_cast<std::size_t>(2 * j + 1)] = odd[static_cast<std::size_t>(j)];
    }
    return BackwardOutput{output, "zrop_even"};
}

BackwardOutput BackwardOddComplexFromIppPublicLayout(const std::vector<float> &spectrum, int n) {
    const int half = n / 2;
    if (n % 2 == 0) throw std::runtime_error("odd complex path requires an odd real length");
    if (static_cast<int>(spectrum.size()) != 2 * (half + 1)) throw std::runtime_error("spectrum length mismatch");

    std::vector<float> in_real(static_cast<std::size_t>(n), 0.0f);
    std::vector<float> in_imag(static_cast<std::size_t>(n), 0.0f);
    in_real[0] = spectrum[0];
    in_imag[0] = 0.0f;
    for (int k = 1; k <= half; k++) {
        const float real = spectrum[static_cast<std::size_t>(2 * k + 0)];
        const float imag = spectrum[static_cast<std::size_t>(2 * k + 1)];
        in_real[static_cast<std::size_t>(k)] = real;
        in_imag[static_cast<std::size_t>(k)] = imag;
        in_real[static_cast<std::size_t>(n - k)] = real;
        in_imag[static_cast<std::size_t>(n - k)] = -imag;
    }

    std::vector<float> out_real(static_cast<std::size_t>(n), 0.0f);
    std::vector<float> out_imag(static_cast<std::size_t>(n), 0.0f);
    std::string backend;

    vDSP_DFT_Setup setup = vDSP_DFT_zop_CreateSetup(nullptr, static_cast<vDSP_Length>(n), vDSP_DFT_INVERSE);
    if (setup) {
        vDSP_DFT_Execute(setup, in_real.data(), in_imag.data(), out_real.data(), out_imag.data());
        vDSP_DFT_DestroySetup(setup);
        backend = "zop_odd";
    } else {
        setup = vDSP_DFT_CreateSetup(nullptr, static_cast<vDSP_Length>(n));
        if (!setup) {
            throw std::runtime_error("no exact-length vDSP complex DFT setup for odd length");
        }
        vDSP_DFT_zop(setup, in_real.data(), in_imag.data(), 1, out_real.data(), out_imag.data(), 1, vDSP_DFT_INVERSE);
        vDSP_DFT_DestroySetup(setup);
        backend = "legacy_zop_odd";
    }

    return BackwardOutput{out_real, backend};
}

BackwardOutput BackwardVdspFromIppPublicLayout(const std::vector<float> &spectrum, int n) {
    if (n % 2 == 0) {
        return BackwardEvenZropFromIppPublicLayout(spectrum, n);
    }
    return BackwardOddComplexFromIppPublicLayout(spectrum, n);
}

GenerateResult GenerateRecord(const std::string &golden_dir, const std::string &output_dir, const Record &record) {
    GenerateResult result;
    result.record = record;
    try {
        const std::vector<float> input = ReadFloatFile(JoinPath(golden_dir, record.input_file), record.input_scalar_count);
        const BackwardOutput output = BackwardVdspFromIppPublicLayout(input, record.length);
        if (static_cast<int>(output.values.size()) != record.output_scalar_count) {
            throw std::runtime_error("candidate output scalar count mismatch");
        }
        WriteFloatFile(JoinPath(output_dir, record.output_file), output.values);
        result.status = "generated";
        result.message = record.output_file;
        result.backend = output.backend;
    } catch (const std::exception &e) {
        result.status = "unsupported";
        result.message = e.what();
    }
    return result;
}

}  // namespace

int main(int argc, char **argv) {
    try {
        if (!IsLittleEndian()) {
            throw std::runtime_error("this tool reads/writes little-endian float payloads and requires a little-endian host");
        }

        const Options options = ParseArgs(argc, argv);
        const std::string golden_dir = ResolveGoldenDir(options.golden_dir);
        const std::string manifest_path = JoinPath(golden_dir, "manifest.json");
        const std::vector<Record> records = ReadRecords(manifest_path);

        MakeDir(options.output_dir);
        MakeDir(JoinPath(options.output_dir, "outputs"));

        int generated_count = 0;
        int unsupported_count = 0;
        int zrop_even_count = 0;
        int zop_odd_count = 0;
        int legacy_zop_odd_count = 0;
        for (const Record &record : records) {
            const GenerateResult result = GenerateRecord(golden_dir, options.output_dir, record);
            if (result.status == "generated") {
                generated_count++;
                if (result.backend == "zrop_even") {
                    zrop_even_count++;
                } else if (result.backend == "zop_odd") {
                    zop_odd_count++;
                } else if (result.backend == "legacy_zop_odd") {
                    legacy_zop_odd_count++;
                }
            } else {
                unsupported_count++;
                std::cerr << record.id << ": " << result.message << "\n";
            }
        }

        std::cout << "read manifest " << manifest_path << "\n";
        std::cout << "generated " << generated_count << " vDSP RealDft<float>::Backward candidate outputs";
        std::cout << ", unsupported=" << unsupported_count << "\n";
        std::cout << "backend counts: zrop_even=" << zrop_even_count;
        std::cout << ", zop_odd=" << zop_odd_count;
        std::cout << ", legacy_zop_odd=" << legacy_zop_odd_count << "\n";
        return unsupported_count == 0 ? 0 : 2;
    } catch (const std::exception &e) {
        std::cerr << "dft_vdsp_backward_prototype: " << e.what() << "\n";
        return 1;
    }
}
