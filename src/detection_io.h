#pragma once

#include "ecg_compare.h"

#include <string>
#include <vector>

namespace signal_synth
{
    enum detection_io_message_code
    {
        detection_io_syntax = 0,
        detection_io_type = 1,
        detection_io_missing_field = 2,
        detection_io_unknown_field = 3,
        detection_io_range = 4,
        detection_io_duplicate_id = 5,
        detection_io_target_mismatch = 6
    };

    struct detection_io_message
    {
        detection_io_message_code code;
        std::string path;
        std::string message;
    };

    struct detection_algorithm_metadata
    {
        std::string name;
        std::string version;
    };

    struct detection_io_event
    {
        detection_io_event();

        double time_seconds;
        bool has_sample_index;
        unsigned int sample_index;
        std::string channel;
        std::string label;
        bool has_confidence;
        double confidence;
        unsigned int original_index;
    };

    struct detection_io_document
    {
        detection_io_document();

        unsigned int schema_version;
        std::string target_name;
        bool has_compare_target;
        ecg_compare_target compare_target;
        detection_algorithm_metadata algorithm;
        std::vector<detection_io_event> events;
    };

    struct detection_io_result
    {
        detection_io_result();

        bool success;
        std::vector<detection_io_message> messages;
        std::string canonical_json;
    };

    const char* detection_io_message_code_name(detection_io_message_code code);
    bool detection_compare_target_from_name(const std::string& name, ecg_compare_target& target);
    bool parse_detection_csv_v2(const std::string& csv, const std::string& target_name, detection_io_document& output, detection_io_result& result);
    bool parse_detection_json_v1(const std::string& json, detection_io_document& output, detection_io_result& result);
    bool write_detection_json_v1(const detection_io_document& document, detection_io_result& result);
    bool detection_events_for_compare(const detection_io_document& document, std::vector<ecg_detected_event>& output, detection_io_result& result);
}
