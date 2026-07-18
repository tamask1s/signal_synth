#include "../src/clinical_ecg.h"
#include "../src/ecg_export.h"
#include "../src/interval_scoring.h"
#include "../src/measurement_scoring.h"
#include "../src/ecg_scenario.h"
#include "../src/ecg_scenario_json.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    bool check(bool condition, const char* name)
    {
        std::cout << (condition ? "PASS " : "FAIL ") << name << '\n';
        return condition;
    }

    bool close(double left, double right, double tolerance = 1e-9)
    {
        return std::fabs(left - right) <= tolerance;
    }

    bool report_has_issue(const signal_synth::ecg_scenario_report& report, signal_synth::ecg_scenario_issue_code code)
    {
        for (unsigned int index = 0; index < report.issue_count(); ++index)
            if (report.issue_code(index) == code)
                return true;
        return false;
    }

    bool all_assertions_passed(const signal_synth::ecg_scenario_report& report)
    {
        if (!report.success() || !report.phenotype_passed() || report.assertion_count() == 0)
            return false;
        for (unsigned int index = 0; index < report.assertion_count(); ++index)
            if (report.assertion_status(index) != signal_synth::ecg_assertion_passed)
                return false;
        return true;
    }

    bool same_lead_ii(const signal_synth::clinical_ecg_record& left, const signal_synth::clinical_ecg_record& right)
    {
        if (left.sample_count() != right.sample_count() || left.episode_count() != right.episode_count() || left.beat_count() != right.beat_count())
            return false;
        for (unsigned int sample = 0; sample < left.sample_count(); ++sample)
            if (left.lead_data(signal_synth::clinical_lead_ii)[sample] != right.lead_data(signal_synth::clinical_lead_ii)[sample])
                return false;
        return true;
    }

    bool episode_contract(const signal_synth::clinical_ecg_record& record, signal_synth::clinical_episode_kind kind, double start, double end, double rate)
    {
        if (record.episode_count() != 1 || !record.episodes())
            return false;
        const signal_synth::clinical_episode_annotation& episode = record.episodes()[0];
        if (episode.kind != kind || !episode.present || !close(episode.start_time_seconds, start) || !close(episode.end_time_seconds, end) || episode.start_sample_index != static_cast<unsigned long long>(std::llround(start * record.sampling_rate_hz())) || episode.end_sample_index != static_cast<unsigned long long>(std::llround(end * record.sampling_rate_hz())))
            return false;
        if (!(episode.onset_transition_start_seconds < episode.start_time_seconds && episode.onset_transition_end_seconds > episode.start_time_seconds && episode.offset_transition_start_seconds < episode.end_time_seconds && episode.offset_transition_end_seconds > episode.end_time_seconds))
            return false;
        if (episode.onset_transition_start_sample_index >= episode.onset_transition_end_sample_index || episode.offset_transition_start_sample_index >= episode.offset_transition_end_sample_index)
            return false;
        unsigned int inside = 0;
        unsigned int outside = 0;
        unsigned int hidden_p = 0;
        unsigned int visible_p = 0;
        double interval_sum = 0.0;
        unsigned int intervals = 0;
        int previous_inside = -1;
        for (unsigned int index = 0; index < record.beat_count(); ++index)
        {
            const signal_synth::clinical_beat_annotation& beat = record.beats()[index];
            const bool in_episode = beat.r_peak_time_seconds >= start && beat.r_peak_time_seconds < end;
            if (in_episode)
            {
                ++inside;
                hidden_p += beat.p_present ? 0U : 1U;
                if (beat.rhythm != signal_synth::clinical_rhythm_supraventricular_tachycardia || beat.qrs_duration_seconds > 0.120)
                    return false;
                if (previous_inside >= 0)
                {
                    interval_sum += beat.r_peak_time_seconds - record.beats()[previous_inside].r_peak_time_seconds;
                    ++intervals;
                }
                previous_inside = static_cast<int>(index);
            }
            else
            {
                ++outside;
                visible_p += beat.p_present ? 1U : 0U;
                if (beat.rhythm != signal_synth::clinical_rhythm_sinus)
                    return false;
            }
        }
        const double measured_rate = intervals && interval_sum > 0.0 ? 60.0 * intervals / interval_sum : 0.0;
        return inside >= 2 && outside >= 2 && hidden_p == inside && visible_p == outside && close(measured_rate, rate, kind == signal_synth::clinical_episode_svarr ? 10.0 : 1e-9);
    }
}

int main()
{
    bool ok = true;
    ok &= check(signal_synth::find_ecg_condition(signal_synth::ecg_condition_psvt)->support == signal_synth::ecg_support_native && signal_synth::find_ecg_condition(signal_synth::ecg_condition_svarr)->support == signal_synth::ecg_support_parameterized && signal_synth::find_ecg_condition(signal_synth::ecg_condition_abqrs)->support == signal_synth::ecg_support_catalog_only && signal_synth::ecg_scenario_engine_version() == 15, "episode_support_levels_and_engine_version");

    signal_synth::ecg_qa_scenario psvt;
    psvt.add_condition(signal_synth::ecg_condition_psvt);
    psvt.set_heart_rate_bpm(70.0);
    psvt.add_rhythm_episode(signal_synth::ecg_episode_psvt, 2.0, 4.0, 0.2, 180.0, 1001);
    signal_synth::clinical_ecg_record record;
    signal_synth::ecg_scenario_report report;
    ok &= check(signal_synth::ecg_scenario_engine().generate(psvt, 5000, record, report) && all_assertions_passed(report) && episode_contract(record, signal_synth::clinical_episode_psvt, 2.0, 6.0, 180.0), "psvt_episode_contract_and_assertions");

    signal_synth::clinical_ecg_record repeated;
    signal_synth::ecg_scenario_report repeated_report;
    ok &= check(signal_synth::ecg_scenario_engine().generate(psvt, 5000, repeated, repeated_report) && same_lead_ii(record, repeated) && report.scenario_fingerprint() == repeated_report.scenario_fingerprint() && report.run_fingerprint() == repeated_report.run_fingerprint(), "episode_generation_is_reproducible");
    signal_synth::ecg_qa_scenario shifted = psvt;
    shifted.clear_rhythm_episodes(); shifted.add_rhythm_episode(signal_synth::ecg_episode_psvt, 2.5, 4.0, 0.2, 180.0, 1001);
    signal_synth::ecg_qa_scenario longer = psvt;
    longer.clear_rhythm_episodes(); longer.add_rhythm_episode(signal_synth::ecg_episode_psvt, 2.0, 5.0, 0.2, 180.0, 1001);
    signal_synth::ecg_qa_scenario faster = psvt;
    faster.clear_rhythm_episodes(); faster.add_rhythm_episode(signal_synth::ecg_episode_psvt, 2.0, 4.0, 0.2, 190.0, 1001);
    signal_synth::ecg_qa_scenario reseeded = psvt;
    reseeded.clear_rhythm_episodes(); reseeded.add_rhythm_episode(signal_synth::ecg_episode_psvt, 2.0, 4.0, 0.2, 180.0, 1002);
    ok &= check(psvt.fingerprint() != shifted.fingerprint() && psvt.fingerprint() != longer.fingerprint() && psvt.fingerprint() != faster.fingerprint() && psvt.fingerprint() != reseeded.fingerprint(), "episode_fingerprint_covers_parameters");

    signal_synth::ecg_qa_scenario svarr;
    svarr.add_condition(signal_synth::ecg_condition_svarr);
    svarr.set_heart_rate_bpm(72.0);
    svarr.add_rhythm_episode(signal_synth::ecg_episode_svarr, 1.5, 3.0, 0.2, 165.0, 1003);
    signal_synth::clinical_ecg_record svarr_record;
    signal_synth::ecg_scenario_report svarr_report;
    ok &= check(signal_synth::ecg_scenario_engine().generate(svarr, 5000, svarr_record, svarr_report) && all_assertions_passed(svarr_report) && report_has_issue(svarr_report, signal_synth::ecg_issue_parameterized_condition) && episode_contract(svarr_record, signal_synth::clinical_episode_svarr, 1.5, 4.5, 165.0), "svarr_canonical_episode_contract");

    signal_synth::ecg_qa_scenario invalid_duration; invalid_duration.add_condition(signal_synth::ecg_condition_psvt); invalid_duration.set_heart_rate_bpm(70.0); invalid_duration.add_rhythm_episode(signal_synth::ecg_episode_psvt, 2.0, 0.2, 0.05, 180.0);
    ok &= check(!signal_synth::ecg_scenario_engine().validate(invalid_duration, report) && report_has_issue(report, signal_synth::ecg_issue_invalid_parameter), "short_episode_is_rejected");
    signal_synth::ecg_qa_scenario mismatched_type = psvt;
    mismatched_type.clear_rhythm_episodes(); mismatched_type.add_rhythm_episode(signal_synth::ecg_episode_svarr, 2.0, 4.0, 0.2, 180.0);
    ok &= check(!signal_synth::ecg_scenario_engine().validate(mismatched_type, report) && report_has_issue(report, signal_synth::ecg_issue_missing_requirement), "mismatched_episode_type_is_rejected");
    signal_synth::ecg_qa_scenario unused_type;
    unused_type.add_condition(signal_synth::ecg_condition_sr);
    unused_type.add_rhythm_episode(signal_synth::ecg_episode_vf, 2.0, 2.0, 0.2, 0.0, 1004);
    ok &= check(signal_synth::ecg_scenario_engine().validate(unused_type, report), "engineering_episode_does_not_require_diagnostic_statement");
    ok &= check(!unused_type.add_rhythm_episode(signal_synth::ecg_episode_asystole, 3.0, 2.0, 0.2, 0.0), "overlapping_episode_is_rejected_at_api_boundary");
    signal_synth::ecg_qa_scenario composed = psvt;
    composed.add_condition(signal_synth::ecg_condition_pvc);
    ok &= check(!signal_synth::ecg_scenario_engine().validate(composed, report) && report_has_issue(report, signal_synth::ecg_issue_condition_conflict), "episode_ectopy_composition_is_rejected");
    signal_synth::ecg_qa_scenario composed_morphology = psvt;
    composed_morphology.add_condition(signal_synth::ecg_condition_clbbb);
    ok &= check(!signal_synth::ecg_scenario_engine().validate(composed_morphology, report) && report_has_issue(report, signal_synth::ecg_issue_condition_conflict), "episode_morphology_composition_is_rejected");
    signal_synth::ecg_qa_scenario abrupt_vf;
    abrupt_vf.add_condition(signal_synth::ecg_condition_sr);
    ok &= check(!abrupt_vf.add_rhythm_episode(signal_synth::ecg_episode_vf, 2.0, 2.0, 0.0, 0.0), "vf_requires_smooth_waveform_transition");
    signal_synth::clinical_ecg_record clipped_record;
    ok &= check(!signal_synth::ecg_scenario_engine().generate(psvt, 2500, clipped_record, report) && report_has_issue(report, signal_synth::ecg_issue_invalid_parameter), "episode_beyond_generated_record_is_rejected");

    signal_synth::ecg_scenario_document document;
    document.schema_version = 2;
    document.scenario_id = "psvt_episode";
    document.ecg.clear_conditions();
    document.ecg.add_condition(signal_synth::ecg_condition_psvt);
    document.ecg.set_heart_rate_bpm(70.0);
    document.ecg.add_rhythm_episode(signal_synth::ecg_episode_psvt, 2.0, 4.0, 0.2, 180.0, 1005);
    signal_synth::ecg_scenario_json_result json;
    signal_synth::ecg_scenario_document roundtrip;
    signal_synth::ecg_scenario_json_result parsed;
    signal_synth::ecg_rhythm_episode roundtrip_episode;
    ok &= check(signal_synth::write_ecg_scenario_json(document, json) && json.canonical_json.find("\"rhythm_episodes\":[{\"type\":\"psvt\"") != std::string::npos && signal_synth::parse_ecg_scenario_json(json.canonical_json, roundtrip, parsed) && roundtrip.ecg.rhythm_episode_count() == 1 && roundtrip.ecg.rhythm_episode(0, roundtrip_episode) && roundtrip_episode.type == signal_synth::ecg_episode_psvt && close(roundtrip_episode.start_seconds, 2.0) && close(roundtrip_episode.duration_seconds, 4.0) && close(roundtrip_episode.rate_bpm, 180.0), "episode_json_roundtrip");

    signal_synth::ecg_render_bundle render;
    signal_synth::ecg_document_render_result render_result;
    signal_synth::ecg_export_result export_result;
    signal_synth::ecg_export_bundle bundle;
    ok &= check(signal_synth::render_ecg_document(document, render, render_result) && signal_synth::build_ecg_export_bundle(render, bundle, export_result) && bundle.find("annotations.json") && bundle.find("annotations.json")->content.find("\"episodes\":[{\"kind\":\"psvt\"") != std::string::npos && bundle.find("annotations.json")->content.find("\"onset_transition_start_seconds\"") != std::string::npos && bundle.find("ground_truth_metrics.json")->content.find("\"episode_count\":1") != std::string::npos, "episode_export_contract");

    signal_synth::ecg_scenario_document burden_document;
    burden_document.schema_version = 2;
    burden_document.scenario_id = "advanced_rhythm_burden";
    burden_document.duration_seconds = 20.0;
    burden_document.ecg.clear_conditions(); burden_document.ecg.add_condition(signal_synth::ecg_condition_sr); burden_document.ecg.set_heart_rate_bpm(70.0);
    burden_document.ecg.add_rhythm_episode(signal_synth::ecg_episode_afib, 2.0, 2.0, 0.2, 105.0, 2001);
    burden_document.ecg.add_rhythm_episode(signal_synth::ecg_episode_psvt, 5.0, 2.0, 0.2, 180.0, 2002);
    burden_document.ecg.add_rhythm_episode(signal_synth::ecg_episode_vt, 8.0, 2.0, 0.2, 150.0, 2003);
    burden_document.ecg.add_rhythm_episode(signal_synth::ecg_episode_vf, 11.0, 2.0, 0.2, 0.0, 2004);
    burden_document.ecg.add_rhythm_episode(signal_synth::ecg_episode_asystole, 14.0, 2.0, 0.2, 0.0, 2005);
    signal_synth::ecg_render_bundle burden_render;
    ok &= check(signal_synth::render_ecg_document(burden_document, burden_render, render_result) && burden_render.record.episode_count() == 5, "multi_episode_render");
    unsigned int vf_beats = 0, asystole_beats = 0;
    for (unsigned int index = 0; index < burden_render.record.beat_count(); ++index)
    {
        const double time = burden_render.record.beats()[index].r_peak_time_seconds;
        vf_beats += time >= 11.0 && time < 13.0 ? 1u : 0u;
        asystole_beats += time >= 14.0 && time < 16.0 ? 1u : 0u;
    }
    double vf_energy = 0.0, asystole_energy = 0.0;
    for (unsigned int sample = 0; sample < burden_render.record.sample_count(); ++sample)
    {
        const double time = static_cast<double>(sample) / burden_render.record.sampling_rate_hz();
        if (time >= 11.25 && time < 12.75) vf_energy += std::fabs(burden_render.record.lead_data(signal_synth::clinical_lead_ii)[sample]);
        if (time >= 14.5 && time < 15.5) asystole_energy += std::fabs(burden_render.record.lead_data(signal_synth::clinical_lead_ii)[sample]);
    }
    ok &= check(vf_beats == 0 && asystole_beats == 0 && vf_energy > 1.0 && asystole_energy < 1e-9, "vf_and_asystole_waveform_contract");
    std::vector<signal_synth::interval_output_event> episode_intervals;
    std::vector<std::string> messages;
    ok &= check(signal_synth::interval_ground_truth_from_render(burden_render, signal_synth::interval_target_rhythm_episode, signal_synth::interval_channel_global, episode_intervals, messages) && episode_intervals.size() == 5, "critical_state_interval_truth");
    std::vector<signal_synth::measurement_truth> burden_truth;
    ok &= check(signal_synth::measurement_ground_truth_from_render(burden_render, "rhythm_burden", burden_truth, messages) && burden_truth.size() == 18, "burden_measurement_truth");
    signal_synth::measurement_output_document burden_predictions;
    for (std::size_t index = 0; index < burden_truth.size(); ++index) burden_predictions.measurements.push_back(burden_truth[index].measurement);
    signal_synth::measurement_score_options burden_options;
    signal_synth::measurement_score_result burden_score;
    ok &= check(signal_synth::score_measurement_output_to_render(burden_render, "rhythm_burden", burden_predictions, burden_options, burden_score) && burden_score.total.tolerance_pass_count == burden_score.total.numeric_pair_count && burden_score.total.missing_count == 0, "burden_perfect_scoring");
    std::ifstream script("../examples/databrowser/074_ECG_Episode_Rhythm_Phenotypes.txt");
    if (!script.good())
    {
        script.clear();
        script.open("../../examples/databrowser/074_ECG_Episode_Rhythm_Phenotypes.txt");
    }
    ok &= check(script.good(), "databrowser_episode_script_is_present");

    return ok ? 0 : 1;
}
