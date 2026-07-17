#include "../src/delineation_scoring.h"
#include "../src/ecg_render.h"

#include <cmath>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace
{
    bool check(bool condition, const char* name)
    {
        if (!condition) std::cerr << "FAIL: " << name << '\n';
        return condition;
    }

    signal_synth::delineation_event event(const char* lead, signal_synth::delineation_kind kind, double time, unsigned int index)
    {
        signal_synth::delineation_event output;
        output.lead = lead;
        output.kind = kind;
        output.time_seconds = time;
        output.original_index = index;
        return output;
    }

    signal_synth::delineation_truth_point truth(signal_synth::delineation_anchor_type anchor_type, unsigned long long anchor_index, const char* lead, signal_synth::delineation_kind kind, signal_synth::delineation_truth_status status, double time, double start, double end, unsigned int index)
    {
        signal_synth::delineation_truth_point output;
        output.anchor_type = anchor_type;
        output.anchor_index = anchor_index;
        output.lead = lead;
        output.kind = kind;
        output.status = status;
        output.reason = status == signal_synth::delineation_truth_absent ? "wave_absent" : status == signal_synth::delineation_truth_not_evaluable ? "below_lead_threshold" : "";
        output.time_seconds = time;
        output.evaluation_start_seconds = start;
        output.evaluation_end_seconds = end;
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

    signal_synth::delineation_evaluation_scope scope(const char* first, const char* second = 0)
    {
        signal_synth::delineation_evaluation_scope output;
        output.leads.push_back(first);
        if (second) output.leads.push_back(second);
        return output;
    }
}

int main()
{
    bool ok = true;
    std::vector<signal_synth::delineation_truth_point> manual_truth;
    manual_truth.push_back(truth(signal_synth::delineation_anchor_atrial_event, 0, "II", signal_synth::delineation_p_onset, signal_synth::delineation_truth_present, 1.0, 0.95, 1.08, 0));
    manual_truth.push_back(truth(signal_synth::delineation_anchor_ventricular_beat, 0, "II", signal_synth::delineation_qrs_onset, signal_synth::delineation_truth_present, 1.2, 1.2, 1.3, 1));
    manual_truth.push_back(truth(signal_synth::delineation_anchor_ventricular_beat, 0, "II", signal_synth::delineation_t_offset, signal_synth::delineation_truth_present, 1.6, 1.35, 1.6, 2));
    manual_truth.push_back(truth(signal_synth::delineation_anchor_ventricular_beat, 1, "II", signal_synth::delineation_p_peak, signal_synth::delineation_truth_absent, 2.0, 1.85, 2.10, 3));
    manual_truth.push_back(truth(signal_synth::delineation_anchor_ventricular_beat, 2, "II", signal_synth::delineation_t_peak, signal_synth::delineation_truth_not_evaluable, 2.5, 2.40, 2.60, 4));
    std::vector<signal_synth::delineation_event> predictions;
    predictions.push_back(event("II", signal_synth::delineation_p_onset, 1.010, 0));
    predictions.push_back(event("II", signal_synth::delineation_qrs_onset, 1.250, 1));
    predictions.push_back(event("II", signal_synth::delineation_p_peak, 2.000, 2));
    predictions.push_back(event("II", signal_synth::delineation_t_peak, 2.500, 3));
    predictions.push_back(event("II", signal_synth::delineation_p_peak, 3.500, 4));
    signal_synth::delineation_score_options options;
    signal_synth::delineation_score_result result;
    const signal_synth::delineation_evaluation_scope ii_scope = scope("II");
    ok &= check(signal_synth::score_delineation_events(4.0, manual_truth, predictions, ii_scope, options, result) && result.success, "score_success");
    ok &= check(result.total.ground_truth_count == 3u && result.total.absent_truth_count == 1u && result.total.not_evaluable_truth_count == 1u, "truth_status_counts");
    ok &= check(result.total.prediction_count == 4u && result.total.excluded_prediction_count == 1u, "not_evaluable_prediction_excluded");
    ok &= check(result.total.paired_count == 2u && result.total.within_tolerance_count == 1u && result.total.out_of_tolerance_count == 1u, "temporal_pair_counts");
    ok &= check(result.total.missing_prediction_count == 1u && result.total.unexpected_prediction_count == 2u, "unpaired_counts");
    ok &= check(result.total.false_negative_count == 2u && result.total.false_positive_count == 3u && std::fabs(result.total.f1_score - 2.0 / 7.0) < 1e-12, "classification_metrics");
    ok &= check(result.matches.size() == 2u && result.matches[0].anchor_type == signal_synth::delineation_anchor_atrial_event, "anchor_preserved_in_match");

    signal_synth::ecg_scenario_document document;
    document.scenario_id = "delineation_clean";
    document.duration_seconds = 6.0;
    signal_synth::ecg_render_bundle render = render_document(document, ok);
    const signal_synth::delineation_evaluation_scope clean_scope = scope("II", "V2");
    std::vector<signal_synth::delineation_truth_point> clean_truth;
    std::vector<std::string> messages;
    ok &= check(signal_synth::delineation_ground_truth_from_render(render, clean_scope, clean_truth, messages) && !clean_truth.empty(), "render_truth");
    signal_synth::delineation_output_document perfect;
    for (std::size_t i = 0; i < clean_truth.size(); ++i)
        if (clean_truth[i].status == signal_synth::delineation_truth_present) perfect.events.push_back(event(clean_truth[i].lead.c_str(), clean_truth[i].kind, clean_truth[i].time_seconds, static_cast<unsigned int>(perfect.events.size())));
    ok &= check(signal_synth::score_delineation_output_to_render(render, perfect, clean_scope, options, result) && result.total.f1_score == 1.0, "perfect_render_score");
    ok &= check(result.leads.size() == 2u && result.kinds.size() == signal_synth::delineation_kind_count && result.kind_leads.size() == 18u, "complete_groups");
    const std::string report = signal_synth::delineation_score_result_json(render, clean_scope, result);
    ok &= check(report.find("\"schema_version\":2") != std::string::npos && report.find("\"anchor_type\":\"atrial_event\"") != std::string::npos && report.find("\"status\":\"present\"") != std::string::npos, "json_truth_contract");

    signal_synth::ecg_scenario_document edge_document;
    edge_document.scenario_id = "delineation_record_edge";
    edge_document.duration_seconds = 10.0;
    edge_document.ecg.set_heart_rate_bpm(70.0);
    signal_synth::ecg_render_bundle edge_render = render_document(edge_document, ok);
    std::vector<signal_synth::delineation_truth_point> edge_truth;
    ok &= check(signal_synth::delineation_ground_truth_from_render(edge_render, ii_scope, edge_truth, messages), "record_edge_truth");
    bool has_record_boundary = false;
    for (std::size_t i = 0; i < edge_truth.size(); ++i)
        has_record_boundary = has_record_boundary || (edge_truth[i].status == signal_synth::delineation_truth_not_evaluable && edge_truth[i].reason == "record_boundary" && edge_truth[i].evaluation_end_seconds > edge_truth[i].evaluation_start_seconds);
    ok &= check(has_record_boundary, "record_boundary_has_nonempty_exclusion_window");

    signal_synth::ecg_scenario_document af_document;
    af_document.scenario_id = "delineation_afib";
    af_document.duration_seconds = 6.0;
    af_document.ecg.clear_conditions();
    af_document.ecg.add_condition(signal_synth::ecg_condition_afib);
    signal_synth::ecg_render_bundle af_render = render_document(af_document, ok);
    std::vector<signal_synth::delineation_truth_point> af_truth;
    ok &= check(signal_synth::delineation_ground_truth_from_render(af_render, ii_scope, af_truth, messages), "af_truth");
    bool has_present_p = false;
    unsigned int absent_p_count = 0;
    for (std::size_t i = 0; i < af_truth.size(); ++i)
    {
        const bool p = af_truth[i].kind == signal_synth::delineation_p_onset || af_truth[i].kind == signal_synth::delineation_p_peak || af_truth[i].kind == signal_synth::delineation_p_offset;
        has_present_p = has_present_p || (p && af_truth[i].status == signal_synth::delineation_truth_present);
        if (p && af_truth[i].status == signal_synth::delineation_truth_absent) ++absent_p_count;
    }
    ok &= check(!has_present_p && absent_p_count > 0u, "af_p_absence_is_explicit_truth");
    signal_synth::delineation_output_document af_prediction;
    af_prediction.events.push_back(event("II", signal_synth::delineation_p_peak, af_truth[0].evaluation_start_seconds + 0.01, 0));
    ok &= check(signal_synth::score_delineation_output_to_render(af_render, af_prediction, ii_scope, options, result) && result.total.unexpected_prediction_count == 1u, "absent_p_prediction_is_false_positive");

    signal_synth::ecg_scenario_document mobitz_document;
    mobitz_document.scenario_id = "delineation_mobitz_ii";
    mobitz_document.duration_seconds = 12.0;
    mobitz_document.ecg.clear_conditions();
    mobitz_document.ecg.add_condition(signal_synth::ecg_condition_2avb);
    mobitz_document.ecg.set_second_degree_av_pattern(signal_synth::ecg_second_degree_mobitz_ii);
    signal_synth::ecg_render_bundle mobitz_render = render_document(mobitz_document, ok);
    std::vector<signal_synth::delineation_truth_point> mobitz_truth;
    ok &= check(signal_synth::delineation_ground_truth_from_render(mobitz_render, ii_scope, mobitz_truth, messages), "mobitz_truth");
    std::set<unsigned long long> linked_atrials;
    for (unsigned int i = 0; i < mobitz_render.record.beat_count(); ++i)
        if (mobitz_render.record.beats()[i].linked_atrial_index >= 0) linked_atrials.insert(static_cast<unsigned long long>(mobitz_render.record.beats()[i].linked_atrial_index));
    bool has_nonconducted_p = false;
    for (std::size_t i = 0; i < mobitz_truth.size(); ++i)
        has_nonconducted_p = has_nonconducted_p || (mobitz_truth[i].anchor_type == signal_synth::delineation_anchor_atrial_event && mobitz_truth[i].kind == signal_synth::delineation_p_peak && mobitz_truth[i].status == signal_synth::delineation_truth_present && linked_atrials.count(mobitz_truth[i].anchor_index) == 0u);
    ok &= check(mobitz_render.record.atrial_event_count() > mobitz_render.record.beat_count() && has_nonconducted_p, "nonconducted_p_has_atrial_truth_anchor");

    std::vector<signal_synth::delineation_truth_point> ordered_truth;
    ordered_truth.push_back(truth(signal_synth::delineation_anchor_ventricular_beat, 0, "V2", signal_synth::delineation_qrs_onset, signal_synth::delineation_truth_present, 1.0, 0.9, 1.1, 0));
    ordered_truth.push_back(truth(signal_synth::delineation_anchor_ventricular_beat, 0, "aVF", signal_synth::delineation_qrs_onset, signal_synth::delineation_truth_present, 1.0, 0.9, 1.1, 1));
    ordered_truth.push_back(truth(signal_synth::delineation_anchor_ventricular_beat, 0, "II", signal_synth::delineation_qrs_onset, signal_synth::delineation_truth_present, 1.0, 0.9, 1.1, 2));
    std::vector<signal_synth::delineation_event> ordered_predictions;
    for (std::size_t i = 0; i < ordered_truth.size(); ++i) ordered_predictions.push_back(event(ordered_truth[i].lead.c_str(), ordered_truth[i].kind, ordered_truth[i].time_seconds, static_cast<unsigned int>(i)));
    signal_synth::delineation_evaluation_scope ordered_scope;
    ordered_scope.leads.push_back("V2");
    ordered_scope.leads.push_back("aVF");
    ordered_scope.leads.push_back("II");
    ok &= check(signal_synth::score_delineation_events(2.0, ordered_truth, ordered_predictions, ordered_scope, options, result) && result.leads.size() == 3u && result.leads[0].lead == "II" && result.leads[1].lead == "aVF" && result.leads[2].lead == "V2", "clinical_lead_order");
    return ok ? 0 : 1;
}
