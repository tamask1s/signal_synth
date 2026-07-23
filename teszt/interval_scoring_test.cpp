#include "../src/interval_scoring.h"
#include "../src/ecg_render.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    bool check(bool condition, const char* name)
    {
        if (!condition) std::cerr << "FAIL: " << name << '\n';
        return condition;
    }

    bool near(double left, double right)
    {
        return std::fabs(left - right) < 1e-12;
    }

    signal_synth::interval_output_event interval(double start, double end, const char* label, const char* channel, unsigned int index)
    {
        signal_synth::interval_output_event output;
        output.start_seconds = start;
        output.end_seconds = end;
        output.label = label;
        output.channel = channel;
        output.original_index = index;
        return output;
    }

    const signal_synth::interval_score_class* score_class(const signal_synth::interval_score_result& result, const std::string& label)
    {
        for (std::size_t i = 0; i < result.classes.size(); ++i)
            if (result.classes[i].label == label) return &result.classes[i];
        return 0;
    }
}

int main()
{
    bool ok = true;
    std::vector<signal_synth::interval_output_event> truth;
    truth.push_back(interval(1.0, 3.0, "a", "global", 0));
    truth.push_back(interval(4.0, 6.0, "b", "global", 1));
    std::vector<signal_synth::interval_output_event> predictions;
    predictions.push_back(interval(1.2, 3.2, "a", "global", 10));
    predictions.push_back(interval(4.0, 6.0, "c", "global", 11));
    predictions.push_back(interval(7.0, 8.0, "a", "global", 12));
    signal_synth::interval_score_options options;
    signal_synth::interval_score_result result;
    ok &= check(signal_synth::score_intervals("rhythm_episode", 10.0, truth, predictions, options, result) && result.success, "score_success");
    ok &= check(result.total.ground_truth_count == 2 && result.total.prediction_count == 3 && result.total.matched_count == 1, "event_counts");
    ok &= check(result.total.false_alarm_count == 2 && result.total.missed_count == 1 && near(result.total.false_alarms_per_hour, 720.0), "false_alarm_metrics");
    ok &= check(near(result.total.overlap_duration_seconds, 1.8) && near(result.total.ground_truth_duration_seconds, 4.0) && near(result.total.prediction_duration_seconds, 5.0), "micro_duration_metrics");
    ok &= check(result.matches.size() == 1 && result.matches[0].ground_truth_index == 0 && result.matches[0].prediction_index == 10, "deterministic_match_identity");
    ok &= check(near(result.matches[0].onset_error_seconds, 0.2) && near(result.matches[0].offset_error_seconds, 0.2), "boundary_errors");
    const signal_synth::interval_score_class* class_a = score_class(result, "a");
    ok &= check(class_a && class_a->metrics.ground_truth_count == 1 && class_a->metrics.prediction_count == 2 && class_a->metrics.matched_count == 1, "class_metrics");
    bool wrong_class_cell = false;
    for (std::size_t i = 0; i < result.confusion_matrix.size(); ++i)
        wrong_class_cell = wrong_class_cell || (result.confusion_matrix[i].ground_truth_label == "b" && result.confusion_matrix[i].prediction_label == "c" && result.confusion_matrix[i].count == 1);
    ok &= check(wrong_class_cell, "wrong_class_confusion");

    std::vector<signal_synth::interval_output_event> split_truth(1, interval(1.0, 3.0, "a", "global", 0));
    std::vector<signal_synth::interval_output_event> split_predictions;
    split_predictions.push_back(interval(1.0, 2.0, "a", "global", 0));
    split_predictions.push_back(interval(2.0, 3.0, "a", "global", 1));
    ok &= check(signal_synth::score_intervals("signal_quality", 10.0, split_truth, split_predictions, options, result), "split_score");
    ok &= check(near(result.total.time_sensitivity, 1.0) && near(result.total.time_precision, 1.0) && result.total.matched_count == 1 && result.total.false_alarm_count == 1, "split_duration_and_event_semantics");

    std::vector<signal_synth::interval_output_event> overlapping_truth;
    overlapping_truth.push_back(interval(1.0, 4.0, "a", "global", 0));
    overlapping_truth.push_back(interval(2.0, 3.0, "b", "global", 1));
    std::vector<signal_synth::interval_output_event> overlapping_predictions = overlapping_truth;
    ok &= check(signal_synth::score_intervals("signal_quality", 10.0, overlapping_truth, overlapping_predictions, options, result), "overlapping_class_score");
    ok &= check(result.total.matched_count == 2 && near(result.total.ground_truth_duration_seconds, 4.0) && near(result.total.time_f1_score, 1.0), "overlapping_classes_scored_independently");

    std::vector<signal_synth::interval_output_event> empty;
    ok &= check(signal_synth::score_intervals("signal_quality", 10.0, truth, empty, options, result), "empty_prediction_score");
    const std::string empty_json = signal_synth::interval_score_result_json(signal_synth::ecg_render_bundle(), signal_synth::interval_output_document(), result);
    ok &= check(result.total.missed_count == truth.size() && empty_json.find("\"time_precision\":null") != std::string::npos, "zero_denominator_json");
    ok &= check(signal_synth::score_intervals("signal_quality", 10.0, empty, split_predictions, options, result) && result.total.false_alarm_count == 2 && result.total.missed_count == 0, "empty_ground_truth_score");

    std::vector<signal_synth::interval_output_event> channel_prediction(1, interval(1.0, 3.0, "a", "II", 0));
    ok &= check(signal_synth::score_intervals("signal_quality", 10.0, truth, channel_prediction, options, result) && result.total.matched_count == 0, "channel_mismatch");
    std::vector<signal_synth::interval_output_event> duplicate_predictions(2, interval(1.0, 3.0, "a", "global", 0));
    duplicate_predictions[1].original_index = 1;
    ok &= check(!signal_synth::score_intervals("signal_quality", 10.0, truth, duplicate_predictions, options, result), "direct_duplicate_rejected");
    std::vector<signal_synth::interval_output_event> mixed_predictions;
    mixed_predictions.push_back(interval(1.0, 3.0, "a", "global", 0));
    mixed_predictions.push_back(interval(4.0, 6.0, "b", "II", 1));
    ok &= check(!signal_synth::score_intervals("signal_quality", 10.0, truth, mixed_predictions, options, result), "direct_mixed_channel_mode_rejected");
    ok &= check(!signal_synth::score_intervals("rhythm_episode", 10.0, empty, channel_prediction, options, result), "direct_physical_rhythm_rejected");
    options.minimum_iou = 0.0;
    ok &= check(!signal_synth::score_intervals("signal_quality", 10.0, truth, predictions, options, result), "invalid_iou_rejected");
    options.minimum_iou = 0.1;

    signal_synth::ecg_scenario_document episode_document;
    episode_document.schema_version = 2;
    episode_document.scenario_id = "interval_episode";
    episode_document.duration_seconds = 12.0;
    episode_document.ecg.clear_conditions();
    episode_document.ecg.add_condition(signal_synth::ecg_condition_psvt);
    episode_document.ecg.set_heart_rate_bpm(70.0);
    episode_document.ecg.add_rhythm_episode(signal_synth::ecg_episode_psvt, 3.0, 4.0, 0.2, 180.0, 7201);
    signal_synth::ecg_render_bundle episode_render;
    signal_synth::ecg_document_render_result render_result;
    ok &= check(signal_synth::render_ecg_document(episode_document, episode_render, render_result), "episode_render");
    signal_synth::interval_output_document episode_predictions;
    episode_predictions.target_name = "rhythm_episode";
    episode_predictions.intervals.push_back(interval(3.0, 7.0, "psvt", "global", 0));
    ok &= check(signal_synth::score_interval_output_to_render(episode_render, episode_predictions, options, result) && result.total.time_f1_score == 1.0, "episode_ground_truth_adapter");

    signal_synth::ecg_scenario_document quality_document;
    quality_document.schema_version = 2;
    quality_document.scenario_id = "interval_quality";
    quality_document.duration_seconds = 8.0;
    signal_synth::signal_quality_artifact_config artifact;
    artifact.type = signal_synth::signal_quality_ecg_baseline_wander;
    artifact.start_seconds = 2.0;
    artifact.duration_seconds = 3.0;
    artifact.severity = 0.4;
    artifact.ecg_leads[signal_synth::clinical_lead_ii] = true;
    quality_document.signal_quality.artifacts.push_back(artifact);
    signal_synth::ecg_render_bundle quality_render;
    ok &= check(signal_synth::render_ecg_document(quality_document, quality_render, render_result), "quality_render");
    signal_synth::interval_output_document quality_predictions;
    quality_predictions.target_name = "signal_quality";
    quality_predictions.intervals.push_back(interval(2.0, 5.0, "ecg_baseline_wander", "II", 0));
    ok &= check(signal_synth::score_interval_output_to_render(quality_render, quality_predictions, options, result) && result.channel_mode == signal_synth::interval_channel_per_channel && result.total.time_f1_score == 1.0, "quality_per_channel_adapter");
    const std::string interval_html = signal_synth::interval_score_report_html(quality_render, result);
    const std::string notice = "Synthetic engineering QA evidence; not diagnosis, nor clinical evidence";
    ok &= check(interval_html.find(notice) != std::string::npos && interval_html.find(notice) == interval_html.rfind(notice)
        && interval_html.find("background:#f3f4f6") != std::string::npos, "html_notice_contract");
    quality_predictions.intervals.push_back(interval(2.0, 5.0, "ecg_baseline_wander", "global", 1));
    ok &= check(!signal_synth::score_interval_output_to_render(quality_render, quality_predictions, options, result), "mixed_channel_mode_rejected");
    return ok ? 0 : 1;
}
