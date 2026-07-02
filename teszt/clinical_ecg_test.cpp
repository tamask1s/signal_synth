#include "../src/clinical_ecg.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
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

    bool close(double left, double right, double tolerance = 1e-12)
    {
        return std::fabs(left - right) <= tolerance;
    }

    bool same_record(const signal_synth::clinical_ecg_record& left, const signal_synth::clinical_ecg_record& right)
    {
        if (left.sampling_rate_hz() != right.sampling_rate_hz() || left.sample_count() != right.sample_count() || left.lead_count() != right.lead_count() || left.source_count() != right.source_count() || left.atrial_event_count() != right.atrial_event_count() || left.beat_count() != right.beat_count() || left.fiducial_count() != right.fiducial_count() || left.episode_count() != right.episode_count())
            return false;
        for (unsigned int lead = 0; lead < left.lead_count(); ++lead)
            for (unsigned int sample = 0; sample < left.sample_count(); ++sample)
                if (left.lead_data(lead)[sample] != right.lead_data(lead)[sample])
                    return false;
        for (unsigned int source = 0; source < left.source_count(); ++source)
            for (unsigned int axis = 0; axis < signal_synth::clinical_axis_count; ++axis)
                for (unsigned int sample = 0; sample < left.sample_count(); ++sample)
                    if (left.source_data(source, axis)[sample] != right.source_data(source, axis)[sample])
                        return false;
        for (unsigned int axis = 0; axis < signal_synth::clinical_axis_count; ++axis)
            for (unsigned int sample = 0; sample < left.sample_count(); ++sample)
                if (left.vcg_data(axis)[sample] != right.vcg_data(axis)[sample])
                    return false;
        const signal_synth::clinical_atrial_event* left_atrial = left.atrial_events();
        const signal_synth::clinical_atrial_event* right_atrial = right.atrial_events();
        for (unsigned int i = 0; i < left.atrial_event_count(); ++i)
            if (left_atrial[i].atrial_index != right_atrial[i].atrial_index || left_atrial[i].onset_time_seconds != right_atrial[i].onset_time_seconds || left_atrial[i].peak_time_seconds != right_atrial[i].peak_time_seconds || left_atrial[i].offset_time_seconds != right_atrial[i].offset_time_seconds || left_atrial[i].visible != right_atrial[i].visible || left_atrial[i].conducted != right_atrial[i].conducted || left_atrial[i].linked_ventricular_index != right_atrial[i].linked_ventricular_index)
                return false;
        const signal_synth::clinical_beat_annotation* left_beats = left.beats();
        const signal_synth::clinical_beat_annotation* right_beats = right.beats();
        for (unsigned int i = 0; i < left.beat_count(); ++i)
            if (left_beats[i].beat_index != right_beats[i].beat_index || left_beats[i].linked_atrial_index != right_beats[i].linked_atrial_index || left_beats[i].origin != right_beats[i].origin || left_beats[i].r_peak_time_seconds != right_beats[i].r_peak_time_seconds || left_beats[i].rr_interval_seconds != right_beats[i].rr_interval_seconds || left_beats[i].rr_was_clipped != right_beats[i].rr_was_clipped)
                return false;
        const signal_synth::clinical_fiducial_annotation* left_fiducials = left.fiducials();
        const signal_synth::clinical_fiducial_annotation* right_fiducials = right.fiducials();
        for (unsigned int i = 0; i < left.fiducial_count(); ++i)
            if (left_fiducials[i].beat_index != right_fiducials[i].beat_index || left_fiducials[i].atrial_index != right_fiducials[i].atrial_index || left_fiducials[i].lead_index != right_fiducials[i].lead_index || left_fiducials[i].kind != right_fiducials[i].kind || left_fiducials[i].source != right_fiducials[i].source || left_fiducials[i].sample_index != right_fiducials[i].sample_index || left_fiducials[i].time_seconds != right_fiducials[i].time_seconds || left_fiducials[i].amplitude_mv != right_fiducials[i].amplitude_mv || left_fiducials[i].present != right_fiducials[i].present)
                return false;
        const signal_synth::clinical_episode_annotation* left_episodes = left.episodes();
        const signal_synth::clinical_episode_annotation* right_episodes = right.episodes();
        for (unsigned int i = 0; i < left.episode_count(); ++i)
            if (left_episodes[i].kind != right_episodes[i].kind || left_episodes[i].start_time_seconds != right_episodes[i].start_time_seconds || left_episodes[i].end_time_seconds != right_episodes[i].end_time_seconds || left_episodes[i].first_beat_index != right_episodes[i].first_beat_index || left_episodes[i].last_beat_index != right_episodes[i].last_beat_index || left_episodes[i].start_sample_index != right_episodes[i].start_sample_index || left_episodes[i].end_sample_index != right_episodes[i].end_sample_index || left_episodes[i].present != right_episodes[i].present)
                return false;
        return true;
    }

    bool exact_source_sum(const signal_synth::clinical_ecg_record& record)
    {
        for (unsigned int axis = 0; axis < signal_synth::clinical_axis_count; ++axis)
        {
            const double* total = record.vcg_data(axis);
            if (!total)
                return false;
            for (unsigned int sample = 0; sample < record.sample_count(); ++sample)
            {
                double sum = 0.0;
                for (unsigned int source = 0; source < record.source_count(); ++source)
                {
                    const double* data = record.source_data(source, axis);
                    if (!data || !std::isfinite(data[sample]))
                        return false;
                    sum += data[sample];
                }
                if (sum != total[sample])
                    return false;
            }
        }
        return true;
    }

    bool expected_sources_present(const signal_synth::clinical_ecg_record& record)
    {
        const unsigned int active_sources[] = {signal_synth::clinical_source_atrial, signal_synth::clinical_source_septal, signal_synth::clinical_source_ventricular, signal_synth::clinical_source_terminal, signal_synth::clinical_source_repolarization};
        for (unsigned int source : active_sources)
        {
            double maximum = 0.0;
            for (unsigned int axis = 0; axis < signal_synth::clinical_axis_count; ++axis)
                for (unsigned int sample = 0; sample < record.sample_count(); ++sample)
                    maximum = std::max(maximum, std::fabs(record.source_data(source, axis)[sample]));
            if (maximum < 0.001)
                return false;
        }
        return true;
    }

    bool finite_nontrivial_leads(const signal_synth::clinical_ecg_record& record)
    {
        for (unsigned int lead = 0; lead < record.lead_count(); ++lead)
        {
            const double* data = record.lead_data(lead);
            if (!data)
                return false;
            double absolute_maximum = 0.0;
            for (unsigned int sample = 0; sample < record.sample_count(); ++sample)
            {
                if (!std::isfinite(data[sample]))
                    return false;
                absolute_maximum = std::max(absolute_maximum, std::fabs(data[sample]));
            }
            if (absolute_maximum < 0.01)
                return false;
        }
        return true;
    }

    bool exact_limb_leads(const signal_synth::clinical_ecg_record& record)
    {
        const double* lead_i = record.lead_data(signal_synth::clinical_lead_i);
        const double* lead_ii = record.lead_data(signal_synth::clinical_lead_ii);
        const double* lead_iii = record.lead_data(signal_synth::clinical_lead_iii);
        const double* avr = record.lead_data(signal_synth::clinical_lead_avr);
        const double* avl = record.lead_data(signal_synth::clinical_lead_avl);
        const double* avf = record.lead_data(signal_synth::clinical_lead_avf);
        for (unsigned int sample = 0; sample < record.sample_count(); ++sample)
            if (!close(lead_iii[sample], lead_ii[sample] - lead_i[sample]) || !close(avr[sample], -(lead_i[sample] + lead_ii[sample]) / 2.0) || !close(avl[sample], lead_i[sample] - lead_ii[sample] / 2.0) || !close(avf[sample], lead_ii[sample] - lead_i[sample] / 2.0))
                return false;
        return true;
    }

    bool valid_timeline(const signal_synth::clinical_ecg_record& record)
    {
        const double duration = static_cast<double>(record.sample_count()) / record.sampling_rate_hz();
        const signal_synth::clinical_atrial_event* atrial = record.atrial_events();
        const signal_synth::clinical_beat_annotation* beats = record.beats();
        for (unsigned int i = 0; i < record.atrial_event_count(); ++i)
        {
            if (atrial[i].atrial_index != i || !(atrial[i].onset_time_seconds < atrial[i].peak_time_seconds && atrial[i].peak_time_seconds < atrial[i].offset_time_seconds) || atrial[i].onset_time_seconds >= duration)
                return false;
            if (atrial[i].conducted)
            {
                if (atrial[i].linked_ventricular_index < 0 || static_cast<unsigned long long>(atrial[i].linked_ventricular_index) >= record.beat_count())
                    return false;
                if (beats[atrial[i].linked_ventricular_index].linked_atrial_index != static_cast<long long>(i))
                    return false;
            }
        }
        for (unsigned int i = 0; i < record.beat_count(); ++i)
        {
            const signal_synth::clinical_beat_annotation& beat = beats[i];
            if (beat.beat_index != i || beat.r_peak_time_seconds >= duration || !(beat.qrs_onset_time_seconds < beat.q_peak_time_seconds && beat.q_peak_time_seconds < beat.r_peak_time_seconds && beat.r_peak_time_seconds < beat.s_peak_time_seconds && beat.s_peak_time_seconds < beat.j_point_time_seconds && beat.j_point_time_seconds == beat.qrs_offset_time_seconds && beat.qrs_offset_time_seconds < beat.t_onset_time_seconds && beat.t_onset_time_seconds < beat.t_peak_time_seconds && beat.t_peak_time_seconds < beat.t_offset_time_seconds))
                return false;
            if (!close(beat.qrs_offset_time_seconds - beat.qrs_onset_time_seconds, beat.qrs_duration_seconds))
                return false;
            if (beat.linked_atrial_index >= 0)
            {
                if (static_cast<unsigned long long>(beat.linked_atrial_index) >= record.atrial_event_count())
                    return false;
                if (atrial[beat.linked_atrial_index].linked_ventricular_index != static_cast<long long>(i))
                    return false;
                if (!close(beat.qrs_onset_time_seconds - atrial[beat.linked_atrial_index].onset_time_seconds, beat.pr_interval_seconds))
                    return false;
            }
        }
        return true;
    }

    unsigned int count_fiducials(const signal_synth::clinical_ecg_record& record, signal_synth::clinical_fiducial_kind kind, signal_synth::clinical_fiducial_source source, int lead_index)
    {
        unsigned int count = 0;
        for (unsigned int i = 0; i < record.fiducial_count(); ++i)
        {
            const signal_synth::clinical_fiducial_annotation& fiducial = record.fiducials()[i];
            if (fiducial.kind == kind && fiducial.source == source && fiducial.lead_index == lead_index)
                ++count;
        }
        return count;
    }

    unsigned int count_present_fiducials(const signal_synth::clinical_ecg_record& record, signal_synth::clinical_fiducial_kind kind, signal_synth::clinical_fiducial_source source)
    {
        unsigned int count = 0;
        for (unsigned int i = 0; i < record.fiducial_count(); ++i)
        {
            const signal_synth::clinical_fiducial_annotation& fiducial = record.fiducials()[i];
            if (fiducial.kind == kind && fiducial.source == source && fiducial.present)
                ++count;
        }
        return count;
    }

    bool all_atrial_unconducted(const signal_synth::clinical_ecg_record& record)
    {
        for (unsigned int i = 0; i < record.atrial_event_count(); ++i)
            if (record.atrial_events()[i].conducted || record.atrial_events()[i].linked_ventricular_index != -1)
                return false;
        return true;
    }

    double first_qt(signal_synth::clinical_ecg_config config)
    {
        signal_synth::clinical_ecg_record record;
        signal_synth::clinical_ecg_generator(config).generate(1500, record);
        return record.beats()[0].qt_interval_seconds;
    }
}

int main()
{
    bool ok = true;
    signal_synth::clinical_ecg_config config;
    signal_synth::clinical_ecg_generator generator(config);
    signal_synth::clinical_ecg_record record;
    ok &= check(generator.valid() && generator.generate(5000, record), "default_generation");
    ok &= check(record.sampling_rate_hz() == 500 && record.sample_count() == 5000 && record.lead_count() == 12 && record.beat_count() == 10 && record.atrial_event_count() == 10, "default_record_shape");
    ok &= check(record.source_count() == signal_synth::clinical_source_count && record.source_name(signal_synth::clinical_source_atrial) == std::string("Atrial") && record.source_name(999)[0] == '\0' && !record.source_data(999, 0) && !record.vcg_data(999), "multi_source_public_contract");
    ok &= check(exact_source_sum(record) && expected_sources_present(record), "source_components_sum_exactly_to_vcg");
    ok &= check(finite_nontrivial_leads(record), "all_leads_finite_and_nontrivial");
    ok &= check(exact_limb_leads(record), "einthoven_and_goldberger_identities");
    ok &= check(valid_timeline(record), "timeline_and_links_are_consistent");
    ok &= check(count_fiducials(record, signal_synth::clinical_r_peak, signal_synth::clinical_fiducial_construction, -1) == record.beat_count() && count_fiducials(record, signal_synth::clinical_r_peak, signal_synth::clinical_fiducial_lead_measurement, signal_synth::clinical_lead_ii) == record.beat_count(), "global_and_per_lead_fiducials");

    signal_synth::clinical_ecg_record repeated;
    signal_synth::clinical_ecg_generator copied_generator = generator;
    copied_generator.generate(5000, repeated);
    ok &= check(same_record(record, repeated), "generation_is_reproducible");
    signal_synth::clinical_ecg_record copied_record = record;
    ok &= check(same_record(record, copied_record), "record_copy_is_exact");

    signal_synth::clinical_ecg_config invalid = config;
    invalid.rhythm.rhythm = static_cast<signal_synth::clinical_rhythm>(99);
    ok &= check(!generator.configure(invalid) && generator.valid() && generator.config().rhythm.rhythm == config.rhythm.rhythm, "invalid_reconfigure_is_transactional");
    signal_synth::clinical_ecg_config contradictory = config;
    contradictory.rhythm.rhythm = signal_synth::clinical_rhythm_atrial_fibrillation;
    contradictory.rhythm.av_conduction = signal_synth::clinical_av_mobitz_ii;
    ok &= check(!signal_synth::clinical_ecg_generator(contradictory).valid(), "unsupported_rhythm_combination_is_rejected");
    signal_synth::clinical_ecg_config invalid_source = config;
    invalid_source.sources.gain[signal_synth::clinical_source_atrial] = std::numeric_limits<double>::quiet_NaN();
    ok &= check(!signal_synth::clinical_ecg_generator(invalid_source).valid(), "invalid_source_configuration_is_rejected");
    signal_synth::clinical_ecg_record preserved = record;
    ok &= check(!generator.generate(0, preserved) && same_record(record, preserved), "failed_generation_preserves_output");
    signal_synth::clinical_ecg_config overflowing = config;
    overflowing.morphology.r_amplitude_mv = std::numeric_limits<double>::max();
    overflowing.leads.lead_gain[signal_synth::clinical_lead_i] = std::numeric_limits<double>::max();
    signal_synth::clinical_ecg_generator overflowing_generator(overflowing);
    ok &= check(overflowing_generator.valid() && !overflowing_generator.generate(1000, preserved) && same_record(record, preserved), "nonfinite_output_is_rejected_transactionally");

    signal_synth::clinical_ecg_config rotated_config = config;
    rotated_config.leads.yaw_degrees = 30.0;
    signal_synth::clinical_ecg_record rotated;
    signal_synth::clinical_ecg_generator(rotated_config).generate(5000, rotated);
    ok &= check(rotated.lead_data(signal_synth::clinical_lead_i)[250] != record.lead_data(signal_synth::clinical_lead_i)[250] && exact_limb_leads(rotated), "vcg_rotation_changes_projection");

    signal_synth::clinical_ecg_config no_repolarization_config = config;
    no_repolarization_config.sources.gain[signal_synth::clinical_source_repolarization] = 0.0;
    signal_synth::clinical_ecg_record no_repolarization;
    signal_synth::clinical_ecg_generator(no_repolarization_config).generate(5000, no_repolarization);
    double repolarization_maximum = 0.0;
    double lead_difference = 0.0;
    for (unsigned int axis = 0; axis < signal_synth::clinical_axis_count; ++axis)
        for (unsigned int sample = 0; sample < no_repolarization.sample_count(); ++sample)
            repolarization_maximum = std::max(repolarization_maximum, std::fabs(no_repolarization.source_data(signal_synth::clinical_source_repolarization, axis)[sample]));
    for (unsigned int sample = 0; sample < record.sample_count(); ++sample)
        lead_difference = std::max(lead_difference, std::fabs(record.lead_data(signal_synth::clinical_lead_ii)[sample] - no_repolarization.lead_data(signal_synth::clinical_lead_ii)[sample]));
    ok &= check(repolarization_maximum == 0.0 && lead_difference > 0.01 && exact_source_sum(no_repolarization), "individual_source_gain_controls_rendering");
    ok &= check(!no_repolarization.beats()[0].t_present && count_present_fiducials(no_repolarization, signal_synth::clinical_t_peak, signal_synth::clinical_fiducial_construction) == 0 && count_present_fiducials(no_repolarization, signal_synth::clinical_t_peak, signal_synth::clinical_fiducial_lead_measurement) == 0, "disabled_repolarization_removes_t_ground_truth");

    signal_synth::clinical_ecg_config no_atrial_source_config = config;
    no_atrial_source_config.sources.gain[signal_synth::clinical_source_atrial] = 0.0;
    signal_synth::clinical_ecg_record no_atrial_source;
    signal_synth::clinical_ecg_generator(no_atrial_source_config).generate(5000, no_atrial_source);
    ok &= check(!no_atrial_source.atrial_events()[0].visible && !no_atrial_source.beats()[0].p_present && count_present_fiducials(no_atrial_source, signal_synth::clinical_p_peak, signal_synth::clinical_fiducial_construction) == 0 && count_present_fiducials(no_atrial_source, signal_synth::clinical_p_peak, signal_synth::clinical_fiducial_lead_measurement) == 0, "disabled_atrial_source_removes_p_ground_truth");

    signal_synth::clinical_ecg_config pvc_config = config;
    pvc_config.scenario.premature_every_n_beats = 5;
    pvc_config.scenario.premature_origin = signal_synth::clinical_origin_pvc;
    signal_synth::clinical_ecg_record pvc;
    signal_synth::clinical_ecg_generator(pvc_config).generate(6000, pvc);
    const signal_synth::clinical_beat_annotation* pvc_beats = pvc.beats();
    ok &= check(pvc_beats[4].origin == signal_synth::clinical_origin_pvc && !pvc_beats[4].p_present && pvc_beats[4].qrs_duration_seconds >= 0.150 && close(pvc_beats[4].rr_interval_seconds, 0.65) && close(pvc_beats[5].rr_interval_seconds, 1.35), "pvc_and_compensatory_pause");

    signal_synth::clinical_ecg_config pac_config = pvc_config;
    pac_config.scenario.premature_origin = signal_synth::clinical_origin_pac;
    signal_synth::clinical_ecg_record pac;
    signal_synth::clinical_ecg_generator(pac_config).generate(6000, pac);
    ok &= check(pac.beats()[4].origin == signal_synth::clinical_origin_pac && pac.beats()[4].p_present && close(pac.beats()[4].qrs_duration_seconds, 0.090), "pac_has_atrial_event_and_narrow_qrs");

    signal_synth::clinical_ecg_config clipped_scenario_config = pvc_config;
    clipped_scenario_config.rhythm.maximum_rr_seconds = 1.1;
    signal_synth::clinical_ecg_record clipped_scenario;
    signal_synth::clinical_ecg_generator(clipped_scenario_config).generate(5000, clipped_scenario);
    ok &= check(close(clipped_scenario.beats()[5].rr_interval_seconds, 1.1) && clipped_scenario.beats()[5].rr_was_clipped, "scenario_rr_limits_are_applied_after_override");

    signal_synth::clinical_ecg_config pause_config = config;
    pause_config.scenario.sinus_pause_every_n_beats = 4;
    signal_synth::clinical_ecg_record pause;
    signal_synth::clinical_ecg_generator(pause_config).generate(5000, pause);
    ok &= check(close(pause.beats()[3].rr_interval_seconds, 2.0), "periodic_sinus_pause");

    signal_synth::clinical_ecg_config first_degree_config = config;
    first_degree_config.rhythm.av_conduction = signal_synth::clinical_av_first_degree;
    first_degree_config.rhythm.atrial_rate_bpm = 60.0;
    first_degree_config.rhythm.first_degree_pr_ms = 260.0;
    signal_synth::clinical_ecg_record first_degree;
    signal_synth::clinical_ecg_generator(first_degree_config).generate(5000, first_degree);
    ok &= check(close(first_degree.beats()[0].pr_interval_seconds, 0.260) && first_degree.beat_count() == first_degree.atrial_event_count(), "first_degree_av_block");

    signal_synth::clinical_ecg_config mobitz_i_config = first_degree_config;
    mobitz_i_config.rhythm.av_conduction = signal_synth::clinical_av_mobitz_i;
    mobitz_i_config.rhythm.mobitz_cycle_length = 4;
    mobitz_i_config.rhythm.wenckebach_pr_increment_ms = 40.0;
    signal_synth::clinical_ecg_record mobitz_i;
    signal_synth::clinical_ecg_generator(mobitz_i_config).generate(6000, mobitz_i);
    ok &= check(mobitz_i.atrial_event_count() > mobitz_i.beat_count() && close(mobitz_i.beats()[0].pr_interval_seconds, 0.160) && close(mobitz_i.beats()[1].pr_interval_seconds, 0.200) && close(mobitz_i.beats()[2].pr_interval_seconds, 0.240) && !mobitz_i.atrial_events()[3].conducted && mobitz_i.beats()[3].linked_atrial_index == 4, "mobitz_i_progressive_pr_and_drop");
    ok &= check(count_fiducials(mobitz_i, signal_synth::clinical_p_peak, signal_synth::clinical_fiducial_lead_measurement, signal_synth::clinical_lead_ii) == mobitz_i.atrial_event_count(), "nonconducted_p_waves_have_per_lead_measurements");

    signal_synth::clinical_ecg_config mobitz_ii_config = mobitz_i_config;
    mobitz_ii_config.rhythm.av_conduction = signal_synth::clinical_av_mobitz_ii;
    signal_synth::clinical_ecg_record mobitz_ii;
    signal_synth::clinical_ecg_generator(mobitz_ii_config).generate(6000, mobitz_ii);
    ok &= check(!mobitz_ii.atrial_events()[3].conducted && close(mobitz_ii.beats()[0].pr_interval_seconds, mobitz_ii.beats()[2].pr_interval_seconds), "mobitz_ii_fixed_pr_and_drop");

    signal_synth::clinical_ecg_config complete_config = config;
    complete_config.rhythm.av_conduction = signal_synth::clinical_av_complete_block;
    complete_config.rhythm.atrial_rate_bpm = 80.0;
    complete_config.rhythm.ventricular_escape_rate_bpm = 35.0;
    signal_synth::clinical_ecg_record complete;
    signal_synth::clinical_ecg_generator(complete_config).generate(10000, complete);
    ok &= check(all_atrial_unconducted(complete) && complete.atrial_event_count() > complete.beat_count() && complete.beats()[0].origin == signal_synth::clinical_origin_ventricular_escape && complete.beats()[0].linked_atrial_index == -1, "complete_av_block_has_av_dissociation");

    signal_synth::clinical_ecg_config af_config = config;
    af_config.rhythm.rhythm = signal_synth::clinical_rhythm_atrial_fibrillation;
    af_config.rhythm.heart_rate_bpm = 90.0;
    af_config.rhythm.rr_variability_seconds = 0.12;
    signal_synth::clinical_ecg_record af;
    signal_synth::clinical_ecg_generator(af_config).generate(10000, af);
    bool variable_rr = false;
    for (unsigned int i = 1; i < af.beat_count(); ++i)
        variable_rr |= !close(af.beats()[i].rr_interval_seconds, af.beats()[0].rr_interval_seconds, 1e-6);
    ok &= check(af.atrial_event_count() == 0 && !af.beats()[0].p_present && variable_rr, "atrial_fibrillation_irregular_without_p");
    signal_synth::clinical_ecg_config clipped_af_config = af_config;
    clipped_af_config.rhythm.minimum_rr_seconds = 0.7;
    clipped_af_config.rhythm.maximum_rr_seconds = 1.0;
    signal_synth::clinical_ecg_record clipped_af;
    signal_synth::clinical_ecg_generator(clipped_af_config).generate(3000, clipped_af);
    bool af_clipped = false;
    for (unsigned int i = 0; i < clipped_af.beat_count(); ++i)
        af_clipped |= clipped_af.beats()[i].rr_was_clipped;
    ok &= check(af_clipped, "atrial_fibrillation_rr_clipping_is_annotated");

    signal_synth::clinical_ecg_config flutter_config = config;
    flutter_config.rhythm.rhythm = signal_synth::clinical_rhythm_atrial_flutter;
    flutter_config.rhythm.atrial_rate_bpm = 300.0;
    flutter_config.rhythm.flutter_conduction_ratio = 3;
    signal_synth::clinical_ecg_record flutter;
    signal_synth::clinical_ecg_generator(flutter_config).generate(5000, flutter);
    ok &= check(flutter.atrial_event_count() >= 3 * flutter.beat_count() && flutter.beat_count() > 0, "atrial_flutter_conduction_ratio");

    signal_synth::clinical_ecg_config lbbb_config = config;
    lbbb_config.rhythm.intraventricular_conduction = signal_synth::clinical_iv_lbbb;
    signal_synth::clinical_ecg_record lbbb;
    signal_synth::clinical_ecg_generator(lbbb_config).generate(3000, lbbb);
    signal_synth::clinical_ecg_config rbbb_config = config;
    rbbb_config.rhythm.intraventricular_conduction = signal_synth::clinical_iv_rbbb;
    signal_synth::clinical_ecg_record rbbb;
    signal_synth::clinical_ecg_generator(rbbb_config).generate(3000, rbbb);
    ok &= check(lbbb.beats()[0].qrs_duration_seconds >= 0.140 && rbbb.beats()[0].qrs_duration_seconds >= 0.130, "bundle_branch_block_qrs_width");

    signal_synth::clinical_ecg_config advanced_conduction_config = config;
    advanced_conduction_config.rhythm.intraventricular_conduction = signal_synth::clinical_iv_incomplete_rbbb;
    signal_synth::clinical_ecg_record incomplete_rbbb;
    signal_synth::clinical_ecg_generator(advanced_conduction_config).generate(3000, incomplete_rbbb);
    advanced_conduction_config.rhythm.intraventricular_conduction = signal_synth::clinical_iv_nonspecific_delay;
    signal_synth::clinical_ecg_record nonspecific_delay;
    signal_synth::clinical_ecg_generator(advanced_conduction_config).generate(3000, nonspecific_delay);
    signal_synth::clinical_ecg_config wpw_config = config;
    wpw_config.rhythm.preexcitation = signal_synth::clinical_preexcitation_wpw;
    signal_synth::clinical_ecg_record wpw;
    signal_synth::clinical_ecg_generator(wpw_config).generate(3000, wpw);
    ok &= check(incomplete_rbbb.beats()[0].qrs_duration_seconds >= 0.100 && incomplete_rbbb.beats()[0].qrs_duration_seconds < 0.120 && nonspecific_delay.beats()[0].qrs_duration_seconds >= 0.122 && wpw.beats()[0].pr_interval_seconds < 0.120 && wpw.beats()[0].qrs_duration_seconds >= 0.120 && wpw.source_count() == signal_synth::clinical_source_count, "advanced_conduction_low_level_contracts");

    signal_synth::clinical_ecg_config paced_config = config;
    paced_config.rhythm.rhythm = signal_synth::clinical_rhythm_paced;
    signal_synth::clinical_ecg_record paced;
    signal_synth::clinical_ecg_generator(paced_config).generate(3000, paced);
    ok &= check(paced.beats()[0].origin == signal_synth::clinical_origin_paced && !paced.beats()[0].p_present && count_fiducials(paced, signal_synth::clinical_pacing_spike, signal_synth::clinical_fiducial_construction, -1) == paced.beat_count(), "paced_rhythm_and_spike_annotations");

    signal_synth::clinical_ecg_config svt_config = config;
    svt_config.rhythm.rhythm = signal_synth::clinical_rhythm_supraventricular_tachycardia;
    svt_config.rhythm.heart_rate_bpm = 180.0;
    signal_synth::clinical_ecg_record svt;
    signal_synth::clinical_ecg_generator(svt_config).generate(3000, svt);
    signal_synth::clinical_ecg_config vt_config = config;
    vt_config.rhythm.rhythm = signal_synth::clinical_rhythm_ventricular_tachycardia;
    vt_config.rhythm.heart_rate_bpm = 160.0;
    signal_synth::clinical_ecg_record vt;
    signal_synth::clinical_ecg_generator(vt_config).generate(3000, vt);
    ok &= check(!svt.beats()[0].p_present && svt.beats()[0].origin == signal_synth::clinical_origin_conducted && close(svt.beats()[0].qrs_duration_seconds, 0.090) && !vt.beats()[0].p_present && vt.beats()[0].origin == signal_synth::clinical_origin_vt && vt.beats()[0].qrs_duration_seconds >= 0.150, "svt_and_vt_morphology");

    signal_synth::clinical_ecg_config qt_config = config;
    qt_config.rhythm.heart_rate_bpm = 100.0;
    qt_config.timing.qt_correction = signal_synth::clinical_qt_fixed;
    qt_config.timing.qt_interval_ms = 380.0;
    const double fixed_qt = first_qt(qt_config);
    qt_config.timing.qt_correction = signal_synth::clinical_qt_bazett;
    const double bazett_qt = first_qt(qt_config);
    qt_config.timing.qt_correction = signal_synth::clinical_qt_fridericia;
    const double fridericia_qt = first_qt(qt_config);
    qt_config.timing.qt_correction = signal_synth::clinical_qt_framingham;
    const double framingham_qt = first_qt(qt_config);
    qt_config.timing.qt_correction = signal_synth::clinical_qt_hodges;
    const double hodges_qt = first_qt(qt_config);
    ok &= check(close(fixed_qt, 0.380) && close(bazett_qt, 0.4 * std::sqrt(0.6)) && close(fridericia_qt, 0.4 * std::cbrt(0.6)) && close(framingham_qt, 0.4 + 0.154 * (0.6 - 1.0)) && close(hodges_qt, 0.4 - 0.00175 * 40.0), "all_qt_corrections");

    signal_synth::clinical_ecg_config st_config = config;
    st_config.morphology.st_j_amplitude_mv = 0.20;
    st_config.morphology.t_amplitude_mv = 0.0;
    signal_synth::clinical_ecg_record st;
    signal_synth::clinical_ecg_generator(st_config).generate(1500, st);
    const unsigned int t_onset_sample = static_cast<unsigned int>(std::floor(st.beats()[0].t_onset_time_seconds * st.sampling_rate_hz()));
    const unsigned int j_after_sample = static_cast<unsigned int>(std::ceil(st.beats()[0].j_point_time_seconds * st.sampling_rate_hz()));
    const double* st_lead = st.lead_data(signal_synth::clinical_lead_ii);
    const double* injury_x = st.source_data(signal_synth::clinical_source_injury, signal_synth::clinical_axis_x);
    ok &= check(std::fabs(injury_x[j_after_sample] - injury_x[j_after_sample - 1]) < 0.25 * std::fabs(injury_x[j_after_sample]), "terminal_qrs_to_j_transition_has_no_level_step");
    ok &= check(std::fabs(st_lead[t_onset_sample + 1] - st_lead[t_onset_sample]) < 0.02, "st_to_t_transition_is_continuous");

    std::cout << (ok ? "All clinical ECG tests passed.\n" : "Clinical ECG test failure.\n");
    return ok ? 0 : 1;
}
