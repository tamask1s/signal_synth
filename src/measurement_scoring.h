#pragma once

#include "ecg_render.h"
#include "measurement_io.h"

#include <string>
#include <vector>

namespace signal_synth
{
    enum measurement_error_model
    {
        measurement_error_linear = 0,
        measurement_error_circular_degrees = 1
    };

    struct measurement_truth
    {
        measurement_truth();

        measurement_value measurement;
        double absolute_tolerance;
        double relative_tolerance_percent;
        measurement_error_model error_model;
        bool has_expected_range;
        double expected_minimum;
        double expected_maximum;
        std::string reason;
    };

    struct measurement_score_options
    {
        measurement_score_options();

        double pairing_window_seconds;
    };

    struct measurement_score_metrics
    {
        measurement_score_metrics();

        unsigned int ground_truth_count;
        unsigned int valid_truth_count;
        unsigned int undefined_truth_count;
        unsigned int absent_truth_count;
        unsigned int not_evaluable_truth_count;
        unsigned int prediction_count;
        unsigned int matched_count;
        unsigned int covered_truth_count;
        unsigned int matched_prediction_count;
        unsigned int numeric_pair_count;
        unsigned int tolerance_pass_count;
        unsigned int status_match_count;
        unsigned int status_mismatch_count;
        unsigned int missing_count;
        unsigned int extra_count;
        unsigned int assertion_comparable_count;
        unsigned int assertion_agreement_count;
        double tolerance_pass_fraction;
        double status_match_fraction;
        double assertion_agreement_fraction;
        double truth_match_fraction;
        double prediction_match_fraction;
        double bias;
        double mean_absolute_error;
        double root_mean_square_error;
        double median_absolute_error;
        double p95_absolute_error;
        double maximum_absolute_error;
    };

    struct measurement_score_group
    {
        std::string name;
        std::string channel;
        measurement_score_metrics metrics;
    };

    struct measurement_score_context_group
    {
        measurement_score_context_group();

        std::string name;
        std::string unit;
        measurement_scope scope;
        std::string channel;
        std::string formula;
        std::string method_id;
        std::string preprocessing_policy_id;
        double window_start_seconds;
        bool has_window_start_seconds;
        double window_end_seconds;
        bool has_window_end_seconds;
        measurement_score_metrics metrics;
    };

    struct measurement_score_match
    {
        measurement_score_match();

        unsigned int ground_truth_index;
        unsigned int prediction_index;
        std::string pairing_method;
        measurement_status ground_truth_status;
        measurement_status prediction_status;
        bool status_matches;
        bool numeric_pair;
        double signed_error;
        double absolute_error;
        double relative_error_percent;
        bool has_relative_error;
        bool within_tolerance;
        bool has_assertion_result;
        bool ground_truth_assertion_passed;
        bool prediction_assertion_passed;
    };

    struct measurement_score_result
    {
        measurement_score_result();

        bool success;
        std::string target;
        double pairing_window_seconds;
        measurement_score_metrics total;
        std::vector<measurement_score_group> measurements;
        std::vector<measurement_score_group> channels;
        std::vector<measurement_score_group> measurement_channels;
        std::vector<measurement_score_context_group> contexts;
        std::vector<measurement_truth> ground_truth;
        std::vector<measurement_value> predictions;
        std::vector<measurement_score_match> matches;
        std::vector<unsigned int> missing_ground_truth_indices;
        std::vector<unsigned int> extra_prediction_indices;
        std::vector<std::string> messages;
    };

    const char* measurement_error_model_name(measurement_error_model model);
    bool measurement_target_supported(const std::string& target);
    bool measurement_ground_truth_from_render(const ecg_render_bundle& render, const std::string& target, std::vector<measurement_truth>& output, std::vector<std::string>& messages);
    bool score_measurements(const std::string& target, const std::vector<measurement_truth>& ground_truth, const std::vector<measurement_value>& predictions, const measurement_score_options& options, measurement_score_result& result);
    bool score_measurement_output_to_render(const ecg_render_bundle& render, const std::string& target, const measurement_output_document& predictions, const measurement_score_options& options, measurement_score_result& result);
    std::string measurement_truth_bundle_json(const ecg_render_bundle& render, const std::vector<std::string>& targets);
    std::string measurement_score_result_json(const ecg_render_bundle& render, const measurement_score_result& result);
    std::string measurement_score_result_csv(const measurement_score_result& result);
    std::string measurement_score_report_html(const ecg_render_bundle& render, const measurement_score_result& result);
}
