#pragma once

#include "delineation_io.h"

#include <string>
#include <vector>

namespace signal_synth
{
    struct ecg_render_bundle;

    struct delineation_score_options
    {
        delineation_score_options();

        double tolerance_seconds;
    };

    struct delineation_score_metrics
    {
        delineation_score_metrics();

        unsigned int ground_truth_count;
        unsigned int prediction_count;
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
        unsigned long long beat_index;
        std::string lead;
        delineation_kind kind;
        double ground_truth_time_seconds;
        double prediction_time_seconds;
        double error_seconds;
        bool within_tolerance;
    };

    struct delineation_score_result
    {
        delineation_score_result();

        bool success;
        double record_duration_seconds;
        double tolerance_seconds;
        delineation_score_metrics total;
        std::vector<delineation_score_group> kinds;
        std::vector<delineation_score_group> leads;
        std::vector<delineation_score_group> kind_leads;
        std::vector<delineation_score_match> matches;
        std::vector<delineation_event> missing_events;
        std::vector<delineation_event> unexpected_events;
        std::vector<std::string> messages;
    };

    bool delineation_ground_truth_from_render(const ecg_render_bundle& render, const delineation_output_document& scope, std::vector<delineation_event>& output, std::vector<std::string>& messages);
    bool score_delineation_events(double record_duration_seconds, const std::vector<delineation_event>& ground_truth, const std::vector<delineation_event>& predictions, const delineation_score_options& options, delineation_score_result& result);
    bool score_delineation_output_to_render(const ecg_render_bundle& render, const delineation_output_document& predictions, const delineation_score_options& options, delineation_score_result& result);
    std::string delineation_score_result_json(const ecg_render_bundle& render, const delineation_output_document& predictions, const delineation_score_result& result);
    std::string delineation_score_result_csv(const delineation_score_result& result);
    std::string delineation_score_report_html(const ecg_render_bundle& render, const delineation_score_result& result);
}
