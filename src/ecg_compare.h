#pragma once

#include <string>
#include <vector>

namespace signal_synth
{
    struct ecg_render_bundle;

    enum ecg_compare_target
    {
        ecg_compare_r_peak = 0,
        ecg_compare_ppg_systolic_peak = 1,
        ecg_compare_beat_classification = 2,
        ecg_compare_ppg_pulse_onset = 3
    };

    struct ecg_detected_event
    {
        ecg_detected_event();

        double time_seconds;
        std::string label;
        unsigned int original_index;
        bool has_original_index;
    };

    struct ecg_compare_options
    {
        ecg_compare_options();

        ecg_compare_target target;
        double tolerance_seconds;
    };

    struct ecg_compare_bin_metrics
    {
        ecg_compare_bin_metrics();

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

    struct ecg_compare_match
    {
        ecg_compare_match();

        unsigned int ground_truth_index;
        unsigned int detection_index;
        double ground_truth_time_seconds;
        double detection_time_seconds;
        double error_seconds;
        bool in_artifact_interval;
        bool in_motion_artifact_interval;
        bool in_dropout_artifact_interval;
        bool low_perfusion;
        bool weak_pulse;
    };

    struct ecg_compare_unmatched_event
    {
        ecg_compare_unmatched_event();

        unsigned int index;
        double time_seconds;
        bool in_artifact_interval;
        bool in_motion_artifact_interval;
        bool in_dropout_artifact_interval;
        bool low_perfusion;
        bool weak_pulse;
        bool missing_pulse_window;
    };

    struct ppg_pulse_timing_metrics
    {
        ppg_pulse_timing_metrics();

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

    struct ecg_compare_result
    {
        ecg_compare_result();

        bool success;
        std::string target_name;
        double tolerance_seconds;
        ecg_compare_bin_metrics total;
        ecg_compare_bin_metrics clean;
        ecg_compare_bin_metrics artifact;
        ecg_compare_bin_metrics motion;
        ecg_compare_bin_metrics dropout;
        ecg_compare_bin_metrics low_perfusion;
        ecg_compare_bin_metrics weak;
        ppg_pulse_timing_metrics pulse_timing;
        unsigned int missing_pulse_opportunity_count;
        unsigned int detections_in_missing_pulse_windows;
        std::vector<ecg_compare_match> matches;
        std::vector<ecg_compare_unmatched_event> false_positives;
        std::vector<ecg_compare_unmatched_event> false_negatives;
        std::vector<std::string> messages;
    };

    double ecg_compare_default_tolerance_seconds(ecg_compare_target target);
    const char* ecg_compare_target_name(ecg_compare_target target);
    bool compare_detections_to_render(const ecg_render_bundle& render, const std::vector<ecg_detected_event>& detections, const ecg_compare_options& options, ecg_compare_result& result);
    std::string ecg_compare_result_json(const ecg_render_bundle& render, const ecg_compare_result& result);
    std::string ecg_compare_result_csv(const ecg_compare_result& result);
    std::string ecg_compare_report_html(const ecg_render_bundle& render, const ecg_compare_result& result);
}
