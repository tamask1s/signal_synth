#include "ecg_scenario_json.h"
#include "challenge_assembly.h"
#include "ecg_export.h"
#include "ecg_pack.h"
#include "ecg_pack_score.h"
#include "ecg_compare.h"
#include "ecg_beat_classification.h"
#include "detection_io.h"
#include "delineation_io.h"
#include "delineation_scoring.h"
#include "hrv_scoring.h"
#include "interval_io.h"
#include "interval_scoring.h"
#include "scenario_authoring.h"
#include "synsigra_api.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <new>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#include <sys/stat.h>
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

    bool file_exists(const std::string& path)
    {
        std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
        return file.good();
    }

    void print_usage()
    {
        std::cerr << "usage: signal-synth contract\n"
                  << "       signal-synth <validate|fingerprint> <scenario.json|->\n"
                  << "       signal-synth render <scenario.json|-> --out <new-directory>\n"
                  << "       signal-synth compare <r_peak|ppg_systolic_peak|ppg_pulse_onset|ecg_beat_classification> <scenario.json|-> <detections.csv|detections.json> --out <new-directory> [--tolerance-ms <ms>]\n"
                  << "       signal-synth interval score <rhythm_episode|signal_quality> <scenario.json|-> <intervals.csv|intervals.json> --out <new-directory> [--minimum-iou <ratio>]\n"
                  << "       signal-synth delineation score <scenario.json|-> <point-events.csv|point-events.json> --out <new-directory> [--tolerance-ms <ms>]\n"
                  << "       signal-synth hrv score <scenario.json|-> <hrv-output.json|-> --out <new-directory>\n"
                  << "       signal-synth pack validate <pack.json>\n"
                  << "       signal-synth pack analyze <pack.json>\n"
                  << "       signal-synth pack render <pack.json> --out <new-directory>\n"
                  << "       signal-synth pack challenge <pack.json> --out <new-directory>\n"
                  << "       signal-synth pack score <pack.json> <detections-directory> --out <new-directory>\n"
                  << "       signal-synth authoring <schema|templates>\n";
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

    bool directory_exists(const std::string& path)
    {
#ifdef _WIN32
        struct _stat info;
        return _stat(path.c_str(), &info) == 0 && (info.st_mode & _S_IFDIR);
#else
        struct stat info;
        return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
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

    bool create_directory_recorded(const std::string& path, std::vector<std::string>& created_directories)
    {
        if (path.empty() || path == ".")
            return true;
        if (directory_exists(path))
            return true;
        if (!create_directory(path))
            return false;
        created_directories.push_back(path);
        return true;
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

    bool create_relative_parent_directories(const std::string& root, const std::string& relative_path, std::vector<std::string>& created_directories)
    {
        std::string::size_type offset = 0;
        while (true)
        {
            const std::string::size_type slash = relative_path.find('/', offset);
            if (slash == std::string::npos)
                return true;
            const std::string partial = relative_path.substr(0, slash);
            if (!partial.empty() && !create_directory_recorded(join_path(root, partial), created_directories))
                return false;
            offset = slash + 1;
        }
    }

    bool write_challenge_file(const std::string& root, const signal_synth::challenge_package_input_file& file, std::vector<std::string>& completed_files, std::vector<std::string>& created_directories)
    {
        if (!create_relative_parent_directories(root, file.path, created_directories))
            return false;
        const std::string final_path = join_path(root, file.path);
        const std::string temporary_path = final_path + ".tmp";
        std::ofstream output(temporary_path.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
        output.write(file.content.data(), static_cast<std::streamsize>(file.content.size()));
        const bool ok = output.good();
        output.close();
        if (!ok)
        {
            std::remove(temporary_path.c_str());
            return false;
        }
        if (std::rename(temporary_path.c_str(), final_path.c_str()) != 0)
        {
            std::remove(temporary_path.c_str());
            return false;
        }
        completed_files.push_back(final_path);
        return true;
    }

    signal_synth::challenge_package_input_file make_challenge_file(const std::string& path, signal_synth::challenge_file_role role, const std::string& media_type, const std::string& content)
    {
        signal_synth::challenge_package_input_file file;
        file.path = path;
        file.role = role;
        file.media_type = media_type;
        file.content = content;
        file.required = true;
        return file;
    }

    void cleanup_challenge_write(const std::vector<std::string>& completed_files, const std::vector<std::string>& created_directories)
    {
        for (std::size_t i = completed_files.size(); i > 0; --i)
            std::remove(completed_files[i - 1].c_str());
        for (std::size_t i = created_directories.size(); i > 0; --i)
            remove_directory(created_directories[i - 1]);
    }

    bool write_challenge_directory(const std::string& path, const std::vector<signal_synth::challenge_package_input_file>& package_files, const std::vector<signal_synth::challenge_package_case_input>& cases, const std::string& manifest_json)
    {
        std::vector<std::string> completed_files;
        std::vector<std::string> created_directories;
        if (!create_directory(path))
            return false;
        created_directories.push_back(path);
        for (std::size_t i = 0; i < package_files.size(); ++i)
        {
            if (!write_challenge_file(path, package_files[i], completed_files, created_directories))
            {
                cleanup_challenge_write(completed_files, created_directories);
                return false;
            }
        }
        for (std::size_t i = 0; i < cases.size(); ++i)
        {
            for (std::size_t f = 0; f < cases[i].files.size(); ++f)
            {
                if (!write_challenge_file(path, cases[i].files[f], completed_files, created_directories))
                {
                    cleanup_challenge_write(completed_files, created_directories);
                    return false;
                }
            }
        }
        if (!write_text_file(join_path(path, "manifest.json"), manifest_json))
        {
            cleanup_challenge_write(completed_files, created_directories);
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
        std::vector<std::string> targets;
        unsigned int sample_rate_hz;
        unsigned int sample_count;
        double duration_seconds;
        unsigned int channel_count;
        unsigned int beat_count;
        unsigned int artifact_count;
        unsigned int ppg_pulse_count;
        double mean_heart_rate_bpm;
        double total_artifact_seconds;
    };

    void add_unique_string(std::vector<std::string>& values, const std::string& value)
    {
        if (std::find(values.begin(), values.end(), value) == values.end())
            values.push_back(value);
    }

    std::vector<std::string> effective_pack_targets(const signal_synth::ecg_pack_manifest& manifest, const signal_synth::ecg_pack_scenario& scenario)
    {
        if (!scenario.targets.empty())
            return scenario.targets;
        return manifest.targets;
    }

    void write_json_string_array(std::ostringstream& output, const std::vector<std::string>& values)
    {
        output << '[';
        for (std::size_t i = 0; i < values.size(); ++i)
            output << (i ? "," : "") << json_text(values[i]);
        output << ']';
    }

    enum scoring_input_kind
    {
        scoring_input_event = 0,
        scoring_input_hrv = 1,
        scoring_input_interval = 2,
        scoring_input_delineation = 3
    };

    bool scoring_command_for_target(const std::string& target, std::string& score_type, std::string& command_name, scoring_input_kind& input_kind);

    std::string submission_output_path(const std::string& case_id, const std::string& target)
    {
        return "outputs/" + case_id + "/" + target + ".json";
    }

    bool scoring_command_for_target(const std::string& target, std::string& score_type, std::string& command_name, scoring_input_kind& input_kind)
    {
        input_kind = scoring_input_event;
        if (target == "hrv")
        {
            score_type = "hrv_metrics";
            command_name = "hrv score";
            input_kind = scoring_input_hrv;
            return true;
        }
        if (target == "ecg_delineation")
        {
            score_type = "ecg_delineation";
            command_name = "delineation score";
            input_kind = scoring_input_delineation;
            return true;
        }
        signal_synth::interval_target interval_target;
        if (signal_synth::interval_target_from_name(target, interval_target))
        {
            score_type = "interval_detection";
            command_name = "interval score " + target;
            input_kind = scoring_input_interval;
            return true;
        }
        signal_synth::ecg_compare_target compare_target;
        if (!signal_synth::detection_compare_target_from_name(target, compare_target))
            return false;
        score_type = compare_target == signal_synth::ecg_compare_beat_classification ? "classification" : "event_detection";
        command_name = "compare " + target;
        return true;
    }

    const char* recommended_submission_format(scoring_input_kind input_kind)
    {
        if (input_kind == scoring_input_hrv)
            return "hrv_metrics_json_v1";
        if (input_kind == scoring_input_interval)
            return "interval_events_json_v1";
        return "point_events_json_v1";
    }

    void write_accepted_submission_formats(std::ostringstream& output, scoring_input_kind input_kind)
    {
        if (input_kind == scoring_input_hrv)
            output << "[\"hrv_metrics_json_v1\"]";
        else if (input_kind == scoring_input_interval)
            output << "[\"interval_events_json_v1\",\"interval_events_csv_v1\"]";
        else
            output << "[\"point_events_json_v1\",\"point_events_csv_v1\"]";
    }

    void write_scoring_entries(std::ostringstream& output, const std::string& case_id, const std::string& scenario_path, const std::vector<std::string>& targets)
    {
        (void)scenario_path;
        output << '[';
        for (std::size_t i = 0; i < targets.size(); ++i)
        {
            std::string score_type;
            std::string command_name;
            scoring_input_kind input_kind = scoring_input_event;
            const bool supported = scoring_command_for_target(targets[i], score_type, command_name, input_kind);
            (void)command_name;
            output << (i ? "," : "") << "{\"target\":" << json_text(targets[i])
                   << ",\"supported\":" << (supported ? "true" : "false");
            if (supported)
            {
                output << ",\"score_type\":" << json_text(score_type) << ",\"accepted_formats\":";
                write_accepted_submission_formats(output, input_kind);
                output << ",\"recommended_format\":" << json_text(recommended_submission_format(input_kind))
                       << ",\"recommended_path\":" << json_text(submission_output_path(case_id, targets[i]));
                if (input_kind == scoring_input_interval)
                    output << ",\"default_minimum_iou\":0.1";
                else if (input_kind == scoring_input_delineation)
                    output << ",\"default_tolerance_seconds\":0.04,\"default_pairing_window_seconds\":0.2,\"evaluation_scope\":{\"mode\":\"all_record\",\"leads\":[\"II\",\"V2\"]}";
                else if (input_kind == scoring_input_event)
                {
                    signal_synth::ecg_compare_target compare_target;
                    signal_synth::detection_compare_target_from_name(targets[i], compare_target);
                    output << ",\"default_tolerance_seconds\":" << signal_synth::ecg_compare_default_tolerance_seconds(compare_target);
                }
            }
            else
            {
                output << ",\"score_type\":\"generated_reference_only\",\"note\":\"No local scoring command is defined for this target yet.\"";
            }
            output << '}';
        }
        output << ']';
    }

    std::string submission_payload_template(scoring_input_kind input_kind)
    {
        if (input_kind == scoring_input_hrv)
            return "{\"schema_version\":1,\"metrics\":{},\"rr_intervals\":[]}\n";
        if (input_kind == scoring_input_interval)
            return "{\"schema_version\":1,\"intervals\":[]}\n";
        return "{\"schema_version\":1,\"events\":[]}\n";
    }

    std::string submission_template_json(const signal_synth::ecg_pack_manifest& manifest, const signal_synth::ecg_pack_json_result& identity, const std::vector<pack_render_row>& rows)
    {
        std::ostringstream output;
        output << "{\"schema_version\":1,\"contract\":\"synsigra_submission_v1\",\"challenge\":{\"package_id\":" << json_text(manifest.pack_id)
               << ",\"pack_version\":" << json_text(manifest.version) << ",\"pack_fingerprint\":" << json_text(identity.pack_fingerprint)
               << "},\"algorithm\":{\"name\":\"REPLACE_WITH_ALGORITHM_NAME\",\"version\":\"REPLACE_WITH_ALGORITHM_VERSION\"},\"outputs\":[";
        bool first = true;
        for (std::size_t row = 0; row < rows.size(); ++row)
        {
            for (std::size_t target = 0; target < rows[row].targets.size(); ++target)
            {
                std::string score_type;
                std::string command_name;
                scoring_input_kind input_kind = scoring_input_event;
                if (!scoring_command_for_target(rows[row].targets[target], score_type, command_name, input_kind))
                    continue;
                output << (first ? "" : ",") << "{\"case_id\":" << json_text(rows[row].id)
                       << ",\"target\":" << json_text(rows[row].targets[target])
                       << ",\"format\":" << json_text(recommended_submission_format(input_kind))
                       << ",\"path\":" << json_text(submission_output_path(rows[row].id, rows[row].targets[target])) << '}';
                first = false;
            }
        }
        output << "]}\n";
        return output.str();
    }

    std::string submission_formats_json()
    {
        return
            "{\"schema_version\":1,\"contract\":\"synsigra_submission_formats_v1\",\"formats\":["
            "{\"name\":\"point_events_json_v1\",\"media_type\":\"application/json\",\"container_fields\":[\"schema_version\",\"events\"],\"record_fields\":[\"time_seconds\",\"sample_index\",\"channel\",\"label\",\"confidence\"],\"required_record_fields\":[\"time_seconds\"]},"
            "{\"name\":\"point_events_csv_v1\",\"media_type\":\"text/csv\",\"columns\":[\"time_seconds\",\"sample_index\",\"channel\",\"label\",\"confidence\"],\"required_columns\":[\"time_seconds\"]},"
            "{\"name\":\"interval_events_json_v1\",\"media_type\":\"application/json\",\"container_fields\":[\"schema_version\",\"intervals\"],\"record_fields\":[\"start_seconds\",\"end_seconds\",\"label\",\"channel\",\"confidence\"],\"required_record_fields\":[\"start_seconds\",\"end_seconds\",\"label\"]},"
            "{\"name\":\"interval_events_csv_v1\",\"media_type\":\"text/csv\",\"columns\":[\"start_seconds\",\"end_seconds\",\"label\",\"channel\",\"confidence\"],\"required_columns\":[\"start_seconds\",\"end_seconds\",\"label\"]},"
            "{\"name\":\"hrv_metrics_json_v1\",\"media_type\":\"application/json\",\"container_fields\":[\"schema_version\",\"metrics\",\"rr_intervals\"],\"rr_record_fields\":[\"beat_time_seconds\",\"rr_seconds\"]}],"
            "\"target_adapters\":{"
            "\"r_peak\":{\"format_family\":\"point_events\",\"required_record_fields\":[\"time_seconds\"]},"
            "\"ppg_systolic_peak\":{\"format_family\":\"point_events\",\"required_record_fields\":[\"time_seconds\"]},"
            "\"ppg_pulse_onset\":{\"format_family\":\"point_events\",\"required_record_fields\":[\"time_seconds\"]},"
            "\"ecg_beat_classification\":{\"format_family\":\"point_events\",\"label\":\"beat_class\",\"required_record_fields\":[\"time_seconds\",\"label\"]},"
            "\"ecg_delineation\":{\"format_family\":\"point_events\",\"channel\":\"standard_ecg_lead\",\"label\":\"fiducial_kind\",\"required_record_fields\":[\"time_seconds\",\"channel\",\"label\"]},"
            "\"hrv\":{\"format_family\":\"hrv_metrics\"},"
            "\"rhythm_episode\":{\"format_family\":\"interval_events\",\"channel\":\"global\"},"
            "\"signal_quality\":{\"format_family\":\"interval_events\",\"channel\":\"global_or_physical_channel\"}}}\n";
    }

    void add_submission_template_files(signal_synth::challenge_package_build_options& options, const signal_synth::ecg_pack_manifest& manifest, const signal_synth::ecg_pack_json_result& identity, const std::vector<pack_render_row>& rows)
    {
        options.package_files.push_back(make_challenge_file("user-output-template/submission.json", signal_synth::challenge_file_other, "application/json", submission_template_json(manifest, identity, rows)));
        options.package_files.push_back(make_challenge_file("user-output-template/formats.json", signal_synth::challenge_file_other, "application/json", submission_formats_json()));
        for (std::size_t row = 0; row < rows.size(); ++row)
        {
            for (std::size_t target = 0; target < rows[row].targets.size(); ++target)
            {
                std::string score_type;
                std::string command_name;
                scoring_input_kind input_kind = scoring_input_event;
                if (!scoring_command_for_target(rows[row].targets[target], score_type, command_name, input_kind))
                    continue;
                options.package_files.push_back(make_challenge_file("user-output-template/" + submission_output_path(rows[row].id, rows[row].targets[target]), signal_synth::challenge_file_other, "application/json", submission_payload_template(input_kind)));
            }
        }
    }

    void write_case_channels(std::ostringstream& output, const signal_synth::ecg_render_bundle& render)
    {
        output << '[';
        bool first = true;
        for (unsigned int lead = 0; lead < render.record.lead_count(); ++lead)
        {
            output << (first ? "" : ",") << "{\"name\":" << json_text(render.record.lead_name(lead)) << ",\"unit\":\"mV\"}";
            first = false;
        }
        for (unsigned int channel = 0; channel < render.ppg.channel_count(); ++channel)
        {
            output << (first ? "" : ",") << "{\"name\":" << json_text(render.ppg.channel_name(channel)) << ",\"unit\":\"a.u.\",\"role\":\"optical_ppg\"}";
            first = false;
        }
        if (!render.signal_quality.accelerometer.empty())
            output << (first ? "" : ",") << "{\"name\":\"accel_motion\",\"unit\":\"g\",\"role\":\"motion_reference\"}";
        output << ']';
    }

    void write_artifact_channel_array(std::ostringstream& output, const signal_synth::ecg_render_bundle& render, const signal_synth::signal_quality_artifact_interval& artifact)
    {
        output << '[';
        bool first = true;
        for (unsigned int lead = 0; lead < render.record.lead_count(); ++lead)
        {
            if (artifact.ecg_leads[lead])
            {
                output << (first ? "" : ",") << json_text(render.record.lead_name(lead));
                first = false;
            }
        }
        if (artifact.ppg)
        {
            output << (first ? "" : ",") << "\"ppg_green\"";
            first = false;
        }
        if (artifact.accelerometer_reference)
            output << (first ? "" : ",") << "\"accel_motion\"";
        output << ']';
    }

    std::string case_summary_json(const signal_synth::ecg_pack_scenario& pack_scenario, const signal_synth::ecg_scenario_document& document, const signal_synth::ecg_render_bundle& render, const std::vector<std::string>& targets)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10)
               << "{\"schema_version\":1,\"case_id\":" << json_text(pack_scenario.id)
               << ",\"scenario_id\":" << json_text(document.scenario_id)
               << ",\"scenario_path\":" << json_text("cases/" + pack_scenario.id + "/scenario.json")
               << ",\"document_fingerprint\":" << json_text(render.document_identity.document_fingerprint)
               << ",\"render_identity\":" << json_text(render.render_identity)
               << ",\"targets\":";
        write_json_string_array(output, targets);
        output << ",\"render\":{\"sample_rate_hz\":" << render.record.sampling_rate_hz()
               << ",\"sample_count\":" << render.record.sample_count()
               << ",\"duration_seconds\":" << document.duration_seconds
               << ",\"channel_count\":" << render.record.lead_count() + render.ppg.channel_count() + (render.signal_quality.accelerometer.empty() ? 0u : 1u)
               << ",\"channels\":";
        write_case_channels(output, render);
        output << "},\"ground_truth\":{\"beat_count\":" << render.metrics.beat_count
               << ",\"atrial_event_count\":" << render.metrics.atrial_event_count
               << ",\"ppg_pulse_count\":" << render.metrics.ppg_pulse_count
               << ",\"artifact_count\":" << render.metrics.artifact_count
               << ",\"total_artifact_seconds\":" << render.metrics.total_artifact_seconds
               << ",\"mean_heart_rate_bpm\":" << render.metrics.mean_heart_rate_bpm
               << ",\"hrv_accepted_interval_count\":" << render.metrics.hrv_accepted_interval_count
               << ",\"hrv_excluded_interval_count\":" << render.metrics.hrv_excluded_interval_count << "}"
               << ",\"artifact_intervals\":[";
        for (std::size_t i = 0; i < render.signal_quality.artifacts.size(); ++i)
        {
            const signal_synth::signal_quality_artifact_interval& artifact = render.signal_quality.artifacts[i];
            output << (i ? "," : "") << "{\"type\":" << json_text(signal_synth::signal_quality_artifact_type_name(artifact.type))
                   << ",\"start_seconds\":" << artifact.start_seconds
                   << ",\"end_seconds\":" << artifact.end_seconds
                   << ",\"severity\":" << artifact.severity
                   << ",\"channels\":";
            write_artifact_channel_array(output, render, artifact);
            output << '}';
        }
        output << "],\"scoring\":";
        write_scoring_entries(output, pack_scenario.id, "cases/" + pack_scenario.id + "/scenario.json", targets);
        output << '}';
        return output.str();
    }

    std::string scoring_manifest_json(const signal_synth::ecg_pack_manifest& manifest, const signal_synth::ecg_pack_json_result& identity, const std::vector<pack_render_row>& rows)
    {
        std::vector<std::string> targets;
        for (std::size_t i = 0; i < rows.size(); ++i)
            for (std::size_t target = 0; target < rows[i].targets.size(); ++target)
                add_unique_string(targets, rows[i].targets[target]);
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10)
               << "{\"schema_version\":1,\"package_id\":" << json_text(manifest.pack_id)
               << ",\"pack_version\":" << json_text(manifest.version)
               << ",\"pack_fingerprint\":" << json_text(identity.pack_fingerprint)
               << ",\"generator_version\":" << json_text(signal_synth::signal_synth_generator_version())
               << ",\"generator_git_commit\":" << json_text(signal_synth::signal_synth_generator_git_commit())
               << ",\"package_contract_version\":" << json_text(signal_synth::signal_synth_package_contract_version())
               << ",\"scoring_manifest_contract_version\":" << json_text(signal_synth::signal_synth_scoring_manifest_contract_version())
               << ",\"submission_contract_version\":\"synsigra_submission_v1\""
               << ",\"submission_template_path\":\"user-output-template/submission.json\""
               << ",\"submission_format_contract_path\":\"user-output-template/formats.json\""
               << ",\"verification_output_directory\":\"verification\""
               << ",\"targets\":[";
        for (std::size_t i = 0; i < targets.size(); ++i)
        {
            std::string score_type;
            std::string command_name;
            scoring_input_kind input_kind = scoring_input_event;
            const bool supported = scoring_command_for_target(targets[i], score_type, command_name, input_kind);
            output << (i ? "," : "") << "{\"target\":" << json_text(targets[i])
                   << ",\"supported\":" << (supported ? "true" : "false");
            if (supported)
            {
                output << ",\"score_type\":" << json_text(score_type) << ",\"accepted_formats\":";
                write_accepted_submission_formats(output, input_kind);
                output << ",\"recommended_format\":" << json_text(recommended_submission_format(input_kind));
                if (input_kind == scoring_input_interval)
                    output << ",\"default_minimum_iou\":0.1";
                else if (input_kind == scoring_input_delineation)
                    output << ",\"default_tolerance_seconds\":0.04,\"default_pairing_window_seconds\":0.2,\"evaluation_scope\":{\"mode\":\"all_record\",\"leads\":[\"II\",\"V2\"]}";
                else if (input_kind == scoring_input_event)
                {
                    signal_synth::ecg_compare_target compare_target;
                    signal_synth::detection_compare_target_from_name(targets[i], compare_target);
                    output << ",\"default_tolerance_seconds\":" << signal_synth::ecg_compare_default_tolerance_seconds(compare_target);
                }
            }
            output << ",\"cases\":[";
            bool first_case = true;
            for (std::size_t row = 0; row < rows.size(); ++row)
            {
                if (std::find(rows[row].targets.begin(), rows[row].targets.end(), targets[i]) == rows[row].targets.end())
                    continue;
                output << (first_case ? "" : ",") << json_text(rows[row].id);
                first_case = false;
            }
            output << "]}";
        }
        output << "],\"cases\":[";
        for (std::size_t i = 0; i < rows.size(); ++i)
        {
            output << (i ? "," : "") << "{\"case_id\":" << json_text(rows[i].id)
                   << ",\"scenario_id\":" << json_text(rows[i].scenario_id)
                   << ",\"scenario_path\":" << json_text("cases/" + rows[i].id + "/scenario.json")
                   << ",\"case_summary_path\":" << json_text("cases/" + rows[i].id + "/case_summary.json")
                   << ",\"targets\":";
            write_json_string_array(output, rows[i].targets);
            output << ",\"scoring\":";
            write_scoring_entries(output, rows[i].id, "cases/" + rows[i].id + "/scenario.json", rows[i].targets);
            output << '}';
        }
        output << "],\"limitations\":{\"not_for\":\"diagnosis, patient monitoring, clinical validation certificate, or standalone conformity assessment\"}}";
        return output.str();
    }

    std::string package_provenance_json(const signal_synth::ecg_pack_manifest& manifest, const signal_synth::ecg_pack_json_result& identity, const std::vector<pack_render_row>& rows)
    {
        std::vector<std::string> targets;
        for (std::size_t i = 0; i < rows.size(); ++i)
            for (std::size_t target = 0; target < rows[i].targets.size(); ++target)
                add_unique_string(targets, rows[i].targets[target]);
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << "{\"schema_version\":1,\"metadata_type\":\"synsigra_package_provenance\""
               << ",\"package\":{\"package_id\":" << json_text(manifest.pack_id)
               << ",\"name\":" << json_text(manifest.name)
               << ",\"version\":" << json_text(manifest.version)
               << ",\"pack_fingerprint\":" << json_text(identity.pack_fingerprint)
               << ",\"case_count\":" << rows.size() << ",\"targets\":";
        write_json_string_array(output, targets);
        output << "}"
               << ",\"generator\":{\"name\":\"signal_synth\",\"product\":\"Synsigra Testbench\",\"version\":"
               << json_text(signal_synth::signal_synth_generator_version())
               << ",\"git_commit\":" << json_text(signal_synth::signal_synth_generator_git_commit())
               << ",\"build_identity\":" << json_text(signal_synth::signal_synth_build_identity()) << '}'
               << ",\"verifier\":{\"name\":\"synsigra\",\"version\":"
               << json_text(signal_synth::signal_synth_verifier_version())
               << ",\"package_contract_version\":" << json_text(signal_synth::signal_synth_package_contract_version())
               << ",\"scoring_manifest_contract_version\":" << json_text(signal_synth::signal_synth_scoring_manifest_contract_version()) << '}'
               << ",\"provenance_checklist\":["
               << "{\"item\":\"manifest_json\",\"artifact\":\"manifest.json\",\"required\":true},"
               << "{\"item\":\"pack_manifest\",\"artifact\":\"pack.json\",\"required\":true},"
               << "{\"item\":\"scoring_manifest\",\"artifact\":\"scoring_manifest.json\",\"required\":true},"
               << "{\"item\":\"package_provenance\",\"artifact\":\"provenance.json\",\"required\":true},"
               << "{\"item\":\"claim_boundary\",\"artifact\":\"ENGINEERING_CLAIM_BOUNDARY.txt\",\"required\":true},"
               << "{\"item\":\"per_case_provenance\",\"artifact\":\"cases/<case_id>/provenance.json\",\"required\":true},"
               << "{\"item\":\"per_case_ground_truth\",\"artifact\":\"cases/<case_id>/annotations.json and ground_truth_metrics.json\",\"required\":true}"
               << "]"
               << ",\"claim_boundary\":{\"intended_use\":\"deterministic synthetic biosignal engineering QA and algorithm verification\""
               << ",\"verifies\":\"Package files are internally consistent with the curated synthetic scenario pack, case ground truth and scoring contract.\""
               << ",\"does_not_verify\":\"Patient physiology, diagnostic performance on real populations, clinical safety, clinical effectiveness, or regulatory conformity.\""
               << ",\"not_for\":\"diagnosis, patient monitoring, clinical validation certificate, or standalone conformity assessment\"}"
               << ",\"determinism\":{\"byte_stable_export\":true,\"timestamp_policy\":\"not_recorded_for_deterministic_local_export\"}}";
        return output.str();
    }

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

    bool select_pack_score_target(const signal_synth::ecg_pack_manifest& manifest, const signal_synth::ecg_pack_scenario& scenario, signal_synth::ecg_compare_target& target, std::string& target_name)
    {
        for (std::size_t i = 0; i < scenario.targets.size(); ++i)
        {
            if (signal_synth::detection_compare_target_from_name(scenario.targets[i], target) && target != signal_synth::ecg_compare_beat_classification)
            {
                target_name = signal_synth::ecg_compare_target_name(target);
                return true;
            }
        }
        for (std::size_t i = 0; i < manifest.targets.size(); ++i)
        {
            if (signal_synth::detection_compare_target_from_name(manifest.targets[i], target) && target != signal_synth::ecg_compare_beat_classification)
            {
                target_name = signal_synth::ecg_compare_target_name(target);
                return true;
            }
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
    if (command == "contract")
    {
        if (argc != 2)
        {
            print_usage();
            return 2;
        }
        std::cout << signal_synth::synsigra_integration_contract_json() << '\n';
        return 0;
    }
    if (command == "authoring")
    {
        if (argc != 3)
        {
            print_usage();
            return 2;
        }
        const std::string action(argv[2]);
        if (action == "schema")
            std::cout << signal_synth::scenario_authoring_metadata_json() << '\n';
        else if (action == "templates")
            std::cout << signal_synth::scenario_template_catalog_json() << '\n';
        else
        {
            print_usage();
            return 2;
        }
        return 0;
    }
    if (command == "delineation")
    {
        if (!((argc == 7 || argc == 9) && std::string(argv[2]) == "score" && std::string(argv[5]) == "--out" && (argc == 7 || std::string(argv[7]) == "--tolerance-ms")))
        {
            print_usage();
            return 2;
        }
        try
        {
            if (std::string(argv[3]) == "-" && std::string(argv[4]) == "-")
            {
                std::cerr << "error=INPUT_READ_FAILED path=- message=scenario and delineation output cannot both be read from stdin\n";
                return 3;
            }
            std::string scenario_json;
            std::string delineation_input;
            if (!read_input(argv[3], scenario_json))
            {
                std::cerr << "error=INPUT_READ_FAILED path=" << argv[3] << " message=unable to read scenario input or input exceeds 16 MiB\n";
                return 3;
            }
            if (!read_input(argv[4], delineation_input))
            {
                std::cerr << "error=INPUT_READ_FAILED path=" << argv[4] << " message=unable to read delineation input or input exceeds 16 MiB\n";
                return 3;
            }
            signal_synth::ecg_scenario_document document;
            signal_synth::ecg_scenario_json_result scenario_result;
            if (!signal_synth::parse_ecg_scenario_json(scenario_json, document, scenario_result))
            {
                print_errors(scenario_result);
                return 4;
            }
            signal_synth::delineation_output_document delineation_document;
            signal_synth::delineation_io_result delineation_result;
            const bool parsed = starts_with_json_object(delineation_input)
                ? signal_synth::parse_delineation_point_events_json_v1(delineation_input, delineation_document, delineation_result)
                : signal_synth::parse_delineation_point_events_csv_v1(delineation_input, delineation_document, delineation_result);
            if (!parsed)
            {
                for (std::size_t i = 0; i < delineation_result.messages.size(); ++i)
                {
                    const signal_synth::delineation_io_message& message = delineation_result.messages[i];
                    std::cerr << "error=" << signal_synth::delineation_io_message_code_name(message.code) << " path=" << message.path << " message=" << message.message << '\n';
                }
                if (delineation_result.messages.empty())
                    std::cerr << "error=DELINEATION_IO_FAILED path=$ message=delineation import failed\n";
                return 4;
            }
            signal_synth::delineation_score_options options;
            if (argc == 9)
            {
                double tolerance_ms = 0.0;
                if (!parse_double_cell(argv[8], tolerance_ms) || tolerance_ms <= 0.0)
                {
                    std::cerr << "error=DELINEATION_OPTIONS_FAILED path=$ message=--tolerance-ms must be positive\n";
                    return 2;
                }
                options.tolerance_seconds = tolerance_ms / 1000.0;
            }
            signal_synth::ecg_render_bundle render;
            signal_synth::ecg_document_render_result render_result;
            if (!signal_synth::render_ecg_document(document, render, render_result))
            {
                std::cerr << "error=RENDER_FAILED path=$ message=" << (render_result.messages.empty() ? "render failed" : render_result.messages[0]) << '\n';
                return 4;
            }
            signal_synth::delineation_evaluation_scope scope;
            for (std::size_t i = 0; i < delineation_document.events.size(); ++i)
                if (std::find(scope.leads.begin(), scope.leads.end(), delineation_document.events[i].lead) == scope.leads.end()) scope.leads.push_back(delineation_document.events[i].lead);
            if (scope.leads.empty()) scope.leads.push_back("II");
            signal_synth::delineation_score_result score;
            if (!signal_synth::score_delineation_output_to_render(render, delineation_document, scope, options, score))
            {
                std::cerr << "error=DELINEATION_SCORE_FAILED path=$ message=" << (score.messages.empty() ? "delineation scoring failed" : score.messages[0]) << '\n';
                return 4;
            }
            const std::string output_directory(argv[6]);
            if (!create_directory(output_directory))
            {
                std::cerr << "error=OUTPUT_WRITE_FAILED path=" << output_directory << " message=output directory must be new and writable\n";
                return 3;
            }
            if (!write_text_file(join_path(output_directory, "delineation_score.json"), signal_synth::delineation_score_result_json(render, scope, score))
                || !write_text_file(join_path(output_directory, "delineation_score.csv"), signal_synth::delineation_score_result_csv(score))
                || !write_text_file(join_path(output_directory, "delineation_score_report.html"), signal_synth::delineation_score_report_html(render, score)))
            {
                std::cerr << "error=OUTPUT_WRITE_FAILED path=" << output_directory << " message=unable to write delineation scoring output files\n";
                return 3;
            }
            std::cout << "status=delineation-scored\n"
                      << "output_directory=" << output_directory << '\n'
                      << "tolerance_seconds=" << score.tolerance_seconds << '\n'
                      << "ground_truth_count=" << score.total.ground_truth_count << '\n'
                      << "prediction_count=" << score.total.prediction_count << '\n'
                      << "within_tolerance_count=" << score.total.within_tolerance_count << '\n'
                      << "f1_score=";
            if (score.total.ground_truth_count + score.total.prediction_count > 0u)
                std::cout << score.total.f1_score;
            else
                std::cout << "NA";
            std::cout << '\n';
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
    if (command == "interval")
    {
        if (!((argc == 8 || argc == 10) && std::string(argv[2]) == "score" && std::string(argv[6]) == "--out" && (argc == 8 || std::string(argv[8]) == "--minimum-iou")))
        {
            print_usage();
            return 2;
        }
        try
        {
            signal_synth::interval_target target;
            if (!signal_synth::interval_target_from_name(argv[3], target))
            {
                std::cerr << "error=INTERVAL_TARGET_FAILED path=$ message=target must be rhythm_episode or signal_quality\n";
                return 2;
            }
            if (std::string(argv[4]) == "-" && std::string(argv[5]) == "-")
            {
                std::cerr << "error=INPUT_READ_FAILED path=- message=scenario and intervals cannot both be read from stdin\n";
                return 3;
            }
            std::string scenario_json;
            std::string interval_input;
            if (!read_input(argv[4], scenario_json))
            {
                std::cerr << "error=INPUT_READ_FAILED path=" << argv[4] << " message=unable to read scenario input or input exceeds 16 MiB\n";
                return 3;
            }
            if (!read_input(argv[5], interval_input))
            {
                std::cerr << "error=INPUT_READ_FAILED path=" << argv[5] << " message=unable to read interval input or input exceeds 16 MiB\n";
                return 3;
            }
            signal_synth::ecg_scenario_document document;
            signal_synth::ecg_scenario_json_result scenario_result;
            if (!signal_synth::parse_ecg_scenario_json(scenario_json, document, scenario_result))
            {
                print_errors(scenario_result);
                return 4;
            }
            signal_synth::interval_output_document interval_document;
            signal_synth::interval_io_result interval_result;
            const bool parsed = starts_with_json_object(interval_input)
                ? signal_synth::parse_interval_json_v1(interval_input, interval_document, interval_result)
                : signal_synth::parse_interval_csv_v1(interval_input, signal_synth::interval_target_name(target), interval_document, interval_result);
            if (!parsed)
            {
                for (std::size_t i = 0; i < interval_result.messages.size(); ++i)
                {
                    const signal_synth::interval_io_message& message = interval_result.messages[i];
                    std::cerr << "error=" << signal_synth::interval_io_message_code_name(message.code) << " path=" << message.path << " message=" << message.message << '\n';
                }
                if (interval_result.messages.empty())
                    std::cerr << "error=INTERVAL_IO_FAILED path=$ message=interval import failed\n";
                return 4;
            }
            if (interval_document.target_name != signal_synth::interval_target_name(target))
            {
                std::cerr << "error=" << signal_synth::interval_io_message_code_name(signal_synth::interval_io_target_mismatch) << " path=$.target message=interval target does not match score command target\n";
                return 4;
            }
            signal_synth::interval_score_options options;
            if (argc == 10 && (!parse_double_cell(argv[9], options.minimum_iou) || options.minimum_iou <= 0.0 || options.minimum_iou > 1.0))
            {
                std::cerr << "error=INTERVAL_OPTIONS_FAILED path=$ message=--minimum-iou must be in the interval (0,1]\n";
                return 2;
            }
            signal_synth::ecg_render_bundle render;
            signal_synth::ecg_document_render_result render_result;
            if (!signal_synth::render_ecg_document(document, render, render_result))
            {
                std::cerr << "error=RENDER_FAILED path=$ message=" << (render_result.messages.empty() ? "render failed" : render_result.messages[0]) << '\n';
                return 4;
            }
            signal_synth::interval_score_result score;
            if (!signal_synth::score_interval_output_to_render(render, interval_document, options, score))
            {
                std::cerr << "error=INTERVAL_SCORE_FAILED path=$ message=" << (score.messages.empty() ? "interval scoring failed" : score.messages[0]) << '\n';
                return 4;
            }
            const std::string output_directory(argv[7]);
            if (!create_directory(output_directory))
            {
                std::cerr << "error=OUTPUT_WRITE_FAILED path=" << output_directory << " message=output directory must be new and writable\n";
                return 3;
            }
            if (!write_text_file(join_path(output_directory, "interval_score.json"), signal_synth::interval_score_result_json(render, interval_document, score))
                || !write_text_file(join_path(output_directory, "interval_score.csv"), signal_synth::interval_score_result_csv(score))
                || !write_text_file(join_path(output_directory, "interval_score_report.html"), signal_synth::interval_score_report_html(render, score)))
            {
                std::cerr << "error=OUTPUT_WRITE_FAILED path=" << output_directory << " message=unable to write interval scoring output files\n";
                return 3;
            }
            std::cout << "status=interval-scored\n"
                      << "output_directory=" << output_directory << '\n'
                      << "target=" << score.target_name << '\n'
                      << "channel_mode=" << signal_synth::interval_channel_mode_name(score.channel_mode) << '\n'
                      << "ground_truth_count=" << score.total.ground_truth_count << '\n'
                      << "prediction_count=" << score.total.prediction_count << '\n'
                      << "matched_count=" << score.total.matched_count << '\n'
                      << "time_f1_score=";
            if (score.total.ground_truth_duration_seconds + score.total.prediction_duration_seconds > 0.0)
                std::cout << score.total.time_f1_score;
            else
                std::cout << "NA";
            std::cout << '\n';
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
    if (command == "hrv")
    {
        if (argc != 7 || std::string(argv[2]) != "score" || std::string(argv[5]) != "--out")
        {
            print_usage();
            return 2;
        }
        try
        {
            if (std::string(argv[3]) == "-" && std::string(argv[4]) == "-")
            {
                std::cerr << "error=INPUT_READ_FAILED path=- message=scenario and HRV output cannot both be read from stdin\n";
                return 3;
            }
            std::string scenario_json;
            std::string user_json;
            if (!read_input(argv[3], scenario_json))
            {
                std::cerr << "error=INPUT_READ_FAILED path=" << argv[3] << " message=unable to read scenario input or input exceeds 16 MiB\n";
                return 3;
            }
            if (!read_input(argv[4], user_json))
            {
                std::cerr << "error=INPUT_READ_FAILED path=" << argv[4] << " message=unable to read HRV output or input exceeds 16 MiB\n";
                return 3;
            }
            signal_synth::ecg_scenario_document document;
            signal_synth::ecg_scenario_json_result scenario_result;
            if (!signal_synth::parse_ecg_scenario_json(scenario_json, document, scenario_result))
            {
                print_errors(scenario_result);
                return 4;
            }
            signal_synth::hrv_user_output user;
            std::vector<std::string> user_messages;
            if (!signal_synth::parse_hrv_user_output_json(user_json, user, user_messages))
            {
                for (std::size_t i = 0; i < user_messages.size(); ++i)
                    std::cerr << "error=HRV_INPUT_FAILED path=$ message=" << user_messages[i] << '\n';
                return 4;
            }
            signal_synth::ecg_render_bundle render;
            signal_synth::ecg_document_render_result render_result;
            if (!signal_synth::render_ecg_document(document, render, render_result))
            {
                std::cerr << "error=RENDER_FAILED path=$ message=" << (render_result.messages.empty() ? "render failed" : render_result.messages[0]) << '\n';
                return 4;
            }
            signal_synth::hrv_score_result score;
            if (!signal_synth::score_hrv_user_output(render, user, score))
            {
                std::cerr << "error=HRV_SCORE_FAILED path=$ message=" << (score.messages.empty() ? "HRV scoring failed" : score.messages[0]) << '\n';
                return 4;
            }
            const std::string output_directory(argv[6]);
            if (!create_directory(output_directory))
            {
                std::cerr << "error=OUTPUT_WRITE_FAILED path=" << output_directory << " message=output directory must be new and writable\n";
                return 3;
            }
            if (!write_text_file(join_path(output_directory, "hrv_score.json"), signal_synth::hrv_score_result_json(score))
                || !write_text_file(join_path(output_directory, "hrv_score.csv"), signal_synth::hrv_score_result_csv(score))
                || !write_text_file(join_path(output_directory, "hrv_score_report.html"), signal_synth::hrv_score_report_html(score)))
            {
                std::cerr << "error=OUTPUT_WRITE_FAILED path=" << output_directory << " message=unable to write HRV scoring output files\n";
                return 3;
            }
            std::cout << "status=hrv-scored\n"
                      << "output_directory=" << output_directory << '\n'
                      << "scenario_id=" << score.scenario_id << '\n'
                      << "metric_count=" << score.metrics.size() << '\n'
                      << "passed_metric_count=" << score.passed_metric_count << '\n'
                      << "metric_pass_fraction=" << score.metric_pass_fraction << '\n'
                      << "rr_matched_count=" << score.rr.matched_count << '\n';
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
            if (!signal_synth::detection_compare_target_from_name(argv[2], target))
            {
                std::cerr << "error=COMPARE_TARGET_FAILED path=$ message=target must be r_peak, ppg_systolic_peak, ppg_pulse_onset, or ecg_beat_classification\n";
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
            signal_synth::ecg_render_bundle render;
            signal_synth::ecg_document_render_result render_result;
            if (!signal_synth::render_ecg_document(document, render, render_result))
            {
                std::cerr << "error=RENDER_FAILED path=$ message=" << (render_result.messages.empty() ? "render failed" : render_result.messages[0]) << '\n';
                return 4;
            }
            double requested_tolerance_seconds = signal_synth::ecg_compare_default_tolerance_seconds(target);
            if (argc == 9)
            {
                double tolerance_ms = 0.0;
                if (!parse_double_cell(argv[8], tolerance_ms) || tolerance_ms <= 0.0)
                {
                    std::cerr << "error=COMPARE_OPTIONS_FAILED path=$ message=--tolerance-ms must be positive\n";
                    return 2;
                }
                requested_tolerance_seconds = tolerance_ms / 1000.0;
            }
            const std::string output_directory(argv[6]);
            if (target == signal_synth::ecg_compare_beat_classification)
            {
                signal_synth::ecg_beat_classification_options options;
                options.tolerance_seconds = requested_tolerance_seconds;
                signal_synth::ecg_beat_classification_result classification_result;
                if (!signal_synth::score_ecg_beat_classification(render, detection_document, options, classification_result))
                {
                    std::cerr << "error=COMPARE_FAILED path=$ message=" << (classification_result.messages.empty() ? "classification comparison failed" : classification_result.messages[0]) << '\n';
                    return 4;
                }
                if (!create_directory(output_directory))
                {
                    std::cerr << "error=OUTPUT_WRITE_FAILED path=" << output_directory << " message=output directory must be new and writable\n";
                    return 3;
                }
                if (!write_text_file(join_path(output_directory, "comparison.json"), signal_synth::ecg_beat_classification_result_json(render, classification_result))
                    || !write_text_file(join_path(output_directory, "comparison.csv"), signal_synth::ecg_beat_classification_result_csv(classification_result))
                    || !write_text_file(join_path(output_directory, "comparison_report.html"), signal_synth::ecg_beat_classification_report_html(render, classification_result)))
                {
                    std::cerr << "error=OUTPUT_WRITE_FAILED path=" << output_directory << " message=unable to write comparison output files\n";
                    return 3;
                }
                std::cout << "status=compared\n"
                          << "output_directory=" << output_directory << '\n'
                          << "target=ecg_beat_classification\n"
                          << "tolerance_seconds=" << classification_result.tolerance_seconds << '\n'
                          << "ground_truth_count=" << classification_result.scored_ground_truth_count << '\n'
                          << "prediction_count=" << classification_result.scored_prediction_count << '\n'
                          << "correct_count=" << classification_result.correct_count << '\n'
                          << "accuracy=" << classification_result.accuracy << '\n'
                          << "f1_score=" << classification_result.micro_f1_score << '\n';
                return 0;
            }
            std::vector<signal_synth::ecg_detected_event> detections;
            if (!signal_synth::detection_events_for_compare(detection_document, detections, detection_result))
            {
                std::cerr << "error=DETECTION_IO_FAILED path=$ message=detection import failed\n";
                return 4;
            }
            signal_synth::ecg_compare_options options;
            options.target = target;
            options.tolerance_seconds = requested_tolerance_seconds;
            signal_synth::ecg_compare_result compare_result;
            if (!signal_synth::compare_detections_to_render(render, detections, options, compare_result))
            {
                std::cerr << "error=COMPARE_FAILED path=$ message=" << (compare_result.messages.empty() ? "comparison failed" : compare_result.messages[0]) << '\n';
                return 4;
            }
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
        const bool pack_challenge = pack_action == "challenge";
        const bool pack_score = pack_action == "score";
        const bool pack_analyze = pack_action == "analyze";
        if (((pack_render || pack_challenge) && (argc != 6 || std::string(argv[4]) != "--out")) || (pack_score && (argc != 7 || std::string(argv[5]) != "--out")) || (!pack_render && !pack_challenge && !pack_score && !pack_analyze && (argc != 4 || pack_action != "validate")) || (pack_analyze && argc != 4))
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
            if (pack_analyze)
            {
                const std::string base_directory = std::string(argv[3]) == "-" ? "." : directory_name(argv[3]);
                std::vector<signal_synth::ecg_scenario_document> documents;
                for (std::size_t i = 0; i < manifest.scenarios.size(); ++i)
                {
                    const std::string scenario_path = join_path(base_directory, manifest.scenarios[i].path);
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
                    documents.push_back(document);
                }
                signal_synth::scenario_pack_analysis analysis;
                signal_synth::analyze_scenario_pack(manifest, documents, analysis);
                std::cout << signal_synth::scenario_pack_analysis_json(analysis) << '\n';
                return analysis.success ? 0 : 4;
            }
            if (!pack_render && !pack_challenge)
            {
                if (!pack_score)
                {
                    std::cout << "status=valid\n"
                              << "pack_id=" << manifest.pack_id << '\n'
                              << "version=" << manifest.version << '\n'
                              << "scenario_count=" << manifest.scenarios.size() << '\n'
                              << "pack_fingerprint=" << pack_result.pack_fingerprint << '\n';
                    return 0;
                }
                const std::string base_directory = std::string(argv[3]) == "-" ? "." : directory_name(argv[3]);
                const std::string detection_directory(argv[4]);
                std::vector<signal_synth::ecg_pack_score_case> cases;
                for (std::size_t i = 0; i < manifest.scenarios.size(); ++i)
                {
                    const signal_synth::ecg_pack_scenario& pack_scenario = manifest.scenarios[i];
                    signal_synth::ecg_compare_target target;
                    std::string target_name;
                    if (!select_pack_score_target(manifest, pack_scenario, target, target_name))
                    {
                        std::cerr << "error=COMPARE_TARGET_FAILED path=" << pack_scenario.id << " message=pack scenario has no supported scoring target\n";
                        return 4;
                    }
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
                    signal_synth::ecg_document_render_result render_result;
                    if (!signal_synth::render_ecg_document(document, render, render_result))
                    {
                        std::cerr << "error=RENDER_FAILED path=" << pack_scenario.path << " message=" << (render_result.messages.empty() ? "render failed" : render_result.messages[0]) << '\n';
                        return 4;
                    }
                    std::string detection_path = join_path(detection_directory, pack_scenario.id + ".json");
                    if (!file_exists(detection_path))
                        detection_path = join_path(detection_directory, pack_scenario.id + ".csv");
                    std::string detection_input;
                    if (!read_input(detection_path, detection_input))
                    {
                        std::cerr << "error=INPUT_READ_FAILED path=" << detection_path << " message=unable to read detection input for pack scenario\n";
                        return 3;
                    }
                    signal_synth::detection_io_document detection_document;
                    signal_synth::detection_io_result detection_result;
                    const bool parsed_detection = starts_with_json_object(detection_input)
                        ? signal_synth::parse_detection_json_v1(detection_input, detection_document, detection_result)
                        : signal_synth::parse_detection_csv_v2(detection_input, target_name, detection_document, detection_result);
                    if (!parsed_detection)
                    {
                        for (std::size_t message_index = 0; message_index < detection_result.messages.size(); ++message_index)
                        {
                            const signal_synth::detection_io_message& message = detection_result.messages[message_index];
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
                                  << " path=$.target message=detection target does not match pack scenario target\n";
                        return 4;
                    }
                    std::vector<signal_synth::ecg_detected_event> detections;
                    if (!signal_synth::detection_events_for_compare(detection_document, detections, detection_result))
                    {
                        std::cerr << "error=DETECTION_IO_FAILED path=$ message=detection import failed\n";
                        return 4;
                    }
                    signal_synth::ecg_compare_options options;
                    options.target = target;
                    signal_synth::ecg_compare_result compare_result;
                    if (!signal_synth::compare_detections_to_render(render, detections, options, compare_result))
                    {
                        std::cerr << "error=COMPARE_FAILED path=" << pack_scenario.id << " message=" << (compare_result.messages.empty() ? "comparison failed" : compare_result.messages[0]) << '\n';
                        return 4;
                    }
                    signal_synth::ecg_pack_score_case score_case;
                    score_case.case_id = pack_scenario.id;
                    score_case.scenario_id = document.scenario_id;
                    score_case.scenario_path = pack_scenario.path;
                    score_case.document_fingerprint = scenario_result.document_fingerprint;
                    score_case.render_identity = render.render_identity;
                    score_case.detection_input_id = detection_path;
                    score_case.detection_algorithm_name = detection_document.algorithm.name;
                    score_case.detection_algorithm_version = detection_document.algorithm.version;
                    score_case.comparison = compare_result;
                    cases.push_back(score_case);
                }
                signal_synth::ecg_pack_score_summary summary;
                if (!signal_synth::build_ecg_pack_score_summary(manifest, pack_result.pack_fingerprint, cases, summary))
                {
                    std::cerr << "error=PACK_SCORE_FAILED path=$ message=" << (summary.messages.empty() ? "pack score failed" : summary.messages[0]) << '\n';
                    return 4;
                }
                const std::string output_directory(argv[6]);
                if (!create_directory(output_directory))
                {
                    std::cerr << "error=OUTPUT_WRITE_FAILED path=" << output_directory << " message=output directory must be new and writable\n";
                    return 3;
                }
                if (!write_text_file(join_path(output_directory, "pack_score_summary.json"), signal_synth::ecg_pack_score_summary_json(summary))
                    || !write_text_file(join_path(output_directory, "pack_score_summary.csv"), signal_synth::ecg_pack_score_summary_csv(summary))
                    || !write_text_file(join_path(output_directory, "pack_score_report.html"), signal_synth::ecg_pack_score_report_html(summary)))
                {
                    std::cerr << "error=OUTPUT_WRITE_FAILED path=" << output_directory << " message=unable to write pack score output files\n";
                    return 3;
                }
                std::cout << "status=pack-scored\n"
                          << "output_directory=" << output_directory << '\n'
                          << "pack_id=" << manifest.pack_id << '\n'
                          << "scenario_count=" << cases.size() << '\n'
                          << "pack_fingerprint=" << pack_result.pack_fingerprint << '\n';
                return 0;
            }
            const std::string output_directory(argv[5]);
            if (pack_render && !create_directory(output_directory))
            {
                std::cerr << "error=OUTPUT_WRITE_FAILED path=" << output_directory << " message=output directory must be new and writable\n";
                return 3;
            }
            const std::string base_directory = std::string(argv[3]) == "-" ? "." : directory_name(argv[3]);
            std::vector<pack_render_row> rows;
            std::vector<signal_synth::challenge_package_case_input> challenge_cases;
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
                signal_synth::ecg_document_render_result render_result;
                signal_synth::ecg_export_result export_result;
                if (!signal_synth::render_ecg_document(document, render, render_result))
                {
                    std::cerr << "error=RENDER_FAILED path=" << pack_scenario.path << " message=" << (render_result.messages.empty() ? "render failed" : render_result.messages[0]) << '\n';
                    return 4;
                }
                if (!signal_synth::build_ecg_export_bundle(render, export_bundle, export_result))
                {
                    std::cerr << "error=EXPORT_FAILED path=" << pack_scenario.path << " message=" << (export_result.messages.empty() ? "export failed" : export_result.messages[0]) << '\n';
                    return 4;
                }
                if (pack_render)
                {
                    const std::string scenario_output = join_path(output_directory, pack_scenario.id);
                    if (!write_export_directory(scenario_output, export_bundle))
                    {
                        std::cerr << "error=OUTPUT_WRITE_FAILED path=" << scenario_output << " message=unable to write scenario export directory\n";
                        return 3;
                    }
                }
                else
                {
                    const std::vector<std::string> targets = effective_pack_targets(manifest, pack_scenario);
                    signal_synth::challenge_package_case_input challenge_case;
                    challenge_case.id = pack_scenario.id;
                    challenge_case.scenario_id = document.scenario_id;
                    challenge_case.scenario_path = "cases/" + pack_scenario.id + "/scenario.json";
                    challenge_case.document_fingerprint = scenario_result.document_fingerprint;
                    challenge_case.render_identity = render.render_identity;
                    for (std::size_t artifact_index = 0; artifact_index < export_bundle.artifacts.size(); ++artifact_index)
                    {
                        const signal_synth::ecg_text_artifact& artifact = export_bundle.artifacts[artifact_index];
                        challenge_case.files.push_back(make_challenge_file("cases/" + pack_scenario.id + "/" + artifact.name, signal_synth::challenge_file_role_for_export_artifact(artifact.name), artifact.media_type, artifact.content));
                    }
                    challenge_case.files.push_back(make_challenge_file("cases/" + pack_scenario.id + "/case_summary.json", signal_synth::challenge_file_metadata_json, "application/json", case_summary_json(pack_scenario, document, render, targets)));
                    challenge_cases.push_back(challenge_case);
                }
                pack_render_row row;
                row.id = pack_scenario.id;
                row.path = pack_scenario.path;
                row.scenario_id = document.scenario_id;
                row.document_fingerprint = scenario_result.document_fingerprint;
                row.render_identity = render.render_identity;
                row.targets = effective_pack_targets(manifest, pack_scenario);
                row.sample_rate_hz = render.record.sampling_rate_hz();
                row.sample_count = document.sample_count();
                row.duration_seconds = document.duration_seconds;
                row.channel_count = render.record.lead_count() + render.ppg.channel_count() + (render.signal_quality.accelerometer.empty() ? 0u : 1u);
                row.beat_count = render.metrics.beat_count;
                row.artifact_count = render.metrics.artifact_count;
                row.ppg_pulse_count = render.metrics.ppg_pulse_count;
                row.mean_heart_rate_bpm = render.metrics.mean_heart_rate_bpm;
                row.total_artifact_seconds = render.metrics.total_artifact_seconds;
                rows.push_back(row);
            }
            const std::string summary_json = pack_summary_json(manifest, pack_result, rows);
            const std::string summary_csv = pack_summary_csv(rows);
            const std::string index_html = pack_index_html(manifest, pack_result, rows);
            const std::string scoring_json = scoring_manifest_json(manifest, pack_result, rows);
            if (pack_challenge)
            {
                signal_synth::challenge_package_build_options options;
                options.package_id = manifest.pack_id;
                options.name = manifest.name;
                options.version = manifest.version;
                options.description = manifest.description;
                options.package_type = signal_synth::challenge_package_scenario_pack;
                options.generator_version = signal_synth::signal_synth_generator_version();
                options.package_files.push_back(make_challenge_file("pack.json", signal_synth::challenge_file_pack_json, "application/json", pack_result.canonical_json));
                options.package_files.push_back(make_challenge_file("provenance.json", signal_synth::challenge_file_metadata_json, "application/json", package_provenance_json(manifest, pack_result, rows)));
                options.package_files.push_back(make_challenge_file("ENGINEERING_CLAIM_BOUNDARY.txt", signal_synth::challenge_file_readme, "text/plain", signal_synth::signal_synth_engineering_claim_boundary_text()));
                options.package_files.push_back(make_challenge_file("summary.json", signal_synth::challenge_file_metadata_json, "application/json", summary_json));
                options.package_files.push_back(make_challenge_file("summary.csv", signal_synth::challenge_file_other, "text/csv", summary_csv));
                options.package_files.push_back(make_challenge_file("index.html", signal_synth::challenge_file_readme, "text/html", index_html));
                options.package_files.push_back(make_challenge_file("scoring_manifest.json", signal_synth::challenge_file_metadata_json, "application/json", scoring_json));
                add_submission_template_files(options, manifest, pack_result, rows);
                signal_synth::challenge_package_build_result challenge_result;
                if (!signal_synth::build_challenge_package_manifest(options, challenge_cases, challenge_result))
                {
                    for (std::size_t message_index = 0; message_index < challenge_result.manifest_json.messages.size(); ++message_index)
                    {
                        const signal_synth::challenge_package_json_message& message = challenge_result.manifest_json.messages[message_index];
                        std::cerr << "error=" << signal_synth::challenge_package_json_message_code_name(message.code)
                                  << " path=" << message.path << " message=" << message.message << '\n';
                    }
                    if (challenge_result.manifest_json.messages.empty())
                        std::cerr << "error=CHALLENGE_PACKAGE_FAILED path=$ message=challenge package manifest assembly failed\n";
                    return 4;
                }
                if (!write_challenge_directory(output_directory, options.package_files, challenge_cases, challenge_result.manifest_json.canonical_json))
                {
                    std::cerr << "error=OUTPUT_WRITE_FAILED path=" << output_directory << " message=unable to write challenge package directory\n";
                    return 3;
                }
                std::cout << "{\"schema_version\":1,\"contract\":" << json_text(signal_synth::synsigra_integration_contract_version())
                          << ",\"status\":\"challenge_rendered\",\"output_directory\":" << json_text(output_directory)
                          << ",\"package_id\":" << json_text(challenge_result.manifest.package_id)
                          << ",\"scenario_count\":" << challenge_result.manifest.cases.size()
                          << ",\"pack_fingerprint\":" << json_text(pack_result.pack_fingerprint)
                          << ",\"package_fingerprint\":" << json_text(challenge_result.manifest_json.package_fingerprint)
                          << ",\"generator\":{\"name\":\"signal_synth\",\"version\":" << json_text(signal_synth::signal_synth_generator_version())
                          << ",\"git_commit\":" << json_text(signal_synth::signal_synth_generator_git_commit())
                          << ",\"build_identity\":" << json_text(signal_synth::signal_synth_build_identity()) << "}"
                          << ",\"contracts\":{\"challenge_package\":" << json_text(signal_synth::signal_synth_package_contract_version())
                          << ",\"scoring_manifest\":" << json_text(signal_synth::signal_synth_scoring_manifest_contract_version()) << "}}\n";
                return 0;
            }
            if (!write_text_file(join_path(output_directory, "pack.json"), pack_result.canonical_json)
                || !write_text_file(join_path(output_directory, "summary.json"), summary_json)
                || !write_text_file(join_path(output_directory, "summary.csv"), summary_csv)
                || !write_text_file(join_path(output_directory, "index.html"), index_html))
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
            signal_synth::ecg_document_render_result render_result;
            signal_synth::ecg_export_result export_result;
            if (!signal_synth::render_ecg_document(document, render, render_result))
            {
                std::cerr << "error=RENDER_FAILED path=$ message=" << (render_result.messages.empty() ? "render failed" : render_result.messages[0]) << '\n';
                return 4;
            }
            if (!signal_synth::build_ecg_export_bundle(render, export_bundle, export_result))
            {
                std::cerr << "error=EXPORT_FAILED path=$ message=" << (export_result.messages.empty() ? "export failed" : export_result.messages[0]) << '\n';
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
