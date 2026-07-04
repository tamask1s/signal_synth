#pragma once

#include "ecg_export.h"

#include <string>
#include <vector>

namespace signal_synth
{
    enum hrv_metric_kind
    {
        hrv_metric_mean_rr_seconds = 0,
        hrv_metric_mean_heart_rate_bpm = 1,
        hrv_metric_sdnn_seconds = 2,
        hrv_metric_rmssd_seconds = 3,
        hrv_metric_pnn50_percent = 4,
        hrv_metric_sd1_seconds = 5,
        hrv_metric_sd2_seconds = 6,
        hrv_metric_sd1_sd2_ratio = 7,
        hrv_metric_lf_power_seconds2 = 8,
        hrv_metric_hf_power_seconds2 = 9,
        hrv_metric_lf_hf_ratio = 10,
        hrv_metric_total_power_seconds2 = 11,
        hrv_metric_count = 12
    };

    struct hrv_user_metric
    {
        hrv_metric_kind kind;
        double value;
    };

    struct hrv_user_rr_interval
    {
        hrv_user_rr_interval();

        double beat_time_seconds;
        double rr_seconds;
        unsigned int original_index;
    };

    struct hrv_user_output
    {
        hrv_user_output();

        unsigned int schema_version;
        std::string algorithm_name;
        std::string algorithm_version;
        std::vector<hrv_user_metric> metrics;
        std::vector<hrv_user_rr_interval> rr_intervals;
    };

    struct hrv_metric_score
    {
        hrv_metric_score();

        hrv_metric_kind kind;
        double ground_truth_value;
        double user_value;
        double absolute_error;
        double relative_error_percent;
        double absolute_tolerance;
        double relative_tolerance_percent;
        bool passed;
    };

    struct hrv_rr_score
    {
        hrv_rr_score();

        bool evaluated;
        unsigned int ground_truth_count;
        unsigned int user_count;
        unsigned int matched_count;
        unsigned int missing_count;
        unsigned int extra_count;
        unsigned int passed_count;
        double time_tolerance_seconds;
        double absolute_tolerance_seconds;
        double relative_tolerance_percent;
        double mean_absolute_error_seconds;
        double rms_error_seconds;
        double max_absolute_error_seconds;
    };

    struct hrv_score_result
    {
        hrv_score_result();

        bool success;
        std::string scoring_version;
        std::string scenario_id;
        std::string document_fingerprint;
        std::string render_identity;
        std::string algorithm_name;
        std::string algorithm_version;
        std::string metric_definition_version;
        std::string exclusion_policy;
        std::string spectral_method;
        std::vector<hrv_metric_score> metrics;
        hrv_rr_score rr;
        unsigned int passed_metric_count;
        double metric_pass_fraction;
        std::vector<std::string> messages;
    };

    const char* hrv_metric_name(hrv_metric_kind kind);
    const char* hrv_metric_unit(hrv_metric_kind kind);
    bool parse_hrv_user_output_json(const std::string& json, hrv_user_output& output, std::vector<std::string>& messages);
    bool score_hrv_user_output(const ecg_render_bundle& render, const hrv_user_output& user, hrv_score_result& result);
    std::string hrv_score_result_json(const hrv_score_result& result);
    std::string hrv_score_result_csv(const hrv_score_result& result);
    std::string hrv_score_report_html(const hrv_score_result& result);
}
