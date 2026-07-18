#pragma once

#include "ecg_scenario.h"
#include "ppg_model.h"
#include "signal_quality.h"
#include "wearable_timebase.h"

#include <string>
#include <vector>

namespace signal_synth
{
    struct hrv_scenario_config
    {
        hrv_scenario_config();

        bool enabled;
        double target_mean_hr_bpm;
        double target_sdnn_seconds;
        double lf_hf_ratio;
        double lf_center_hz;
        double lf_bandwidth_hz;
        double hf_center_hz;
        double hf_bandwidth_hz;
        double respiratory_frequency_hz;
        double respiratory_amplitude_seconds;
        double minimum_rr_seconds;
        double maximum_rr_seconds;
        unsigned long long seed;
    };

    struct scenario_randomization_envelope
    {
        std::string parameter;
        double minimum;
        double maximum;
    };

    struct scenario_randomization_config
    {
        scenario_randomization_config();

        bool enabled;
        unsigned long long seed;
        std::vector<scenario_randomization_envelope> envelopes;
    };

    struct physiology_coupling_config
    {
        physiology_coupling_config();

        double respiration_frequency_hz;
        double respiratory_rr_amplitude_seconds;
        double ecg_baseline_amplitude_mv;
        double ppg_amplitude_modulation_ratio;
        double ppg_delay_modulation_ms;
        double accelerometer_respiration_amplitude_g;
        double activity_start_seconds;
        double activity_duration_seconds;
        double activity_intensity;
        unsigned long long seed;
    };

    struct scenario_output_config
    {
        scenario_output_config();

        bool compact;
        bool retain_source_channels;
        bool include_waveform_csv;
        bool include_edf_bdf;
    };

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
        hrv_scenario_config hrv;
        ppg_config ppg;
        scenario_randomization_config randomization;
        physiology_coupling_config physiology;
        scenario_output_config output;
        signal_quality_config signal_quality;
        wearable_timebase_config wearable;

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
