#pragma once

#include <string>
#include <vector>

namespace signal_synth
{
    enum delineation_kind
    {
        delineation_p_onset = 0,
        delineation_p_peak = 1,
        delineation_p_offset = 2,
        delineation_qrs_onset = 3,
        delineation_j_point = 4,
        delineation_qrs_offset = 5,
        delineation_t_onset = 6,
        delineation_t_peak = 7,
        delineation_t_offset = 8,
        delineation_p_secondary_peak = 9,
        delineation_p_notch = 10,
        delineation_r_prime = 11,
        delineation_qrs_fragment = 12,
        delineation_t_secondary_peak = 13,
        delineation_t_notch = 14,
        delineation_u_onset = 15,
        delineation_u_peak = 16,
        delineation_u_offset = 17,
        delineation_kind_count = 18
    };

    enum delineation_io_message_code
    {
        delineation_io_syntax = 0,
        delineation_io_type = 1,
        delineation_io_missing_field = 2,
        delineation_io_unknown_field = 3,
        delineation_io_range = 4,
        delineation_io_duplicate = 5
    };

    struct delineation_io_message
    {
        delineation_io_message_code code;
        std::string path;
        std::string message;
    };

    struct delineation_event
    {
        delineation_event();

        std::string lead;
        delineation_kind kind;
        double time_seconds;
        bool has_sample_index;
        unsigned long long sample_index;
        bool has_confidence;
        double confidence;
        unsigned int original_index;
    };

    struct delineation_output_document
    {
        delineation_output_document();

        unsigned int schema_version;
        std::vector<delineation_event> events;
    };

    struct delineation_io_result
    {
        delineation_io_result();

        bool success;
        std::vector<delineation_io_message> messages;
        std::string canonical_json;
        std::string canonical_csv;
    };

    const char* delineation_kind_name(delineation_kind kind);
    bool delineation_kind_from_name(const std::string& name, delineation_kind& kind);
    const char* delineation_io_message_code_name(delineation_io_message_code code);
    bool parse_delineation_point_events_json_v1(const std::string& json, delineation_output_document& output, delineation_io_result& result);
    bool parse_delineation_point_events_csv_v1(const std::string& csv, delineation_output_document& output, delineation_io_result& result);
    bool write_delineation_point_events(const delineation_output_document& document, delineation_io_result& result);
}
