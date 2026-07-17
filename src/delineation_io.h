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
        delineation_kind_count = 9
    };

    enum delineation_scope_mode
    {
        delineation_scope_all_beats = 0,
        delineation_scope_selected_beats = 1
    };

    enum delineation_io_message_code
    {
        delineation_io_syntax = 0,
        delineation_io_type = 1,
        delineation_io_missing_field = 2,
        delineation_io_unknown_field = 3,
        delineation_io_range = 4,
        delineation_io_duplicate = 5,
        delineation_io_scope = 6
    };

    struct delineation_io_message
    {
        delineation_io_message_code code;
        std::string path;
        std::string message;
    };

    struct delineation_algorithm_metadata
    {
        std::string name;
        std::string version;
    };

    struct delineation_event
    {
        delineation_event();

        unsigned long long beat_index;
        std::string lead;
        delineation_kind kind;
        double time_seconds;
        bool has_confidence;
        double confidence;
        unsigned int original_index;
    };

    struct delineation_output_document
    {
        delineation_output_document();

        unsigned int schema_version;
        std::string target_name;
        delineation_algorithm_metadata algorithm;
        delineation_scope_mode scope_mode;
        std::vector<unsigned long long> beat_indices;
        std::vector<std::string> leads;
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
    const char* delineation_scope_mode_name(delineation_scope_mode mode);
    bool delineation_scope_mode_from_name(const std::string& name, delineation_scope_mode& mode);
    const char* delineation_io_message_code_name(delineation_io_message_code code);
    bool parse_delineation_json_v1(const std::string& json, delineation_output_document& output, delineation_io_result& result);
    bool parse_delineation_csv_v1(const std::string& csv, delineation_output_document& output, delineation_io_result& result);
    bool write_delineation_output(const delineation_output_document& document, delineation_io_result& result);
}
