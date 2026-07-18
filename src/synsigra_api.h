#pragma once

#include <string>
#include <vector>

namespace signal_synth
{
    enum synsigra_compare_target
    {
        synsigra_compare_r_peak = 0,
        synsigra_compare_ppg_systolic_peak = 1,
        synsigra_compare_ppg_pulse_onset = 2
    };

    struct synsigra_message
    {
        synsigra_message();

        std::string code;
        std::string path;
        std::string message;
    };

    struct synsigra_artifact
    {
        std::string name;
        std::string media_type;
        std::string content;
    };

    struct synsigra_external_noise_asset
    {
        std::string id;
        std::string csv_content;
    };

    struct synsigra_identity
    {
        synsigra_identity();

        std::string scenario_id;
        unsigned int schema_version;
        unsigned int sample_count;
        std::string document_fingerprint;
        unsigned long long generation_fingerprint;
        std::string render_identity;
        std::string generator_version;
    };

    struct synsigra_validation_result
    {
        synsigra_validation_result();

        bool success;
        synsigra_identity identity;
        std::string canonical_scenario_json;
        std::vector<synsigra_message> messages;
    };

    struct synsigra_render_result
    {
        synsigra_render_result();

        bool success;
        synsigra_identity identity;
        std::vector<synsigra_artifact> artifacts;
        std::vector<synsigra_message> messages;

        const synsigra_artifact* find_artifact(const std::string& name) const;
    };

    struct synsigra_detection_event
    {
        synsigra_detection_event();

        double time_seconds;
        std::string label;
    };

    struct synsigra_compare_options
    {
        synsigra_compare_options();

        synsigra_compare_target target;
        double tolerance_seconds;
    };

    struct synsigra_compare_metrics
    {
        synsigra_compare_metrics();

        unsigned int ground_truth_count;
        unsigned int detection_count;
        unsigned int true_positive_count;
        unsigned int false_positive_count;
        unsigned int false_negative_count;
        double sensitivity;
        double positive_predictive_value;
        double f1_score;
        double mean_absolute_error_seconds;
        double median_absolute_error_seconds;
        double rms_error_seconds;
        double max_absolute_error_seconds;
    };

    struct synsigra_ppg_timing_metrics
    {
        synsigra_ppg_timing_metrics();

        unsigned int ground_truth_interval_count;
        unsigned int detection_interval_count;
        unsigned int matched_interval_count;
        double mean_absolute_interval_error_seconds;
        double rms_interval_error_seconds;
        double max_absolute_interval_error_seconds;
        double ground_truth_mean_pulse_rate_bpm;
        double detection_mean_pulse_rate_bpm;
        double absolute_pulse_rate_error_bpm;
    };

    struct synsigra_compare_result
    {
        synsigra_compare_result();

        bool success;
        synsigra_identity identity;
        std::string target_name;
        double tolerance_seconds;
        synsigra_compare_metrics total;
        synsigra_compare_metrics clean;
        synsigra_compare_metrics artifact;
        synsigra_compare_metrics motion;
        synsigra_compare_metrics dropout;
        synsigra_compare_metrics low_perfusion;
        synsigra_compare_metrics weak;
        synsigra_ppg_timing_metrics pulse_timing;
        std::vector<synsigra_artifact> artifacts;
        std::vector<synsigra_message> messages;

        const synsigra_artifact* find_artifact(const std::string& name) const;
    };

    const char* synsigra_api_version();
    const char* synsigra_integration_contract_version();
    std::string synsigra_integration_contract_json();
    double synsigra_default_compare_tolerance_seconds(synsigra_compare_target target);
    const char* synsigra_compare_target_name(synsigra_compare_target target);
    bool synsigra_validate_scenario_json(const std::string& scenario_json, synsigra_validation_result& result);
    bool synsigra_render_scenario_json(const std::string& scenario_json, synsigra_render_result& result);
    bool synsigra_render_scenario_json(const std::string& scenario_json, const std::vector<synsigra_external_noise_asset>& external_noise_assets, synsigra_render_result& result);
    bool synsigra_compare_scenario_detections(const std::string& scenario_json, const std::vector<synsigra_detection_event>& detections, const synsigra_compare_options& options, synsigra_compare_result& result);
    bool synsigra_compare_scenario_detections(const std::string& scenario_json, const std::vector<synsigra_external_noise_asset>& external_noise_assets, const std::vector<synsigra_detection_event>& detections, const synsigra_compare_options& options, synsigra_compare_result& result);
}
