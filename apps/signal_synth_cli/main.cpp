#include "ecg_scenario_json.h"
#include "ecg_export.h"
#include "ecg_pack.h"
#include "ecg_compare.h"
#include "detection_io.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <locale>
#include <new>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace
{
    const std::size_t maximum_input_size = 16u * 1024u * 1024u;

    bool read_stream(std::istream& input, std::string& output)
    {
        output.clear();
        char buffer[16384];
        while (input)
        {
            input.read(buffer, sizeof(buffer));
            const std::streamsize count = input.gcount();
            if (count > 0)
            {
                if (output.size() > maximum_input_size - static_cast<std::size_t>(count))
                    return false;
                output.append(buffer, static_cast<std::size_t>(count));
            }
        }
        return input.eof();
    }

    bool read_input(const std::string& path, std::string& output)
    {
        if (path == "-")
            return read_stream(std::cin, output);
        std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
        return file && read_stream(file, output);
    }

    void print_usage()
    {
        std::cerr << "usage: signal-synth <validate|fingerprint> <scenario.json|->\n"
                  << "       signal-synth render <scenario.json|-> --out <new-directory>\n"
                  << "       signal-synth compare <rpeaks|ppg-peaks> <scenario.json|-> <detections.csv|detections.json> --out <new-directory> [--tolerance-ms <ms>]\n"
                  << "       signal-synth pack validate <pack.json>\n"
                  << "       signal-synth pack render <pack.json> --out <new-directory>\n";
    }

    void print_errors(const signal_synth::ecg_scenario_json_result& result)
    {
        for (std::size_t i = 0; i < result.messages.size(); ++i)
        {
            const signal_synth::ecg_scenario_json_message& message = result.messages[i];
            std::cerr << "error=" << signal_synth::ecg_scenario_json_message_code_name(message.code)
                      << " path=" << message.path << " message=" << message.message << '\n';
        }
    }

    bool create_directory(const std::string& path)
    {
#ifdef _WIN32
        return _mkdir(path.c_str()) == 0;
#else
        return mkdir(path.c_str(), 0777) == 0;
#endif
    }

    bool remove_directory(const std::string& path)
    {
#ifdef _WIN32
        return _rmdir(path.c_str()) == 0;
#else
        return rmdir(path.c_str()) == 0;
#endif
    }

    bool write_export_directory(const std::string& path, const signal_synth::ecg_export_bundle& bundle)
    {
        if (!create_directory(path))
            return false;
        std::vector<std::string> temporary;
        std::vector<std::string> completed;
        bool success = true;
        for (std::size_t i = 0; i < bundle.artifacts.size() && success; ++i)
        {
            const std::string final_name = path + "/" + bundle.artifacts[i].name;
            const std::string temporary_name = final_name + ".tmp";
            temporary.push_back(temporary_name);
            std::ofstream file(temporary_name.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
            file.write(bundle.artifacts[i].content.data(), static_cast<std::streamsize>(bundle.artifacts[i].content.size()));
            success = file.good();
            file.close();
            if (success)
            {
                success = std::rename(temporary_name.c_str(), final_name.c_str()) == 0;
                if (success)
                    completed.push_back(final_name);
            }
        }
        if (!success)
        {
            for (std::size_t i = 0; i < temporary.size(); ++i)
                std::remove(temporary[i].c_str());
            for (std::size_t i = 0; i < completed.size(); ++i)
                std::remove(completed[i].c_str());
            remove_directory(path);
        }
        return success;
    }

    std::string directory_name(const std::string& path)
    {
        const std::string::size_type slash = path.find_last_of("/\\");
        if (slash == std::string::npos)
            return ".";
        if (slash == 0)
            return path.substr(0, 1);
        return path.substr(0, slash);
    }

    std::string join_path(const std::string& directory, const std::string& name)
    {
        if (directory.empty() || directory == ".")
            return name;
        const char last = directory[directory.size() - 1];
        if (last == '/' || last == '\\')
            return directory + name;
        return directory + "/" + name;
    }

    std::string json_text(const std::string& value)
    {
        std::ostringstream output;
        output << '"';
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            const unsigned char ch = static_cast<unsigned char>(value[i]);
            switch (ch)
            {
            case '"': output << "\\\""; break;
            case '\\': output << "\\\\"; break;
            case '\b': output << "\\b"; break;
            case '\f': output << "\\f"; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default:
                if (ch < 0x20)
                    output << "\\u00" << "0123456789abcdef"[ch >> 4] << "0123456789abcdef"[ch & 15u];
                else
                    output << static_cast<char>(ch);
                break;
            }
        }
        output << '"';
        return output.str();
    }

    std::string html_text(const std::string& value)
    {
        std::string output;
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            switch (value[i])
            {
            case '&': output += "&amp;"; break;
            case '<': output += "&lt;"; break;
            case '>': output += "&gt;"; break;
            case '"': output += "&quot;"; break;
            default: output += value[i]; break;
            }
        }
        return output;
    }

    bool write_text_file(const std::string& path, const std::string& content)
    {
        const std::string temporary = path + ".tmp";
        std::ofstream file(temporary.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
        file.write(content.data(), static_cast<std::streamsize>(content.size()));
        const bool ok = file.good();
        file.close();
        if (!ok)
        {
            std::remove(temporary.c_str());
            return false;
        }
        if (std::rename(temporary.c_str(), path.c_str()) != 0)
        {
            std::remove(temporary.c_str());
            return false;
        }
        return true;
    }

    struct pack_render_row
    {
        std::string id;
        std::string path;
        std::string scenario_id;
        std::string document_fingerprint;
        std::string render_identity;
        unsigned int sample_count;
        unsigned int beat_count;
        unsigned int artifact_count;
        unsigned int ppg_pulse_count;
        double mean_heart_rate_bpm;
        double total_artifact_seconds;
    };

    std::string csv_cell(const std::string& value)
    {
        bool quote = false;
        for (std::size_t i = 0; i < value.size(); ++i)
            if (value[i] == ',' || value[i] == '"' || value[i] == '\n' || value[i] == '\r')
                quote = true;
        if (!quote)
            return value;
        std::string output = "\"";
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            if (value[i] == '"')
                output += "\"\"";
            else
                output += value[i];
        }
        output += '"';
        return output;
    }

    std::string pack_summary_json(const signal_synth::ecg_pack_manifest& manifest, const signal_synth::ecg_pack_json_result& identity, const std::vector<pack_render_row>& rows)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << "{\"schema_version\":1,\"pack_id\":" << json_text(manifest.pack_id)
               << ",\"name\":" << json_text(manifest.name)
               << ",\"version\":" << json_text(manifest.version)
               << ",\"pack_fingerprint\":" << json_text(identity.pack_fingerprint)
               << ",\"scenario_count\":" << rows.size()
               << ",\"scenarios\":[";
        for (std::size_t i = 0; i < rows.size(); ++i)
        {
            const pack_render_row& row = rows[i];
            output << (i ? "," : "") << "{\"id\":" << json_text(row.id)
                   << ",\"path\":" << json_text(row.path)
                   << ",\"scenario_id\":" << json_text(row.scenario_id)
                   << ",\"document_fingerprint\":" << json_text(row.document_fingerprint)
                   << ",\"render_identity\":" << json_text(row.render_identity)
                   << ",\"sample_count\":" << row.sample_count
                   << ",\"beat_count\":" << row.beat_count
                   << ",\"mean_heart_rate_bpm\":" << row.mean_heart_rate_bpm
                   << ",\"artifact_count\":" << row.artifact_count
                   << ",\"total_artifact_seconds\":" << row.total_artifact_seconds
                   << ",\"ppg_pulse_count\":" << row.ppg_pulse_count << "}";
        }
        output << "]}";
        return output.str();
    }

    std::string pack_summary_csv(const std::vector<pack_render_row>& rows)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << "id,path,scenario_id,document_fingerprint,render_identity,sample_count,beat_count,mean_heart_rate_bpm,artifact_count,total_artifact_seconds,ppg_pulse_count\n";
        for (std::size_t i = 0; i < rows.size(); ++i)
        {
            const pack_render_row& row = rows[i];
            output << csv_cell(row.id) << ',' << csv_cell(row.path) << ',' << csv_cell(row.scenario_id) << ','
                   << csv_cell(row.document_fingerprint) << ',' << csv_cell(row.render_identity) << ','
                   << row.sample_count << ',' << row.beat_count << ',' << row.mean_heart_rate_bpm << ','
                   << row.artifact_count << ',' << row.total_artifact_seconds << ',' << row.ppg_pulse_count << '\n';
        }
        return output.str();
    }

    std::string pack_index_html(const signal_synth::ecg_pack_manifest& manifest, const signal_synth::ecg_pack_json_result& identity, const std::vector<pack_render_row>& rows)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << "<!doctype html><html><head><meta charset=\"utf-8\"><title>" << html_text(manifest.name)
               << "</title><style>body{font-family:Arial,sans-serif;margin:24px;line-height:1.45}table{border-collapse:collapse;width:100%}th,td{border:1px solid #d1d5db;padding:6px 8px;text-align:left}th{background:#f3f4f6}code{font-family:monospace}</style></head><body>"
               << "<h1>" << html_text(manifest.name) << "</h1><p>" << html_text(manifest.description)
               << "</p><table><tr><th>Pack ID</th><td><code>" << html_text(manifest.pack_id)
               << "</code></td></tr><tr><th>Version</th><td>" << html_text(manifest.version)
               << "</td></tr><tr><th>Fingerprint</th><td><code>" << html_text(identity.pack_fingerprint)
               << "</code></td></tr><tr><th>Scenario count</th><td>" << rows.size()
               << "</td></tr></table><h2>Scenarios</h2><table><tr><th>ID</th><th>Scenario</th><th>Samples</th><th>Beats</th><th>Mean HR</th><th>Artifacts</th><th>PPG pulses</th><th>Report</th></tr>";
        for (std::size_t i = 0; i < rows.size(); ++i)
        {
            const pack_render_row& row = rows[i];
            output << "<tr><td>" << html_text(row.id) << "</td><td>" << html_text(row.scenario_id)
                   << "</td><td>" << row.sample_count << "</td><td>" << row.beat_count
                   << "</td><td>" << row.mean_heart_rate_bpm << "</td><td>" << row.artifact_count
                   << "</td><td>" << row.ppg_pulse_count << "</td><td><a href=\"" << html_text(row.id)
                   << "/report.html\">report</a></td></tr>";
        }
        output << "</table><h2>Limitations</h2><p>This is an engineering QA pack, not a clinical validation dataset or diagnostic product.</p></body></html>";
        return output.str();
    }

    void print_pack_errors(const signal_synth::ecg_pack_json_result& result)
    {
        for (std::size_t i = 0; i < result.messages.size(); ++i)
        {
            const signal_synth::ecg_pack_json_message& message = result.messages[i];
            std::cerr << "error=" << signal_synth::ecg_pack_json_message_code_name(message.code)
                      << " path=" << message.path << " message=" << message.message << '\n';
        }
    }

    bool parse_double_cell(const std::string& text, double& value)
    {
        errno = 0;
        char* end = 0;
        const double parsed = std::strtod(text.c_str(), &end);
        if (end == text.c_str() || errno == ERANGE)
            return false;
        while (end && (*end == ' ' || *end == '\t' || *end == '\r'))
            ++end;
        if (end && *end != '\0')
            return false;
        value = parsed;
        return true;
    }

    bool starts_with_json_object(const std::string& text)
    {
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            if (text[i] == ' ' || text[i] == '\t' || text[i] == '\r' || text[i] == '\n')
                continue;
            return text[i] == '{';
        }
        return false;
    }

    bool parse_compare_target(const std::string& value, signal_synth::ecg_compare_target& target)
    {
        if (value == "rpeaks" || value == "r-peak" || value == "r_peak")
        {
            target = signal_synth::ecg_compare_r_peak;
            return true;
        }
        if (value == "ppg-peaks" || value == "ppg_peak" || value == "ppg-systolic-peak")
        {
            target = signal_synth::ecg_compare_ppg_systolic_peak;
            return true;
        }
        return false;
    }
}

int main(int argc, char** argv)
{
    std::cout.imbue(std::locale::classic());
    std::cerr.imbue(std::locale::classic());
    if (argc < 2)
    {
        print_usage();
        return 2;
    }
    const std::string command(argv[1]);
    if (command == "compare")
    {
        if (!((argc == 7 || argc == 9) && std::string(argv[5]) == "--out" && (argc == 7 || std::string(argv[7]) == "--tolerance-ms")))
        {
            print_usage();
            return 2;
        }
        try
        {
            signal_synth::ecg_compare_target target;
            if (!parse_compare_target(argv[2], target))
            {
                std::cerr << "error=COMPARE_TARGET_FAILED path=$ message=target must be rpeaks or ppg-peaks\n";
                return 2;
            }
            if (std::string(argv[3]) == "-" && std::string(argv[4]) == "-")
            {
                std::cerr << "error=INPUT_READ_FAILED path=- message=scenario and detections cannot both be read from stdin\n";
                return 3;
            }
            std::string scenario_json;
            if (!read_input(argv[3], scenario_json))
            {
                std::cerr << "error=INPUT_READ_FAILED path=" << argv[3] << " message=unable to read scenario input or input exceeds 16 MiB\n";
                return 3;
            }
            std::string detection_input;
            if (!read_input(argv[4], detection_input))
            {
                std::cerr << "error=INPUT_READ_FAILED path=" << argv[4] << " message=unable to read detection input or input exceeds 16 MiB\n";
                return 3;
            }
            signal_synth::ecg_scenario_document document;
            signal_synth::ecg_scenario_json_result scenario_result;
            if (!signal_synth::parse_ecg_scenario_json(scenario_json, document, scenario_result))
            {
                print_errors(scenario_result);
                return 4;
            }
            signal_synth::detection_io_document detection_document;
            signal_synth::detection_io_result detection_result;
            const bool parsed_detection = starts_with_json_object(detection_input)
                ? signal_synth::parse_detection_json_v1(detection_input, detection_document, detection_result)
                : signal_synth::parse_detection_csv_v2(detection_input, signal_synth::ecg_compare_target_name(target), detection_document, detection_result);
            if (!parsed_detection)
            {
                for (std::size_t i = 0; i < detection_result.messages.size(); ++i)
                {
                    const signal_synth::detection_io_message& message = detection_result.messages[i];
                    std::cerr << "error=" << signal_synth::detection_io_message_code_name(message.code)
                              << " path=" << message.path << " message=" << message.message << '\n';
                }
                if (detection_result.messages.empty())
                    std::cerr << "error=DETECTION_IO_FAILED path=$ message=detection import failed\n";
                return 4;
            }
            if (!detection_document.has_compare_target || detection_document.compare_target != target)
            {
                std::cerr << "error=" << signal_synth::detection_io_message_code_name(signal_synth::detection_io_target_mismatch)
                          << " path=$.target message=detection target does not match compare command target\n";
                return 4;
            }
            std::vector<signal_synth::ecg_detected_event> detections;
            if (!signal_synth::detection_events_for_compare(detection_document, detections, detection_result))
            {
                std::cerr << "error=DETECTION_IO_FAILED path=$ message=detection import failed\n";
                return 4;
            }
            signal_synth::ecg_render_bundle render;
            signal_synth::ecg_export_result export_result;
            if (!signal_synth::render_ecg_document(document, render, export_result))
            {
                std::cerr << "error=RENDER_FAILED path=$ message=" << (export_result.messages.empty() ? "render failed" : export_result.messages[0]) << '\n';
                return 4;
            }
            signal_synth::ecg_compare_options options;
            options.target = target;
            if (argc == 9)
            {
                double tolerance_ms = 0.0;
                if (!parse_double_cell(argv[8], tolerance_ms) || tolerance_ms <= 0.0)
                {
                    std::cerr << "error=COMPARE_OPTIONS_FAILED path=$ message=--tolerance-ms must be positive\n";
                    return 2;
                }
                options.tolerance_seconds = tolerance_ms / 1000.0;
            }
            signal_synth::ecg_compare_result compare_result;
            if (!signal_synth::compare_detections_to_render(render, detections, options, compare_result))
            {
                std::cerr << "error=COMPARE_FAILED path=$ message=" << (compare_result.messages.empty() ? "comparison failed" : compare_result.messages[0]) << '\n';
                return 4;
            }
            const std::string output_directory(argv[6]);
            if (!create_directory(output_directory))
            {
                std::cerr << "error=OUTPUT_WRITE_FAILED path=" << output_directory << " message=output directory must be new and writable\n";
                return 3;
            }
            if (!write_text_file(join_path(output_directory, "comparison.json"), signal_synth::ecg_compare_result_json(render, compare_result))
                || !write_text_file(join_path(output_directory, "comparison.csv"), signal_synth::ecg_compare_result_csv(compare_result))
                || !write_text_file(join_path(output_directory, "comparison_report.html"), signal_synth::ecg_compare_report_html(render, compare_result)))
            {
                std::cerr << "error=OUTPUT_WRITE_FAILED path=" << output_directory << " message=unable to write comparison output files\n";
                return 3;
            }
            std::cout << "status=compared\n"
                      << "output_directory=" << output_directory << '\n'
                      << "target=" << compare_result.target_name << '\n'
                      << "tolerance_seconds=" << compare_result.tolerance_seconds << '\n'
                      << "ground_truth_count=" << compare_result.total.ground_truth_count << '\n'
                      << "detection_count=" << compare_result.total.detection_count << '\n'
                      << "true_positive_count=" << compare_result.total.true_positive_count << '\n'
                      << "false_positive_count=" << compare_result.total.false_positive_count << '\n'
                      << "false_negative_count=" << compare_result.total.false_negative_count << '\n'
                      << "f1_score=" << compare_result.total.f1_score << '\n';
            return 0;
        }
        catch (const std::bad_alloc&)
        {
            std::cerr << "error=INTERNAL_ERROR path=$ message=memory allocation failed\n";
        }
        catch (...)
        {
            std::cerr << "error=INTERNAL_ERROR path=$ message=unexpected failure\n";
        }
        return 5;
    }
    const bool pack_command = command == "pack";
    if (pack_command)
    {
        if (argc < 4)
        {
            print_usage();
            return 2;
        }
        const std::string pack_action(argv[2]);
        const bool pack_render = pack_action == "render";
        if ((pack_render && (argc != 6 || std::string(argv[4]) != "--out")) || (!pack_render && (argc != 4 || pack_action != "validate")))
        {
            print_usage();
            return 2;
        }
        try
        {
            std::string json;
            if (!read_input(argv[3], json))
            {
                std::cerr << "error=INPUT_READ_FAILED path=" << argv[3] << " message=unable to read input or input exceeds 16 MiB\n";
                return 3;
            }
            signal_synth::ecg_pack_manifest manifest;
            signal_synth::ecg_pack_json_result pack_result;
            if (!signal_synth::parse_ecg_pack_json(json, manifest, pack_result))
            {
                print_pack_errors(pack_result);
                return 4;
            }
            if (!pack_render)
            {
                std::cout << "status=valid\n"
                          << "pack_id=" << manifest.pack_id << '\n'
                          << "version=" << manifest.version << '\n'
                          << "scenario_count=" << manifest.scenarios.size() << '\n'
                          << "pack_fingerprint=" << pack_result.pack_fingerprint << '\n';
                return 0;
            }
            const std::string output_directory(argv[5]);
            if (!create_directory(output_directory))
            {
                std::cerr << "error=OUTPUT_WRITE_FAILED path=" << output_directory << " message=output directory must be new and writable\n";
                return 3;
            }
            const std::string base_directory = std::string(argv[3]) == "-" ? "." : directory_name(argv[3]);
            std::vector<pack_render_row> rows;
            for (std::size_t i = 0; i < manifest.scenarios.size(); ++i)
            {
                const signal_synth::ecg_pack_scenario& pack_scenario = manifest.scenarios[i];
                const std::string scenario_path = join_path(base_directory, pack_scenario.path);
                std::string scenario_json;
                if (!read_input(scenario_path, scenario_json))
                {
                    std::cerr << "error=INPUT_READ_FAILED path=" << scenario_path << " message=unable to read pack scenario\n";
                    return 3;
                }
                signal_synth::ecg_scenario_document document;
                signal_synth::ecg_scenario_json_result scenario_result;
                if (!signal_synth::parse_ecg_scenario_json(scenario_json, document, scenario_result))
                {
                    print_errors(scenario_result);
                    return 4;
                }
                signal_synth::ecg_render_bundle render;
                signal_synth::ecg_export_bundle export_bundle;
                signal_synth::ecg_export_result export_result;
                if (!signal_synth::render_ecg_document(document, render, export_result) || !signal_synth::build_ecg_export_bundle(render, export_bundle, export_result))
                {
                    std::cerr << "error=RENDER_FAILED path=" << pack_scenario.path << " message=" << (export_result.messages.empty() ? "render or export failed" : export_result.messages[0]) << '\n';
                    return 4;
                }
                const std::string scenario_output = join_path(output_directory, pack_scenario.id);
                if (!write_export_directory(scenario_output, export_bundle))
                {
                    std::cerr << "error=OUTPUT_WRITE_FAILED path=" << scenario_output << " message=unable to write scenario export directory\n";
                    return 3;
                }
                pack_render_row row;
                row.id = pack_scenario.id;
                row.path = pack_scenario.path;
                row.scenario_id = document.scenario_id;
                row.document_fingerprint = scenario_result.document_fingerprint;
                row.render_identity = render.render_identity;
                row.sample_count = document.sample_count();
                row.beat_count = render.metrics.beat_count;
                row.artifact_count = render.metrics.artifact_count;
                row.ppg_pulse_count = render.metrics.ppg_pulse_count;
                row.mean_heart_rate_bpm = render.metrics.mean_heart_rate_bpm;
                row.total_artifact_seconds = render.metrics.total_artifact_seconds;
                rows.push_back(row);
            }
            if (!write_text_file(join_path(output_directory, "pack.json"), pack_result.canonical_json)
                || !write_text_file(join_path(output_directory, "summary.json"), pack_summary_json(manifest, pack_result, rows))
                || !write_text_file(join_path(output_directory, "summary.csv"), pack_summary_csv(rows))
                || !write_text_file(join_path(output_directory, "index.html"), pack_index_html(manifest, pack_result, rows)))
            {
                std::cerr << "error=OUTPUT_WRITE_FAILED path=" << output_directory << " message=unable to write pack summary files\n";
                return 3;
            }
            std::cout << "status=pack-rendered\n"
                      << "output_directory=" << output_directory << '\n'
                      << "pack_id=" << manifest.pack_id << '\n'
                      << "scenario_count=" << rows.size() << '\n'
                      << "pack_fingerprint=" << pack_result.pack_fingerprint << '\n';
            return 0;
        }
        catch (const std::bad_alloc&)
        {
            std::cerr << "error=INTERNAL_ERROR path=$ message=memory allocation failed\n";
        }
        catch (...)
        {
            std::cerr << "error=INTERNAL_ERROR path=$ message=unexpected failure\n";
        }
        return 5;
    }
    const bool render_command = command == "render";
    if ((render_command && (argc != 5 || std::string(argv[3]) != "--out")) || (!render_command && (argc != 3 || (command != "validate" && command != "fingerprint"))))
    {
        print_usage();
        return 2;
    }

    try
    {
        std::string json;
        if (!read_input(argv[2], json))
        {
            std::cerr << "error=INPUT_READ_FAILED path=" << argv[2] << " message=unable to read input or input exceeds 16 MiB\n";
            return 3;
        }
        signal_synth::ecg_scenario_document document;
        signal_synth::ecg_scenario_json_result result;
        if (!signal_synth::parse_ecg_scenario_json(json, document, result))
        {
            print_errors(result);
            return 4;
        }
        if (render_command)
        {
            signal_synth::ecg_render_bundle render;
            signal_synth::ecg_export_bundle export_bundle;
            signal_synth::ecg_export_result export_result;
            if (!signal_synth::render_ecg_document(document, render, export_result) || !signal_synth::build_ecg_export_bundle(render, export_bundle, export_result))
            {
                std::cerr << "error=RENDER_FAILED path=$ message=" << (export_result.messages.empty() ? "render or export failed" : export_result.messages[0]) << '\n';
                return 4;
            }
            if (!write_export_directory(argv[4], export_bundle))
            {
                std::cerr << "error=OUTPUT_WRITE_FAILED path=" << argv[4] << " message=output directory must be new and writable\n";
                return 3;
            }
            std::cout << "status=rendered\n"
                      << "output_directory=" << argv[4] << '\n'
                      << "artifact_count=" << export_bundle.artifacts.size() << '\n'
                      << "document_fingerprint=" << result.document_fingerprint << '\n'
                      << "generation_fingerprint=" << result.generation_fingerprint << '\n'
                      << "render_identity=" << render.render_identity << '\n'
                      << "ecg_run_fingerprint=" << render.scenario_report.run_fingerprint() << '\n';
            return 0;
        }
        if (command == "validate")
        {
            std::cout << "status=valid\n"
                      << "schema_version=" << document.schema_version << '\n'
                      << "scenario_id=" << document.scenario_id << '\n'
                      << "sample_count=" << document.sample_count() << '\n';
        }
        std::cout << "document_fingerprint=" << result.document_fingerprint << '\n'
                  << "generation_fingerprint=" << result.generation_fingerprint << '\n';
        return 0;
    }
    catch (const std::bad_alloc&)
    {
        std::cerr << "error=INTERNAL_ERROR path=$ message=memory allocation failed\n";
    }
    catch (...)
    {
        std::cerr << "error=INTERNAL_ERROR path=$ message=unexpected failure\n";
    }
    return 5;
}
