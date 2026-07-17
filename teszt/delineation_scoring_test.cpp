#include "../src/delineation_scoring.h"
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

    signal_synth::delineation_event event(unsigned long long beat, const char* lead, signal_synth::delineation_kind kind, double time, unsigned int index)
    {
        signal_synth::delineation_event output;
        output.beat_index = beat;
        output.lead = lead;
        output.kind = kind;
        output.time_seconds = time;
        output.original_index = index;
        return output;
    }

    signal_synth::ecg_render_bundle render_document(signal_synth::ecg_scenario_document document, bool& ok)
    {
        signal_synth::ecg_render_bundle render;
        signal_synth::ecg_document_render_result result;
        ok &= check(signal_synth::render_ecg_document(document, render, result), "render");
        return render;
    }
}

int main()
{
    bool ok = true;
    std::vector<signal_synth::delineation_event> truth;
    truth.push_back(event(0, "II", signal_synth::delineation_p_onset, 1.0, 0));
    truth.push_back(event(0, "II", signal_synth::delineation_qrs_onset, 1.2, 1));
    truth.push_back(event(0, "II", signal_synth::delineation_t_offset, 1.6, 2));
    std::vector<signal_synth::delineation_event> predictions = truth;
    predictions[0].time_seconds += 0.010;
    predictions[1].time_seconds += 0.050;
    predictions.erase(predictions.begin() + 2);
    predictions.push_back(event(1, "II", signal_synth::delineation_p_peak, 2.0, 7));
    signal_synth::delineation_score_options options;
    signal_synth::delineation_score_result result;
    ok &= check(signal_synth::score_delineation_events(4.0, truth, predictions, options, result) && result.success, "score_success");
    ok &= check(result.total.ground_truth_count == 3u && result.total.prediction_count == 3u && result.total.paired_count == 2u, "identity_pair_counts");
    ok &= check(result.total.within_tolerance_count == 1u && result.total.out_of_tolerance_count == 1u, "tolerance_counts");
    ok &= check(result.total.missing_prediction_count == 1u && result.total.unexpected_prediction_count == 1u, "unpaired_counts");
    ok &= check(result.total.false_negative_count == 2u && result.total.false_positive_count == 2u && std::fabs(result.total.f1_score - 1.0 / 3.0) < 1e-12, "classification_metrics");
    ok &= check(result.matches.size() == 2u && std::fabs(result.matches[1].error_seconds - 0.050) < 1e-12, "timing_error_preserved");

    signal_synth::ecg_scenario_document document;
    document.scenario_id = "delineation_clean";
    document.duration_seconds = 6.0;
    signal_synth::ecg_render_bundle render = render_document(document, ok);
    signal_synth::delineation_output_document output;
    output.algorithm.name = "perfect";
    output.algorithm.version = "1";
    output.leads.push_back("II");
    output.leads.push_back("V2");
    std::vector<std::string> messages;
    ok &= check(signal_synth::delineation_ground_truth_from_render(render, output, output.events, messages) && !output.events.empty(), "all_beat_truth");
    ok &= check(signal_synth::score_delineation_output_to_render(render, output, options, result) && result.total.f1_score == 1.0, "perfect_render_score");
    ok &= check(result.leads.size() == 2u && result.kinds.size() == signal_synth::delineation_kind_count && result.kind_leads.size() == 18u, "complete_groups");
    const std::string report = signal_synth::delineation_score_result_json(render, output, result);
    ok &= check(report.find("\"score_type\":\"ecg_delineation_qa\"") != std::string::npos && report.find("\"beat_index\":\"0\"") != std::string::npos, "json_report");

    signal_synth::delineation_output_document selected;
    selected.algorithm.name = "selected";
    selected.algorithm.version = "1";
    selected.scope_mode = signal_synth::delineation_scope_selected_beats;
    selected.beat_indices.push_back(render.record.beats()[1].beat_index);
    selected.leads.push_back("II");
    ok &= check(signal_synth::delineation_ground_truth_from_render(render, selected, selected.events, messages) && !selected.events.empty(), "selected_beat_truth");
    for (std::size_t i = 0; i < selected.events.size(); ++i)
        ok &= check(selected.events[i].beat_index == selected.beat_indices[0], "selected_scope_only");
    selected.beat_indices[0] = 999999u;
    selected.events.clear();
    ok &= check(!signal_synth::delineation_ground_truth_from_render(render, selected, selected.events, messages), "unknown_selected_beat_rejected");

    signal_synth::ecg_scenario_document af_document;
    af_document.scenario_id = "delineation_afib";
    af_document.duration_seconds = 6.0;
    af_document.ecg.clear_conditions();
    af_document.ecg.add_condition(signal_synth::ecg_condition_afib);
    signal_synth::ecg_render_bundle af_render = render_document(af_document, ok);
    signal_synth::delineation_output_document af_scope;
    af_scope.algorithm.name = "af";
    af_scope.algorithm.version = "1";
    af_scope.leads.push_back("II");
    ok &= check(signal_synth::delineation_ground_truth_from_render(af_render, af_scope, af_scope.events, messages), "af_truth");
    bool has_p = false;
    for (std::size_t i = 0; i < af_scope.events.size(); ++i)
        has_p = has_p || af_scope.events[i].kind == signal_synth::delineation_p_onset || af_scope.events[i].kind == signal_synth::delineation_p_peak || af_scope.events[i].kind == signal_synth::delineation_p_offset;
    ok &= check(!has_p, "absent_p_not_truth");
    af_scope.events.push_back(event(af_render.record.beats()[0].beat_index, "II", signal_synth::delineation_p_peak, af_render.record.beats()[0].r_peak_time_seconds - 0.15, 999));
    ok &= check(signal_synth::score_delineation_output_to_render(af_render, af_scope, options, result) && result.total.unexpected_prediction_count == 1u, "absent_p_prediction_unexpected");

    std::vector<signal_synth::delineation_event> ordered_leads;
    ordered_leads.push_back(event(0, "V2", signal_synth::delineation_qrs_onset, 1.0, 0));
    ordered_leads.push_back(event(0, "aVF", signal_synth::delineation_qrs_onset, 1.0, 1));
    ordered_leads.push_back(event(0, "II", signal_synth::delineation_qrs_onset, 1.0, 2));
    ok &= check(signal_synth::score_delineation_events(2.0, ordered_leads, ordered_leads, options, result)
        && result.leads.size() == 3u && result.leads[0].lead == "II" && result.leads[1].lead == "aVF" && result.leads[2].lead == "V2", "clinical_lead_order");
    return ok ? 0 : 1;
}
