#pragma once

#include "ecg_scenario.h"
#include "ppg_model.h"
#include "signal_quality.h"

#include <string>
#include <vector>

namespace signal_synth
{
    enum ecg_scenario_json_message_code
    {
        ecg_json_syntax = 0,
        ecg_json_duplicate_key = 1,
        ecg_json_unknown_field = 2,
        ecg_json_missing_field = 3,
        ecg_json_type = 4,
        ecg_json_range = 5,
        ecg_json_schema_version = 6,
        ecg_json_duplicate_condition = 7,
        ecg_json_duplicate_tag = 8,
        ecg_json_semantic = 9,
        ecg_json_internal = 10
    };

    struct ecg_scenario_json_message
    {
        ecg_scenario_json_message_code code;
        std::string path;
        std::string message;
    };

    const char* ecg_scenario_json_message_code_name(ecg_scenario_json_message_code code);

    struct ecg_scenario_document
    {
        ecg_scenario_document();

        unsigned int schema_version;
        std::string scenario_id;
        std::string name;
        std::string description;
        std::string author;
        std::vector<std::string> tags;
        double duration_seconds;
        ecg_qa_scenario ecg;
        ppg_config ppg;
        signal_quality_config signal_quality;

        unsigned int sample_count() const;
    };

    struct ecg_scenario_json_result
    {
        ecg_scenario_json_result();

        bool success;
        std::string canonical_json;
        std::string document_fingerprint;
        unsigned long long generation_fingerprint;
        std::vector<ecg_scenario_json_message> messages;
    };

    bool parse_ecg_scenario_json(const std::string& json, ecg_scenario_document& output, ecg_scenario_json_result& result);
    bool write_ecg_scenario_json(const ecg_scenario_document& document, ecg_scenario_json_result& result);
}
