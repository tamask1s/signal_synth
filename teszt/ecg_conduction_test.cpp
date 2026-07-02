#include "../src/clinical_ecg.h"
#include "../src/ecg_scenario.h"

#include <cmath>
#include <iostream>

namespace
{
    bool check(bool condition, const char* name)
    {
        std::cout << (condition ? "PASS " : "FAIL ") << name << '\n';
        return condition;
    }

    bool report_has_issue(const signal_synth::ecg_scenario_report& report, signal_synth::ecg_scenario_issue_code code)
    {
        for (unsigned int index = 0; index < report.issue_count(); ++index)
            if (report.issue_code(index) == code)
                return true;
        return false;
    }

    bool assertion_passed(const signal_synth::ecg_scenario_report& report, signal_synth::ecg_condition_code condition, signal_synth::ecg_phenotype_assertion_code code)
    {
        for (unsigned int index = 0; index < report.assertion_count(); ++index)
            if (report.assertion_condition(index) == condition && report.assertion_code(index) == code && report.assertion_status(index) == signal_synth::ecg_assertion_passed)
                return true;
        return false;
    }

    bool all_assertions_passed(const signal_synth::ecg_scenario_report& report)
    {
        if (!report.success() || !report.phenotype_passed() || report.assertion_count() == 0)
            return false;
        for (unsigned int index = 0; index < report.assertion_count(); ++index)
            if (report.assertion_status(index) != signal_synth::ecg_assertion_passed || !report.assertion_name(index)[0] || !report.assertion_unit(index)[0])
                return false;
        return true;
    }

    bool same_signal(const signal_synth::clinical_ecg_record& left, const signal_synth::clinical_ecg_record& right)
    {
        if (left.sample_count() != right.sample_count() || left.lead_count() != right.lead_count())
            return false;
        for (unsigned int lead = 0; lead < left.lead_count(); ++lead)
            for (unsigned int sample = 0; sample < left.sample_count(); ++sample)
                if (left.lead_data(lead)[sample] != right.lead_data(lead)[sample])
                    return false;
        return true;
    }

    signal_synth::ecg_qa_scenario make_scenario(signal_synth::ecg_condition_code code, double severity, unsigned int rate)
    {
        signal_synth::ecg_qa_scenario scenario;
        scenario.add_condition(code, severity);
        scenario.set_heart_rate_bpm(70.0);
        scenario.set_sampling_rate_hz(rate);
        scenario.set_seed(0xadc000ULL + static_cast<unsigned int>(code) * 17ULL + static_cast<unsigned int>(rate));
        return scenario;
    }

    bool generate_and_check(signal_synth::ecg_condition_code code, double severity, unsigned int rate)
    {
        signal_synth::ecg_qa_scenario scenario = make_scenario(code, severity, rate);
        signal_synth::clinical_ecg_record record;
        signal_synth::ecg_scenario_report report;
        signal_synth::ecg_scenario_engine engine;
        const bool generated = engine.generate(scenario, rate * 8U, record, report);
        bool ok = generated && all_assertions_passed(report) && record.lead_count() == signal_synth::clinical_lead_count && record.source_count() == signal_synth::clinical_source_count && record.beat_count() > 2;
        if (code == signal_synth::ecg_condition_lafb || code == signal_synth::ecg_condition_lpfb)
            ok = ok && assertion_passed(report, code, signal_synth::ecg_assert_frontal_axis) && assertion_passed(report, code, signal_synth::ecg_assert_lateral_qrs_polarity) && assertion_passed(report, code, signal_synth::ecg_assert_inferior_qrs_polarity);
        if (code == signal_synth::ecg_condition_irbbb || code == signal_synth::ecg_condition_ilbbb)
            ok = ok && assertion_passed(report, code, signal_synth::ecg_assert_qrs_duration) && assertion_passed(report, code, signal_synth::ecg_assert_terminal_v1_polarity);
        if (code == signal_synth::ecg_condition_ivcd)
            ok = ok && assertion_passed(report, code, signal_synth::ecg_assert_complete_bbb_exclusion);
        if (code == signal_synth::ecg_condition_wpw)
            ok = ok && assertion_passed(report, code, signal_synth::ecg_assert_pr_interval) && assertion_passed(report, code, signal_synth::ecg_assert_delta_wave);
        if (!ok)
        {
            const signal_synth::ecg_condition_info* info = signal_synth::find_ecg_condition(code);
            std::cerr << "Advanced conduction failure: " << (info ? info->scp_code : "?") << " severity=" << severity << " rate=" << rate << '\n';
            for (unsigned int index = 0; index < report.assertion_count(); ++index)
                if (report.assertion_status(index) != signal_synth::ecg_assertion_passed)
                    std::cerr << "  " << report.assertion_name(index) << " measured=" << report.assertion_measured_value(index) << " expected=[" << report.assertion_minimum(index) << ", " << report.assertion_maximum(index) << "]\n";
        }
        return ok;
    }
}

int main()
{
    bool ok = true;
    const signal_synth::ecg_condition_code conditions[] = {
        signal_synth::ecg_condition_lafb,
        signal_synth::ecg_condition_irbbb,
        signal_synth::ecg_condition_ivcd,
        signal_synth::ecg_condition_lpfb,
        signal_synth::ecg_condition_wpw,
        signal_synth::ecg_condition_ilbbb};
    for (signal_synth::ecg_condition_code code : conditions)
        ok &= check(signal_synth::find_ecg_condition(code)->support == signal_synth::ecg_support_parameterized, "advanced_conduction_condition_is_parameterized");

    for (signal_synth::ecg_condition_code code : conditions)
    {
        ok &= check(generate_and_check(code, 1.0, 100), "default_advanced_conduction_at_100_hz");
        ok &= check(generate_and_check(code, 1.0, 500), "default_advanced_conduction_at_500_hz");
        ok &= check(generate_and_check(code, 1.0, 1000), "default_advanced_conduction_at_1000_hz");
        ok &= check(generate_and_check(code, 0.3, 500), "mild_advanced_conduction_at_500_hz");
    }

    signal_synth::ecg_scenario_engine engine;
    signal_synth::ecg_scenario_report report;
    signal_synth::clinical_ecg_config mild;
    signal_synth::clinical_ecg_config full;
    for (signal_synth::ecg_condition_code code : conditions)
    {
        ok &= check(engine.compile(make_scenario(code, 0.3, 500), mild, report) && engine.compile(make_scenario(code, 1.0, 500), full, report), "advanced_conduction_compiles");
        if (code == signal_synth::ecg_condition_wpw)
            ok &= check(full.timing.qrs_duration_ms > mild.timing.qrs_duration_ms && full.timing.pr_interval_ms < mild.timing.pr_interval_ms && full.rhythm.preexcitation == signal_synth::clinical_preexcitation_wpw, "wpw_severity_is_monotonic");
        else
            ok &= check(full.timing.qrs_duration_ms > mild.timing.qrs_duration_ms && full.rhythm.intraventricular_conduction != signal_synth::clinical_iv_normal, "conduction_severity_is_monotonic");
    }

    signal_synth::ecg_qa_scenario conflict;
    conflict.add_condition(signal_synth::ecg_condition_lafb);
    conflict.add_condition(signal_synth::ecg_condition_wpw);
    ok &= check(!engine.validate(conflict, report) && report_has_issue(report, signal_synth::ecg_issue_condition_conflict), "advanced_conduction_modes_are_mutually_exclusive");
    conflict.clear_conditions();
    conflict.add_condition(signal_synth::ecg_condition_irbbb);
    conflict.add_condition(signal_synth::ecg_condition_afib);
    ok &= check(!engine.validate(conflict, report) && report_has_issue(report, signal_synth::ecg_issue_condition_conflict), "advanced_conduction_rejects_complex_rhythm");
    conflict.clear_conditions();
    conflict.add_condition(signal_synth::ecg_condition_ivcd);
    conflict.add_condition(signal_synth::ecg_condition_lvh);
    ok &= check(!engine.validate(conflict, report) && report_has_issue(report, signal_synth::ecg_issue_condition_conflict), "advanced_conduction_rejects_clean_morphology_composition");
    conflict.clear_conditions();
    conflict.add_condition(signal_synth::ecg_condition_wpw);
    conflict.add_condition(signal_synth::ecg_condition_1avb);
    ok &= check(!engine.validate(conflict, report) && report_has_issue(report, signal_synth::ecg_issue_condition_conflict), "advanced_conduction_rejects_av_block_composition");

    signal_synth::ecg_qa_scenario reproducible = make_scenario(signal_synth::ecg_condition_wpw, 1.0, 500);
    signal_synth::clinical_ecg_record first;
    signal_synth::clinical_ecg_record second;
    signal_synth::ecg_scenario_report first_report;
    signal_synth::ecg_scenario_report second_report;
    ok &= check(engine.generate(reproducible, 4000, first, first_report) && engine.generate(reproducible, 4000, second, second_report) && same_signal(first, second) && first_report.run_fingerprint() == second_report.run_fingerprint(), "advanced_conduction_generation_is_reproducible");
    signal_synth::ecg_qa_scenario changed = make_scenario(signal_synth::ecg_condition_wpw, 0.3, 500);
    ok &= check(reproducible.fingerprint() != changed.fingerprint(), "advanced_conduction_fingerprint_covers_severity");
    ok &= check(signal_synth::ecg_scenario_engine_version() == 9, "advanced_conduction_engine_identity");

    std::cout << (ok ? "All advanced conduction tests passed.\n" : "Advanced conduction test failure.\n");
    return ok ? 0 : 1;
}
