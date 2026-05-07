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
    std::string case_name;
    std::string input_file;
    std::string output_file;
    int length = 0;
    int input_scalar_count = 0;
    int output_scalar_count = 0;
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
            throw std::runtime_error("usage: dft_vdsp_forward_prototype --golden-dir <path> --output-dir <path>");
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
    if (precision != "float" || method != "Forward") {
        throw std::runtime_error("only RealDft<float>::Forward manifests are supported");
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
        record.case_name = RequireString(record_object, "case_name");
        record.input_file = RequireString(record_object, "input_file");
        record.output_file = RequireString(record_object, "output_file");
        record.length = RequireInt(record_object, "length");
        record.input_scalar_count = RequireInt(record_object, "input_scalar_count");
        record.output_scalar_count = RequireInt(record_object, "output_scalar_count");
        if (record.precision != "float" || record.method != "Forward") {
            throw std::runtime_error("only float Forward records are supported: " + record.id);
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

struct ForwardOutput {
    std::vector<float> values;
    std::string backend;
};

ForwardOutput ForwardEvenZropToIppPublicLayout(const std::vector<float> &input, int n, int output_scalar_count) {
    const int half = n / 2;
    if (static_cast<int>(input.size()) != n) throw std::runtime_error("input length mismatch");
    if (output_scalar_count != 2 * (half + 1)) throw std::runtime_error("unexpected Forward output scalar count");

    std::vector<float> even(static_cast<std::size_t>(half));
    std::vector<float> odd(static_cast<std::size_t>(half));
    for (int j = 0; j < half; j++) {
        even[static_cast<std::size_t>(j)] = input[static_cast<std::size_t>(2 * j)];
        odd[static_cast<std::size_t>(j)] = input[static_cast<std::size_t>(2 * j + 1)];
    }

    vDSP_DFT_Setup setup = vDSP_DFT_zrop_CreateSetup(nullptr, static_cast<vDSP_Length>(n), vDSP_DFT_FORWARD);
    if (!setup) {
        throw std::runtime_error("vDSP_DFT_zrop_CreateSetup returned null for this length");
    }

    std::vector<float> out_real(static_cast<std::size_t>(half), 0.0f);
    std::vector<float> out_imag(static_cast<std::size_t>(half), 0.0f);
    vDSP_DFT_Execute(setup, even.data(), odd.data(), out_real.data(), out_imag.data());
    vDSP_DFT_DestroySetup(setup);

    std::vector<float> output(static_cast<std::size_t>(output_scalar_count), 0.0f);

    // vDSP zrop forward returns 2 * the mathematical real DFT.  Current IPP
    // golden output was captured with IPP_FFT_NODIV_BY_ANY, so divide by 2
    // here to make an IPP-compatible candidate rather than a raw vDSP dump.
    constexpr float scale = 0.5f;

    // vDSP zrop split layout:
    //   H[0]     is real only and stored in out_real[0].
    //   H[N / 2] is real only and stored in out_imag[0] for even N.
    //   1 <= k < N / 2 are stored as out_real[k] + i * out_imag[k].
    //
    // RealDft<float>::Forward public candidate layout for IPP compatibility:
    //   output[2 * k + 0] = real(H[k])
    //   output[2 * k + 1] = imag(H[k])
    // for k = 0..N/2, with imag(DC) and imag(Nyquist) explicitly zero.
    output[0] = scale * out_real[0];
    output[1] = 0.0f;
    for (int k = 1; k < half; k++) {
        output[static_cast<std::size_t>(2 * k + 0)] = scale * out_real[static_cast<std::size_t>(k)];
        output[static_cast<std::size_t>(2 * k + 1)] = scale * out_imag[static_cast<std::size_t>(k)];
    }
    output[static_cast<std::size_t>(2 * half + 0)] = scale * out_imag[0];
    output[static_cast<std::size_t>(2 * half + 1)] = 0.0f;

    return ForwardOutput{output, "zrop_even"};
}

ForwardOutput ForwardOddComplexToIppPublicLayout(const std::vector<float> &input, int n, int output_scalar_count) {
    const int half = n / 2;
    if (n % 2 == 0) throw std::runtime_error("odd complex path requires an odd real length");
    if (static_cast<int>(input.size()) != n) throw std::runtime_error("input length mismatch");
    if (output_scalar_count != 2 * (half + 1)) throw std::runtime_error("unexpected odd Forward output scalar count");

    std::vector<float> in_real(static_cast<std::size_t>(n));
    std::vector<float> in_imag(static_cast<std::size_t>(n), 0.0f);
    std::copy(input.begin(), input.end(), in_real.begin());

    std::vector<float> out_real(static_cast<std::size_t>(n), 0.0f);
    std::vector<float> out_imag(static_cast<std::size_t>(n), 0.0f);
    std::string backend;

    vDSP_DFT_Setup setup = vDSP_DFT_zop_CreateSetup(nullptr, static_cast<vDSP_Length>(n), vDSP_DFT_FORWARD);
    if (setup) {
        vDSP_DFT_Execute(setup, in_real.data(), in_imag.data(), out_real.data(), out_imag.data());
        vDSP_DFT_DestroySetup(setup);
        backend = "zop_odd";
    } else {
        // Fallback to the older exact-length complex DFT entry point. This is
        // still an N-point complex DFT of the original real input, not a padded
        // transform. It is expected to be slower for unsupported fast lengths.
        setup = vDSP_DFT_CreateSetup(nullptr, static_cast<vDSP_Length>(n));
        if (!setup) {
            throw std::runtime_error("no exact-length vDSP complex DFT setup for odd length");
        }
        vDSP_DFT_zop(setup, in_real.data(), in_imag.data(), 1, out_real.data(), out_imag.data(), 1, vDSP_DFT_FORWARD);
        vDSP_DFT_DestroySetup(setup);
        backend = "legacy_zop_odd";
    }

    std::vector<float> output(static_cast<std::size_t>(output_scalar_count), 0.0f);

    // Odd-length RealDft<float>::Forward public layout stores bins
    // 0..floor(N/2) as interleaved complex scalars. There is no Nyquist
    // singleton for odd N, so the final stored odd bin keeps its imaginary
    // component. Complex vDSP forward is unnormalized and does not use the
    // zrop forward factor C=2, so no 0.5 scaling is applied here.
    output[0] = out_real[0];
    output[1] = 0.0f;
    for (int k = 1; k <= half; k++) {
        output[static_cast<std::size_t>(2 * k + 0)] = out_real[static_cast<std::size_t>(k)];
        output[static_cast<std::size_t>(2 * k + 1)] = out_imag[static_cast<std::size_t>(k)];
    }

    return ForwardOutput{output, backend};
}

ForwardOutput ForwardVdspRealToIppPublicLayout(const std::vector<float> &input, int n, int output_scalar_count) {
    if (n % 2 == 0) {
        return ForwardEvenZropToIppPublicLayout(input, n, output_scalar_count);
    }
    return ForwardOddComplexToIppPublicLayout(input, n, output_scalar_count);
}

GenerateResult GenerateRecord(const std::string &golden_dir, const std::string &output_dir, const Record &record) {
    GenerateResult result;
    result.record = record;
    try {
        const std::vector<float> input = ReadFloatFile(JoinPath(golden_dir, record.input_file), record.input_scalar_count);
        const ForwardOutput output = ForwardVdspRealToIppPublicLayout(input, record.length, record.output_scalar_count);
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
        std::cout << "generated " << generated_count << " vDSP RealDft<float>::Forward candidate outputs";
        std::cout << ", unsupported=" << unsupported_count << "\n";
        std::cout << "backend counts: zrop_even=" << zrop_even_count;
        std::cout << ", zop_odd=" << zop_odd_count;
        std::cout << ", legacy_zop_odd=" << legacy_zop_odd_count << "\n";
        return unsupported_count == 0 ? 0 : 2;
    } catch (const std::exception &e) {
        std::cerr << "dft_vdsp_forward_prototype: " << e.what() << "\n";
        return 1;
    }
}
