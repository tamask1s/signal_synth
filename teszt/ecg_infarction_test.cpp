#include "../src/ecg_scenario.h"
#include "../src/clinical_ecg.h"

#include <cmath>
#include <iostream>

namespace
{
    const signal_synth::ecg_condition_code infarction_conditions[] = {
        signal_synth::ecg_condition_imi,
        signal_synth::ecg_condition_asmi,
        signal_synth::ecg_condition_ilmi,
        signal_synth::ecg_condition_ami,
        signal_synth::ecg_condition_almi,
        signal_synth::ecg_condition_lmi,
        signal_synth::ecg_condition_iplmi,
        signal_synth::ecg_condition_ipmi,
        signal_synth::ecg_condition_pmi};

    const signal_synth::ecg_condition_code injury_conditions[] = {
        signal_synth::ecg_condition_injas,
        signal_synth::ecg_condition_injal,
        signal_synth::ecg_condition_injin,
        signal_synth::ecg_condition_injla,
        signal_synth::ecg_condition_injil};

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

    bool report_has_assertion(const signal_synth::ecg_scenario_report& report, signal_synth::ecg_phenotype_assertion_code code)
    {
        for (unsigned int index = 0; index < report.assertion_count(); ++index)
            if (report.assertion_code(index) == code)
                return true;
        return false;
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

    signal_synth::ecg_qa_scenario make_scenario(signal_synth::ecg_condition_code code, double severity = 1.0, unsigned int sampling_rate = 500)
    {
        signal_synth::ecg_qa_scenario scenario;
        scenario.add_condition(code, severity);
        scenario.set_sampling_rate_hz(sampling_rate);
        scenario.set_heart_rate_bpm(70.0);
        scenario.set_seed(7000 + static_cast<unsigned int>(code));
        return scenario;
    }

    bool generated_phenotype_passes(const signal_synth::ecg_scenario_engine& engine, const signal_synth::ecg_qa_scenario& scenario, unsigned int sample_count, signal_synth::ecg_scenario_report* captured = 0)
    {
        signal_synth::clinical_ecg_record record;
        signal_synth::ecg_scenario_report report;
        const bool generated = engine.generate(scenario, sample_count, record, report);
        const bool passed = generated && report.success() && all_assertions_passed(report);
        if (!passed)
        {
            const signal_synth::ecg_condition_info* info = scenario.condition_count() ? signal_synth::find_ecg_condition(scenario.condition(0)) : 0;
            std::cerr << "Phenotype assertion failure for " << (info ? info->scp_code : "unknown") << '\n';
            for (unsigned int index = 0; index < report.assertion_count(); ++index)
                if (report.assertion_status(index) != signal_synth::ecg_assertion_passed)
                    std::cerr << "  " << report.assertion_name(index) << " measured=" << report.assertion_measured_value(index) << " expected=[" << report.assertion_minimum(index) << ", " << report.assertion_maximum(index) << "]\n";
        }
        if (captured)
            *captured = report;
        return passed;
    }
}

int main()
{
    bool ok = true;
    signal_synth::ecg_scenario_engine engine;
    signal_synth::ecg_scenario_report report;

    bool support_levels = true;
    for (unsigned int index = 0; index < sizeof(infarction_conditions) / sizeof(infarction_conditions[0]); ++index)
        support_levels = support_levels && signal_synth::find_ecg_condition(infarction_conditions[index])->support == signal_synth::ecg_support_parameterized;
    for (unsigned int index = 0; index < sizeof(injury_conditions) / sizeof(injury_conditions[0]); ++index)
        support_levels = support_levels && signal_synth::find_ecg_condition(injury_conditions[index])->support == signal_synth::ecg_support_parameterized;
    ok &= check(support_levels, "all_infarction_injury_conditions_are_parameterized");
    ok &= check(signal_synth::ecg_scenario_engine_version() == 9, "engine_version_includes_current_waveform_semantics");

    bool default_phenotypes = true;
    bool mild_phenotypes = true;
    bool assertion_ownership = true;
    for (unsigned int index = 0; index < sizeof(infarction_conditions) / sizeof(infarction_conditions[0]); ++index)
    {
        signal_synth::ecg_scenario_report default_report;
        default_phenotypes = generated_phenotype_passes(engine, make_scenario(infarction_conditions[index]), 6000, &default_report) && default_phenotypes;
        mild_phenotypes = generated_phenotype_passes(engine, make_scenario(infarction_conditions[index], 0.1), 6000) && mild_phenotypes;
        if (infarction_conditions[index] != signal_synth::ecg_condition_pmi)
            assertion_ownership = assertion_ownership && report_has_assertion(default_report, signal_synth::ecg_assert_q_wave_amplitude) && report_has_assertion(default_report, signal_synth::ecg_assert_q_wave_duration) && report_has_assertion(default_report, signal_synth::ecg_assert_q_wave_lead_count);
        if (infarction_conditions[index] == signal_synth::ecg_condition_pmi || infarction_conditions[index] == signal_synth::ecg_condition_ipmi || infarction_conditions[index] == signal_synth::ecg_condition_iplmi)
            assertion_ownership = assertion_ownership && report_has_assertion(default_report, signal_synth::ecg_assert_posterior_reciprocal_r_amplitude) && report_has_assertion(default_report, signal_synth::ecg_assert_posterior_reciprocal_lead_count);
    }
    for (unsigned int index = 0; index < sizeof(injury_conditions) / sizeof(injury_conditions[0]); ++index)
    {
        signal_synth::ecg_scenario_report default_report;
        default_phenotypes = generated_phenotype_passes(engine, make_scenario(injury_conditions[index]), 6000, &default_report) && default_phenotypes;
        mild_phenotypes = generated_phenotype_passes(engine, make_scenario(injury_conditions[index], 0.1), 6000) && mild_phenotypes;
        assertion_ownership = assertion_ownership && report_has_assertion(default_report, signal_synth::ecg_assert_injury_st_deviation) && report_has_assertion(default_report, signal_synth::ecg_assert_injury_st_lead_count);
    }
    ok &= check(default_phenotypes, "default_infarction_injury_phenotypes");
    ok &= check(mild_phenotypes, "mild_infarction_injury_phenotypes");
    ok &= check(assertion_ownership, "condition_specific_assertion_contract");

    bool monotonic = true;
    for (unsigned int index = 0; index < sizeof(infarction_conditions) / sizeof(infarction_conditions[0]); ++index)
    {
        signal_synth::clinical_ecg_config mild;
        signal_synth::clinical_ecg_config full;
        const bool compiled = engine.compile(make_scenario(infarction_conditions[index], 0.1), mild, report) && engine.compile(make_scenario(infarction_conditions[index]), full, report);
        monotonic = monotonic && compiled;
        if (infarction_conditions[index] != signal_synth::ecg_condition_pmi)
            monotonic = monotonic && std::fabs(full.morphology.q_amplitude_mv) > std::fabs(mild.morphology.q_amplitude_mv) && full.timing.qrs_q_fraction > mild.timing.qrs_q_fraction && full.sources.gain[signal_synth::clinical_source_septal] > mild.sources.gain[signal_synth::clinical_source_septal];
        if (infarction_conditions[index] == signal_synth::ecg_condition_pmi || infarction_conditions[index] == signal_synth::ecg_condition_ipmi || infarction_conditions[index] == signal_synth::ecg_condition_iplmi)
            monotonic = monotonic && full.sources.gain[signal_synth::clinical_source_ventricular] > mild.sources.gain[signal_synth::clinical_source_ventricular];
    }
    for (unsigned int index = 0; index < sizeof(injury_conditions) / sizeof(injury_conditions[0]); ++index)
    {
        signal_synth::clinical_ecg_config mild;
        signal_synth::clinical_ecg_config full;
        monotonic = monotonic && engine.compile(make_scenario(injury_conditions[index], 0.1), mild, report) && engine.compile(make_scenario(injury_conditions[index]), full, report) && std::fabs(full.morphology.st_j_amplitude_mv) > std::fabs(mild.morphology.st_j_amplitude_mv) && full.sources.gain[signal_synth::clinical_source_septal] == 1.0 && full.sources.gain[signal_synth::clinical_source_repolarization] == 1.0;
    }
    ok &= check(monotonic, "territorial_severity_is_monotonic");

    signal_synth::ecg_qa_scenario inferior = make_scenario(signal_synth::ecg_condition_imi);
    engine.validate(inferior, report);
    const int inferior_qwave = effective_index(report, signal_synth::ecg_condition_qwave);
    const int inferior_abqrs = effective_index(report, signal_synth::ecg_condition_abqrs);
    signal_synth::ecg_qa_scenario posterior = make_scenario(signal_synth::ecg_condition_pmi);
    engine.validate(posterior, report);
    const int posterior_qwave = effective_index(report, signal_synth::ecg_condition_qwave);
    const int posterior_abqrs = effective_index(report, signal_synth::ecg_condition_abqrs);
    signal_synth::ecg_qa_scenario injury = make_scenario(signal_synth::ecg_condition_injin);
    engine.validate(injury, report);
    ok &= check(inferior_qwave >= 0 && inferior_abqrs >= 0 && posterior_qwave < 0 && posterior_abqrs >= 0 && effective_index(report, signal_synth::ecg_condition_qwave) < 0 && effective_index(report, signal_synth::ecg_condition_abqrs) < 0, "effective_condition_implications");

    signal_synth::ecg_qa_scenario native_only = inferior;
    native_only.set_fidelity_policy(signal_synth::ecg_fidelity_native_only);
    ok &= check(!engine.validate(native_only, report) && report_has_issue(report, signal_synth::ecg_issue_fidelity_policy), "parameterized_fidelity_is_audited");
    signal_synth::ecg_qa_scenario family_conflict = inferior;
    family_conflict.add_condition(signal_synth::ecg_condition_injin);
    ok &= check(!engine.validate(family_conflict, report) && report_has_issue(report, signal_synth::ecg_issue_condition_conflict), "family_composition_is_rejected");
    signal_synth::ecg_qa_scenario morphology_conflict = inferior;
    morphology_conflict.add_condition(signal_synth::ecg_condition_qwave);
    morphology_conflict.set_q_wave_territory(signal_synth::ecg_q_wave_inferior);
    ok &= check(!engine.validate(morphology_conflict, report) && report_has_issue(report, signal_synth::ecg_issue_condition_conflict), "explicit_q_wave_composition_is_rejected");
    signal_synth::ecg_qa_scenario hypertrophy_conflict = inferior;
    hypertrophy_conflict.add_condition(signal_synth::ecg_condition_lvh);
    ok &= check(!engine.validate(hypertrophy_conflict, report) && report_has_issue(report, signal_synth::ecg_issue_condition_conflict), "hypertrophy_composition_is_rejected");
    signal_synth::ecg_qa_scenario rhythm_conflict = injury;
    rhythm_conflict.add_condition(signal_synth::ecg_condition_afib);
    ok &= check(!engine.validate(rhythm_conflict, report) && report_has_issue(report, signal_synth::ecg_issue_condition_conflict), "complex_rhythm_composition_is_rejected");

    signal_synth::clinical_ecg_record first;
    signal_synth::clinical_ecg_record second;
    signal_synth::ecg_scenario_report first_report;
    signal_synth::ecg_scenario_report second_report;
    ok &= check(engine.generate(posterior, 6000, first, first_report) && engine.generate(posterior, 6000, second, second_report) && same_signal(first, second) && first_report.run_fingerprint() == second_report.run_fingerprint(), "posterior_generation_is_reproducible");
    signal_synth::clinical_ecg_record anteroseptal_record;
    signal_synth::clinical_ecg_record anterior_record;
    signal_synth::ecg_scenario_report localization_report;
    signal_synth::ecg_qa_scenario anteroseptal = make_scenario(signal_synth::ecg_condition_asmi);
    signal_synth::ecg_qa_scenario anterior = make_scenario(signal_synth::ecg_condition_ami);
    anteroseptal.set_seed(7777);
    anterior.set_seed(7777);
    ok &= check(engine.generate(anteroseptal, 6000, anteroseptal_record, localization_report) && engine.generate(anterior, 6000, anterior_record, localization_report) && !same_signal(anteroseptal_record, anterior_record), "anteroseptal_and_anterior_sources_are_distinct");
    signal_synth::clinical_ecg_record preserved = first;
    ok &= check(!engine.generate(family_conflict, 6000, preserved, report) && same_signal(first, preserved), "failed_generation_preserves_output");
    signal_synth::clinical_ecg_config preserved_config;
    preserved_config.rhythm.heart_rate_bpm = 91.0;
    ok &= check(!engine.compile(family_conflict, preserved_config, report) && preserved_config.rhythm.heart_rate_bpm == 91.0, "failed_compile_preserves_output");

    ok &= check(generated_phenotype_passes(engine, make_scenario(signal_synth::ecg_condition_imi, 1.0, 100), 1200) && generated_phenotype_passes(engine, make_scenario(signal_synth::ecg_condition_pmi, 1.0, 100), 1200) && generated_phenotype_passes(engine, make_scenario(signal_synth::ecg_condition_injal, 1.0, 1000), 12000), "territorial_assertions_across_sampling_rates");

    std::cout << (ok ? "All ECG infarction/injury tests passed.\n" : "ECG infarction/injury test failure.\n");
    return ok ? 0 : 1;
}
