#include "../src/clinical_ecg.h"
#include "../src/ecg_export.h"
#include "../src/ecg_scenario.h"
#include "../src/ecg_scenario_json.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <string>

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
        return inside >= 2 && outside >= 2 && hidden_p == inside && visible_p == outside && close(measured_rate, rate, 1e-9);
    }
}

int main()
{
    bool ok = true;
    ok &= check(signal_synth::find_ecg_condition(signal_synth::ecg_condition_psvt)->support == signal_synth::ecg_support_native && signal_synth::find_ecg_condition(signal_synth::ecg_condition_svarr)->support == signal_synth::ecg_support_parameterized && signal_synth::find_ecg_condition(signal_synth::ecg_condition_abqrs)->support == signal_synth::ecg_support_catalog_only && signal_synth::ecg_scenario_engine_version() == 13, "episode_support_levels_and_engine_version");

    signal_synth::ecg_qa_scenario psvt;
    psvt.add_condition(signal_synth::ecg_condition_psvt);
    psvt.set_heart_rate_bpm(70.0);
    psvt.set_episode_start_seconds(2.0);
    psvt.set_episode_duration_seconds(4.0);
    psvt.set_episode_rate_bpm(180.0);
    signal_synth::clinical_ecg_record record;
    signal_synth::ecg_scenario_report report;
    ok &= check(signal_synth::ecg_scenario_engine().generate(psvt, 5000, record, report) && all_assertions_passed(report) && episode_contract(record, signal_synth::clinical_episode_psvt, 2.0, 6.0, 180.0), "psvt_episode_contract_and_assertions");

    signal_synth::clinical_ecg_record repeated;
    signal_synth::ecg_scenario_report repeated_report;
    ok &= check(signal_synth::ecg_scenario_engine().generate(psvt, 5000, repeated, repeated_report) && same_lead_ii(record, repeated) && report.scenario_fingerprint() == repeated_report.scenario_fingerprint() && report.run_fingerprint() == repeated_report.run_fingerprint(), "episode_generation_is_reproducible");
    signal_synth::ecg_qa_scenario shifted = psvt;
    shifted.set_episode_start_seconds(2.5);
    signal_synth::ecg_qa_scenario longer = psvt;
    longer.set_episode_duration_seconds(5.0);
    signal_synth::ecg_qa_scenario faster = psvt;
    faster.set_episode_rate_bpm(190.0);
    signal_synth::ecg_qa_scenario typed = psvt;
    typed.set_episode_type(signal_synth::ecg_episode_psvt);
    ok &= check(psvt.fingerprint() != shifted.fingerprint() && psvt.fingerprint() != longer.fingerprint() && psvt.fingerprint() != faster.fingerprint() && psvt.fingerprint() != typed.fingerprint(), "episode_fingerprint_covers_parameters");

    signal_synth::ecg_qa_scenario svarr;
    svarr.add_condition(signal_synth::ecg_condition_svarr);
    svarr.set_heart_rate_bpm(72.0);
    svarr.set_episode_type(signal_synth::ecg_episode_svarr);
    svarr.set_episode_start_seconds(1.5);
    svarr.set_episode_duration_seconds(3.0);
    svarr.set_episode_rate_bpm(165.0);
    signal_synth::clinical_ecg_record svarr_record;
    signal_synth::ecg_scenario_report svarr_report;
    ok &= check(signal_synth::ecg_scenario_engine().generate(svarr, 5000, svarr_record, svarr_report) && all_assertions_passed(svarr_report) && report_has_issue(svarr_report, signal_synth::ecg_issue_parameterized_condition) && episode_contract(svarr_record, signal_synth::clinical_episode_svarr, 1.5, 4.5, 165.0), "svarr_canonical_episode_contract");

    signal_synth::ecg_qa_scenario invalid_duration = psvt;
    invalid_duration.set_episode_duration_seconds(0.2);
    ok &= check(!signal_synth::ecg_scenario_engine().validate(invalid_duration, report) && report_has_issue(report, signal_synth::ecg_issue_invalid_parameter), "short_episode_is_rejected");
    signal_synth::ecg_qa_scenario mismatched_type = psvt;
    mismatched_type.set_episode_type(signal_synth::ecg_episode_svarr);
    ok &= check(!signal_synth::ecg_scenario_engine().validate(mismatched_type, report) && report_has_issue(report, signal_synth::ecg_issue_invalid_parameter), "mismatched_episode_type_is_rejected");
    signal_synth::ecg_qa_scenario unused_type;
    unused_type.add_condition(signal_synth::ecg_condition_sr);
    unused_type.set_episode_type(signal_synth::ecg_episode_psvt);
    ok &= check(!signal_synth::ecg_scenario_engine().validate(unused_type, report) && report_has_issue(report, signal_synth::ecg_issue_missing_requirement), "unused_episode_type_is_rejected");
    signal_synth::ecg_qa_scenario composed = psvt;
    composed.add_condition(signal_synth::ecg_condition_pvc);
    ok &= check(!signal_synth::ecg_scenario_engine().validate(composed, report) && report_has_issue(report, signal_synth::ecg_issue_condition_conflict), "episode_ectopy_composition_is_rejected");

    signal_synth::ecg_scenario_document document;
    document.schema_version = 2;
    document.scenario_id = "psvt_episode";
    document.ecg.clear_conditions();
    document.ecg.add_condition(signal_synth::ecg_condition_psvt);
    document.ecg.set_heart_rate_bpm(70.0);
    document.ecg.set_episode_type(signal_synth::ecg_episode_psvt);
    document.ecg.set_episode_start_seconds(2.0);
    document.ecg.set_episode_duration_seconds(4.0);
    document.ecg.set_episode_rate_bpm(180.0);
    signal_synth::ecg_scenario_json_result json;
    signal_synth::ecg_scenario_document roundtrip;
    signal_synth::ecg_scenario_json_result parsed;
    ok &= check(signal_synth::write_ecg_scenario_json(document, json) && json.canonical_json.find("\"episode_type\":\"psvt\"") != std::string::npos && signal_synth::parse_ecg_scenario_json(json.canonical_json, roundtrip, parsed) && roundtrip.ecg.episode_type() == signal_synth::ecg_episode_psvt && close(roundtrip.ecg.episode_start_seconds(), 2.0) && close(roundtrip.ecg.episode_duration_seconds(), 4.0) && close(roundtrip.ecg.episode_rate_bpm(), 180.0), "episode_json_roundtrip");

    signal_synth::ecg_render_bundle render;
    signal_synth::ecg_export_result export_result;
    signal_synth::ecg_export_bundle bundle;
    ok &= check(signal_synth::render_ecg_document(document, render, export_result) && signal_synth::build_ecg_export_bundle(render, bundle, export_result) && bundle.find("annotations.json") && bundle.find("annotations.json")->content.find("\"episodes\":[{\"kind\":\"psvt\"") != std::string::npos && bundle.find("annotations.json")->content.find("\"onset_transition_start_seconds\"") != std::string::npos && bundle.find("ground_truth_metrics.json")->content.find("\"episode_count\":1") != std::string::npos, "episode_export_contract");
    std::ifstream script("../examples/databrowser/074_ECG_Episode_Rhythm_Phenotypes.txt");
    if (!script.good())
    {
        script.clear();
        script.open("../../examples/databrowser/074_ECG_Episode_Rhythm_Phenotypes.txt");
    }
    ok &= check(script.good(), "databrowser_episode_script_is_present");

    return ok ? 0 : 1;
}
