#pragma once

#include <string>
#include <vector>

namespace signal_synth
{
    enum interval_target
    {
        interval_target_rhythm_episode = 0,
        interval_target_signal_quality = 1
    };

    enum interval_io_message_code
    {
        interval_io_syntax = 0,
        interval_io_type = 1,
        interval_io_missing_field = 2,
        interval_io_unknown_field = 3,
        interval_io_range = 4,
        interval_io_duplicate = 5,
        interval_io_target_mismatch = 6
    };

    struct interval_io_message
    {
        interval_io_message_code code;
        std::string path;
        std::string message;
    };

    struct interval_algorithm_metadata
    {
        std::string name;
        std::string version;
    };

    struct interval_output_event
    {
        interval_output_event();

        double start_seconds;
        double end_seconds;
        std::string label;
        std::string channel;
        bool has_confidence;
        double confidence;
        unsigned int original_index;
    };

    struct interval_output_document
    {
        interval_output_document();

        unsigned int schema_version;
        std::string target_name;
        interval_algorithm_metadata algorithm;
        std::vector<interval_output_event> intervals;
    };

    struct interval_io_result
    {
        interval_io_result();

        bool success;
        std::vector<interval_io_message> messages;
        std::string canonical_json;
        std::string canonical_csv;
    };

    const char* interval_target_name(interval_target target);
    bool interval_target_from_name(const std::string& name, interval_target& target);
    const char* interval_io_message_code_name(interval_io_message_code code);
    bool parse_interval_json_v1(const std::string& json, interval_output_document& output, interval_io_result& result);
    bool parse_interval_csv_v1(const std::string& csv, const std::string& target_name, interval_output_document& output, interval_io_result& result);
    bool write_interval_output(const interval_output_document& document, interval_io_result& result);
}

