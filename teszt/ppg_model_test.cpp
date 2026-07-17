#include "../src/ppg_model.h"

#include "../src/clinical_ecg.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace
{
    bool check(bool condition, const char* name)
    {
        if (!condition)
            std::cerr << "FAIL: " << name << '\n';
        return condition;
    }

    bool same_record(const signal_synth::ppg_record& left, const signal_synth::ppg_record& right)
    {
        if (left.sampling_rate_hz() != right.sampling_rate_hz()
            || left.sample_count() != right.sample_count()
            || left.channel_count() != right.channel_count()
            || left.annotation_count() != right.annotation_count()
            || left.pulse_count() != right.pulse_count())
            return false;
        for (unsigned int channel = 0; channel < left.channel_count(); ++channel)
        {
            if (left.channel_kind(channel) != right.channel_kind(channel)
                || left.channel_annotation_count(channel) != right.channel_annotation_count(channel)
                || left.channel_dc_au(channel) != right.channel_dc_au(channel)
                || left.channel_sensor_gain(channel) != right.channel_sensor_gain(channel)
                || left.channel_delay_ms(channel) != right.channel_delay_ms(channel)
                || left.channel_noise_std_au(channel) != right.channel_noise_std_au(channel)
                || left.channel_seed(channel) != right.channel_seed(channel))
                return false;
            if (left.sample_count() && std::memcmp(left.channel_samples(channel), right.channel_samples(channel), left.sample_count() * sizeof(double)) != 0)
                return false;
            for (unsigned int i = 0; i < left.channel_annotation_count(channel); ++i)
            {
                const signal_synth::ppg_annotation& a = left.channel_annotations(channel)[i];
                const signal_synth::ppg_annotation& b = right.channel_annotations(channel)[i];
                if (a.ecg_beat_index != b.ecg_beat_index || a.ecg_r_time_seconds != b.ecg_r_time_seconds || a.kind != b.kind || a.source != b.source || a.sample_index != b.sample_index || a.time_seconds != b.time_seconds || a.value_au != b.value_au)
                    return false;
            }
        }
        for (unsigned int i = 0; i < left.pulse_count(); ++i)
        {
            const signal_synth::ppg_pulse_annotation& a = left.pulses()[i];
            const signal_synth::ppg_pulse_annotation& b = right.pulses()[i];
            if (a.ecg_beat_index != b.ecg_beat_index || a.ecg_r_time_seconds != b.ecg_r_time_seconds
                || a.pulse_delay_seconds != b.pulse_delay_seconds || a.expected_onset_time_seconds != b.expected_onset_time_seconds
                || a.expected_peak_time_seconds != b.expected_peak_time_seconds || a.expected_offset_time_seconds != b.expected_offset_time_seconds
                || a.effective_amplitude_au != b.effective_amplitude_au || a.effective_rise_time_seconds != b.effective_rise_time_seconds
                || a.effective_decay_time_seconds != b.effective_decay_time_seconds || a.state != b.state
                || a.low_perfusion != b.low_perfusion || a.arrhythmia_linked != b.arrhythmia_linked || a.arrhythmia_amplitude_scale != b.arrhythmia_amplitude_scale
                || a.valid_for_peak_scoring != b.valid_for_peak_scoring
                || a.generated != b.generated || a.intentionally_missing != b.intentionally_missing)
                return false;
        }
        return true;
    }

    const signal_synth::ppg_annotation* find_annotation(const signal_synth::ppg_record& record, unsigned long long beat, signal_synth::ppg_fiducial_kind kind, signal_synth::ppg_fiducial_source source)
    {
        for (unsigned int i = 0; i < record.annotation_count(); ++i)
            if (record.annotations()[i].ecg_beat_index == beat && record.annotations()[i].kind == kind && record.annotations()[i].source == source)
                return &record.annotations()[i];
        return 0;
    }

    const signal_synth::ppg_annotation* find_channel_annotation(const signal_synth::ppg_record& record, unsigned int channel, unsigned long long beat, signal_synth::ppg_fiducial_kind kind, signal_synth::ppg_fiducial_source source)
    {
        const signal_synth::ppg_annotation* annotations = record.channel_annotations(channel);
        for (unsigned int i = 0; i < record.channel_annotation_count(channel); ++i)
            if (annotations[i].ecg_beat_index == beat && annotations[i].kind == kind && annotations[i].source == source)
                return &annotations[i];
        return 0;
    }

    bool run_rate(unsigned int rate)
    {
        signal_synth::clinical_ecg_config ecg_config;
        ecg_config.sampling_rate_hz = rate;
        signal_synth::clinical_ecg_record ecg;
        if (!signal_synth::clinical_ecg_generator(ecg_config).generate(rate * 10, ecg))
            return false;
        signal_synth::ppg_config config;
        config.enabled = true;
        signal_synth::ppg_record ppg;
        if (!signal_synth::ppg_generator(config).generate(ecg, ppg))
            return false;
        if (ppg.sample_count() != ecg.sample_count() || !ppg.annotation_count())
            return false;
        for (unsigned int i = 0; i < ppg.annotation_count(); ++i)
        {
            const signal_synth::ppg_annotation& annotation = ppg.annotations()[i];
            if (annotation.sample_index >= ppg.sample_count() || annotation.ecg_beat_index >= ecg.beat_count())
                return false;
            if (annotation.ecg_r_time_seconds != ecg.beats()[static_cast<unsigned int>(annotation.ecg_beat_index)].r_peak_time_seconds)
                return false;
            if (annotation.source == signal_synth::ppg_fiducial_measurement)
            {
                if (annotation.value_au != ppg.samples()[annotation.sample_index])
                    return false;
                if (annotation.kind == signal_synth::ppg_systolic_peak)
                {
                    const unsigned long long left = annotation.sample_index ? annotation.sample_index - 1 : annotation.sample_index;
                    const unsigned long long right = std::min<unsigned long long>(ppg.sample_count() - 1, annotation.sample_index + 1);
                    if (ppg.samples()[annotation.sample_index] < ppg.samples()[left] || ppg.samples()[annotation.sample_index] < ppg.samples()[right])
                        return false;
                }
            }
        }
        const signal_synth::ppg_annotation* onset = find_annotation(ppg, ppg.annotations()[0].ecg_beat_index, signal_synth::ppg_pulse_onset, signal_synth::ppg_fiducial_construction);
        return onset && std::fabs((onset->time_seconds - onset->ecg_r_time_seconds) * 1000.0 - config.pulse_delay_ms) < 1e-9;
    }
}

int main()
{
    bool ok = true;
    signal_synth::clinical_ecg_config ecg_config;
    signal_synth::clinical_ecg_record ecg;
    ok &= check(signal_synth::clinical_ecg_generator(ecg_config).generate(5000, ecg), "ecg_timeline_generation");

    signal_synth::ppg_config config;
    config.enabled = true;
    signal_synth::ppg_generator generator(config);
    signal_synth::ppg_record first;
    signal_synth::ppg_record second;
    ok &= check(generator.valid() && generator.generate(ecg, first) && generator.generate(ecg, second), "ppg_generation");
    ok &= check(same_record(first, second), "ppg_is_deterministic");
    ok &= check(first.sample_count() == ecg.sample_count() && std::string(first.channel_name()) == "ppg_green" && std::string(first.unit()) == "a.u.", "ppg_public_contract");
    ok &= check(first.channel_count() == 1u && std::string(first.channel_name(0)) == "ppg_green" && first.channel_kind(0) == signal_synth::ppg_channel_green, "green_channel_contract");
    ok &= check(first.annotation_count() > 0 && first.annotation_count() % 7 == 0, "construction_and_measured_annotations");
    std::vector<double> modified(first.samples(), first.samples() + first.sample_count());
    const unsigned long long first_beat = first.annotations()[0].ecg_beat_index;
    const signal_synth::ppg_annotation* first_onset = find_annotation(first, first_beat, signal_synth::ppg_pulse_onset, signal_synth::ppg_fiducial_construction);
    const unsigned long long forced_peak = first_onset ? first_onset->sample_index + 1u : 0u;
    if (forced_peak < modified.size())
        modified[static_cast<std::size_t>(forced_peak)] = 100.0;
    ok &= check(first_onset && signal_synth::remeasure_ppg_systolic_peaks(modified.data(), static_cast<unsigned int>(modified.size()), first)
        && find_annotation(first, first_beat, signal_synth::ppg_systolic_peak, signal_synth::ppg_fiducial_measurement)->sample_index == forced_peak, "remeasure_final_waveform_peak");

    signal_synth::ppg_config disabled = config;
    disabled.enabled = false;
    signal_synth::ppg_record disabled_record;
    ok &= check(signal_synth::ppg_generator(disabled).generate(ecg, disabled_record) && disabled_record.sample_count() == 0 && disabled_record.annotation_count() == 0, "disabled_ppg_is_empty");

    signal_synth::ppg_config invalid = config;
    invalid.rise_time_ms = std::numeric_limits<double>::quiet_NaN();
    signal_synth::ppg_record preserved = first;
    ok &= check(!signal_synth::ppg_generator(invalid).valid() && !signal_synth::ppg_generator(invalid).generate(ecg, preserved) && same_record(preserved, first), "invalid_generation_is_transactional");
    invalid = config;
    invalid.dicrotic_amplitude_ratio = 1.1;
    ok &= check(!signal_synth::ppg_generator(invalid).valid(), "invalid_dicrotic_ratio");
    invalid = config;
    invalid.dicrotic_delay_ms = invalid.decay_time_ms + 1.0;
    ok &= check(!signal_synth::ppg_generator(invalid).valid(), "dicrotic_feature_must_precede_offset");

    signal_synth::ppg_generator reconfigured(config);
    invalid = config;
    invalid.decay_time_ms = 6000.0;
    ok &= check(!reconfigured.configure(invalid) && reconfigured.valid() && reconfigured.config().decay_time_ms == config.decay_time_ms, "invalid_reconfigure_is_transactional");
    ok &= check(run_rate(250) && run_rate(500) && run_rate(1000), "timing_and_peaks_across_sample_rates");

    signal_synth::ppg_config optical_config = config;
    optical_config.optical.enabled = true;
    optical_config.optical.red.delay_ms = 20.0;
    optical_config.optical.red.dc_au = 0.9;
    optical_config.optical.red.noise_std_au = 0.001;
    optical_config.optical.infrared.delay_ms = 30.0;
    optical_config.optical.infrared.dc_au = 1.3;
    signal_synth::ppg_record optical;
    signal_synth::ppg_record optical_repeat;
    ok &= check(signal_synth::ppg_generator(optical_config).generate(ecg, optical)
        && signal_synth::ppg_generator(optical_config).generate(ecg, optical_repeat)
        && same_record(optical, optical_repeat), "optical_multichannel_generation");
    ok &= check(optical.channel_count() == 3u
        && std::string(optical.channel_name(1)) == "ppg_red"
        && std::string(optical.channel_name(2)) == "ppg_infrared"
        && optical.channel_samples(1) && optical.channel_samples(2), "optical_channel_contract");
    const unsigned long long optical_beat = optical.annotations()[0].ecg_beat_index;
    const signal_synth::ppg_annotation* green_peak = find_channel_annotation(optical, 0, optical_beat, signal_synth::ppg_systolic_peak, signal_synth::ppg_fiducial_construction);
    const signal_synth::ppg_annotation* red_peak = find_channel_annotation(optical, 1, optical_beat, signal_synth::ppg_systolic_peak, signal_synth::ppg_fiducial_construction);
    const signal_synth::ppg_annotation* infrared_peak = find_channel_annotation(optical, 2, optical_beat, signal_synth::ppg_systolic_peak, signal_synth::ppg_fiducial_construction);
    ok &= check(green_peak && red_peak && infrared_peak
        && std::fabs((red_peak->time_seconds - green_peak->time_seconds) * 1000.0 - optical_config.optical.red.delay_ms) <= 4.0
        && std::fabs((infrared_peak->time_seconds - green_peak->time_seconds) * 1000.0 - optical_config.optical.infrared.delay_ms) <= 4.0, "optical_channel_delay");
    const signal_synth::ppg_optical_pulse_state* optical_state = optical.optical_states();
    ok &= check(optical_state && optical.optical_state_count() == optical.pulse_count()
        && std::fabs(optical_state[0].infrared_ac_au / optical_state[0].infrared_dc_au * 100.0 - optical_state[0].infrared_perfusion_index_percent) < 1e-12
        && std::fabs((optical_state[0].red_ac_au / optical_state[0].red_dc_au) / (optical_state[0].infrared_ac_au / optical_state[0].infrared_dc_au) - optical_state[0].ratio_of_ratios) < 1e-12, "optical_ac_dc_equations");
    const signal_synth::ppg_annotation* measured_red_peak = find_channel_annotation(optical, 1, optical_beat, signal_synth::ppg_systolic_peak, signal_synth::ppg_fiducial_measurement);
    ok &= check(measured_red_peak && measured_red_peak->sample_index < optical.sample_count()
        && measured_red_peak->value_au == optical.channel_samples(1)[measured_red_peak->sample_index], "optical_fiducials_reference_channel_samples");
    invalid = optical_config;
    invalid.optical.red.delay_ms = -1.0;
    ok &= check(!signal_synth::ppg_generator(invalid).valid(), "invalid_red_delay");

    signal_synth::clinical_ecg_config ectopic_config;
    ectopic_config.scenario.premature_every_n_beats = 3;
    ectopic_config.scenario.premature_origin = signal_synth::clinical_origin_pvc;
    signal_synth::clinical_ecg_record ectopic_ecg;
    signal_synth::ppg_config pulse_loss_config;
    pulse_loss_config.enabled = true;
    pulse_loss_config.pvc_pulse_amplitude_scale = 0.0;
    signal_synth::ppg_record pulse_loss;
    ok &= check(signal_synth::clinical_ecg_generator(ectopic_config).generate(10000, ectopic_ecg)
        && signal_synth::ppg_generator(pulse_loss_config).generate(ectopic_ecg, pulse_loss), "arrhythmia_timeline_ppg_generation");
    unsigned int linked_missing = 0;
    bool linked_has_fiducial = false;
    for (unsigned int i = 0; i < pulse_loss.pulse_count(); ++i)
    {
        const signal_synth::ppg_pulse_annotation& pulse = pulse_loss.pulses()[i];
        if (!pulse.arrhythmia_linked)
            continue;
        linked_missing += pulse.state == signal_synth::ppg_pulse_missing && pulse.intentionally_missing && !pulse.generated && pulse.arrhythmia_amplitude_scale == 0.0 ? 1u : 0u;
        for (unsigned int annotation = 0; annotation < pulse_loss.annotation_count(); ++annotation)
            linked_has_fiducial = linked_has_fiducial || pulse_loss.annotations()[annotation].ecg_beat_index == pulse.ecg_beat_index;
    }
    ok &= check(linked_missing > 0u && !linked_has_fiducial, "pvc_linked_missing_pulses");
    signal_synth::ppg_config weak_pvc_config = pulse_loss_config;
    weak_pvc_config.pvc_pulse_amplitude_scale = 0.35;
    signal_synth::ppg_record weak_pvc;
    ok &= check(signal_synth::ppg_generator(weak_pvc_config).generate(ectopic_ecg, weak_pvc), "weak_pvc_ppg_generation");
    bool saw_weak_pvc = false;
    for (unsigned int i = 0; i < weak_pvc.pulse_count(); ++i)
        saw_weak_pvc = saw_weak_pvc || (weak_pvc.pulses()[i].arrhythmia_linked && weak_pvc.pulses()[i].state == signal_synth::ppg_pulse_weak && weak_pvc.pulses()[i].generated);
    ok &= check(saw_weak_pvc, "pvc_linked_weak_pulses");
    invalid = config;
    invalid.pvc_pulse_amplitude_scale = -0.01;
    ok &= check(!signal_synth::ppg_generator(invalid).valid(), "invalid_arrhythmia_pulse_scale");

    signal_synth::ppg_record copied = first;
    ok &= check(same_record(copied, first), "record_copy");
    return ok ? 0 : 1;
}
