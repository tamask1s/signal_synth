#pragma once

#include "clinical_ecg.h"
#include "detection_io.h"

#include <string>
#include <vector>

namespace signal_synth
{
    struct ecg_render_bundle;

    enum ecg_beat_class
    {
        ecg_beat_normal = 0,
        ecg_beat_supraventricular_ectopic = 1,
        ecg_beat_ventricular_ectopic = 2,
        ecg_beat_paced = 3,
        ecg_beat_escape = 4,
        ecg_beat_fusion = 5,
        ecg_beat_unscored = 6,
        ecg_beat_class_count = 7
    };

    const char* ecg_beat_class_name(ecg_beat_class value);
    bool ecg_beat_class_from_name(const std::string& name, ecg_beat_class& value);
    ecg_beat_class ecg_beat_class_from_origin(clinical_ventricular_origin origin);

    struct ecg_classified_beat_event
    {
        ecg_classified_beat_event();

        double time_seconds;
        ecg_beat_class beat_class;
        unsigned int original_index;
        std::string exclusion_reason;
    };

    struct ecg_beat_classification_options
    {
        ecg_beat_classification_options();

        double tolerance_seconds;
    };

    struct ecg_beat_class_metrics
    {
        ecg_beat_class_metrics();

        bool scored;
        unsigned int ground_truth_count;
        unsigned int prediction_count;
        unsigned int true_positive_count;
        unsigned int false_positive_count;
        unsigned int false_negative_count;
        double precision;
        double recall;
        double f1_score;
    };

    struct ecg_beat_classification_match
    {
        ecg_beat_classification_match();

        unsigned int ground_truth_index;
        unsigned int prediction_index;
        double ground_truth_time_seconds;
        double prediction_time_seconds;
        double error_seconds;
        ecg_beat_class actual_class;
        ecg_beat_class predicted_class;
        bool scored;
        bool correct;
        std::string exclusion_reason;
    };

    struct ecg_beat_classification_unmatched
    {
        ecg_beat_classification_unmatched();

        unsigned int index;
        double time_seconds;
        ecg_beat_class beat_class;
        std::string exclusion_reason;
    };

    struct ecg_beat_classification_result
    {
        ecg_beat_classification_result();

        bool success;
        double tolerance_seconds;
        std::string algorithm_name;
        std::string algorithm_version;
        unsigned int scored_ground_truth_count;
        unsigned int scored_prediction_count;
        unsigned int matched_count;
        unsigned int correct_count;
        unsigned int unscored_match_count;
        unsigned int excluded_ground_truth_count;
        double accuracy;
        double micro_precision;
        double micro_recall;
        double micro_f1_score;
        double mean_absolute_error_seconds;
        double max_absolute_error_seconds;
        unsigned int confusion[ecg_beat_class_count][ecg_beat_class_count];
        ecg_beat_class_metrics classes[ecg_beat_class_count];
        std::vector<ecg_beat_classification_match> matches;
        std::vector<ecg_beat_classification_unmatched> unmatched_ground_truth;
        std::vector<ecg_beat_classification_unmatched> unmatched_predictions;
        std::vector<std::string> messages;
    };

    bool beat_classification_events_from_detection(const detection_io_document& document, std::vector<ecg_classified_beat_event>& output, std::vector<std::string>& messages);
    bool score_ecg_beat_classification(const ecg_render_bundle& render, const detection_io_document& predictions, const ecg_beat_classification_options& options, ecg_beat_classification_result& result);
    std::string ecg_beat_classification_result_json(const ecg_render_bundle& render, const ecg_beat_classification_result& result);
    std::string ecg_beat_classification_result_csv(const ecg_beat_classification_result& result);
    std::string ecg_beat_classification_report_html(const ecg_render_bundle& render, const ecg_beat_classification_result& result);
}
