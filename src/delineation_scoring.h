#pragma once

#include "delineation_io.h"

#include <string>
#include <vector>

namespace signal_synth
{
    struct ecg_render_bundle;

    enum delineation_anchor_type
    {
        delineation_anchor_atrial_event = 0,
        delineation_anchor_ventricular_beat = 1
    };

    enum delineation_truth_status
    {
        delineation_truth_present = 0,
        delineation_truth_absent = 1,
        delineation_truth_not_evaluable = 2
    };

    struct delineation_time_window
    {
        delineation_time_window();
        delineation_time_window(double start, double end);

        double start_seconds;
        double end_seconds;
    };

    struct delineation_evaluation_scope
    {
        std::vector<std::string> leads;
        std::vector<delineation_time_window> windows;
    };

    struct delineation_truth_point
    {
        delineation_truth_point();

        delineation_anchor_type anchor_type;
        unsigned long long anchor_index;
        std::string lead;
        delineation_kind kind;
        delineation_truth_status status;
        std::string reason;
        double time_seconds;
        double evaluation_start_seconds;
        double evaluation_end_seconds;
        unsigned int original_index;
    };

    struct delineation_score_options
    {
        delineation_score_options();

        double tolerance_seconds;
        double pairing_window_seconds;
    };

    struct delineation_score_metrics
    {
        delineation_score_metrics();

        unsigned int ground_truth_count;
        unsigned int absent_truth_count;
        unsigned int not_evaluable_truth_count;
        unsigned int prediction_count;
        unsigned int excluded_prediction_count;
        unsigned int paired_count;
        unsigned int within_tolerance_count;
        unsigned int missing_prediction_count;
        unsigned int unexpected_prediction_count;
        unsigned int out_of_tolerance_count;
        unsigned int false_negative_count;
        unsigned int false_positive_count;
        double sensitivity;
        double positive_predictive_value;
        double f1_score;
        double within_tolerance_fraction;
        double mean_error_seconds;
        double mean_absolute_error_seconds;
        double median_absolute_error_seconds;
        double rms_error_seconds;
        double p95_absolute_error_seconds;
        double max_absolute_error_seconds;
    };

    struct delineation_score_group
    {
        std::string kind;
        std::string lead;
        delineation_score_metrics metrics;
    };

    struct delineation_score_match
    {
        unsigned int ground_truth_index;
        unsigned int prediction_index;
        delineation_anchor_type anchor_type;
        unsigned long long anchor_index;
        std::string lead;
        delineation_kind kind;
        double ground_truth_time_seconds;
        double prediction_time_seconds;
        double error_seconds;
        bool within_tolerance;
    };

    struct delineation_excluded_prediction
    {
        delineation_event event;
        std::string reason;
    };

    struct delineation_score_result
    {
        delineation_score_result();

        bool success;
        double record_duration_seconds;
        double tolerance_seconds;
        double pairing_window_seconds;
        delineation_score_metrics total;
        std::vector<delineation_score_group> kinds;
        std::vector<delineation_score_group> leads;
        std::vector<delineation_score_group> kind_leads;
        std::vector<delineation_truth_point> truth;
        std::vector<delineation_score_match> matches;
        std::vector<delineation_truth_point> missing_events;
        std::vector<delineation_event> unexpected_events;
        std::vector<delineation_excluded_prediction> excluded_predictions;
        std::vector<std::string> messages;
    };

    const char* delineation_anchor_type_name(delineation_anchor_type type);
    const char* delineation_truth_status_name(delineation_truth_status status);
    bool delineation_ground_truth_from_render(const ecg_render_bundle& render, const delineation_evaluation_scope& scope, std::vector<delineation_truth_point>& output, std::vector<std::string>& messages);
    bool score_delineation_events(double record_duration_seconds, const std::vector<delineation_truth_point>& ground_truth, const std::vector<delineation_event>& predictions, const delineation_evaluation_scope& scope, const delineation_score_options& options, delineation_score_result& result);
    bool score_delineation_output_to_render(const ecg_render_bundle& render, const delineation_output_document& predictions, const delineation_evaluation_scope& scope, const delineation_score_options& options, delineation_score_result& result);
    std::string delineation_score_result_json(const ecg_render_bundle& render, const delineation_evaluation_scope& scope, const delineation_score_result& result);
    std::string delineation_score_result_csv(const delineation_score_result& result);
    std::string delineation_score_report_html(const ecg_render_bundle& render, const delineation_score_result& result);
}
