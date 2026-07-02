#include "../src/clinical_ecg.h"
#include "../src/ecg_scenario.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    struct audit_case
    {
        std::string name;
        signal_synth::ecg_qa_scenario scenario;
    };

    bool close(double left, double right, double tolerance = 1e-9)
    {
        return std::fabs(left - right) <= tolerance;
    }

    bool check(bool condition, const char* name)
    {
        std::cout << (condition ? "PASS " : "FAIL ") << name << '\n';
        return condition;
    }

    audit_case make_case(signal_synth::ecg_condition_code code, const char* suffix = "")
    {
        const signal_synth::ecg_condition_info* info = signal_synth::find_ecg_condition(code);
        audit_case result;
        result.name = std::string(info ? info->scp_code : "UNKNOWN") + suffix;
        result.scenario.add_condition(code);
        result.scenario.set_sampling_rate_hz(500);
        result.scenario.set_seed(9000 + static_cast<unsigned int>(code));
        if (code == signal_synth::ecg_condition_stach)
            result.scenario.set_heart_rate_bpm(120.0);
        else if (code == signal_synth::ecg_condition_sbrad)
            result.scenario.set_heart_rate_bpm(45.0);
        else if (code == signal_synth::ecg_condition_svtac)
            result.scenario.set_heart_rate_bpm(160.0);
        else if (code == signal_synth::ecg_condition_aflt)
            result.scenario.set_heart_rate_bpm(150.0);
        else
            result.scenario.set_heart_rate_bpm(70.0);
        return result;
    }

    bool assertions_pass(const signal_synth::ecg_scenario_report& report)
    {
        if (!report.success() || !report.phenotype_passed() || report.assertion_count() == 0)
            return false;
        for (unsigned int index = 0; index < report.assertion_count(); ++index)
            if (report.assertion_status(index) != signal_synth::ecg_assertion_passed)
                return false;
        return true;
    }

    bool exact_limb_identities(const signal_synth::clinical_ecg_record& record)
    {
        const double* lead_i = record.lead_data(signal_synth::clinical_lead_i);
        const double* lead_ii = record.lead_data(signal_synth::clinical_lead_ii);
        const double* lead_iii = record.lead_data(signal_synth::clinical_lead_iii);
        const double* avr = record.lead_data(signal_synth::clinical_lead_avr);
        const double* avl = record.lead_data(signal_synth::clinical_lead_avl);
        const double* avf = record.lead_data(signal_synth::clinical_lead_avf);
        for (unsigned int sample = 0; sample < record.sample_count(); ++sample)
            if (!close(lead_iii[sample], lead_ii[sample] - lead_i[sample], 1e-12) || !close(avr[sample], -(lead_i[sample] + lead_ii[sample]) / 2.0, 1e-12) || !close(avl[sample], lead_i[sample] - lead_ii[sample] / 2.0, 1e-12) || !close(avf[sample], lead_ii[sample] - lead_i[sample] / 2.0, 1e-12))
                return false;
        return true;
    }

    bool ordered_fiducials(const signal_synth::clinical_ecg_record& record)
    {
        for (unsigned int index = 0; index < record.beat_count(); ++index)
        {
            const signal_synth::clinical_beat_annotation& beat = record.beats()[index];
            if (!(beat.qrs_onset_time_seconds < beat.q_peak_time_seconds && beat.q_peak_time_seconds < beat.r_peak_time_seconds && beat.r_peak_time_seconds < beat.s_peak_time_seconds && beat.s_peak_time_seconds < beat.j_point_time_seconds && beat.j_point_time_seconds == beat.qrs_offset_time_seconds && beat.qrs_offset_time_seconds < beat.t_onset_time_seconds && beat.t_onset_time_seconds < beat.t_peak_time_seconds && beat.t_peak_time_seconds < beat.t_offset_time_seconds))
                return false;
        }
        return true;
    }

    bool finite_plausible_output(const signal_synth::clinical_ecg_record& record)
    {
        double maximum = 0.0;
        for (unsigned int lead = 0; lead < record.lead_count(); ++lead)
        {
            const double* signal = record.lead_data(lead);
            for (unsigned int sample = 0; sample < record.sample_count(); ++sample)
            {
                if (!std::isfinite(signal[sample]))
                    return false;
                maximum = std::max(maximum, std::fabs(signal[sample]));
            }
        }
        return maximum > 0.01 && maximum < 10.0;
    }

    bool bounded_non_qrs_steps(const signal_synth::clinical_ecg_record& record)
    {
        std::vector<bool> qrs(record.sample_count(), false);
        for (unsigned int index = 0; index < record.beat_count(); ++index)
        {
            const signal_synth::clinical_beat_annotation& beat = record.beats()[index];
            const int first = std::max(0, static_cast<int>(std::floor(beat.qrs_onset_time_seconds * record.sampling_rate_hz())) - 1);
            const int last = std::min(static_cast<int>(record.sample_count()) - 1, static_cast<int>(std::ceil(beat.qrs_offset_time_seconds * record.sampling_rate_hz())) + 1);
            for (int sample = first; sample <= last; ++sample)
                qrs[static_cast<unsigned int>(sample)] = true;
        }
        for (unsigned int lead = 0; lead < record.lead_count(); ++lead)
        {
            const double* signal = record.lead_data(lead);
            for (unsigned int sample = 1; sample < record.sample_count(); ++sample)
                if (!qrs[sample - 1] && !qrs[sample] && std::fabs(signal[sample] - signal[sample - 1]) > 0.08)
                    return false;
        }
        return true;
    }

    unsigned int sample_at(const signal_synth::clinical_ecg_record& record, double time_seconds)
    {
        return static_cast<unsigned int>(std::llround(time_seconds * record.sampling_rate_hz()));
    }

    bool rbbb_lead_contract(const signal_synth::clinical_ecg_record& record)
    {
        if (record.beat_count() < 3)
            return false;
        const signal_synth::clinical_beat_annotation& beat = record.beats()[2];
        const unsigned int q = sample_at(record, beat.q_peak_time_seconds);
        const unsigned int r = sample_at(record, beat.r_peak_time_seconds);
        const unsigned int s = sample_at(record, beat.s_peak_time_seconds);
        const unsigned int t = sample_at(record, beat.t_peak_time_seconds);
        const double* lead_i = record.lead_data(signal_synth::clinical_lead_i);
        const double* v1 = record.lead_data(signal_synth::clinical_lead_v1);
        const double* v6 = record.lead_data(signal_synth::clinical_lead_v6);
        return beat.qrs_duration_seconds >= 0.120 && v1[q] > 0.02 && v1[r] < -0.20 && v1[s] > 0.15 && lead_i[s] < -0.15 && v6[s] < -0.15 && v1[t] < -0.05 && lead_i[t] > 0.05 && v6[t] > 0.05;
    }

    bool lbbb_lead_contract(const signal_synth::clinical_ecg_record& record)
    {
        if (record.beat_count() < 3)
            return false;
        const signal_synth::clinical_beat_annotation& beat = record.beats()[2];
        const unsigned int q = sample_at(record, beat.q_peak_time_seconds);
        const unsigned int r = sample_at(record, beat.r_peak_time_seconds);
        const unsigned int s = sample_at(record, beat.s_peak_time_seconds);
        const unsigned int t = sample_at(record, beat.t_peak_time_seconds);
        const double* lead_i = record.lead_data(signal_synth::clinical_lead_i);
        const double* v1 = record.lead_data(signal_synth::clinical_lead_v1);
        const double* v5 = record.lead_data(signal_synth::clinical_lead_v5);
        const double* v6 = record.lead_data(signal_synth::clinical_lead_v6);
        return beat.qrs_duration_seconds >= 0.120 && std::fabs(lead_i[q]) < 0.01 && std::fabs(v6[q]) < 0.01 && v1[r] < -0.20 && v1[s] < -0.10 && lead_i[r] > 0.30 && lead_i[s] > 0.10 && v5[s] > 0.10 && v6[r] > 0.30 && v6[s] > 0.10 && v1[t] > 0.05 && lead_i[t] < -0.05 && v6[t] < -0.05;
    }

    bool flutter_contract(const signal_synth::clinical_ecg_record& record, unsigned int conduction_ratio)
    {
        if (record.atrial_event_count() < 4 || record.beat_count() == 0)
            return false;
        const double* atrial_x = record.source_data(signal_synth::clinical_source_atrial, signal_synth::clinical_axis_x);
        const double* atrial_y = record.source_data(signal_synth::clinical_source_atrial, signal_synth::clinical_axis_y);
        for (unsigned int index = 0; index + 1 < record.atrial_event_count(); ++index)
        {
            const signal_synth::clinical_atrial_event& event = record.atrial_events()[index];
            const signal_synth::clinical_atrial_event& next = record.atrial_events()[index + 1];
            const double period = event.offset_time_seconds - event.onset_time_seconds;
            if (!close(event.offset_time_seconds, next.onset_time_seconds) || !close((event.peak_time_seconds - event.onset_time_seconds) / period, 0.73) || event.conducted != ((index + 1) % conduction_ratio == 0))
                return false;
            const unsigned int first = sample_at(record, event.onset_time_seconds);
            const unsigned int last = std::min(record.sample_count() - 1, sample_at(record, event.offset_time_seconds));
            double minimum = 0.0;
            unsigned int zero_run = 0;
            unsigned int longest_zero_run = 0;
            for (unsigned int sample = first; sample <= last; ++sample)
            {
                const double inferior = 0.5 * atrial_x[sample] + 0.8660254037844386 * atrial_y[sample];
                minimum = std::min(minimum, inferior);
                zero_run = std::fabs(inferior) < 0.002 ? zero_run + 1 : 0;
                longest_zero_run = std::max(longest_zero_run, zero_run);
            }
            if (minimum > -0.04 || longest_zero_run > static_cast<unsigned int>(0.15 * period * record.sampling_rate_hz()) + 1)
                return false;
        }
        return true;
    }

    bool audit_record(const signal_synth::clinical_ecg_record& record)
    {
        return finite_plausible_output(record) && exact_limb_identities(record) && ordered_fiducials(record) && bounded_non_qrs_steps(record);
    }
}

int main()
{
    std::vector<audit_case> cases;
    for (unsigned int index = 0; index < signal_synth::ecg_condition_catalog_size(); ++index)
    {
        const signal_synth::ecg_condition_info& info = signal_synth::ecg_condition_catalog()[index];
        if (info.support == signal_synth::ecg_support_catalog_only || info.code == signal_synth::ecg_condition_prc || info.code == signal_synth::ecg_condition_bigu || info.code == signal_synth::ecg_condition_trigu)
            continue;
        audit_case entry = make_case(info.code);
        if (info.code == signal_synth::ecg_condition_2avb)
            entry.scenario.set_second_degree_av_pattern(signal_synth::ecg_second_degree_mobitz_i);
        if (info.code == signal_synth::ecg_condition_qwave)
            entry.scenario.set_q_wave_territory(signal_synth::ecg_q_wave_inferior);
        cases.push_back(entry);
    }

    audit_case mobitz_ii = make_case(signal_synth::ecg_condition_2avb, "_MobitzII");
    mobitz_ii.scenario.set_second_degree_av_pattern(signal_synth::ecg_second_degree_mobitz_ii);
    cases.push_back(mobitz_ii);
    audit_case q_anterior = make_case(signal_synth::ecg_condition_qwave, "_anterior");
    q_anterior.scenario.set_q_wave_territory(signal_synth::ecg_q_wave_anterior);
    cases.push_back(q_anterior);
    audit_case q_lateral = make_case(signal_synth::ecg_condition_qwave, "_lateral");
    q_lateral.scenario.set_q_wave_territory(signal_synth::ecg_q_wave_lateral);
    cases.push_back(q_lateral);

    const signal_synth::ecg_condition_code origins[] = {signal_synth::ecg_condition_pac, signal_synth::ecg_condition_pvc};
    const signal_synth::ecg_condition_code cadences[] = {signal_synth::ecg_condition_bigu, signal_synth::ecg_condition_trigu};
    for (signal_synth::ecg_condition_code origin : origins)
        for (signal_synth::ecg_condition_code cadence : cadences)
        {
            audit_case entry = make_case(origin, cadence == signal_synth::ecg_condition_bigu ? "_BIGU" : "_TRIGU");
            entry.scenario.add_condition(cadence);
            cases.push_back(entry);
        }

    bool catalog_ok = true;
    bool rbbb_ok = false;
    bool lbbb_ok = false;
    bool flutter_ok = false;
    signal_synth::ecg_scenario_engine engine;
    for (const audit_case& entry : cases)
    {
        signal_synth::clinical_ecg_record record;
        signal_synth::ecg_scenario_report report;
        const bool generated = engine.generate(entry.scenario, 6000, record, report);
        const bool accepted = generated && assertions_pass(report) && audit_record(record);
        if (!accepted)
            std::cerr << "Catalog morphology failure: " << entry.name << '\n';
        catalog_ok &= accepted;
        if (entry.scenario.has_condition(signal_synth::ecg_condition_crbbb))
            rbbb_ok = generated && rbbb_lead_contract(record);
        if (entry.scenario.has_condition(signal_synth::ecg_condition_clbbb))
            lbbb_ok = generated && lbbb_lead_contract(record);
        if (entry.scenario.has_condition(signal_synth::ecg_condition_aflt))
            flutter_ok = generated && flutter_contract(record, 2);
    }

    bool ok = true;
    ok &= check(cases.size() == 72 && catalog_ok, "all_72_supported_condition_variants_pass_waveform_quality_gate");
    ok &= check(rbbb_ok, "complete_rbbb_has_canonical_terminal_forces_and_secondary_t_discordance");
    ok &= check(lbbb_ok, "complete_lbbb_has_canonical_lateral_forces_and_secondary_t_discordance");
    ok &= check(flutter_ok, "typical_flutter_has_continuous_inferior_negative_f_waves");
    ok &= check(signal_synth::ecg_scenario_engine_version() == 9, "morphology_correction_increments_engine_identity");
    return ok ? 0 : 1;
}
