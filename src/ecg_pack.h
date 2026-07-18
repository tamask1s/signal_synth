#pragma once

#include <string>
#include <vector>

namespace signal_synth
{
    enum ecg_pack_json_message_code
    {
        ecg_pack_json_syntax,
        ecg_pack_json_type,
        ecg_pack_json_missing_field,
        ecg_pack_json_unknown_field,
        ecg_pack_json_range,
        ecg_pack_json_duplicate_id
    };

    struct ecg_pack_json_message
    {
        ecg_pack_json_message_code code;
        std::string path;
        std::string message;
    };

    struct ecg_pack_scenario
    {
        std::string id;
        std::string path;
        std::vector<std::string> targets;
    };

    struct ecg_pack_manifest
    {
        ecg_pack_manifest();

        unsigned int schema_version;
        std::string pack_id;
        std::string name;
        std::string version;
        std::string description;
        std::string verification_protocol_path;
        std::vector<std::string> targets;
        std::vector<ecg_pack_scenario> scenarios;
    };

    struct ecg_pack_json_result
    {
        ecg_pack_json_result();

        bool success;
        std::vector<ecg_pack_json_message> messages;
        std::string canonical_json;
        std::string pack_fingerprint;
    };

    const char* ecg_pack_json_message_code_name(ecg_pack_json_message_code code);
    bool parse_ecg_pack_json(const std::string& json, ecg_pack_manifest& output, ecg_pack_json_result& result);
    bool write_ecg_pack_json(const ecg_pack_manifest& manifest, ecg_pack_json_result& result);
}
