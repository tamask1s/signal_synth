#include "../src/ecg_scenario.h"
#include "../src/clinical_ecg.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <set>
#include <string>

namespace
{
    bool check(bool condition, const char* name)
    {
        if (condition)
        {
            std::cout << "PASS " << name << '\n';
            return true;
        }
        std::cerr << "FAIL " << name << '\n';
        return false;
    }

    bool report_has_issue(const signal_synth::ecg_scenario_report& report, signal_synth::ecg_scenario_issue_code code)
    {
        for (unsigned int index = 0; index < report.issue_count(); ++index)
            if (report.issue_code(index) == code)
                return true;
        return false;
    }

    int effective_index(const signal_synth::ecg_scenario_report& report, signal_synth::ecg_condition_code code)
    {
        for (unsigned int index = 0; index < report.effective_condition_count(); ++index)
            if (report.effective_condition(index) == code)
                return static_cast<int>(index);
        return -1;
    }

    bool same_signal(const signal_synth::clinical_ecg_record& left, const signal_synth::clinical_ecg_record& right)
    {
        if (left.sample_count() != right.sample_count() || left.lead_count() != right.lead_count() || left.beat_count() != right.beat_count())
            return false;
        for (unsigned int lead = 0; lead < left.lead_count(); ++lead)
            for (unsigned int sample = 0; sample < left.sample_count(); ++sample)
                if (left.lead_data(lead)[sample] != right.lead_data(lead)[sample])
                    return false;
        return true;
    }

    bool all_assertions_passed(const signal_synth::ecg_scenario_report& report)
    {
        if (!report.phenotype_passed() || report.assertion_count() == 0)
            return false;
        for (unsigned int index = 0; index < report.assertion_count(); ++index)
            if (report.assertion_status(index) != signal_synth::ecg_assertion_passed || report.assertion_condition(index) == signal_synth::ecg_condition_count || report.assertion_code(index) == signal_synth::ecg_assertion_code_count || !report.assertion_name(index)[0] || !report.assertion_unit(index)[0])
                return false;
        return true;
    }

    bool generated_phenotype_passes(const signal_synth::ecg_scenario_engine& engine, const signal_synth::ecg_qa_scenario& scenario)
    {
        signal_synth::clinical_ecg_record record;
        signal_synth::ecg_scenario_report report;
        const bool generated = engine.generate(scenario, 5000, record, report);
        const bool passed = generated && report.success() && all_assertions_passed(report);
        if (!passed)
        {
            std::cerr << "Phenotype assertion failure for condition " << (scenario.condition_count() ? static_cast<int>(scenario.condition(0)) : -1) << '\n';
            for (unsigned int index = 0; index < report.assertion_count(); ++index)
                if (report.assertion_status(index) != signal_synth::ecg_assertion_passed)
                    std::cerr << "  " << report.assertion_name(index) << " measured=" << report.assertion_measured_value(index) << " expected=[" << report.assertion_minimum(index) << ", " << report.assertion_maximum(index) << "]\n";
        }
        return passed;
    }
}

int main()
{
    bool ok = true;
    const signal_synth::ecg_condition_info* catalog = signal_synth::ecg_condition_catalog();
    unsigned int diagnostic_count = 0;
    unsigned int form_count = 0;
    unsigned int rhythm_count = 0;
    std::set<std::string> scp_codes;
    bool catalog_order = catalog != 0;
    for (unsigned int index = 0; index < signal_synth::ecg_condition_catalog_size(); ++index)
    {
        const signal_synth::ecg_condition_info& info = catalog[index];
        catalog_order &= info.code == static_cast<signal_synth::ecg_condition_code>(index) && info.scp_code && info.scp_code[0] && info.name && info.name[0] && signal_synth::find_ecg_condition(info.code) == &info && signal_synth::find_ecg_condition(info.scp_code) == &info;
        scp_codes.insert(info.scp_code);
        diagnostic_count += info.diagnostic_statement ? 1U : 0U;
        form_count += info.form_statement ? 1U : 0U;
        rhythm_count += info.rhythm_statement ? 1U : 0U;
    }
    ok &= check(signal_synth::ecg_condition_catalog_size() == 71 && scp_codes.size() == 71 && catalog_order, "complete_unique_ordered_scp_catalog");
    ok &= check(diagnostic_count == 44 && form_count == 19 && rhythm_count == 12, "scp_statement_classification_counts");
    ok &= check(!signal_synth::find_ecg_condition("UNKNOWN") && !signal_synth::find_ecg_condition(static_cast<signal_synth::ecg_condition_code>(999)), "unknown_condition_lookup");
    ok &= check(signal_synth::find_ecg_condition(signal_synth::ecg_condition_lvh)->support == signal_synth::ecg_support_catalog_only && signal_synth::find_ecg_condition(signal_synth::ecg_condition_crbbb)->support == signal_synth::ecg_support_parameterized && signal_synth::find_ecg_condition(signal_synth::ecg_condition_afib)->support == signal_synth::ecg_support_native, "support_levels_are_explicit");

    signal_synth::ecg_qa_scenario first;
    first.add_condition(signal_synth::ecg_condition_pvc, 0.8);
    first.add_condition(signal_synth::ecg_condition_bigu);
    first.set_seed(1234);
    first.set_heart_rate_bpm(72.0);
    signal_synth::ecg_qa_scenario second;
    second.add_condition(signal_synth::ecg_condition_bigu);
    second.add_condition(signal_synth::ecg_condition_pvc, 0.8);
    second.set_heart_rate_bpm(72.0);
    second.set_seed(1234);
    ok &= check(first.fingerprint() == second.fingerprint() && first.schema_version() == 1 && signal_synth::ecg_scenario_engine_version() == 2, "fingerprint_is_order_independent_and_versioned");
    signal_synth::ecg_qa_scenario changed = first;
    changed.set_seed(1235);
    ok &= check(first.fingerprint() != changed.fingerprint(), "fingerprint_covers_generation_seed");
    ok &= check(!first.add_condition(signal_synth::ecg_condition_pac, 0.0) && !first.set_sampling_rate_hz(20) && !first.set_heart_rate_bpm(std::numeric_limits<double>::infinity()) && !first.set_rr_variability_seconds(-1.0), "invalid_scenario_parameters_are_rejected");

    signal_synth::ecg_scenario_engine engine;
    signal_synth::ecg_scenario_report report;
    ok &= check(engine.validate(first, report) && report.success() && report.scenario_fingerprint() == first.fingerprint() && report.engine_version() == signal_synth::ecg_scenario_engine_version(), "valid_bigeminy_scenario");
    const int prc_index = effective_index(report, signal_synth::ecg_condition_prc);
    ok &= check(prc_index >= 0 && report.condition_was_inferred(static_cast<unsigned int>(prc_index)) && report.effective_condition_severity(static_cast<unsigned int>(prc_index)) == 0.8, "pvc_implies_premature_complex");

    signal_synth::clinical_ecg_config compiled;
    ok &= check(engine.compile(first, compiled, report) && compiled.rhythm.rhythm == signal_synth::clinical_rhythm_sinus && compiled.scenario.premature_origin == signal_synth::clinical_origin_pvc && compiled.scenario.premature_every_n_beats == 2 && std::fabs(compiled.scenario.premature_coupling_ratio - 0.65) < 1e-12 && compiled.rhythm.seed == 1234, "scenario_compiles_to_clinical_config");

    signal_synth::ecg_qa_scenario unsupported;
    unsupported.add_condition(signal_synth::ecg_condition_lvh);
    signal_synth::clinical_ecg_config preserved;
    preserved.rhythm.heart_rate_bpm = 91.0;
    ok &= check(!engine.compile(unsupported, preserved, report) && !report.success() && report_has_issue(report, signal_synth::ecg_issue_unsupported_condition) && preserved.rhythm.heart_rate_bpm == 91.0, "catalog_only_condition_fails_transactionally");

    signal_synth::ecg_qa_scenario fidelity;
    fidelity.add_condition(signal_synth::ecg_condition_crbbb);
    fidelity.set_fidelity_policy(signal_synth::ecg_fidelity_native_only);
    ok &= check(!engine.validate(fidelity, report) && report_has_issue(report, signal_synth::ecg_issue_fidelity_policy), "native_only_policy_rejects_parameterized_model");
    fidelity.set_fidelity_policy(signal_synth::ecg_fidelity_allow_parameterized);
    ok &= check(engine.compile(fidelity, compiled, report) && report_has_issue(report, signal_synth::ecg_issue_parameterized_condition) && compiled.rhythm.intraventricular_conduction == signal_synth::clinical_iv_rbbb, "parameterized_model_is_audited");

    signal_synth::ecg_qa_scenario conflict;
    conflict.add_condition(signal_synth::ecg_condition_norm);
    conflict.add_condition(signal_synth::ecg_condition_afib);
    ok &= check(!engine.validate(conflict, report) && report_has_issue(report, signal_synth::ecg_issue_condition_conflict), "normal_abnormal_conflict");
    conflict.clear_conditions();
    conflict.add_condition(signal_synth::ecg_condition_afib);
    conflict.add_condition(signal_synth::ecg_condition_sr);
    ok &= check(!engine.validate(conflict, report) && report_has_issue(report, signal_synth::ecg_issue_condition_conflict), "primary_rhythm_conflict");
    conflict.clear_conditions();
    conflict.add_condition(signal_synth::ecg_condition_afib);
    conflict.add_condition(signal_synth::ecg_condition_pvc);
    ok &= check(!engine.validate(conflict, report) && report_has_issue(report, signal_synth::ecg_issue_condition_conflict), "unsupported_rhythm_scenario_combination");

    signal_synth::ecg_qa_scenario fixed_severity;
    fixed_severity.add_condition(signal_synth::ecg_condition_afib, 0.5);
    ok &= check(!engine.validate(fixed_severity, report) && report_has_issue(report, signal_synth::ecg_issue_invalid_parameter), "unused_severity_is_rejected");

    signal_synth::ecg_qa_scenario second_degree;
    second_degree.add_condition(signal_synth::ecg_condition_2avb);
    ok &= check(!engine.validate(second_degree, report) && report_has_issue(report, signal_synth::ecg_issue_missing_requirement), "second_degree_requires_subtype");
    second_degree.set_second_degree_av_pattern(signal_synth::ecg_second_degree_mobitz_i);
    ok &= check(engine.compile(second_degree, compiled, report) && compiled.rhythm.av_conduction == signal_synth::clinical_av_mobitz_i, "second_degree_subtype_compiles");
    signal_synth::ecg_qa_scenario unused_subtype;
    unused_subtype.add_condition(signal_synth::ecg_condition_sr);
    unused_subtype.set_second_degree_av_pattern(signal_synth::ecg_second_degree_mobitz_ii);
    ok &= check(!engine.validate(unused_subtype, report) && report_has_issue(report, signal_synth::ecg_issue_invalid_parameter), "unused_second_degree_subtype_is_rejected");
    signal_synth::ecg_qa_scenario ignored_ectopy;
    ignored_ectopy.add_condition(signal_synth::ecg_condition_sr);
    ignored_ectopy.set_ectopic_every_n_beats(4);
    ok &= check(!engine.validate(ignored_ectopy, report) && report_has_issue(report, signal_synth::ecg_issue_missing_requirement), "ectopy_cadence_requires_origin");
    signal_synth::ecg_qa_scenario av_ectopy;
    av_ectopy.add_condition(signal_synth::ecg_condition_1avb);
    av_ectopy.add_condition(signal_synth::ecg_condition_pvc);
    ok &= check(!engine.validate(av_ectopy, report) && report_has_issue(report, signal_synth::ecg_issue_condition_conflict), "ignored_av_ectopy_composition_is_rejected");

    signal_synth::ecg_qa_scenario tachycardia;
    tachycardia.add_condition(signal_synth::ecg_condition_stach);
    tachycardia.set_heart_rate_bpm(80.0);
    ok &= check(!engine.validate(tachycardia, report) && report_has_issue(report, signal_synth::ecg_issue_invalid_parameter), "condition_parameter_invariant");
    tachycardia.set_heart_rate_bpm(120.0);
    ok &= check(engine.compile(tachycardia, compiled, report) && compiled.rhythm.heart_rate_bpm == 120.0, "explicit_condition_parameter_compiles");
    signal_synth::ecg_qa_scenario unlabeled_tachycardia;
    unlabeled_tachycardia.add_condition(signal_synth::ecg_condition_sr);
    unlabeled_tachycardia.set_heart_rate_bpm(120.0);
    ok &= check(!engine.validate(unlabeled_tachycardia, report) && report_has_issue(report, signal_synth::ecg_issue_missing_requirement), "sinus_rate_requires_matching_statement");
    signal_synth::ecg_qa_scenario tachy_block;
    tachy_block.add_condition(signal_synth::ecg_condition_stach);
    tachy_block.add_condition(signal_synth::ecg_condition_2avb);
    tachy_block.set_heart_rate_bpm(120.0);
    tachy_block.set_second_degree_av_pattern(signal_synth::ecg_second_degree_mobitz_ii);
    ok &= check(engine.compile(tachy_block, compiled, report) && compiled.rhythm.atrial_rate_bpm == 120.0 && compiled.rhythm.av_conduction == signal_synth::clinical_av_mobitz_ii, "sinus_rate_drives_av_block_atrial_timeline");
    signal_synth::ecg_qa_scenario flutter;
    flutter.add_condition(signal_synth::ecg_condition_aflt);
    flutter.set_heart_rate_bpm(75.0);
    ok &= check(engine.compile(flutter, compiled, report) && compiled.rhythm.flutter_conduction_ratio == 4 && compiled.rhythm.atrial_rate_bpm == 300.0, "flutter_rate_compiles_to_atrial_rate_and_ratio");
    flutter.set_rr_variability_seconds(0.1);
    ok &= check(!engine.validate(flutter, report) && report_has_issue(report, signal_synth::ecg_issue_invalid_parameter), "unused_timeline_variability_is_rejected");

    signal_synth::ecg_qa_scenario atrial_fibrillation;
    atrial_fibrillation.add_condition(signal_synth::ecg_condition_afib);
    atrial_fibrillation.set_seed(987654321);
    atrial_fibrillation.set_rr_variability_seconds(0.12);
    signal_synth::clinical_ecg_record generated_a;
    signal_synth::clinical_ecg_record generated_b;
    ok &= check(engine.generate(atrial_fibrillation, 5000, generated_a, report) && report.success() && report.phenotype_passed() && report.assertion_count() == 3 && report.generated_sample_count() == 5000 && report.run_fingerprint() != report.scenario_fingerprint() && generated_a.lead_count() == 12 && generated_a.atrial_event_count() == 0 && generated_a.beat_count() > 0, "high_level_scenario_generation");
    signal_synth::ecg_scenario_report repeated_report;
    engine.generate(atrial_fibrillation, 5000, generated_b, repeated_report);
    ok &= check(same_signal(generated_a, generated_b) && report.scenario_fingerprint() == repeated_report.scenario_fingerprint() && report.run_fingerprint() == repeated_report.run_fingerprint(), "scenario_generation_is_reproducible");

    signal_synth::clinical_ecg_record preserved_record = generated_a;
    ok &= check(!engine.generate(unsupported, 5000, preserved_record, report) && same_signal(generated_a, preserved_record), "failed_scenario_generation_preserves_output");
    ok &= check(!engine.generate(atrial_fibrillation, 0, preserved_record, report) && report_has_issue(report, signal_synth::ecg_issue_invalid_parameter) && same_signal(generated_a, preserved_record), "zero_length_generation_is_transactional");

    signal_synth::ecg_qa_scenario normal;
    normal.add_condition(signal_synth::ecg_condition_norm);
    signal_synth::ecg_qa_scenario sinus;
    sinus.add_condition(signal_synth::ecg_condition_sr);
    signal_synth::ecg_qa_scenario flutter_assertions;
    flutter_assertions.add_condition(signal_synth::ecg_condition_aflt);
    signal_synth::ecg_qa_scenario svt;
    svt.add_condition(signal_synth::ecg_condition_svtac);
    signal_synth::ecg_qa_scenario sinus_tachycardia;
    sinus_tachycardia.add_condition(signal_synth::ecg_condition_stach);
    signal_synth::ecg_qa_scenario sinus_bradycardia;
    sinus_bradycardia.add_condition(signal_synth::ecg_condition_sbrad);
    signal_synth::ecg_qa_scenario sinus_arrhythmia;
    sinus_arrhythmia.add_condition(signal_synth::ecg_condition_sarrh);
    signal_synth::ecg_qa_scenario paced;
    paced.add_condition(signal_synth::ecg_condition_pace);
    signal_synth::ecg_qa_scenario tachycardia_with_pvc;
    tachycardia_with_pvc.add_condition(signal_synth::ecg_condition_stach);
    tachycardia_with_pvc.add_condition(signal_synth::ecg_condition_pvc);
    tachycardia_with_pvc.set_heart_rate_bpm(120.0);
    ok &= check(generated_phenotype_passes(engine, normal) && generated_phenotype_passes(engine, sinus) && generated_phenotype_passes(engine, atrial_fibrillation) && generated_phenotype_passes(engine, flutter_assertions) && generated_phenotype_passes(engine, svt) && generated_phenotype_passes(engine, sinus_tachycardia) && generated_phenotype_passes(engine, sinus_bradycardia) && generated_phenotype_passes(engine, sinus_arrhythmia) && generated_phenotype_passes(engine, paced) && generated_phenotype_passes(engine, tachycardia_with_pvc), "rhythm_phenotype_assertions");

    signal_synth::ecg_qa_scenario pac_assertions;
    pac_assertions.add_condition(signal_synth::ecg_condition_pac);
    signal_synth::ecg_qa_scenario pvc_assertions;
    pvc_assertions.add_condition(signal_synth::ecg_condition_pvc);
    signal_synth::ecg_qa_scenario bigeminy;
    bigeminy.add_condition(signal_synth::ecg_condition_pvc);
    bigeminy.add_condition(signal_synth::ecg_condition_bigu);
    signal_synth::ecg_qa_scenario trigeminy;
    trigeminy.add_condition(signal_synth::ecg_condition_pac);
    trigeminy.add_condition(signal_synth::ecg_condition_trigu);
    ok &= check(generated_phenotype_passes(engine, pac_assertions) && generated_phenotype_passes(engine, pvc_assertions) && generated_phenotype_passes(engine, bigeminy) && generated_phenotype_passes(engine, trigeminy), "ectopy_phenotype_assertions");
    signal_synth::clinical_ecg_record too_short_record;
    signal_synth::ecg_scenario_report too_short_report;
    ok &= check(engine.generate(pvc_assertions, 400, too_short_record, too_short_report) && too_short_report.success() && !too_short_report.phenotype_passed() && too_short_report.assertion_count() > 0, "failed_phenotype_assertions_preserve_generated_waveform");

    signal_synth::ecg_qa_scenario first_degree_assertions;
    first_degree_assertions.add_condition(signal_synth::ecg_condition_1avb);
    signal_synth::ecg_qa_scenario prolonged_pr;
    prolonged_pr.add_condition(signal_synth::ecg_condition_lpr);
    signal_synth::ecg_qa_scenario mobitz_i;
    mobitz_i.add_condition(signal_synth::ecg_condition_2avb);
    mobitz_i.set_second_degree_av_pattern(signal_synth::ecg_second_degree_mobitz_i);
    signal_synth::ecg_qa_scenario mobitz_ii;
    mobitz_ii.add_condition(signal_synth::ecg_condition_2avb);
    mobitz_ii.set_second_degree_av_pattern(signal_synth::ecg_second_degree_mobitz_ii);
    signal_synth::ecg_qa_scenario complete_block;
    complete_block.add_condition(signal_synth::ecg_condition_3avb);
    signal_synth::ecg_qa_scenario right_bundle;
    right_bundle.add_condition(signal_synth::ecg_condition_crbbb);
    signal_synth::ecg_qa_scenario left_bundle;
    left_bundle.add_condition(signal_synth::ecg_condition_clbbb);
    signal_synth::ecg_qa_scenario long_qt;
    long_qt.add_condition(signal_synth::ecg_condition_lngqt);
    ok &= check(generated_phenotype_passes(engine, first_degree_assertions) && generated_phenotype_passes(engine, prolonged_pr) && generated_phenotype_passes(engine, mobitz_i) && generated_phenotype_passes(engine, mobitz_ii) && generated_phenotype_passes(engine, complete_block) && generated_phenotype_passes(engine, right_bundle) && generated_phenotype_passes(engine, left_bundle) && generated_phenotype_passes(engine, long_qt) && generated_phenotype_passes(engine, tachy_block), "conduction_phenotype_assertions");

    std::cout << (ok ? "All ECG scenario tests passed.\n" : "ECG scenario test failure.\n");
    return ok ? 0 : 1;
}
