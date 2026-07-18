#include "../src/ecg_scenario.h"
#include "../src/clinical_ecg.h"
#include "../src/ecg_export.h"
#include "../src/ecg_scenario_json.h"

#include <algorithm>
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

    const signal_synth::ecg_condition_code injury_rendering_conditions[] = {
        signal_synth::ecg_condition_nst,
        signal_synth::ecg_condition_dig,
        signal_synth::ecg_condition_isc,
        signal_synth::ecg_condition_iscal,
        signal_synth::ecg_condition_iscin,
        signal_synth::ecg_condition_iscil,
        signal_synth::ecg_condition_iscas,
        signal_synth::ecg_condition_iscla,
        signal_synth::ecg_condition_aneur,
        signal_synth::ecg_condition_iscan,
        signal_synth::ecg_condition_std,
        signal_synth::ecg_condition_ste,
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

    double source_scale(const signal_synth::clinical_ecg_record& record, unsigned int axis)
    {
        const double* data = record.source_data(signal_synth::clinical_source_injury, axis);
        double maximum = 0.0;
        for (unsigned int sample = 0; sample < record.sample_count(); ++sample)
            maximum = std::max(maximum, std::fabs(data[sample]));
        return maximum;
    }

    double boundary_curvature_ratio(const signal_synth::clinical_ecg_record& record, unsigned int axis, double time_seconds, double scale)
    {
        if (scale <= 1e-12)
            return 0.0;
        const double* data = record.source_data(signal_synth::clinical_source_injury, axis);
        const unsigned int center = static_cast<unsigned int>(std::llround(time_seconds * record.sampling_rate_hz()));
        double maximum = 0.0;
        for (unsigned int sample = center - 2; sample <= center + 2; ++sample)
            maximum = std::max(maximum, std::fabs(data[sample + 1] - 2.0 * data[sample] + data[sample - 1]) / scale);
        return maximum;
    }

    double j_step_ratio(const signal_synth::clinical_ecg_record& record, unsigned int axis, double time_seconds)
    {
        const double* data = record.source_data(signal_synth::clinical_source_injury, axis);
        const unsigned int center = static_cast<unsigned int>(std::ceil(time_seconds * record.sampling_rate_hz()));
        double local_scale = 0.0;
        for (unsigned int sample = center - 2; sample <= center + 2; ++sample)
            local_scale = std::max(local_scale, std::fabs(data[sample]));
        return local_scale > 1e-12 ? std::fabs(data[center] - data[center - 1]) / local_scale : 0.0;
    }

    bool injury_boundaries_are_smooth(const signal_synth::clinical_ecg_record& record)
    {
        const double j_curvature_limit = record.sampling_rate_hz() <= 100 ? 0.80 : record.sampling_rate_hz() <= 500 ? 0.08 : 0.025;
        const double j_step_limit = record.sampling_rate_hz() <= 100 ? 0.75 : 0.20;
        const double t_curvature_limit = record.sampling_rate_hz() <= 100 ? 0.04 : record.sampling_rate_hz() <= 500 ? 0.005 : 0.002;
        double scales[signal_synth::clinical_axis_count] = {};
        for (unsigned int axis = 0; axis < signal_synth::clinical_axis_count; ++axis)
            scales[axis] = source_scale(record, axis);
        for (unsigned int beat_index = 0; beat_index < record.beat_count(); ++beat_index)
        {
            const signal_synth::clinical_beat_annotation& beat = record.beats()[beat_index];
            if (beat.s_peak_time_seconds < 0.05 || beat.t_offset_time_seconds + 0.05 >= static_cast<double>(record.sample_count()) / record.sampling_rate_hz())
                continue;
            for (unsigned int axis = 0; axis < signal_synth::clinical_axis_count; ++axis)
            {
                if (j_step_ratio(record, axis, beat.j_point_time_seconds) > j_step_limit || boundary_curvature_ratio(record, axis, beat.j_point_time_seconds, scales[axis]) > j_curvature_limit || boundary_curvature_ratio(record, axis, beat.t_onset_time_seconds, scales[axis]) > t_curvature_limit || boundary_curvature_ratio(record, axis, beat.t_offset_time_seconds, scales[axis]) > t_curvature_limit)
                    return false;
            }
        }
        return true;
    }

    double dynamic_trace_extreme(const signal_synth::clinical_ecg_record& record, signal_synth::clinical_dynamic_annotation_kind kind, bool maximum)
    {
        double result = maximum ? -1e100 : 1e100;
        bool found = false;
        for (unsigned int index = 0; index < record.dynamic_annotation_count(); ++index)
        {
            const signal_synth::clinical_dynamic_annotation& annotation = record.dynamic_annotations()[index];
            if (annotation.kind != kind || !annotation.present)
                continue;
            result = maximum ? std::max(result, annotation.value) : std::min(result, annotation.value);
            found = true;
        }
        return found ? result : 0.0;
    }

    bool dynamic_trace_is_reasonably_continuous(const signal_synth::clinical_ecg_record& record, signal_synth::clinical_dynamic_annotation_kind kind, double maximum_step)
    {
        bool found_previous = false;
        double previous = 0.0;
        for (unsigned int index = 0; index < record.dynamic_annotation_count(); ++index)
        {
            const signal_synth::clinical_dynamic_annotation& annotation = record.dynamic_annotations()[index];
            if (annotation.kind != kind || !annotation.present)
                continue;
            if (found_previous && std::fabs(annotation.value - previous) > maximum_step)
                return false;
            previous = annotation.value;
            found_previous = true;
        }
        return found_previous;
    }

    bool qt_adapts_to_rr(const signal_synth::clinical_ecg_record& record)
    {
        if (record.beat_count() < 4)
            return false;
        double min_qt = 1e100, max_qt = -1e100, min_qtc = 1e100, max_qtc = -1e100;
        for (unsigned int index = 0; index < record.beat_count(); ++index)
        {
            min_qt = std::min(min_qt, record.beats()[index].qt_interval_seconds);
            max_qt = std::max(max_qt, record.beats()[index].qt_interval_seconds);
            min_qtc = std::min(min_qtc, record.beats()[index].qtc_interval_seconds);
            max_qtc = std::max(max_qtc, record.beats()[index].qtc_interval_seconds);
        }
        return max_qt - min_qt > 0.010 && min_qtc > 0.409 && max_qtc < 0.411;
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
    ok &= check(signal_synth::ecg_scenario_engine_version() == 16, "current_waveform_semantics_increment_engine_version");

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

    bool continuous_boundaries = true;
    const unsigned int continuity_rates[] = {100, 500, 1000};
    for (unsigned int rate_index = 0; rate_index < sizeof(continuity_rates) / sizeof(continuity_rates[0]); ++rate_index)
    {
        for (unsigned int condition_index = 0; condition_index < sizeof(injury_rendering_conditions) / sizeof(injury_rendering_conditions[0]); ++condition_index)
        {
            const unsigned int rate = continuity_rates[rate_index];
            signal_synth::clinical_ecg_record record;
            signal_synth::ecg_scenario_report continuity_report;
            const bool generated = engine.generate(make_scenario(injury_rendering_conditions[condition_index], 1.0, rate), rate * 12, record, continuity_report);
            const bool smooth = generated && injury_boundaries_are_smooth(record);
            if (!smooth)
                std::cerr << "Continuity failure for " << signal_synth::find_ecg_condition(injury_rendering_conditions[condition_index])->scp_code << " at " << rate << " Hz\n";
            continuous_boundaries = smooth && continuous_boundaries;
        }
    }
    ok &= check(continuous_boundaries, "injury_source_boundaries_are_sampled_smoothly");

    signal_synth::ecg_qa_scenario dynamic_stt;
    dynamic_stt.clear_conditions();
    dynamic_stt.add_condition(signal_synth::ecg_condition_sr);
    dynamic_stt.set_sampling_rate_hz(500);
    dynamic_stt.set_heart_rate_bpm(72.0);
    dynamic_stt.set_rr_variability_seconds(0.12);
    dynamic_stt.set_hrv_modulation(1.0, 0.10, 0.04, 0.25, 0.12, 0.25, 0.05);
    dynamic_stt.set_qt_adaptation(signal_synth::ecg_qt_adaptation_fridericia, 410.0);
    dynamic_stt.add_repolarization_episode(signal_synth::ecg_condition_iscal, 1.0, 8.0, 2.0, 1.0);
    signal_synth::clinical_ecg_record dynamic_record;
    signal_synth::ecg_scenario_report dynamic_report;
    const bool dynamic_generated = engine.generate(dynamic_stt, 6000, dynamic_record, dynamic_report) && dynamic_report.success();
    ok &= check(dynamic_generated, "dynamic_repolarization_generates");
    ok &= check(dynamic_generated && dynamic_record.episode_count() == 1 && dynamic_record.episodes()[0].kind == signal_synth::clinical_episode_repolarization, "dynamic_repolarization_episode_boundary_exported");
    ok &= check(dynamic_generated && dynamic_record.dynamic_annotation_count() >= dynamic_record.beat_count() * 6, "dynamic_repolarization_trace_annotations_exported");
    ok &= check(dynamic_generated && dynamic_trace_extreme(dynamic_record, signal_synth::clinical_dynamic_repolarization_severity, true) > 0.8 && dynamic_trace_extreme(dynamic_record, signal_synth::clinical_dynamic_repolarization_severity, false) < 0.05, "dynamic_repolarization_severity_trace_range");
    ok &= check(dynamic_generated && dynamic_trace_extreme(dynamic_record, signal_synth::clinical_dynamic_st_j_amplitude_mv, false) < -0.15 && dynamic_trace_extreme(dynamic_record, signal_synth::clinical_dynamic_t_amplitude_mv, false) < -0.20, "dynamic_repolarization_st_t_targets");
    ok &= check(dynamic_generated && dynamic_trace_is_reasonably_continuous(dynamic_record, signal_synth::clinical_dynamic_repolarization_severity, 0.80), "dynamic_repolarization_severity_trace_continuity");
    ok &= check(dynamic_generated && qt_adapts_to_rr(dynamic_record), "dynamic_qt_adapts_to_rr");
    ok &= check(dynamic_generated && injury_boundaries_are_smooth(dynamic_record), "dynamic_repolarization_waveform_continuity");

    signal_synth::ecg_qa_scenario dynamic_repeated = dynamic_stt;
    signal_synth::clinical_ecg_record dynamic_record_repeated;
    signal_synth::ecg_scenario_report dynamic_report_repeated;
    signal_synth::ecg_qa_scenario dynamic_shifted = dynamic_stt;
    dynamic_shifted.clear_repolarization_episodes();
    dynamic_shifted.add_repolarization_episode(signal_synth::ecg_condition_iscal, 2.0, 8.0, 2.0, 1.0);
    ok &= check(engine.generate(dynamic_repeated, 6000, dynamic_record_repeated, dynamic_report_repeated) && same_signal(dynamic_record, dynamic_record_repeated) && dynamic_report.run_fingerprint() == dynamic_report_repeated.run_fingerprint() && dynamic_stt.fingerprint() != dynamic_shifted.fingerprint(), "dynamic_repolarization_is_reproducible_and_fingerprinted");

    signal_synth::ecg_qa_scenario dynamic_invalid_norm;
    dynamic_invalid_norm.add_condition(signal_synth::ecg_condition_norm);
    dynamic_invalid_norm.add_repolarization_episode(signal_synth::ecg_condition_ste, 1.0, 4.0, 1.0, 1.0);
    signal_synth::ecg_qa_scenario dynamic_overlap = dynamic_stt;
    dynamic_overlap.add_repolarization_episode(signal_synth::ecg_condition_std, 5.0, 2.0, 0.5, 0.5);
    ok &= check(!engine.validate(dynamic_invalid_norm, report) && report_has_issue(report, signal_synth::ecg_issue_condition_conflict), "dynamic_repolarization_norm_composition_is_rejected");
    ok &= check(!engine.validate(dynamic_overlap, report) && report_has_issue(report, signal_synth::ecg_issue_condition_conflict), "dynamic_repolarization_overlap_is_rejected");

    signal_synth::ecg_scenario_document document;
    document.schema_version = 3;
    document.scenario_id = "dynamic_repolarization_episode";
    document.name = "Dynamic repolarization episode";
    document.duration_seconds = 12.0;
    signal_synth::ecg_qa_scenario dynamic_json_scenario;
    dynamic_json_scenario.clear_conditions();
    dynamic_json_scenario.add_condition(signal_synth::ecg_condition_sr);
    dynamic_json_scenario.set_sampling_rate_hz(500);
    dynamic_json_scenario.set_heart_rate_bpm(72.0);
    dynamic_json_scenario.set_qt_adaptation(signal_synth::ecg_qt_adaptation_fridericia, 410.0);
    dynamic_json_scenario.add_repolarization_episode(signal_synth::ecg_condition_iscal, 1.0, 8.0, 2.0, 1.0);
    document.ecg = dynamic_json_scenario;
    signal_synth::ecg_scenario_json_result json;
    signal_synth::ecg_scenario_document parsed;
    signal_synth::ecg_scenario_json_result parsed_result;
    signal_synth::ecg_render_bundle render;
    signal_synth::ecg_document_render_result render_result;
    signal_synth::ecg_export_result export_result;
    signal_synth::ecg_export_bundle bundle;
    ok &= check(signal_synth::write_ecg_scenario_json(document, json) && json.canonical_json.find("\"qt_adaptation\"") != std::string::npos && json.canonical_json.find("\"repolarization_episodes\"") != std::string::npos && signal_synth::parse_ecg_scenario_json(json.canonical_json, parsed, parsed_result) && parsed.ecg.qt_adaptation_enabled() && parsed.ecg.repolarization_episode_count() == 1 && signal_synth::render_ecg_document(parsed, render, render_result) && signal_synth::build_ecg_export_bundle(render, bundle, export_result) && bundle.find("annotations.json") && bundle.find("annotations.json")->content.find("\"kind\":\"dynamic_repolarization\"") != std::string::npos && bundle.find("annotations.json")->content.find("\"dynamic_traces\"") != std::string::npos && bundle.find("annotations.json")->content.find("\"kind\":\"repolarization_severity\"") != std::string::npos, "dynamic_repolarization_json_and_export_contract");

    std::cout << (ok ? "All ECG ischemia/ST-T tests passed.\n" : "ECG ischemia/ST-T test failure.\n");
    return ok ? 0 : 1;
}
