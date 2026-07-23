#include "../src/measurement_scoring.h"

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

    signal_synth::measurement_truth truth(const char* name, double value, const char* unit, signal_synth::measurement_scope scope, double time, unsigned long long beat)
    {
        signal_synth::measurement_truth output;
        output.measurement.name = name;
        output.measurement.value = value;
        output.measurement.has_value = true;
        output.measurement.unit = unit;
        output.measurement.status = signal_synth::measurement_valid;
        output.measurement.scope = scope;
        if (scope == signal_synth::measurement_beat || scope == signal_synth::measurement_beat_lead || scope == signal_synth::measurement_paired_signal)
        {
            output.measurement.has_time_seconds = true;
            output.measurement.time_seconds = time;
            output.measurement.has_beat_index = true;
            output.measurement.beat_index = beat;
        }
        output.absolute_tolerance = unit == std::string("deg") ? 5.0 : 0.02;
        output.relative_tolerance_percent = 5.0;
        return output;
    }
}

int main()
{
    bool ok = true;
    std::vector<signal_synth::measurement_truth> manual_truth;
    manual_truth.push_back(truth("qrs_duration", 0.10, "s", signal_synth::measurement_beat, 1.0, 10));
    manual_truth.push_back(truth("qrs_duration", 0.12, "s", signal_synth::measurement_beat, 2.0, 11));
    signal_synth::measurement_truth axis = truth("qrs_axis", 179.0, "deg", signal_synth::measurement_record, 0.0, 0);
    axis.error_model = signal_synth::measurement_error_circular_degrees;
    manual_truth.push_back(axis);
    signal_synth::measurement_truth absent;
    absent.measurement.name = "t_amplitude";
    absent.measurement.unit = "mV";
    absent.measurement.status = signal_synth::measurement_absent;
    absent.measurement.scope = signal_synth::measurement_beat_lead;
    absent.measurement.has_time_seconds = true;
    absent.measurement.time_seconds = 3.0;
    absent.measurement.channel = "II";
    manual_truth.push_back(absent);

    std::vector<signal_synth::measurement_value> predictions;
    for (std::size_t i = 0; i < manual_truth.size(); ++i) predictions.push_back(manual_truth[i].measurement);
    predictions[0].has_beat_index = false;
    predictions[0].time_seconds = 1.01;
    predictions[1].value = 0.16;
    predictions[2].value = -179.0;
    predictions[3].status = signal_synth::measurement_not_evaluable;
    signal_synth::measurement_score_options options;
    signal_synth::measurement_score_result score;
    ok &= check(signal_synth::score_measurements("morphology_assertions", manual_truth, predictions, options, score) && score.success, "manual_score");
    ok &= check(score.total.matched_count == 4u && score.total.numeric_pair_count == 3u && score.total.tolerance_pass_count == 2u, "numeric_counts");
    ok &= check(score.total.status_mismatch_count == 1u && score.total.missing_count == 0u && score.total.extra_count == 0u, "status_counts");
    bool circular_axis_error = false;
    for (std::size_t i = 0; i < score.matches.size(); ++i)
        if (manual_truth[score.matches[i].ground_truth_index].measurement.name == "qrs_axis")
            circular_axis_error = std::fabs(score.matches[i].signed_error - 2.0) < 1e-12 && score.matches[i].within_tolerance;
    ok &= check(circular_axis_error, "circular_axis_error");

    signal_synth::measurement_truth window = truth("sdnn_seconds", 0.05, "s", signal_synth::measurement_window, 0.0, 0);
    window.measurement.has_window_start_seconds = true;
    window.measurement.window_start_seconds = 0.0;
    window.measurement.has_window_end_seconds = true;
    window.measurement.window_end_seconds = 60.0;
    window.measurement.method_id = "synsigra_hrv_metrics_v2";
    window.measurement.preprocessing_policy_id = "synsigra_nn_exclusion_v2";
    std::vector<signal_synth::measurement_truth> window_truth(1u, window);
    std::vector<signal_synth::measurement_value> window_prediction(1u, window.measurement);
    ok &= check(signal_synth::score_measurements("hrv", window_truth, window_prediction, options, score) && score.total.matched_count == 1u && score.contexts.size() == 1u, "window_method_match");
    window_prediction[0].method_id = "another_method_v1";
    ok &= check(signal_synth::score_measurements("hrv", window_truth, window_prediction, options, score) && score.total.missing_count == 1u && score.total.extra_count == 1u, "method_identity_mismatch");

    predictions.pop_back();
    signal_synth::measurement_value extra = manual_truth[0].measurement;
    extra.name = "unknown_measurement";
    predictions.push_back(extra);
    ok &= check(signal_synth::score_measurements("morphology_assertions", manual_truth, predictions, options, score) && score.total.missing_count == 1u && score.total.extra_count == 1u && score.total.truth_match_fraction < 1.0 && score.total.prediction_match_fraction < 1.0, "missing_and_extra");

    signal_synth::ecg_scenario_document document;
    document.schema_version = 2;
    document.scenario_id = "measurement_truth";
    document.duration_seconds = 8.0;
    document.ecg.clear_conditions();
    document.ecg.add_condition(signal_synth::ecg_condition_nst);
    document.ecg.set_fidelity_policy(signal_synth::ecg_fidelity_allow_parameterized);
    document.ppg.enabled = true;
    signal_synth::ecg_render_bundle render;
    signal_synth::ecg_document_render_result render_result;
    ok &= check(signal_synth::render_ecg_document(document, render, render_result), "render");
    std::vector<signal_synth::measurement_truth> morphology;
    std::vector<std::string> messages;
    ok &= check(signal_synth::measurement_ground_truth_from_render(render, "morphology_assertions", morphology, messages) && !morphology.empty(), "morphology_truth");
    bool has_qtc = false, has_st = false, has_axis = false, has_assertion = false;
    for (std::size_t i = 0; i < morphology.size(); ++i)
    {
        has_qtc = has_qtc || (morphology[i].measurement.name == "qtc_interval" && !morphology[i].measurement.formula.empty());
        has_st = has_st || morphology[i].measurement.name == "st_slope";
        has_axis = has_axis || morphology[i].measurement.name == "qrs_axis";
        has_assertion = has_assertion || morphology[i].measurement.name.find("assertion.") == 0u;
    }
    if (!(has_qtc && has_st && has_axis && has_assertion))
        std::cerr << "coverage qtc=" << has_qtc << " st=" << has_st << " axis=" << has_axis << " assertion=" << has_assertion << '\n';
    ok &= check(has_qtc && has_st && has_axis && has_assertion, "morphology_adapter_coverage");
    signal_synth::measurement_output_document perfect;
    for (std::size_t i = 0; i < morphology.size(); ++i) perfect.measurements.push_back(morphology[i].measurement);
    ok &= check(signal_synth::score_measurement_output_to_render(render, "morphology_assertions", perfect, options, score) && score.total.missing_count == 0u && score.total.extra_count == 0u && score.total.status_mismatch_count == 0u && score.total.tolerance_pass_count == score.total.numeric_pair_count, "morphology_perfect");

    std::vector<signal_synth::measurement_truth> alignment;
    ok &= check(signal_synth::measurement_ground_truth_from_render(render, "ecg_ppg_alignment", alignment, messages) && alignment.size() == render.ppg.pulse_count() * 2u, "alignment_truth");
    bool has_ptt = false, has_peak_delay = false;
    for (std::size_t i = 0; i < alignment.size(); ++i)
    {
        has_ptt = has_ptt || alignment[i].measurement.name == "pulse_transit_time";
        has_peak_delay = has_peak_delay || alignment[i].measurement.name == "ecg_ppg_peak_delay";
    }
    ok &= check(has_ptt && has_peak_delay, "alignment_adapter_coverage");
    const std::string truth_json = signal_synth::measurement_truth_bundle_json(render, std::vector<std::string>{"morphology_assertions", "ecg_ppg_alignment"});
    ok &= check(truth_json.find("\"contract\":\"synsigra_measurement_truth_v2\"") != std::string::npos && truth_json.find("\"target\":\"ecg_ppg_alignment\"") != std::string::npos, "truth_bundle");
    std::vector<signal_synth::measurement_truth> hrv;
    ok &= check(signal_synth::measurement_ground_truth_from_render(render, "hrv", hrv, messages) && hrv.size() > 15u, "generic_hrv_truth");
    bool has_window_metric = false, has_nn_rr = false;
    for (std::size_t i = 0; i < hrv.size(); ++i)
    {
        has_window_metric = has_window_metric || (hrv[i].measurement.name == "sdnn_seconds" && hrv[i].measurement.scope == signal_synth::measurement_window && hrv[i].measurement.method_id == "synsigra_hrv_metrics_v2");
        has_nn_rr = has_nn_rr || (hrv[i].measurement.name == "rr_interval" && hrv[i].measurement.preprocessing_policy_id == "synsigra_nn_exclusion_v2");
    }
    ok &= check(has_window_metric && has_nn_rr, "generic_hrv_context");
    signal_synth::measurement_output_document hrv_perfect;
    for (std::size_t i = 0; i < hrv.size(); ++i) hrv_perfect.measurements.push_back(hrv[i].measurement);
    ok &= check(signal_synth::score_measurement_output_to_render(render, "hrv", hrv_perfect, options, score) && score.total.tolerance_pass_count == score.total.numeric_pair_count, "generic_hrv_perfect");
    const std::string score_json = signal_synth::measurement_score_result_json(render, score);
    const std::string score_html = signal_synth::measurement_score_report_html(render, score);
    const std::string notice = "Synthetic engineering QA evidence; not diagnosis, nor clinical evidence";
    ok &= check(score_json.find("\"contract\":\"synsigra_measurement_score_v2\"") != std::string::npos && score_json.find("\"by_measurement_context\"") != std::string::npos
        && !signal_synth::measurement_score_result_csv(score).empty() && score_html.find("Preprocessing") != std::string::npos
        && score_html.find(notice) != std::string::npos && score_html.find(notice) == score_html.rfind(notice)
        && score_html.find("background:#f3f4f6") != std::string::npos, "reports");
    return ok ? 0 : 1;
}
