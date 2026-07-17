#pragma once

#include <string>
#include <vector>

namespace signal_synth
{
    enum challenge_package_json_message_code
    {
        challenge_package_json_syntax = 0,
        challenge_package_json_type = 1,
        challenge_package_json_missing_field = 2,
        challenge_package_json_unknown_field = 3,
        challenge_package_json_range = 4,
        challenge_package_json_duplicate_id = 5
    };

    enum challenge_package_type
    {
        challenge_package_single_scenario = 0,
        challenge_package_scenario_pack = 1
    };

    enum challenge_file_role
    {
        challenge_file_scenario_json = 0,
        challenge_file_pack_json = 1,
        challenge_file_metadata_json = 2,
        challenge_file_waveform_csv = 3,
        challenge_file_annotations_json = 4,
        challenge_file_ground_truth_metrics_json = 5,
        challenge_file_report_html = 6,
        challenge_file_readme = 7,
        challenge_file_wfdb_header = 8,
        challenge_file_wfdb_signal = 9,
        challenge_file_wfdb_annotation = 10,
        challenge_file_edf = 11,
        challenge_file_bdf = 12,
        challenge_file_measurement_truth_json = 13,
        challenge_file_other = 14
    };

    struct challenge_package_json_message
    {
        challenge_package_json_message_code code;
        std::string path;
        std::string message;
    };

    struct challenge_package_file
    {
        challenge_package_file();

        std::string path;
        challenge_file_role role;
        std::string media_type;
        std::string sha256;
        unsigned long long size_bytes;
        bool required;
    };

    struct challenge_package_case
    {
        std::string id;
        std::string scenario_id;
        std::string scenario_path;
        std::string document_fingerprint;
        std::string render_identity;
        std::vector<std::string> files;
    };

    struct challenge_package_manifest
    {
        challenge_package_manifest();

        unsigned int schema_version;
        std::string package_id;
        std::string name;
        std::string version;
        std::string description;
        challenge_package_type package_type;
        bool ground_truth_included;
        std::vector<std::string> waveform_formats;
        std::string generator_version;
        std::string usage_restrictions;
        std::string not_for;
        std::vector<challenge_package_file> files;
        std::vector<challenge_package_case> cases;
    };

    struct challenge_package_json_result
    {
        challenge_package_json_result();

        bool success;
        std::vector<challenge_package_json_message> messages;
        std::string canonical_json;
        std::string package_fingerprint;
    };

    const char* challenge_package_json_message_code_name(challenge_package_json_message_code code);
    const char* challenge_package_type_name(challenge_package_type type);
    const char* challenge_file_role_name(challenge_file_role role);
    bool challenge_package_type_from_name(const std::string& name, challenge_package_type& output);
    bool challenge_file_role_from_name(const std::string& name, challenge_file_role& output);
    std::string challenge_package_content_sha256(const std::string& content);
    bool parse_challenge_package_json(const std::string& json, challenge_package_manifest& output, challenge_package_json_result& result);
    bool write_challenge_package_json(const challenge_package_manifest& manifest, challenge_package_json_result& result);
}
