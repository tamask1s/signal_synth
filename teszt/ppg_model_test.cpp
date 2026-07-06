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
            || left.annotation_count() != right.annotation_count())
            return false;
        if (left.sample_count() && std::memcmp(left.samples(), right.samples(), left.sample_count() * sizeof(double)) != 0)
            return false;
        for (unsigned int i = 0; i < left.annotation_count(); ++i)
        {
            const signal_synth::ppg_annotation& a = left.annotations()[i];
            const signal_synth::ppg_annotation& b = right.annotations()[i];
            if (a.ecg_beat_index != b.ecg_beat_index || a.ecg_r_time_seconds != b.ecg_r_time_seconds || a.kind != b.kind || a.source != b.source || a.sample_index != b.sample_index || a.time_seconds != b.time_seconds || a.value_au != b.value_au)
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
                const unsigned long long left = annotation.sample_index ? annotation.sample_index - 1 : annotation.sample_index;
                const unsigned long long right = std::min<unsigned long long>(ppg.sample_count() - 1, annotation.sample_index + 1);
                if (ppg.samples()[annotation.sample_index] < ppg.samples()[left] || ppg.samples()[annotation.sample_index] < ppg.samples()[right])
                    return false;
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
    ok &= check(first.annotation_count() > 0 && first.annotation_count() % 5 == 0, "construction_and_measured_annotations");
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

    signal_synth::ppg_record copied = first;
    ok &= check(same_record(copied, first), "record_copy");
    return ok ? 0 : 1;
}
