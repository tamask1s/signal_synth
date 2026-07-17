#pragma once

#include <string>
#include <vector>

namespace signal_synth
{
    enum measurement_status
    {
        measurement_valid = 0,
        measurement_undefined = 1,
        measurement_absent = 2,
        measurement_not_evaluable = 3
    };

    enum measurement_scope
    {
        measurement_record = 0,
        measurement_lead = 1,
        measurement_beat = 2,
        measurement_beat_lead = 3,
        measurement_paired_signal = 4
    };

    struct measurement_value
    {
        measurement_value();

        std::string name;
        double value;
        bool has_value;
        std::string unit;
        measurement_status status;
        measurement_scope scope;
        double time_seconds;
        bool has_time_seconds;
        unsigned long long beat_index;
        bool has_beat_index;
        std::string channel;
        std::string formula;
        double confidence;
        bool has_confidence;
        unsigned int original_index;
    };

    struct measurement_output_document
    {
        measurement_output_document();

        unsigned int schema_version;
        std::vector<measurement_value> measurements;
    };

    enum measurement_io_message_code
    {
        measurement_io_syntax = 0,
        measurement_io_schema = 1,
        measurement_io_missing_field = 2,
        measurement_io_unknown_field = 3,
        measurement_io_invalid_value = 4,
        measurement_io_duplicate = 5
    };

    struct measurement_io_message
    {
        measurement_io_message_code code;
        std::string path;
        std::string message;
    };

    struct measurement_io_result
    {
        measurement_io_result();

        bool success;
        std::vector<measurement_io_message> messages;
        std::string canonical_json;
        std::string canonical_csv;
    };

    const char* measurement_status_name(measurement_status status);
    bool measurement_status_from_name(const std::string& name, measurement_status& status);
    const char* measurement_scope_name(measurement_scope scope);
    bool measurement_scope_from_name(const std::string& name, measurement_scope& scope);
    const char* measurement_io_message_code_name(measurement_io_message_code code);
    bool parse_measurement_values_json_v1(const std::string& input, measurement_output_document& output, measurement_io_result& result);
    bool parse_measurement_values_csv_v1(const std::string& input, measurement_output_document& output, measurement_io_result& result);
}
