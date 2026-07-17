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
        const signal_synth::ppg_optical_channel_config* optical[] = {&config.red, &config.infrared};
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
        for (std::size_t channel = 0; channel < sizeof(optical) / sizeof(optical[0]); ++channel)
        {
            const signal_synth::ppg_optical_channel_config& opt = *optical[channel];
            if (!(finite(opt.amplitude_gain) && opt.amplitude_gain > 0.0 && opt.amplitude_gain <= 100.0
                && finite(opt.baseline_au) && std::fabs(opt.baseline_au) <= 100.0
                && finite(opt.delay_ms) && opt.delay_ms >= 0.0 && opt.delay_ms <= 2000.0
                && finite(opt.noise_std_au) && opt.noise_std_au >= 0.0 && opt.noise_std_au <= 100.0))
                return false;
            if (opt.enabled && !config.enabled)
                return false;
        }
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

    bool remeasure_annotation_vector(const double* samples, unsigned int sample_count, unsigned int sampling_rate_hz, std::vector<signal_synth::ppg_annotation>& annotations)
    {
        if (!samples || !sample_count || !sampling_rate_hz)
            return false;
        for (std::size_t i = 0; i < annotations.size(); ++i)
        {
            signal_synth::ppg_annotation& measured = annotations[i];
            if (measured.source != signal_synth::ppg_fiducial_measurement)
                continue;
            const signal_synth::ppg_annotation* onset = 0;
            const signal_synth::ppg_annotation* offset = 0;
            const signal_synth::ppg_annotation* construction = 0;
            for (std::size_t j = i; j > 0; --j)
            {
                const signal_synth::ppg_annotation& candidate = annotations[j - 1u];
                if (candidate.ecg_beat_index != measured.ecg_beat_index)
                    break;
                if (candidate.source != signal_synth::ppg_fiducial_construction)
                    continue;
                if (candidate.kind == signal_synth::ppg_pulse_onset)
                    onset = &candidate;
                else if (candidate.kind == signal_synth::ppg_pulse_offset)
                    offset = &candidate;
                if (candidate.kind == measured.kind)
                    construction = &candidate;
            }
            if (!construction)
                return false;
            if (measured.kind != signal_synth::ppg_systolic_peak)
            {
                measured.sample_index = std::min<unsigned long long>(construction->sample_index, sample_count - 1u);
                measured.time_seconds = static_cast<double>(measured.sample_index) / sampling_rate_hz;
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
            measured.time_seconds = static_cast<double>(peak) / sampling_rate_hz;
            measured.value_au = samples[peak];
        }
        return true;
    }
}

namespace signal_synth
{
    struct ppg_record::implementation
    {
        struct channel_data
        {
            ppg_channel_kind kind;
            double amplitude_gain;
            double baseline_au;
            double delay_ms;
            double noise_std_au;
            unsigned long long seed;
            std::vector<double> samples;
            std::vector<ppg_annotation> annotations;

            channel_data()
                : kind(ppg_channel_green), amplitude_gain(1.0), baseline_au(0.0), delay_ms(0.0), noise_std_au(0.0), seed(0), samples(), annotations()
            {
            }
        };

        unsigned int sampling_rate_hz;
        std::vector<channel_data> channels;
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

    const char* ppg_channel_kind_name(ppg_channel_kind kind)
    {
        switch (kind)
        {
        case ppg_channel_green: return "ppg_green";
        case ppg_channel_red: return "ppg_red";
        case ppg_channel_infrared: return "ppg_infrared";
        }
        return "unknown";
    }

    ppg_optical_channel_config::ppg_optical_channel_config()
        : enabled(false), amplitude_gain(1.0), baseline_au(0.0), delay_ms(0.0), noise_std_au(0.0), seed(0x5050475f4f505431ULL)
    {
    }

    ppg_perfusion_episode_config::ppg_perfusion_episode_config()
        : start_seconds(0.0), duration_seconds(1.0), amplitude_scale(0.35), rise_time_scale(1.0), decay_time_scale(1.0), weak_pulse_every_n_beats(0), weak_pulse_amplitude_scale(0.35), missing_pulse_every_n_beats(0)
    {
    }

    ppg_config::ppg_config()
        : enabled(false), pulse_delay_ms(180.0), rise_time_ms(120.0), decay_time_ms(300.0), amplitude_au(1.0), baseline_au(0.0), dicrotic_delay_ms(180.0), dicrotic_width_ms(80.0), dicrotic_amplitude_ratio(0.15), pulse_delay_variation_ms(0.0), pulse_delay_variation_hz(0.0), missing_pulse_every_n_beats(0), pulse_delay_jitter_ms(0.0), low_frequency_amplitude_modulation_ratio(0.0), low_frequency_amplitude_modulation_hz(0.1), rise_time_variation_ratio(0.0), decay_time_variation_ratio(0.0), pac_pulse_amplitude_scale(1.0), pvc_pulse_amplitude_scale(1.0), paced_pulse_amplitude_scale(1.0), seed(0x5050475f53545253ULL), red(), infrared(), perfusion_episodes()
    {
        red.amplitude_gain = 0.85;
        red.baseline_au = 0.10;
        red.delay_ms = 8.0;
        red.seed = 0x5050475f52454431ULL;
        infrared.amplitude_gain = 1.15;
        infrared.baseline_au = 0.20;
        infrared.delay_ms = 12.0;
        infrared.seed = 0x5050475f49523131ULL;
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
    unsigned int ppg_record::sample_count() const { return implementation_->channels.empty() ? 0u : static_cast<unsigned int>(implementation_->channels[0].samples.size()); }
    const char* ppg_record::channel_name() const { return "ppg_green"; }
    const char* ppg_record::unit() const { return "a.u."; }
    const double* ppg_record::samples() const { return channel_samples(0); }
    unsigned int ppg_record::channel_count() const { return static_cast<unsigned int>(implementation_->channels.size()); }
    ppg_channel_kind ppg_record::channel_kind(unsigned int channel_index) const { return channel_index < implementation_->channels.size() ? implementation_->channels[channel_index].kind : ppg_channel_green; }
    const char* ppg_record::channel_name(unsigned int channel_index) const { return ppg_channel_kind_name(channel_kind(channel_index)); }
    const char* ppg_record::channel_unit(unsigned int) const { return "a.u."; }
    const double* ppg_record::channel_samples(unsigned int channel_index) const
    {
        if (channel_index >= implementation_->channels.size() || implementation_->channels[channel_index].samples.empty())
            return 0;
        return &implementation_->channels[channel_index].samples[0];
    }
    double ppg_record::channel_amplitude_gain(unsigned int channel_index) const { return channel_index < implementation_->channels.size() ? implementation_->channels[channel_index].amplitude_gain : 0.0; }
    double ppg_record::channel_baseline_au(unsigned int channel_index) const { return channel_index < implementation_->channels.size() ? implementation_->channels[channel_index].baseline_au : 0.0; }
    double ppg_record::channel_delay_ms(unsigned int channel_index) const { return channel_index < implementation_->channels.size() ? implementation_->channels[channel_index].delay_ms : 0.0; }
    double ppg_record::channel_noise_std_au(unsigned int channel_index) const { return channel_index < implementation_->channels.size() ? implementation_->channels[channel_index].noise_std_au : 0.0; }
    unsigned long long ppg_record::channel_seed(unsigned int channel_index) const { return channel_index < implementation_->channels.size() ? implementation_->channels[channel_index].seed : 0u; }
    unsigned int ppg_record::channel_annotation_count(unsigned int channel_index) const { return channel_index < implementation_->channels.size() ? static_cast<unsigned int>(implementation_->channels[channel_index].annotations.size()) : 0u; }
    const ppg_annotation* ppg_record::channel_annotations(unsigned int channel_index) const
    {
        if (channel_index >= implementation_->channels.size() || implementation_->channels[channel_index].annotations.empty())
            return 0;
        return &implementation_->channels[channel_index].annotations[0];
    }
    unsigned int ppg_record::annotation_count() const { return channel_annotation_count(0); }
    const ppg_annotation* ppg_record::annotations() const { return channel_annotations(0); }
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
            ppg_record::implementation::channel_data green;
            green.kind = ppg_channel_green;
            green.amplitude_gain = 1.0;
            green.baseline_au = config_.baseline_au;
            green.delay_ms = 0.0;
            green.noise_std_au = 0.0;
            green.seed = config_.seed;
            green.samples.assign(timeline.sample_count(), config_.baseline_au);
            fresh.implementation_->channels.push_back(green);
            if (config_.red.enabled)
            {
                ppg_record::implementation::channel_data red;
                red.kind = ppg_channel_red;
                red.amplitude_gain = config_.red.amplitude_gain;
                red.baseline_au = config_.red.baseline_au;
                red.delay_ms = config_.red.delay_ms;
                red.noise_std_au = config_.red.noise_std_au;
                red.seed = config_.red.seed;
                red.samples.assign(timeline.sample_count(), red.baseline_au);
                fresh.implementation_->channels.push_back(red);
            }
            if (config_.infrared.enabled)
            {
                ppg_record::implementation::channel_data infrared;
                infrared.kind = ppg_channel_infrared;
                infrared.amplitude_gain = config_.infrared.amplitude_gain;
                infrared.baseline_au = config_.infrared.baseline_au;
                infrared.delay_ms = config_.infrared.delay_ms;
                infrared.noise_std_au = config_.infrared.noise_std_au;
                infrared.seed = config_.infrared.seed;
                infrared.samples.assign(timeline.sample_count(), infrared.baseline_au);
                fresh.implementation_->channels.push_back(infrared);
            }
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
                const double pulse_delay_seconds = (config_.pulse_delay_ms + variable_delay_ms + jitter_ms) / 1000.0;
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
                for (std::size_t channel_index = 0; channel_index < fresh.implementation_->channels.size(); ++channel_index)
                {
                    ppg_record::implementation::channel_data& channel = fresh.implementation_->channels[channel_index];
                    const double channel_onset = onset + channel.delay_ms / 1000.0;
                    const double channel_peak = channel_onset + rise_seconds;
                    const double channel_offset = channel_peak + decay_seconds;
                    if (channel_onset < 0.0 || channel_offset > record_end)
                        continue;
                    const double channel_amplitude = effective_amplitude * channel.amplitude_gain;
                    const unsigned long long first = sample_at_or_after(channel_onset, timeline.sampling_rate_hz());
                    const unsigned long long last = sample_at_or_after(channel_offset, timeline.sampling_rate_hz());
                    for (unsigned long long sample = first; sample <= last && sample < timeline.sample_count(); ++sample)
                    {
                        const double time = static_cast<double>(sample) / timeline.sampling_rate_hz();
                        channel.samples[static_cast<std::size_t>(sample)] += pulse_value(time, channel_onset, channel_peak, channel_offset, channel_amplitude, config_.dicrotic_delay_ms / 1000.0, config_.dicrotic_width_ms / 1000.0, config_.dicrotic_amplitude_ratio);
                    }

                    add_annotation(channel.annotations, beat, ppg_pulse_onset, ppg_fiducial_construction, channel_onset, channel.baseline_au, timeline.sampling_rate_hz());
                    add_annotation(channel.annotations, beat, ppg_systolic_peak, ppg_fiducial_construction, channel_peak, channel.baseline_au + channel_amplitude, timeline.sampling_rate_hz());
                    if (config_.dicrotic_amplitude_ratio > 0.0)
                        add_annotation(channel.annotations, beat, ppg_dicrotic_feature, ppg_fiducial_construction, channel_peak + config_.dicrotic_delay_ms / 1000.0, channel.baseline_au + channel_amplitude * config_.dicrotic_amplitude_ratio, timeline.sampling_rate_hz());
                    add_annotation(channel.annotations, beat, ppg_pulse_offset, ppg_fiducial_construction, channel_offset, channel.baseline_au, timeline.sampling_rate_hz());
                    add_annotation(channel.annotations, beat, ppg_pulse_onset, ppg_fiducial_measurement, static_cast<double>(first) / timeline.sampling_rate_hz(), channel.samples[static_cast<std::size_t>(first)], timeline.sampling_rate_hz());

                    unsigned long long measured_sample = first;
                    for (unsigned long long sample = first + 1; sample <= last && sample < timeline.sample_count(); ++sample)
                        if (channel.samples[static_cast<std::size_t>(sample)] > channel.samples[static_cast<std::size_t>(measured_sample)])
                            measured_sample = sample;
                    add_annotation(channel.annotations, beat, ppg_systolic_peak, ppg_fiducial_measurement, static_cast<double>(measured_sample) / timeline.sampling_rate_hz(), channel.samples[static_cast<std::size_t>(measured_sample)], timeline.sampling_rate_hz());
                    const unsigned long long measured_offset = std::min<unsigned long long>(last, timeline.sample_count() - 1u);
                    add_annotation(channel.annotations, beat, ppg_pulse_offset, ppg_fiducial_measurement, static_cast<double>(measured_offset) / timeline.sampling_rate_hz(), channel.samples[static_cast<std::size_t>(measured_offset)], timeline.sampling_rate_hz());
                }
            }
            for (std::size_t channel_index = 0; channel_index < fresh.implementation_->channels.size(); ++channel_index)
            {
                ppg_record::implementation::channel_data& channel = fresh.implementation_->channels[channel_index];
                if (channel.noise_std_au > 0.0)
                    for (std::size_t i = 0; i < channel.samples.size(); ++i)
                        channel.samples[i] += channel.noise_std_au * deterministic_signed(channel.seed ^ 0x4f50545f4e4f4953ULL, static_cast<unsigned long long>(i), static_cast<unsigned long long>(channel.kind) + 1u);
                if (!remeasure_annotation_vector(channel.samples.empty() ? 0 : &channel.samples[0], static_cast<unsigned int>(channel.samples.size()), timeline.sampling_rate_hz(), channel.annotations))
                    return false;
                for (std::size_t i = 0; i < channel.samples.size(); ++i)
                    if (!std::isfinite(channel.samples[i]))
                        return false;
            }
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
        if (record.implementation_->channels.empty())
            return false;
        return remeasure_annotation_vector(samples, sample_count, record.sampling_rate_hz(), record.implementation_->channels[0].annotations);
    }

    bool remeasure_ppg_systolic_peaks(const double* samples, unsigned int sample_count, ppg_record& record)
    {
        return remeasure_ppg_fiducials(samples, sample_count, record);
    }
}
