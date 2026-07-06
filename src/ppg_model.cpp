#include "ppg_model.h"

#include "clinical_ecg.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace
{
    const double pi = 3.141592653589793238462643383279502884;

    bool finite(double value)
    {
        return std::isfinite(value);
    }

    bool valid_config(const signal_synth::ppg_config& config)
    {
        return finite(config.pulse_delay_ms) && config.pulse_delay_ms >= 0.0 && config.pulse_delay_ms <= 2000.0
            && finite(config.rise_time_ms) && config.rise_time_ms >= 10.0 && config.rise_time_ms <= 1000.0
            && finite(config.decay_time_ms) && config.decay_time_ms >= 10.0 && config.decay_time_ms <= 3000.0
            && config.rise_time_ms + config.decay_time_ms <= 5000.0
            && finite(config.amplitude_au) && config.amplitude_au > 0.0 && config.amplitude_au <= 100.0
            && finite(config.baseline_au) && std::fabs(config.baseline_au) <= 100.0
            && finite(config.dicrotic_delay_ms) && config.dicrotic_delay_ms >= 0.0 && config.dicrotic_delay_ms <= 1000.0
            && config.dicrotic_delay_ms <= config.decay_time_ms
            && finite(config.dicrotic_width_ms) && config.dicrotic_width_ms >= 1.0 && config.dicrotic_width_ms <= 500.0
            && finite(config.dicrotic_amplitude_ratio) && config.dicrotic_amplitude_ratio >= 0.0 && config.dicrotic_amplitude_ratio <= 1.0
            && finite(config.pulse_delay_variation_ms) && config.pulse_delay_variation_ms >= 0.0 && config.pulse_delay_variation_ms <= 1000.0
            && finite(config.pulse_delay_variation_hz) && config.pulse_delay_variation_hz >= 0.0 && config.pulse_delay_variation_hz <= 10.0
            && finite(config.clock_drift_ppm) && std::fabs(config.clock_drift_ppm) <= 100000.0;
    }

    double deterministic_unit(unsigned long long seed)
    {
        seed ^= seed >> 33;
        seed *= 0xff51afd7ed558ccdULL;
        seed ^= seed >> 33;
        seed *= 0xc4ceb9fe1a85ec53ULL;
        seed ^= seed >> 33;
        return static_cast<double>(seed >> 11) * (1.0 / 9007199254740992.0);
    }

    unsigned long long sample_at_or_after(double time_seconds, unsigned int sampling_rate_hz)
    {
        const double sample = std::ceil(time_seconds * sampling_rate_hz - 1e-12);
        return sample <= 0.0 ? 0u : static_cast<unsigned long long>(sample);
    }

    double pulse_value(double time, double onset, double peak, double offset, const signal_synth::ppg_config& config)
    {
        double primary = 0.0;
        if (time >= onset && time <= peak)
        {
            const double phase = (time - onset) / (peak - onset);
            primary = 0.5 * (1.0 - std::cos(pi * phase));
        }
        else if (time > peak && time <= offset)
        {
            const double phase = (time - peak) / (offset - peak);
            primary = 0.5 * (1.0 + std::cos(pi * phase));
        }

        double dicrotic = 0.0;
        if (config.dicrotic_amplitude_ratio > 0.0)
        {
            const double center = peak + config.dicrotic_delay_ms / 1000.0;
            const double sigma = config.dicrotic_width_ms / 1000.0 / 2.3548200450309493;
            const double normalized = (time - center) / sigma;
            dicrotic = config.dicrotic_amplitude_ratio * std::exp(-0.5 * normalized * normalized);
        }
        return config.amplitude_au * (primary + dicrotic);
    }

    void add_annotation(std::vector<signal_synth::ppg_annotation>& annotations, const signal_synth::clinical_beat_annotation& beat, signal_synth::ppg_fiducial_kind kind, signal_synth::ppg_fiducial_source source, double time, double value, unsigned int sampling_rate)
    {
        signal_synth::ppg_annotation annotation;
        annotation.ecg_beat_index = beat.beat_index;
        annotation.ecg_r_time_seconds = beat.r_peak_time_seconds;
        annotation.kind = kind;
        annotation.source = source;
        annotation.sample_index = sample_at_or_after(time, sampling_rate);
        annotation.time_seconds = time;
        annotation.value_au = value;
        annotations.push_back(annotation);
    }
}

namespace signal_synth
{
    struct ppg_record::implementation
    {
        unsigned int sampling_rate_hz;
        std::vector<double> samples;
        std::vector<ppg_annotation> annotations;
        std::vector<ppg_pulse_annotation> pulses;

        implementation() : sampling_rate_hz(0) {}
    };

    ppg_config::ppg_config()
        : enabled(false), pulse_delay_ms(180.0), rise_time_ms(120.0), decay_time_ms(300.0), amplitude_au(1.0), baseline_au(0.0), dicrotic_delay_ms(180.0), dicrotic_width_ms(80.0), dicrotic_amplitude_ratio(0.15), pulse_delay_variation_ms(0.0), pulse_delay_variation_hz(0.0), missing_pulse_every_n_beats(0), clock_drift_ppm(0.0), seed(0x5050475f53545253ULL)
    {
    }

    ppg_record::ppg_record() : implementation_(new implementation)
    {
    }

    ppg_record::ppg_record(const ppg_record& other) : implementation_(new implementation(*other.implementation_))
    {
    }

    ppg_record& ppg_record::operator=(const ppg_record& other)
    {
        if (this != &other)
            *implementation_ = *other.implementation_;
        return *this;
    }

    ppg_record::~ppg_record()
    {
        delete implementation_;
    }

    unsigned int ppg_record::sampling_rate_hz() const { return implementation_->sampling_rate_hz; }
    unsigned int ppg_record::sample_count() const { return static_cast<unsigned int>(implementation_->samples.size()); }
    const char* ppg_record::channel_name() const { return "ppg_green"; }
    const char* ppg_record::unit() const { return "a.u."; }
    const double* ppg_record::samples() const { return implementation_->samples.empty() ? 0 : &implementation_->samples[0]; }
    unsigned int ppg_record::annotation_count() const { return static_cast<unsigned int>(implementation_->annotations.size()); }
    const ppg_annotation* ppg_record::annotations() const { return implementation_->annotations.empty() ? 0 : &implementation_->annotations[0]; }
    unsigned int ppg_record::pulse_count() const { return static_cast<unsigned int>(implementation_->pulses.size()); }
    const ppg_pulse_annotation* ppg_record::pulses() const { return implementation_->pulses.empty() ? 0 : &implementation_->pulses[0]; }

    ppg_generator::ppg_generator() : config_(), valid_(valid_config(config_))
    {
    }

    ppg_generator::ppg_generator(const ppg_config& config) : config_(config), valid_(valid_config(config))
    {
    }

    bool ppg_generator::configure(const ppg_config& config)
    {
        if (!valid_config(config))
            return false;
        config_ = config;
        valid_ = true;
        return true;
    }

    bool ppg_generator::valid() const { return valid_; }
    const ppg_config& ppg_generator::config() const { return config_; }

    bool ppg_generator::generate(const clinical_ecg_record& timeline, ppg_record& output) const
    {
        if (!valid_ || !timeline.sampling_rate_hz() || !timeline.sample_count())
            return false;
        ppg_record fresh;
        fresh.implementation_->sampling_rate_hz = timeline.sampling_rate_hz();
        if (!config_.enabled)
        {
            output = fresh;
            return true;
        }

        try
        {
            fresh.implementation_->samples.assign(timeline.sample_count(), config_.baseline_au);
            const double record_end = static_cast<double>(timeline.sample_count() - 1) / timeline.sampling_rate_hz();
            const double variation_phase = 2.0 * pi * deterministic_unit(config_.seed);
            for (unsigned int beat_index = 0; beat_index < timeline.beat_count(); ++beat_index)
            {
                const clinical_beat_annotation& beat = timeline.beats()[beat_index];
                const double variable_delay_ms = config_.pulse_delay_variation_ms * std::sin(2.0 * pi * config_.pulse_delay_variation_hz * beat.r_peak_time_seconds + variation_phase);
                const double drift_seconds = beat.r_peak_time_seconds * config_.clock_drift_ppm * 1e-6;
                const double pulse_delay_seconds = (config_.pulse_delay_ms + variable_delay_ms) / 1000.0 + drift_seconds;
                const double onset = beat.r_peak_time_seconds + pulse_delay_seconds;
                const double peak = onset + config_.rise_time_ms / 1000.0;
                const double offset = peak + config_.decay_time_ms / 1000.0;
                ppg_pulse_annotation pulse;
                pulse.ecg_beat_index = beat.beat_index;
                pulse.ecg_r_time_seconds = beat.r_peak_time_seconds;
                pulse.pulse_delay_seconds = pulse_delay_seconds;
                pulse.expected_onset_time_seconds = onset;
                pulse.intentionally_missing = config_.missing_pulse_every_n_beats != 0 && (beat_index + 1u) % config_.missing_pulse_every_n_beats == 0;
                pulse.generated = !pulse.intentionally_missing && onset >= 0.0 && offset <= record_end;
                fresh.implementation_->pulses.push_back(pulse);
                if (!pulse.generated)
                    continue;
                const unsigned long long first = sample_at_or_after(onset, timeline.sampling_rate_hz());
                const unsigned long long last = sample_at_or_after(offset, timeline.sampling_rate_hz());
                for (unsigned long long sample = first; sample <= last && sample < timeline.sample_count(); ++sample)
                {
                    const double time = static_cast<double>(sample) / timeline.sampling_rate_hz();
                    fresh.implementation_->samples[static_cast<std::size_t>(sample)] += pulse_value(time, onset, peak, offset, config_);
                }

                add_annotation(fresh.implementation_->annotations, beat, ppg_pulse_onset, ppg_fiducial_construction, onset, config_.baseline_au, timeline.sampling_rate_hz());
                add_annotation(fresh.implementation_->annotations, beat, ppg_systolic_peak, ppg_fiducial_construction, peak, config_.baseline_au + config_.amplitude_au, timeline.sampling_rate_hz());
                if (config_.dicrotic_amplitude_ratio > 0.0)
                    add_annotation(fresh.implementation_->annotations, beat, ppg_dicrotic_feature, ppg_fiducial_construction, peak + config_.dicrotic_delay_ms / 1000.0, config_.baseline_au + config_.amplitude_au * config_.dicrotic_amplitude_ratio, timeline.sampling_rate_hz());
                add_annotation(fresh.implementation_->annotations, beat, ppg_pulse_offset, ppg_fiducial_construction, offset, config_.baseline_au, timeline.sampling_rate_hz());

                unsigned long long measured_sample = first;
                for (unsigned long long sample = first + 1; sample <= last && sample < timeline.sample_count(); ++sample)
                    if (fresh.implementation_->samples[static_cast<std::size_t>(sample)] > fresh.implementation_->samples[static_cast<std::size_t>(measured_sample)])
                        measured_sample = sample;
                add_annotation(fresh.implementation_->annotations, beat, ppg_systolic_peak, ppg_fiducial_measurement, static_cast<double>(measured_sample) / timeline.sampling_rate_hz(), fresh.implementation_->samples[static_cast<std::size_t>(measured_sample)], timeline.sampling_rate_hz());
            }
            for (std::size_t i = 0; i < fresh.implementation_->samples.size(); ++i)
                if (!std::isfinite(fresh.implementation_->samples[i]))
                    return false;
        }
        catch (...)
        {
            return false;
        }
        output = fresh;
        return true;
    }

    bool remeasure_ppg_systolic_peaks(const double* samples, unsigned int sample_count, ppg_record& record)
    {
        if (!samples || sample_count != record.sample_count() || !record.sampling_rate_hz())
            return false;
        for (std::size_t i = 0; i < record.implementation_->annotations.size(); ++i)
        {
            ppg_annotation& measured = record.implementation_->annotations[i];
            if (measured.kind != ppg_systolic_peak || measured.source != ppg_fiducial_measurement)
                continue;
            const ppg_annotation* onset = 0;
            const ppg_annotation* offset = 0;
            for (std::size_t j = i; j > 0; --j)
            {
                const ppg_annotation& candidate = record.implementation_->annotations[j - 1u];
                if (candidate.ecg_beat_index != measured.ecg_beat_index)
                    break;
                if (candidate.source != ppg_fiducial_construction)
                    continue;
                if (candidate.kind == ppg_pulse_onset)
                    onset = &candidate;
                else if (candidate.kind == ppg_pulse_offset)
                    offset = &candidate;
            }
            if (!onset || !offset)
                return false;
            const unsigned long long first = std::min<unsigned long long>(onset->sample_index, sample_count - 1u);
            const unsigned long long last = std::min<unsigned long long>(offset->sample_index, sample_count - 1u);
            if (last < first)
                return false;
            unsigned long long peak = first;
            for (unsigned long long sample = first + 1u; sample <= last; ++sample)
                if (samples[sample] > samples[peak])
                    peak = sample;
            measured.sample_index = peak;
            measured.time_seconds = static_cast<double>(peak) / record.sampling_rate_hz();
            measured.value_au = samples[peak];
        }
        return true;
    }
}
