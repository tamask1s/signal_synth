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
        if (!(finite(config.pulse_delay_ms) && config.pulse_delay_ms >= 0.0 && config.pulse_delay_ms <= 2000.0
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
            && finite(config.clock_drift_ppm) && std::fabs(config.clock_drift_ppm) <= 100000.0
            && finite(config.pulse_delay_jitter_ms) && config.pulse_delay_jitter_ms >= 0.0 && config.pulse_delay_jitter_ms <= 1000.0
            && finite(config.low_frequency_amplitude_modulation_ratio) && config.low_frequency_amplitude_modulation_ratio >= 0.0 && config.low_frequency_amplitude_modulation_ratio <= 0.95
            && finite(config.low_frequency_amplitude_modulation_hz) && config.low_frequency_amplitude_modulation_hz > 0.0 && config.low_frequency_amplitude_modulation_hz <= 1.0
            && finite(config.rise_time_variation_ratio) && config.rise_time_variation_ratio >= 0.0 && config.rise_time_variation_ratio <= 0.9
            && finite(config.decay_time_variation_ratio) && config.decay_time_variation_ratio >= 0.0 && config.decay_time_variation_ratio <= 0.9
            && finite(config.pac_pulse_amplitude_scale) && config.pac_pulse_amplitude_scale >= 0.0 && config.pac_pulse_amplitude_scale <= 1.0
            && finite(config.pvc_pulse_amplitude_scale) && config.pvc_pulse_amplitude_scale >= 0.0 && config.pvc_pulse_amplitude_scale <= 1.0
            && finite(config.paced_pulse_amplitude_scale) && config.paced_pulse_amplitude_scale >= 0.0 && config.paced_pulse_amplitude_scale <= 1.0
            && config.pulse_delay_ms >= config.pulse_delay_variation_ms + config.pulse_delay_jitter_ms
            && config.perfusion_episodes.size() <= 64u))
            return false;
        for (std::size_t i = 0; i < config.perfusion_episodes.size(); ++i)
        {
            const signal_synth::ppg_perfusion_episode_config& episode = config.perfusion_episodes[i];
            if (!finite(episode.start_seconds) || !finite(episode.duration_seconds) || !finite(episode.amplitude_scale)
                || !finite(episode.rise_time_scale) || !finite(episode.decay_time_scale) || !finite(episode.weak_pulse_amplitude_scale)
                || episode.start_seconds < 0.0 || episode.duration_seconds <= 0.0
                || episode.amplitude_scale <= 0.0 || episode.amplitude_scale > 1.0
                || episode.rise_time_scale < 0.25 || episode.rise_time_scale > 4.0
                || episode.decay_time_scale < 0.25 || episode.decay_time_scale > 4.0
                || episode.weak_pulse_amplitude_scale <= 0.0 || episode.weak_pulse_amplitude_scale > 1.0)
                return false;
            const double end = episode.start_seconds + episode.duration_seconds;
            if (!finite(end))
                return false;
            for (std::size_t previous = 0; previous < i; ++previous)
            {
                const signal_synth::ppg_perfusion_episode_config& other = config.perfusion_episodes[previous];
                const double other_end = other.start_seconds + other.duration_seconds;
                if (episode.start_seconds < other_end && other.start_seconds < end)
                    return false;
            }
        }
        double maximum_rise_scale = 1.0;
        double maximum_decay_scale = 1.0;
        for (std::size_t i = 0; i < config.perfusion_episodes.size(); ++i)
        {
            maximum_rise_scale = std::max(maximum_rise_scale, config.perfusion_episodes[i].rise_time_scale);
            maximum_decay_scale = std::max(maximum_decay_scale, config.perfusion_episodes[i].decay_time_scale);
        }
        if (config.rise_time_ms * (1.0 + config.rise_time_variation_ratio) * maximum_rise_scale
            + config.decay_time_ms * (1.0 + config.decay_time_variation_ratio) * maximum_decay_scale > 5000.0)
            return false;
        return true;
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

    double deterministic_signed(unsigned long long seed, unsigned long long index, unsigned long long stream)
    {
        return 2.0 * deterministic_unit(seed ^ (0x9e3779b97f4a7c15ULL * (index + 1u)) ^ stream) - 1.0;
    }

    unsigned long long sample_at_or_after(double time_seconds, unsigned int sampling_rate_hz)
    {
        const double sample = std::ceil(time_seconds * sampling_rate_hz - 1e-12);
        return sample <= 0.0 ? 0u : static_cast<unsigned long long>(sample);
    }

    double pulse_value(double time, double onset, double peak, double offset, double amplitude, double dicrotic_delay_seconds, double dicrotic_width_seconds, double dicrotic_amplitude_ratio)
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
        if (dicrotic_amplitude_ratio > 0.0)
        {
            const double center = peak + dicrotic_delay_seconds;
            const double sigma = dicrotic_width_seconds / 2.3548200450309493;
            const double normalized = (time - center) / sigma;
            dicrotic = dicrotic_amplitude_ratio * std::exp(-0.5 * normalized * normalized);
        }
        return amplitude * (primary + dicrotic);
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

    int perfusion_episode_index_at(const signal_synth::ppg_config& config, double time_seconds)
    {
        for (std::size_t i = 0; i < config.perfusion_episodes.size(); ++i)
            if (time_seconds >= config.perfusion_episodes[i].start_seconds && time_seconds < config.perfusion_episodes[i].start_seconds + config.perfusion_episodes[i].duration_seconds)
                return static_cast<int>(i);
        return -1;
    }

    double arrhythmia_pulse_scale(const signal_synth::ppg_config& config, const signal_synth::clinical_beat_annotation& beat)
    {
        switch (beat.origin)
        {
        case signal_synth::clinical_origin_pac:
            return config.pac_pulse_amplitude_scale;
        case signal_synth::clinical_origin_pvc:
        case signal_synth::clinical_origin_ventricular_escape:
        case signal_synth::clinical_origin_vt:
            return config.pvc_pulse_amplitude_scale;
        case signal_synth::clinical_origin_paced:
        case signal_synth::clinical_origin_atrial_paced:
            return config.paced_pulse_amplitude_scale;
        case signal_synth::clinical_origin_conducted:
        case signal_synth::clinical_origin_junctional_escape:
            break;
        }
        return 1.0;
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

    const char* ppg_pulse_state_name(ppg_pulse_state state)
    {
        switch (state)
        {
        case ppg_pulse_valid: return "valid";
        case ppg_pulse_weak: return "weak";
        case ppg_pulse_missing: return "missing";
        case ppg_pulse_out_of_record: return "out_of_record";
        }
        return "unknown";
    }

    ppg_perfusion_episode_config::ppg_perfusion_episode_config()
        : start_seconds(0.0), duration_seconds(1.0), amplitude_scale(0.35), rise_time_scale(1.0), decay_time_scale(1.0), weak_pulse_every_n_beats(0), weak_pulse_amplitude_scale(0.35), missing_pulse_every_n_beats(0)
    {
    }

    ppg_config::ppg_config()
        : enabled(false), pulse_delay_ms(180.0), rise_time_ms(120.0), decay_time_ms(300.0), amplitude_au(1.0), baseline_au(0.0), dicrotic_delay_ms(180.0), dicrotic_width_ms(80.0), dicrotic_amplitude_ratio(0.15), pulse_delay_variation_ms(0.0), pulse_delay_variation_hz(0.0), missing_pulse_every_n_beats(0), clock_drift_ppm(0.0), pulse_delay_jitter_ms(0.0), low_frequency_amplitude_modulation_ratio(0.0), low_frequency_amplitude_modulation_hz(0.1), rise_time_variation_ratio(0.0), decay_time_variation_ratio(0.0), pac_pulse_amplitude_scale(1.0), pvc_pulse_amplitude_scale(1.0), paced_pulse_amplitude_scale(1.0), seed(0x5050475f53545253ULL), perfusion_episodes()
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
            for (std::size_t i = 0; i < config_.perfusion_episodes.size(); ++i)
                if (config_.perfusion_episodes[i].start_seconds + config_.perfusion_episodes[i].duration_seconds > record_end + 1.0 / timeline.sampling_rate_hz())
                    return false;
            const double variation_phase = 2.0 * pi * deterministic_unit(config_.seed);
            const double amplitude_phase = 2.0 * pi * deterministic_unit(config_.seed ^ 0x4c465f414d505f31ULL);
            std::vector<unsigned int> episode_pulse_counts(config_.perfusion_episodes.size(), 0u);
            for (unsigned int beat_index = 0; beat_index < timeline.beat_count(); ++beat_index)
            {
                const clinical_beat_annotation& beat = timeline.beats()[beat_index];
                const int episode_index = perfusion_episode_index_at(config_, beat.r_peak_time_seconds);
                const ppg_perfusion_episode_config* perfusion = episode_index >= 0 ? &config_.perfusion_episodes[static_cast<std::size_t>(episode_index)] : 0;
                const unsigned int episode_pulse_index = episode_index >= 0 ? ++episode_pulse_counts[static_cast<std::size_t>(episode_index)] : 0u;
                const bool globally_missing = config_.missing_pulse_every_n_beats != 0 && (beat_index + 1u) % config_.missing_pulse_every_n_beats == 0;
                const bool episode_missing = perfusion && perfusion->missing_pulse_every_n_beats != 0 && episode_pulse_index % perfusion->missing_pulse_every_n_beats == 0;
                const bool weak = perfusion && perfusion->weak_pulse_every_n_beats != 0 && episode_pulse_index % perfusion->weak_pulse_every_n_beats == 0;
                const double arrhythmia_scale = arrhythmia_pulse_scale(config_, beat);
                const bool arrhythmia_linked = arrhythmia_scale < 1.0;
                const bool arrhythmia_missing = arrhythmia_scale == 0.0;
                const bool arrhythmia_weak = arrhythmia_scale > 0.0 && arrhythmia_scale < 1.0;
                const double variable_delay_ms = config_.pulse_delay_variation_ms * std::sin(2.0 * pi * config_.pulse_delay_variation_hz * beat.r_peak_time_seconds + variation_phase);
                const double jitter_ms = config_.pulse_delay_jitter_ms * deterministic_signed(config_.seed, beat_index, 0x5054545f4a495454ULL);
                const double drift_seconds = beat.r_peak_time_seconds * config_.clock_drift_ppm * 1e-6;
                const double pulse_delay_seconds = (config_.pulse_delay_ms + variable_delay_ms + jitter_ms) / 1000.0 + drift_seconds;
                if (pulse_delay_seconds < 0.0)
                    return false;
                const double low_frequency_scale = std::max(0.0, 1.0 + config_.low_frequency_amplitude_modulation_ratio * std::sin(2.0 * pi * config_.low_frequency_amplitude_modulation_hz * beat.r_peak_time_seconds + amplitude_phase));
                const double perfusion_amplitude_scale = perfusion ? perfusion->amplitude_scale : 1.0;
                const double weak_amplitude_scale = weak ? perfusion->weak_pulse_amplitude_scale : 1.0;
                const double effective_amplitude = config_.amplitude_au * low_frequency_scale * perfusion_amplitude_scale * weak_amplitude_scale * arrhythmia_scale;
                const double rise_variation = 1.0 + config_.rise_time_variation_ratio * deterministic_signed(config_.seed, beat_index, 0x524953455f564152ULL);
                const double decay_variation = 1.0 + config_.decay_time_variation_ratio * deterministic_signed(config_.seed, beat_index, 0x44454341595f5641ULL);
                const double rise_seconds = config_.rise_time_ms / 1000.0 * rise_variation * (perfusion ? perfusion->rise_time_scale : 1.0);
                const double decay_seconds = config_.decay_time_ms / 1000.0 * decay_variation * (perfusion ? perfusion->decay_time_scale : 1.0);
                const double onset = beat.r_peak_time_seconds + pulse_delay_seconds;
                const double peak = onset + rise_seconds;
                const double offset = peak + decay_seconds;
                ppg_pulse_annotation pulse;
                pulse.ecg_beat_index = beat.beat_index;
                pulse.ecg_r_time_seconds = beat.r_peak_time_seconds;
                pulse.pulse_delay_seconds = pulse_delay_seconds;
                pulse.expected_onset_time_seconds = onset;
                pulse.expected_peak_time_seconds = peak;
                pulse.expected_offset_time_seconds = offset;
                pulse.effective_amplitude_au = effective_amplitude;
                pulse.effective_rise_time_seconds = rise_seconds;
                pulse.effective_decay_time_seconds = decay_seconds;
                pulse.low_perfusion = perfusion != 0;
                pulse.arrhythmia_linked = arrhythmia_linked;
                pulse.arrhythmia_amplitude_scale = arrhythmia_scale;
                pulse.intentionally_missing = globally_missing || episode_missing || arrhythmia_missing;
                pulse.generated = !pulse.intentionally_missing && onset >= 0.0 && offset <= record_end;
                pulse.state = pulse.intentionally_missing ? ppg_pulse_missing : pulse.generated ? ((weak || arrhythmia_weak) ? ppg_pulse_weak : ppg_pulse_valid) : ppg_pulse_out_of_record;
                pulse.valid_for_peak_scoring = pulse.generated;
                fresh.implementation_->pulses.push_back(pulse);
                if (!pulse.generated)
                    continue;
                const unsigned long long first = sample_at_or_after(onset, timeline.sampling_rate_hz());
                const unsigned long long last = sample_at_or_after(offset, timeline.sampling_rate_hz());
                for (unsigned long long sample = first; sample <= last && sample < timeline.sample_count(); ++sample)
                {
                    const double time = static_cast<double>(sample) / timeline.sampling_rate_hz();
                    fresh.implementation_->samples[static_cast<std::size_t>(sample)] += pulse_value(time, onset, peak, offset, effective_amplitude, config_.dicrotic_delay_ms / 1000.0, config_.dicrotic_width_ms / 1000.0, config_.dicrotic_amplitude_ratio);
                }

                add_annotation(fresh.implementation_->annotations, beat, ppg_pulse_onset, ppg_fiducial_construction, onset, config_.baseline_au, timeline.sampling_rate_hz());
                add_annotation(fresh.implementation_->annotations, beat, ppg_systolic_peak, ppg_fiducial_construction, peak, config_.baseline_au + effective_amplitude, timeline.sampling_rate_hz());
                if (config_.dicrotic_amplitude_ratio > 0.0)
                    add_annotation(fresh.implementation_->annotations, beat, ppg_dicrotic_feature, ppg_fiducial_construction, peak + config_.dicrotic_delay_ms / 1000.0, config_.baseline_au + effective_amplitude * config_.dicrotic_amplitude_ratio, timeline.sampling_rate_hz());
                add_annotation(fresh.implementation_->annotations, beat, ppg_pulse_offset, ppg_fiducial_construction, offset, config_.baseline_au, timeline.sampling_rate_hz());
                add_annotation(fresh.implementation_->annotations, beat, ppg_pulse_onset, ppg_fiducial_measurement, static_cast<double>(first) / timeline.sampling_rate_hz(), fresh.implementation_->samples[static_cast<std::size_t>(first)], timeline.sampling_rate_hz());

                unsigned long long measured_sample = first;
                for (unsigned long long sample = first + 1; sample <= last && sample < timeline.sample_count(); ++sample)
                    if (fresh.implementation_->samples[static_cast<std::size_t>(sample)] > fresh.implementation_->samples[static_cast<std::size_t>(measured_sample)])
                        measured_sample = sample;
                add_annotation(fresh.implementation_->annotations, beat, ppg_systolic_peak, ppg_fiducial_measurement, static_cast<double>(measured_sample) / timeline.sampling_rate_hz(), fresh.implementation_->samples[static_cast<std::size_t>(measured_sample)], timeline.sampling_rate_hz());
                const unsigned long long measured_offset = std::min<unsigned long long>(last, timeline.sample_count() - 1u);
                add_annotation(fresh.implementation_->annotations, beat, ppg_pulse_offset, ppg_fiducial_measurement, static_cast<double>(measured_offset) / timeline.sampling_rate_hz(), fresh.implementation_->samples[static_cast<std::size_t>(measured_offset)], timeline.sampling_rate_hz());
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

    bool remeasure_ppg_fiducials(const double* samples, unsigned int sample_count, ppg_record& record)
    {
        if (!samples || sample_count != record.sample_count() || !record.sampling_rate_hz())
            return false;
        for (std::size_t i = 0; i < record.implementation_->annotations.size(); ++i)
        {
            ppg_annotation& measured = record.implementation_->annotations[i];
            if (measured.source != ppg_fiducial_measurement)
                continue;
            const ppg_annotation* onset = 0;
            const ppg_annotation* offset = 0;
            const ppg_annotation* construction = 0;
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
                if (candidate.kind == measured.kind)
                    construction = &candidate;
            }
            if (!construction)
                return false;
            if (measured.kind != ppg_systolic_peak)
            {
                measured.sample_index = std::min<unsigned long long>(construction->sample_index, sample_count - 1u);
                measured.time_seconds = static_cast<double>(measured.sample_index) / record.sampling_rate_hz();
                measured.value_au = samples[measured.sample_index];
                continue;
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

    bool remeasure_ppg_systolic_peaks(const double* samples, unsigned int sample_count, ppg_record& record)
    {
        return remeasure_ppg_fiducials(samples, sample_count, record);
    }
}
