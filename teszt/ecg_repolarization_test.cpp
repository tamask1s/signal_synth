#include "../src/ecg_scenario.h"
#include "../src/clinical_ecg.h"

#include <cmath>
#include <iostream>

namespace
{
    const signal_synth::ecg_condition_code ischemia_conditions[] = {
        signal_synth::ecg_condition_isc,
        signal_synth::ecg_condition_iscal,
        signal_synth::ecg_condition_iscin,
        signal_synth::ecg_condition_iscil,
        signal_synth::ecg_condition_iscas,
        signal_synth::ecg_condition_iscla,
        signal_synth::ecg_condition_iscan};

    const signal_synth::ecg_condition_code stt_conditions[] = {
        signal_synth::ecg_condition_ndt,
        signal_synth::ecg_condition_nst,
        signal_synth::ecg_condition_dig,
        signal_synth::ecg_condition_lngqt,
        signal_synth::ecg_condition_aneur,
        signal_synth::ecg_condition_el,
        signal_synth::ecg_condition_std,
        signal_synth::ecg_condition_lowt,
        signal_synth::ecg_condition_nt,
        signal_synth::ecg_condition_invt,
        signal_synth::ecg_condition_tab,
        signal_synth::ecg_condition_ste};

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
        scenario.set_seed(8000 + static_cast<unsigned int>(code));
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
    for (unsigned int index = 0; index < sizeof(ischemia_conditions) / sizeof(ischemia_conditions[0]); ++index)
        support_levels = support_levels && signal_synth::find_ecg_condition(ischemia_conditions[index])->support == signal_synth::ecg_support_parameterized;
    for (unsigned int index = 0; index < sizeof(stt_conditions) / sizeof(stt_conditions[0]); ++index)
        support_levels = support_levels && signal_synth::find_ecg_condition(stt_conditions[index])->support == signal_synth::ecg_support_parameterized;
    ok &= check(support_levels, "all_ischemia_stt_conditions_are_parameterized");
    ok &= check(signal_synth::ecg_scenario_engine_version() == 6, "repolarization_semantics_increment_engine_version");

    bool default_phenotypes = true;
    bool mild_phenotypes = true;
    bool assertion_ownership = true;
    for (unsigned int index = 0; index < sizeof(ischemia_conditions) / sizeof(ischemia_conditions[0]); ++index)
    {
        signal_synth::ecg_scenario_report generated_report;
        default_phenotypes = generated_phenotype_passes(engine, make_scenario(ischemia_conditions[index]), 6000, &generated_report) && default_phenotypes;
        mild_phenotypes = generated_phenotype_passes(engine, make_scenario(ischemia_conditions[index], 0.1), 6000) && mild_phenotypes;
        assertion_ownership = assertion_ownership && report_has_assertion(generated_report, signal_synth::ecg_assert_st_deviation) && report_has_assertion(generated_report, signal_synth::ecg_assert_st_lead_count) && report_has_assertion(generated_report, signal_synth::ecg_assert_t_amplitude) && report_has_assertion(generated_report, signal_synth::ecg_assert_t_lead_count);
    }
    for (unsigned int index = 0; index < sizeof(stt_conditions) / sizeof(stt_conditions[0]); ++index)
    {
        signal_synth::ecg_scenario_report generated_report;
        default_phenotypes = generated_phenotype_passes(engine, make_scenario(stt_conditions[index]), 6000, &generated_report) && default_phenotypes;
        mild_phenotypes = generated_phenotype_passes(engine, make_scenario(stt_conditions[index], 0.1), 6000) && mild_phenotypes;
        if (stt_conditions[index] == signal_synth::ecg_condition_lngqt)
            assertion_ownership = assertion_ownership && report_has_assertion(generated_report, signal_synth::ecg_assert_qtc_interval);
        else if (stt_conditions[index] == signal_synth::ecg_condition_aneur)
            assertion_ownership = assertion_ownership && report_has_assertion(generated_report, signal_synth::ecg_assert_q_wave_amplitude) && report_has_assertion(generated_report, signal_synth::ecg_assert_st_deviation);
        else if (stt_conditions[index] == signal_synth::ecg_condition_el)
            assertion_ownership = assertion_ownership && report_has_assertion(generated_report, signal_synth::ecg_assert_t_amplitude) && report_has_assertion(generated_report, signal_synth::ecg_assert_t_duration);
        else if (stt_conditions[index] == signal_synth::ecg_condition_nst || stt_conditions[index] == signal_synth::ecg_condition_dig)
            assertion_ownership = assertion_ownership && report_has_assertion(generated_report, signal_synth::ecg_assert_st_deviation) && report_has_assertion(generated_report, signal_synth::ecg_assert_st_slope);
        else
            assertion_ownership = assertion_ownership && generated_report.assertion_count() > 0;
    }
    ok &= check(default_phenotypes, "default_ischemia_stt_phenotypes");
    ok &= check(mild_phenotypes, "mild_ischemia_stt_phenotypes");
    ok &= check(assertion_ownership, "condition_specific_assertion_contract");

    bool monotonic = true;
    for (unsigned int index = 0; index < sizeof(ischemia_conditions) / sizeof(ischemia_conditions[0]); ++index)
    {
        signal_synth::clinical_ecg_config mild;
        signal_synth::clinical_ecg_config full;
        monotonic = monotonic && engine.compile(make_scenario(ischemia_conditions[index], 0.1), mild, report) && engine.compile(make_scenario(ischemia_conditions[index]), full, report) && std::fabs(full.morphology.st_j_amplitude_mv) > std::fabs(mild.morphology.st_j_amplitude_mv) && std::fabs(full.morphology.t_amplitude_mv) > std::fabs(mild.morphology.t_amplitude_mv);
    }
    const signal_synth::ecg_condition_code increasing_st[] = {signal_synth::ecg_condition_nst, signal_synth::ecg_condition_dig, signal_synth::ecg_condition_std, signal_synth::ecg_condition_ste, signal_synth::ecg_condition_aneur};
    for (unsigned int index = 0; index < sizeof(increasing_st) / sizeof(increasing_st[0]); ++index)
    {
        signal_synth::clinical_ecg_config mild;
        signal_synth::clinical_ecg_config full;
        monotonic = monotonic && engine.compile(make_scenario(increasing_st[index], 0.1), mild, report) && engine.compile(make_scenario(increasing_st[index]), full, report) && std::fabs(full.morphology.st_j_amplitude_mv) > std::fabs(mild.morphology.st_j_amplitude_mv);
    }
    signal_synth::clinical_ecg_config mild_invt;
    signal_synth::clinical_ecg_config full_invt;
    signal_synth::clinical_ecg_config mild_el;
    signal_synth::clinical_ecg_config full_el;
    monotonic = monotonic && engine.compile(make_scenario(signal_synth::ecg_condition_invt, 0.1), mild_invt, report) && engine.compile(make_scenario(signal_synth::ecg_condition_invt), full_invt, report) && std::fabs(full_invt.morphology.t_amplitude_mv) > std::fabs(mild_invt.morphology.t_amplitude_mv);
    monotonic = monotonic && engine.compile(make_scenario(signal_synth::ecg_condition_el, 0.1), mild_el, report) && engine.compile(make_scenario(signal_synth::ecg_condition_el), full_el, report) && full_el.morphology.t_amplitude_mv > mild_el.morphology.t_amplitude_mv && full_el.timing.t_duration_ms < mild_el.timing.t_duration_ms;
    ok &= check(monotonic, "severity_is_monotonic");

    signal_synth::ecg_qa_scenario territorial = make_scenario(signal_synth::ecg_condition_iscal);
    engine.validate(territorial, report);
    ok &= check(effective_index(report, signal_synth::ecg_condition_std) >= 0 && effective_index(report, signal_synth::ecg_condition_invt) >= 0, "territorial_ischemia_implications");
    signal_synth::ecg_qa_scenario digitalis = make_scenario(signal_synth::ecg_condition_dig);
    engine.validate(digitalis, report);
    ok &= check(effective_index(report, signal_synth::ecg_condition_std) >= 0, "digitalis_implies_st_depression");
    signal_synth::ecg_qa_scenario aneurysm = make_scenario(signal_synth::ecg_condition_aneur);
    engine.validate(aneurysm, report);
    ok &= check(effective_index(report, signal_synth::ecg_condition_ste) >= 0 && effective_index(report, signal_synth::ecg_condition_qwave) >= 0 && effective_index(report, signal_synth::ecg_condition_abqrs) >= 0, "aneurysm_proxy_implications");
    signal_synth::ecg_qa_scenario electrolyte = make_scenario(signal_synth::ecg_condition_el);
    engine.validate(electrolyte, report);
    ok &= check(effective_index(report, signal_synth::ecg_condition_tab) >= 0, "electrolyte_proxy_implies_t_abnormality");

    signal_synth::ecg_qa_scenario native_only = territorial;
    native_only.set_fidelity_policy(signal_synth::ecg_fidelity_native_only);
    ok &= check(!engine.validate(native_only, report) && report_has_issue(report, signal_synth::ecg_issue_fidelity_policy), "parameterized_fidelity_is_audited");
    signal_synth::ecg_qa_scenario family_conflict = territorial;
    family_conflict.add_condition(signal_synth::ecg_condition_ste);
    ok &= check(!engine.validate(family_conflict, report) && report_has_issue(report, signal_synth::ecg_issue_condition_conflict), "family_composition_is_rejected");
    signal_synth::ecg_qa_scenario infarction_conflict = territorial;
    infarction_conflict.add_condition(signal_synth::ecg_condition_ami);
    ok &= check(!engine.validate(infarction_conflict, report) && report_has_issue(report, signal_synth::ecg_issue_condition_conflict), "infarction_composition_is_rejected");
    signal_synth::ecg_qa_scenario rhythm_conflict = territorial;
    rhythm_conflict.add_condition(signal_synth::ecg_condition_afib);
    ok &= check(!engine.validate(rhythm_conflict, report) && report_has_issue(report, signal_synth::ecg_issue_condition_conflict), "complex_rhythm_composition_is_rejected");

    signal_synth::clinical_ecg_record first;
    signal_synth::clinical_ecg_record second;
    signal_synth::ecg_scenario_report first_report;
    signal_synth::ecg_scenario_report second_report;
    ok &= check(engine.generate(territorial, 6000, first, first_report) && engine.generate(territorial, 6000, second, second_report) && same_signal(first, second) && first_report.run_fingerprint() == second_report.run_fingerprint(), "ischemia_generation_is_reproducible");
    signal_synth::clinical_ecg_record anterior_record;
    signal_synth::clinical_ecg_record inferior_record;
    signal_synth::ecg_scenario_report localization_report;
    signal_synth::ecg_qa_scenario anterior = make_scenario(signal_synth::ecg_condition_iscan);
    signal_synth::ecg_qa_scenario inferior = make_scenario(signal_synth::ecg_condition_iscin);
    anterior.set_seed(8888);
    inferior.set_seed(8888);
    ok &= check(engine.generate(anterior, 6000, anterior_record, localization_report) && engine.generate(inferior, 6000, inferior_record, localization_report) && !same_signal(anterior_record, inferior_record), "anterior_and_inferior_sources_are_distinct");
    signal_synth::clinical_ecg_record preserved = first;
    ok &= check(!engine.generate(family_conflict, 6000, preserved, report) && same_signal(first, preserved), "failed_generation_preserves_output");

    bool sampling_rates = true;
    for (unsigned int index = 0; index < sizeof(ischemia_conditions) / sizeof(ischemia_conditions[0]); ++index)
        sampling_rates = generated_phenotype_passes(engine, make_scenario(ischemia_conditions[index], 1.0, 100), 1200) && generated_phenotype_passes(engine, make_scenario(ischemia_conditions[index], 1.0, 1000), 12000) && sampling_rates;
    for (unsigned int index = 0; index < sizeof(stt_conditions) / sizeof(stt_conditions[0]); ++index)
        sampling_rates = generated_phenotype_passes(engine, make_scenario(stt_conditions[index], 1.0, 100), 1200) && generated_phenotype_passes(engine, make_scenario(stt_conditions[index], 1.0, 1000), 12000) && sampling_rates;
    ok &= check(sampling_rates, "assertions_across_sampling_rates");

    std::cout << (ok ? "All ECG ischemia/ST-T tests passed.\n" : "ECG ischemia/ST-T test failure.\n");
    return ok ? 0 : 1;
}
