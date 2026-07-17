#pragma once

#include "interval_io.h"

#include <string>
#include <vector>

namespace signal_synth
{
    struct ecg_render_bundle;

    enum interval_channel_mode
    {
        interval_channel_global = 0,
        interval_channel_per_channel = 1
    };

    struct interval_score_options
    {
        interval_score_options();

        double minimum_iou;
    };

    struct interval_score_metrics
    {
        interval_score_metrics();

        unsigned int ground_truth_count;
        unsigned int prediction_count;
        unsigned int matched_count;
        unsigned int false_alarm_count;
        unsigned int missed_count;
        double ground_truth_duration_seconds;
        double prediction_duration_seconds;
        double overlap_duration_seconds;
        double time_sensitivity;
        double time_precision;
        double time_f1_score;
        double temporal_iou;
        double event_sensitivity;
        double event_precision;
        double false_alarms_per_hour;
        double mean_onset_error_seconds;
        double mean_absolute_onset_error_seconds;
        double median_absolute_onset_error_seconds;
        double max_absolute_onset_error_seconds;
        double mean_offset_error_seconds;
        double mean_absolute_offset_error_seconds;
        double median_absolute_offset_error_seconds;
        double max_absolute_offset_error_seconds;
    };

    struct interval_score_class
    {
        std::string label;
        interval_score_metrics metrics;
    };

    struct interval_score_match
    {
        unsigned int ground_truth_index;
        unsigned int prediction_index;
        std::string ground_truth_label;
        std::string prediction_label;
        std::string channel;
        double intersection_over_union;
        double overlap_seconds;
        double onset_error_seconds;
        double offset_error_seconds;
    };

    struct interval_confusion_cell
    {
        std::string ground_truth_label;
        std::string prediction_label;
        unsigned int count;
    };

    struct interval_score_result
    {
        interval_score_result();

        bool success;
        std::string target_name;
        interval_channel_mode channel_mode;
        double record_duration_seconds;
        double minimum_iou;
        interval_score_metrics total;
        std::vector<interval_score_class> classes;
        std::vector<interval_score_match> matches;
        std::vector<unsigned int> false_positive_indices;
        std::vector<unsigned int> false_negative_indices;
        std::vector<interval_confusion_cell> confusion_matrix;
        std::vector<std::string> messages;
    };

    const char* interval_channel_mode_name(interval_channel_mode mode);
    bool interval_ground_truth_from_render(const ecg_render_bundle& render, interval_target target, interval_channel_mode mode, std::vector<interval_output_event>& output, std::vector<std::string>& messages);
    bool score_intervals(const std::string& target_name, double record_duration_seconds, const std::vector<interval_output_event>& ground_truth, const std::vector<interval_output_event>& predictions, const interval_score_options& options, interval_score_result& result);
    bool score_interval_output_to_render(const ecg_render_bundle& render, const interval_output_document& predictions, const interval_score_options& options, interval_score_result& result);
    std::string interval_score_result_json(const ecg_render_bundle& render, const interval_output_document& predictions, const interval_score_result& result);
    std::string interval_score_result_csv(const interval_score_result& result);
    std::string interval_score_report_html(const ecg_render_bundle& render, const interval_score_result& result);
}

