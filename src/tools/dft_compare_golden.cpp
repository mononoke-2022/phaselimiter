#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "picojson.h"

namespace {

constexpr const char *kSchema = "phase_limiter.dft_compare.v1";

struct Options {
    std::string golden_dir;
    std::string candidate_dir;
    std::string output_report;
};

struct Record {
    std::string id;
    std::string precision;
    std::string method;
    std::string case_name;
    std::string output_file;
    int length = 0;
    int output_scalar_count = 0;
};

struct RecordResult {
    Record record;
    std::string status = "pass";
    std::string error;
    double max_abs_error = 0.0;
    double rms_error = 0.0;
    bool nan_or_inf = false;
};

std::string JoinPath(const std::string &a, const std::string &b) {
    if (a.empty()) return b;
    const char last = a[a.size() - 1];
    if (last == '/' || last == '\\') return a + b;
    return a + "/" + b;
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

Options ParseArgs(int argc, char **argv) {
    Options options;
    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        if (arg == "--golden-dir" && i + 1 < argc) {
            options.golden_dir = argv[++i];
        } else if (arg == "--candidate-dir" && i + 1 < argc) {
            options.candidate_dir = argv[++i];
        } else if (arg == "--output-report" && i + 1 < argc) {
            options.output_report = argv[++i];
        } else {
            throw std::runtime_error("usage: dft_compare_golden --golden-dir <dir> --candidate-dir <dir> --output-report <path>");
        }
    }
    if (options.golden_dir.empty()) throw std::runtime_error("--golden-dir is required");
    if (options.candidate_dir.empty()) throw std::runtime_error("--candidate-dir is required");
    if (options.output_report.empty()) throw std::runtime_error("--output-report is required");
    return options;
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
        record.output_file = RequireString(record_object, "output_file");
        record.length = RequireInt(record_object, "length");
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

RecordResult CompareRecord(const Options &options, const Record &record) {
    RecordResult result;
    result.record = record;

    try {
        const std::string golden_path = JoinPath(options.golden_dir, record.output_file);
        const std::string candidate_path = JoinPath(options.candidate_dir, record.output_file);
        const std::vector<float> golden = ReadFloatFile(golden_path, record.output_scalar_count);
        const std::vector<float> candidate = ReadFloatFile(candidate_path, record.output_scalar_count);

        double sum_square_error = 0.0;
        for (std::size_t i = 0; i < golden.size(); i++) {
            if (!std::isfinite(golden[i]) || !std::isfinite(candidate[i])) {
                result.nan_or_inf = true;
            }
            const double diff = static_cast<double>(candidate[i]) - static_cast<double>(golden[i]);
            result.max_abs_error = std::max(result.max_abs_error, std::abs(diff));
            sum_square_error += diff * diff;
        }
        result.rms_error = golden.empty() ? 0.0 : std::sqrt(sum_square_error / static_cast<double>(golden.size()));
        if (result.nan_or_inf) {
            result.status = "fail";
            result.error = "NaN or Inf detected";
        }
    } catch (const std::exception &e) {
        result.status = "fail";
        result.error = e.what();
    }

    return result;
}

void WriteReport(const std::string &path, const Options &options, const std::vector<RecordResult> &results) {
    int passed_count = 0;
    int failed_count = 0;
    double max_abs_error_global = 0.0;
    double sum_weighted_square_error = 0.0;
    std::uint64_t scalar_count_global = 0;

    for (const RecordResult &result : results) {
        if (result.status == "pass") {
            passed_count++;
        } else {
            failed_count++;
        }
        max_abs_error_global = std::max(max_abs_error_global, result.max_abs_error);
        sum_weighted_square_error += result.rms_error * result.rms_error * result.record.output_scalar_count;
        scalar_count_global += static_cast<std::uint64_t>(result.record.output_scalar_count);
    }

    const double rms_error_global = scalar_count_global == 0
        ? 0.0
        : std::sqrt(sum_weighted_square_error / static_cast<double>(scalar_count_global));

    std::ofstream out(path.c_str());
    if (!out) throw std::runtime_error("failed to open output report: " + path);
    out << std::setprecision(10);
    out << "{\n";
    out << "  \"schema\": \"" << kSchema << "\",\n";
    out << "  \"golden_dir\": \"" << JsonEscape(options.golden_dir) << "\",\n";
    out << "  \"candidate_dir\": \"" << JsonEscape(options.candidate_dir) << "\",\n";
    out << "  \"precision\": \"float\",\n";
    out << "  \"method\": \"Forward\",\n";
    out << "  \"record_count\": " << results.size() << ",\n";
    out << "  \"passed_count\": " << passed_count << ",\n";
    out << "  \"failed_count\": " << failed_count << ",\n";
    out << "  \"max_abs_error_global\": " << max_abs_error_global << ",\n";
    out << "  \"rms_error_global\": " << rms_error_global << ",\n";
    out << "  \"records\": [\n";
    for (std::size_t i = 0; i < results.size(); i++) {
        const RecordResult &result = results[i];
        out << "    {\n";
        out << "      \"id\": \"" << JsonEscape(result.record.id) << "\",\n";
        out << "      \"length\": " << result.record.length << ",\n";
        out << "      \"case_name\": \"" << JsonEscape(result.record.case_name) << "\",\n";
        out << "      \"output_file\": \"" << JsonEscape(result.record.output_file) << "\",\n";
        out << "      \"scalar_count\": " << result.record.output_scalar_count << ",\n";
        out << "      \"max_abs_error\": " << result.max_abs_error << ",\n";
        out << "      \"rms_error\": " << result.rms_error << ",\n";
        out << "      \"nan_or_inf\": " << (result.nan_or_inf ? "true" : "false") << ",\n";
        out << "      \"status\": \"" << result.status << "\"";
        if (!result.error.empty()) {
            out << ",\n";
            out << "      \"error\": \"" << JsonEscape(result.error) << "\"\n";
        } else {
            out << "\n";
        }
        out << "    }" << (i + 1 == results.size() ? "\n" : ",\n");
    }
    out << "  ]\n";
    out << "}\n";
    if (!out) throw std::runtime_error("failed to write output report: " + path);
}

}  // namespace

int main(int argc, char **argv) {
    try {
        const Options options = ParseArgs(argc, argv);
        const std::string manifest_path = JoinPath(options.golden_dir, "manifest.json");
        const std::vector<Record> records = ReadRecords(manifest_path);

        std::vector<RecordResult> results;
        results.reserve(records.size());
        int failed_count = 0;
        for (const Record &record : records) {
            RecordResult result = CompareRecord(options, record);
            if (result.status != "pass") {
                failed_count++;
                std::cerr << result.record.id << ": " << result.error << "\n";
            }
            results.push_back(result);
        }

        WriteReport(options.output_report, options, results);
        std::cout << "compared " << results.size() << " records, failures=" << failed_count << "\n";
        return failed_count == 0 ? 0 : 2;
    } catch (const std::exception &e) {
        std::cerr << "dft_compare_golden: " << e.what() << "\n";
        return 1;
    }
}
