#include "signal_quality.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
    const double pi = 3.141592653589793238462643383279502884;
    const double two_pi = 2.0 * pi;

    bool is_finite(double value)
    {
        return std::isfinite(value);
    }

    bool is_ecg_type(signal_synth::signal_quality_artifact_type type)
    {
        return !signal_synth::signal_quality_artifact_is_ppg(type);
    }

    bool is_valid_type(signal_synth::signal_quality_artifact_type type)
    {
        switch (type)
        {
        case signal_synth::signal_quality_ecg_baseline_wander:
        case signal_synth::signal_quality_ecg_powerline:
        case signal_synth::signal_quality_ecg_emg_noise:
        case signal_synth::signal_quality_ecg_dropout:
        case signal_synth::signal_quality_ecg_saturation:
        case signal_synth::signal_quality_ppg_dropout:
        case signal_synth::signal_quality_ecg_lead_reversal:
        case signal_synth::signal_quality_ecg_lead_swap:
        case signal_synth::signal_quality_ecg_electrode_misplacement:
        case signal_synth::signal_quality_ecg_gain_mismatch:
        case signal_synth::signal_quality_ecg_offset_drift:
        case signal_synth::signal_quality_ecg_clock_drift:
        case signal_synth::signal_quality_ecg_dropped_samples:
        case signal_synth::signal_quality_ecg_quantization:
        case signal_synth::signal_quality_ecg_adc_clipping:
        case signal_synth::signal_quality_ppg_motion_periodic:
        case signal_synth::signal_quality_ppg_motion_burst:
        case signal_synth::signal_quality_ppg_motion_broadband:
        case signal_synth::signal_quality_ppg_ambient_light:
        case signal_synth::signal_quality_ppg_sensor_saturation:
            return true;
        case signal_synth::signal_quality_ecg_external_noise:
            return false;
        }
        return false;
    }

    unsigned long long ceil_sample(double time_seconds, unsigned int sampling_rate_hz)
    {
        const double sample = std::ceil(time_seconds * sampling_rate_hz - 1e-12);
        return sample <= 0.0 ? 0u : static_cast<unsigned long long>(sample);
    }

    double unit_from_seed(unsigned long long seed)
    {
        seed ^= seed >> 33;
        seed *= 0xff51afd7ed558ccdULL;
        seed ^= seed >> 33;
        seed *= 0xc4ceb9fe1a85ec53ULL;
        seed ^= seed >> 33;
        return static_cast<double>(seed >> 11) * (1.0 / 9007199254740992.0);
    }

    unsigned long long next_state(unsigned long long& state)
    {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return state;
    }

    double signed_noise(unsigned long long& state)
    {
        return 2.0 * unit_from_seed(next_state(state)) - 1.0;
    }

    double envelope(unsigned long long index, unsigned long long first, unsigned long long past)
    {
        if (past <= first + 1)
            return 1.0;
        const unsigned long long length = past - first;
        unsigned long long taper = length / 20u;
        if (taper < 1u)
            taper = 1u;
        if (taper > length / 2u)
            taper = length / 2u;
        const unsigned long long offset = index - first;
        const unsigned long long remaining = past - 1u - index;
        const unsigned long long edge = std::min(offset, remaining);
        if (edge >= taper)
            return 1.0;
        const double phase = static_cast<double>(edge) / static_cast<double>(taper);
        return 0.5 - 0.5 * std::cos(pi * phase);
    }

    double clipped(double value, double threshold)
    {
        return std::max(-threshold, std::min(threshold, value));
    }

    unsigned int selected_lead_count(const signal_synth::signal_quality_artifact_config& config)
    {
        unsigned int count = 0;
        for (unsigned int lead = 0; lead < signal_synth::clinical_lead_count; ++lead)
            if (config.ecg_leads[lead])
                ++count;
        return count;
    }

    int selected_lead_at(const signal_synth::signal_quality_artifact_config& config, unsigned int ordinal)
    {
        for (unsigned int lead = 0; lead < signal_synth::clinical_lead_count; ++lead)
            if (config.ecg_leads[lead] && ordinal-- == 0)
                return static_cast<int>(lead);
        return -1;
    }

    void apply_lead_swap(std::vector<std::vector<double> >& ecg_leads, const signal_synth::signal_quality_artifact_config& config, unsigned long long first, unsigned long long past)
    {
        const int first_lead = selected_lead_at(config, 0);
        const int second_lead = selected_lead_at(config, 1);
        if (first_lead < 0 || second_lead < 0)
            return;
        for (unsigned long long sample = first; sample < past; ++sample)
            std::swap(ecg_leads[static_cast<unsigned int>(first_lead)][static_cast<std::size_t>(sample)], ecg_leads[static_cast<unsigned int>(second_lead)][static_cast<std::size_t>(sample)]);
    }

    unsigned int misplacement_neighbor(unsigned int lead, unsigned long long seed)
    {
        unsigned int offset = static_cast<unsigned int>(seed % (signal_synth::clinical_lead_count - 1u)) + 1u;
        return (lead + offset) % signal_synth::clinical_lead_count;
    }

    double quantized(double value, double step)
    {
        return step > 0.0 ? std::floor(value / step + (value >= 0.0 ? 0.5 : -0.5)) * step : value;
    }

    void apply_ecg_artifact(std::vector<double>& samples, const std::vector<std::vector<double> >& reference_leads, const signal_synth::signal_quality_artifact_config& config, unsigned int lead, unsigned int sampling_rate_hz, unsigned long long first, unsigned long long past)
    {
        const double phase = two_pi * unit_from_seed(config.seed ^ (0x9e3779b97f4a7c15ULL + lead));
        const double baseline_frequency = 0.18 + 0.28 * unit_from_seed(config.seed ^ 0x6a09e667f3bcc909ULL);
        unsigned long long noise_state = config.seed ^ (0xbf58476d1ce4e5b9ULL * (lead + 1u));
        double previous_noise = 0.0;
        for (unsigned long long sample = first; sample < past; ++sample)
        {
            const std::size_t index = static_cast<std::size_t>(sample);
            const double time = static_cast<double>(sample) / sampling_rate_hz;
            const double weight = envelope(sample, first, past);
            if (config.type == signal_synth::signal_quality_ecg_baseline_wander)
                samples[index] += weight * config.severity * 0.35 * std::sin(two_pi * baseline_frequency * time + phase);
            else if (config.type == signal_synth::signal_quality_ecg_powerline)
                samples[index] += weight * config.severity * 0.08 * std::sin(two_pi * 50.0 * time + phase);
            else if (config.type == signal_synth::signal_quality_ecg_emg_noise)
            {
                const double raw = signed_noise(noise_state);
                const double high = raw - 0.65 * previous_noise;
                previous_noise = raw;
                samples[index] += weight * config.severity * 0.08 * high;
            }
            else if (config.type == signal_synth::signal_quality_ecg_dropout)
            {
                const double attenuation = 1.0 - weight * config.severity * 0.98;
                samples[index] *= attenuation;
            }
            else if (config.type == signal_synth::signal_quality_ecg_saturation)
            {
                const double threshold = 2.2 - 1.85 * config.severity;
                const double saturated = clipped(samples[index], threshold);
                samples[index] = samples[index] * (1.0 - weight) + saturated * weight;
            }
            else if (config.type == signal_synth::signal_quality_ecg_lead_reversal)
                samples[index] = -samples[index];
            else if (config.type == signal_synth::signal_quality_ecg_electrode_misplacement)
            {
                const unsigned int neighbor = misplacement_neighbor(lead, config.seed);
                const double blend = std::min(0.85, 0.15 + 0.70 * config.severity) * weight;
                samples[index] = reference_leads[lead][index] * (1.0 - blend) + reference_leads[neighbor][index] * blend;
            }
            else if (config.type == signal_synth::signal_quality_ecg_gain_mismatch)
            {
                const double direction = unit_from_seed(config.seed ^ (0x510e527fade682d1ULL + lead)) < 0.5 ? -1.0 : 1.0;
                const double gain = 1.0 + direction * config.severity * 0.35 * weight;
                samples[index] = reference_leads[lead][index] * gain;
            }
            else if (config.type == signal_synth::signal_quality_ecg_offset_drift)
            {
                const double span = past <= first + 1 ? 1.0 : static_cast<double>(sample - first) / static_cast<double>(past - first - 1u);
                const double polarity = unit_from_seed(config.seed ^ (0x1f83d9abfb41bd6bULL + lead)) < 0.5 ? -1.0 : 1.0;
                samples[index] += weight * polarity * config.severity * 0.45 * (2.0 * span - 1.0);
            }
            else if (config.type == signal_synth::signal_quality_ecg_clock_drift)
            {
                const double drift = (2.0 * unit_from_seed(config.seed ^ (0xa54ff53a5f1d36f1ULL + lead)) - 1.0) * config.severity * 0.040;
                const double source_offset = static_cast<double>(sample - first) * (1.0 + drift);
                long long source_index = static_cast<long long>(first) + static_cast<long long>(std::llround(source_offset));
                if (source_index < static_cast<long long>(first))
                    source_index = static_cast<long long>(first);
                if (source_index >= static_cast<long long>(past))
                    source_index = static_cast<long long>(past - 1u);
                samples[index] = reference_leads[lead][static_cast<std::size_t>(source_index)];
            }
            else if (config.type == signal_synth::signal_quality_ecg_dropped_samples)
            {
                long long raw_period = std::llround(18.0 - 16.0 * config.severity);
                if (raw_period < 2)
                    raw_period = 2;
                const unsigned long long period = static_cast<unsigned long long>(raw_period);
                if (((sample - first) + (config.seed % period) + lead) % period == 0 && sample > first)
                    samples[index] = samples[static_cast<std::size_t>(sample - 1u)];
            }
            else if (config.type == signal_synth::signal_quality_ecg_quantization)
            {
                const double step = 0.002 + config.severity * 0.048;
                samples[index] = quantized(samples[index], step);
            }
            else if (config.type == signal_synth::signal_quality_ecg_adc_clipping)
            {
                const double threshold = 1.8 - 1.45 * config.severity;
                samples[index] = clipped(samples[index], threshold);
            }
        }
    }

    void apply_ppg_artifact(std::vector<double>& samples, std::vector<double>& accelerometer, const signal_synth::signal_quality_artifact_config& config, unsigned int sampling_rate_hz, unsigned long long first, unsigned long long past, double baseline, double span, double motion_sensitivity, double ambient_sensitivity, bool update_accelerometer)
    {
        if (samples.empty())
            return;
        const double phase = two_pi * unit_from_seed(config.seed ^ 0x243f6a8885a308d3ULL);
        const double frequency = 0.8 + 2.2 * unit_from_seed(config.seed ^ 0x13198a2e03707344ULL);
        unsigned long long ppg_noise_state = config.seed ^ 0xa4093822299f31d0ULL;
        unsigned long long acceleration_noise_state = config.seed ^ 0x082efa98ec4e6c89ULL;
        double previous_ppg_noise = 0.0;
        double previous_acceleration_noise = 0.0;
        for (unsigned long long sample = first; sample < past; ++sample)
        {
            const std::size_t index = static_cast<std::size_t>(sample);
            const double time = static_cast<double>(sample) / sampling_rate_hz;
            const double edge_weight = envelope(sample, first, past);
            const double weight = edge_weight * config.severity;
            if (config.type == signal_synth::signal_quality_ppg_dropout)
                samples[index] = samples[index] * (1.0 - weight) + baseline * weight;
            else if (config.type == signal_synth::signal_quality_ppg_motion_periodic)
            {
                const double acceleration = weight * (0.35 * std::sin(two_pi * frequency * time + phase) + 0.10 * std::sin(two_pi * 2.0 * frequency * time + 0.4 + phase));
                if (update_accelerometer) accelerometer[index] += acceleration;
                samples[index] += motion_sensitivity * span * (1.15 * acceleration + weight * 0.12 * std::sin(two_pi * 0.5 * frequency * time + phase));
            }
            else if (config.type == signal_synth::signal_quality_ppg_motion_burst)
            {
                const double burst_phase = std::sin(two_pi * 0.35 * frequency * time + phase);
                const double gate = burst_phase > 0.25 ? std::pow((burst_phase - 0.25) / 0.75, 2.0) : 0.0;
                const double acceleration = weight * gate * (0.55 * std::sin(two_pi * 2.4 * frequency * time + phase) + 0.16 * std::sin(two_pi * 4.1 * frequency * time));
                if (update_accelerometer) accelerometer[index] += acceleration;
                samples[index] += motion_sensitivity * span * (0.95 * acceleration + weight * gate * 0.18);
            }
            else if (config.type == signal_synth::signal_quality_ppg_motion_broadband)
            {
                const double ppg_raw = signed_noise(ppg_noise_state);
                const double acceleration_raw = signed_noise(acceleration_noise_state);
                const double ppg_band = ppg_raw - 0.72 * previous_ppg_noise;
                const double acceleration_band = acceleration_raw - 0.58 * previous_acceleration_noise;
                previous_ppg_noise = ppg_raw;
                previous_acceleration_noise = acceleration_raw;
                const double acceleration = weight * 0.32 * acceleration_band;
                if (update_accelerometer) accelerometer[index] += acceleration;
                samples[index] += motion_sensitivity * span * weight * (0.55 * ppg_band + 0.80 * acceleration_band);
            }
            else if (config.type == signal_synth::signal_quality_ppg_ambient_light)
                samples[index] += ambient_sensitivity * span * weight * (0.55 + 0.12 * std::sin(two_pi * 100.0 * time + phase));
            else if (config.type == signal_synth::signal_quality_ppg_sensor_saturation)
            {
                const bool upper = unit_from_seed(config.seed ^ 0x452821e638d01377ULL) >= 0.5;
                const double rail = upper ? baseline + span * (1.0 - 0.65 * config.severity) : baseline + span * 0.65 * config.severity;
                const double saturated = upper ? std::min(samples[index], rail) : std::max(samples[index], rail);
                samples[index] = samples[index] * (1.0 - edge_weight) + saturated * edge_weight;
            }
        }
    }

    bool interval_samples(const signal_synth::signal_quality_artifact_config& artifact, unsigned int sampling_rate_hz, unsigned int sample_count, unsigned long long& first, unsigned long long& past)
    {
        const double end = artifact.start_seconds + artifact.duration_seconds;
        first = ceil_sample(artifact.start_seconds, sampling_rate_hz);
        past = ceil_sample(end, sampling_rate_hz);
        return first < past && past <= sample_count;
    }
}

namespace signal_synth
{
    const char* signal_quality_artifact_type_name(signal_quality_artifact_type type)
    {
        switch (type)
        {
        case signal_quality_ecg_baseline_wander: return "ecg_baseline_wander";
        case signal_quality_ecg_powerline: return "ecg_powerline";
        case signal_quality_ecg_emg_noise: return "ecg_emg_noise";
        case signal_quality_ecg_dropout: return "ecg_dropout";
        case signal_quality_ecg_saturation: return "ecg_saturation";
        case signal_quality_ppg_dropout: return "ppg_dropout";
        case signal_quality_ecg_lead_reversal: return "ecg_lead_reversal";
        case signal_quality_ecg_lead_swap: return "ecg_lead_swap";
        case signal_quality_ecg_electrode_misplacement: return "ecg_electrode_misplacement";
        case signal_quality_ecg_gain_mismatch: return "ecg_gain_mismatch";
        case signal_quality_ecg_offset_drift: return "ecg_offset_drift";
        case signal_quality_ecg_clock_drift: return "ecg_clock_drift";
        case signal_quality_ecg_dropped_samples: return "ecg_dropped_samples";
        case signal_quality_ecg_quantization: return "ecg_quantization";
        case signal_quality_ecg_adc_clipping: return "ecg_adc_clipping";
        case signal_quality_ppg_motion_periodic: return "ppg_motion_periodic";
        case signal_quality_ppg_motion_burst: return "ppg_motion_burst";
        case signal_quality_ppg_motion_broadband: return "ppg_motion_broadband";
        case signal_quality_ppg_ambient_light: return "ppg_ambient_light";
        case signal_quality_ppg_sensor_saturation: return "ppg_sensor_saturation";
        case signal_quality_ecg_external_noise: return "ecg_external_noise";
        }
        return "unknown";
    }

    bool signal_quality_artifact_is_ppg(signal_quality_artifact_type type)
    {
        return type == signal_quality_ppg_dropout
            || type == signal_quality_ppg_motion_periodic
            || type == signal_quality_ppg_motion_burst
            || type == signal_quality_ppg_motion_broadband
            || type == signal_quality_ppg_ambient_light
            || type == signal_quality_ppg_sensor_saturation;
    }

    bool signal_quality_artifact_is_motion(signal_quality_artifact_type type)
    {
        return type == signal_quality_ppg_motion_periodic
            || type == signal_quality_ppg_motion_burst
            || type == signal_quality_ppg_motion_broadband;
    }

    signal_quality_artifact_config::signal_quality_artifact_config()
        : type(signal_quality_ecg_baseline_wander), start_seconds(0.0), duration_seconds(1.0), severity(1.0), seed(1), ppg(false)
    {
        for (unsigned int lead = 0; lead < clinical_lead_count; ++lead)
            ecg_leads[lead] = false;
    }

    bool signal_quality_artifact_config::affects_ecg() const
    {
        for (unsigned int lead = 0; lead < clinical_lead_count; ++lead)
            if (ecg_leads[lead])
                return true;
        return false;
    }

    bool signal_quality_artifact_config::affects_ppg() const
    {
        return ppg;
    }

    signal_quality_artifact_interval::signal_quality_artifact_interval()
        : type(signal_quality_ecg_baseline_wander), start_seconds(0.0), end_seconds(0.0), start_sample_index(0), end_sample_index(0), severity(0.0), seed(0), ppg(false), accelerometer_reference(false)
    {
        for (unsigned int lead = 0; lead < clinical_lead_count; ++lead)
            ecg_leads[lead] = false;
    }

    bool validate_signal_quality_config(const signal_quality_config& config, double duration_seconds, unsigned int sampling_rate_hz, bool ppg_enabled)
    {
        if (!is_finite(duration_seconds) || duration_seconds <= 0.0 || sampling_rate_hz == 0)
            return false;
        const double sample_count_value = duration_seconds * sampling_rate_hz;
        if (!is_finite(sample_count_value) || sample_count_value < 1.0 || sample_count_value > static_cast<double>(std::numeric_limits<unsigned int>::max()) || std::floor(sample_count_value) != sample_count_value)
            return false;
        const unsigned int sample_count = static_cast<unsigned int>(sample_count_value);
        for (std::size_t i = 0; i < config.artifacts.size(); ++i)
        {
            const signal_quality_artifact_config& artifact = config.artifacts[i];
            if (!is_finite(artifact.start_seconds) || !is_finite(artifact.duration_seconds) || !is_finite(artifact.severity))
                return false;
            const double end_seconds = artifact.start_seconds + artifact.duration_seconds;
            if (!is_valid_type(artifact.type) || !is_finite(end_seconds))
                return false;
            if (artifact.start_seconds < 0.0 || artifact.duration_seconds <= 0.0 || artifact.severity < 0.0 || artifact.severity > 1.0)
                return false;
            if (end_seconds > duration_seconds + 1e-12)
                return false;
            unsigned long long first = 0, past = 0;
            if (!interval_samples(artifact, sampling_rate_hz, sample_count, first, past))
                return false;
            if (is_ecg_type(artifact.type))
            {
                if (!artifact.affects_ecg() || artifact.affects_ppg())
                    return false;
                if (artifact.type == signal_quality_ecg_lead_swap && selected_lead_count(artifact) != 2u)
                    return false;
            }
            else if (!artifact.affects_ppg() || artifact.affects_ecg() || !ppg_enabled)
                return false;
        }
        return true;
    }

    bool initialize_signal_quality_waveforms(const clinical_ecg_record& ecg, const ppg_record& ppg, signal_quality_waveforms& output)
    {
        if (!ecg.sample_count() || !ecg.sampling_rate_hz() || ecg.lead_count() != clinical_lead_count)
            return false;
        signal_quality_waveforms fresh;
        try
        {
            fresh.ecg_leads.assign(clinical_lead_count, std::vector<double>());
            for (unsigned int lead = 0; lead < clinical_lead_count; ++lead)
            {
                const double* source = ecg.lead_data(lead);
                if (!source)
                    return false;
                fresh.ecg_leads[lead].assign(source, source + ecg.sample_count());
            }
            if (ppg.sample_count())
            {
                if (ppg.sample_count() != ecg.sample_count() || ppg.sampling_rate_hz() != ecg.sampling_rate_hz())
                    return false;
                fresh.ppg_channels.resize(ppg.channel_count());
                for (unsigned int channel = 0; channel < ppg.channel_count(); ++channel)
                {
                    const double* source = ppg.channel_samples(channel);
                    if (!source) return false;
                    fresh.ppg_channels[channel].assign(source, source + ppg.sample_count());
                }
            }
        }
        catch (...)
        {
            return false;
        }
        output = fresh;
        return true;
    }

    bool apply_signal_quality_artifacts_in_place(const signal_quality_config& config, const clinical_ecg_record& ecg, const ppg_record& ppg, signal_quality_waveforms& output)
    {
        if (!ecg.sample_count() || !ecg.sampling_rate_hz() || ecg.lead_count() != clinical_lead_count || output.ecg_leads.size() != clinical_lead_count)
            return false;
        signal_quality_waveforms fresh = output;
        try
        {
            for (unsigned int lead = 0; lead < clinical_lead_count; ++lead)
                if (fresh.ecg_leads[lead].size() != ecg.sample_count()) return false;
            if (ppg.sample_count())
            {
                if (ppg.sample_count() != ecg.sample_count() || ppg.sampling_rate_hz() != ecg.sampling_rate_hz() || fresh.ppg_channels.size() != ppg.channel_count()) return false;
                for (unsigned int channel = 0; channel < ppg.channel_count(); ++channel)
                    if (fresh.ppg_channels[channel].size() != ppg.sample_count()) return false;
            }
            else if (!fresh.ppg_channels.empty())
                return false;
            fresh.artifacts.clear();
            if (!fresh.accelerometer.empty() && fresh.accelerometer.size() != ecg.sample_count()) return false;
            std::vector<double> ppg_spans(fresh.ppg_channels.size(), 0.0);
            for (std::size_t channel = 0; channel < fresh.ppg_channels.size(); ++channel)
            {
                const double maximum = *std::max_element(fresh.ppg_channels[channel].begin(), fresh.ppg_channels[channel].end());
                const double minimum = *std::min_element(fresh.ppg_channels[channel].begin(), fresh.ppg_channels[channel].end());
                ppg_spans[channel] = std::max(1e-9, maximum - minimum);
            }
            for (std::size_t i = 0; i < config.artifacts.size(); ++i)
                if (signal_quality_artifact_is_motion(config.artifacts[i].type))
                {
                    if (fresh.accelerometer.empty()) fresh.accelerometer.assign(ecg.sample_count(), 0.0);
                    break;
                }
            const double duration_seconds = static_cast<double>(ecg.sample_count()) / ecg.sampling_rate_hz();
            if (!validate_signal_quality_config(config, duration_seconds, ecg.sampling_rate_hz(), ppg.sample_count() != 0))
                return false;
            for (std::size_t i = 0; i < config.artifacts.size(); ++i)
            {
                const signal_quality_artifact_config& artifact = config.artifacts[i];
                unsigned long long first = 0, past = 0;
                if (!interval_samples(artifact, ecg.sampling_rate_hz(), ecg.sample_count(), first, past))
                    return false;

                if (is_ecg_type(artifact.type))
                {
                    if (artifact.type == signal_quality_ecg_lead_swap)
                        apply_lead_swap(fresh.ecg_leads, artifact, first, past);
                    else
                    {
                        const std::vector<std::vector<double> > reference_leads = fresh.ecg_leads;
                        for (unsigned int lead = 0; lead < clinical_lead_count; ++lead)
                            if (artifact.ecg_leads[lead])
                                apply_ecg_artifact(fresh.ecg_leads[lead], reference_leads, artifact, lead, ecg.sampling_rate_hz(), first, past);
                    }
                }
                else
                    for (unsigned int channel = 0; channel < fresh.ppg_channels.size(); ++channel)
                    {
                        const double baseline = ppg.channel_dc_au(channel);
                        apply_ppg_artifact(fresh.ppg_channels[channel], fresh.accelerometer, artifact, ecg.sampling_rate_hz(), first, past, baseline, ppg_spans[channel], ppg.channel_motion_sensitivity(channel), ppg.channel_ambient_sensitivity(channel), channel == 0u);
                    }

                signal_quality_artifact_interval interval;
                interval.type = artifact.type;
                interval.start_seconds = artifact.start_seconds;
                interval.end_seconds = artifact.start_seconds + artifact.duration_seconds;
                interval.start_sample_index = first;
                interval.end_sample_index = past - 1u;
                interval.severity = artifact.severity;
                interval.seed = artifact.seed;
                interval.ppg = artifact.ppg;
                interval.accelerometer_reference = signal_quality_artifact_is_motion(artifact.type);
                for (unsigned int lead = 0; lead < clinical_lead_count; ++lead)
                    interval.ecg_leads[lead] = artifact.ecg_leads[lead];
                fresh.artifacts.push_back(interval);
            }
            for (unsigned int lead = 0; lead < clinical_lead_count; ++lead)
                for (std::size_t sample = 0; sample < fresh.ecg_leads[lead].size(); ++sample)
                    if (!is_finite(fresh.ecg_leads[lead][sample]))
                        return false;
            for (std::size_t channel = 0; channel < fresh.ppg_channels.size(); ++channel)
                for (std::size_t sample = 0; sample < fresh.ppg_channels[channel].size(); ++sample)
                    if (!is_finite(fresh.ppg_channels[channel][sample])) return false;
            for (std::size_t sample = 0; sample < fresh.accelerometer.size(); ++sample)
                if (!is_finite(fresh.accelerometer[sample]))
                    return false;
        }
        catch (...)
        {
            return false;
        }
        output = fresh;
        return true;
    }

    bool apply_signal_quality_artifacts(const signal_quality_config& config, const clinical_ecg_record& ecg, const ppg_record& ppg, signal_quality_waveforms& output)
    {
        signal_quality_waveforms fresh;
        if (!initialize_signal_quality_waveforms(ecg, ppg, fresh) || !apply_signal_quality_artifacts_in_place(config, ecg, ppg, fresh)) return false;
        output = fresh;
        return true;
    }

    bool finalize_ppg_sensor(const ppg_record& ppg, signal_quality_waveforms& waveforms, std::vector<unsigned long long>& clipping_counts)
    {
        if (waveforms.ppg_channels.size() != ppg.channel_count()) return false;
        std::vector<std::vector<double> > source = waveforms.ppg_channels;
        std::vector<std::vector<double> > fresh = source;
        std::vector<unsigned long long> fresh_clipping(ppg.channel_count(), 0u);
        try
        {
            for (unsigned int channel = 0; channel < ppg.channel_count(); ++channel)
            {
                if (source[channel].size() != ppg.sample_count()) return false;
                unsigned long long noise_state = ppg.channel_seed(channel) ^ 0x4f50545f4e4f4953ULL ^ (static_cast<unsigned long long>(ppg.channel_kind(channel)) + 1u);
                int other = -1;
                if (ppg.channel_kind(channel) == ppg_channel_red)
                {
                    for (unsigned int candidate = 0; candidate < ppg.channel_count(); ++candidate) if (ppg.channel_kind(candidate) == ppg_channel_infrared) other = static_cast<int>(candidate);
                }
                else if (ppg.channel_kind(channel) == ppg_channel_infrared)
                {
                    for (unsigned int candidate = 0; candidate < ppg.channel_count(); ++candidate) if (ppg.channel_kind(candidate) == ppg_channel_red) other = static_cast<int>(candidate);
                }
                const double minimum = ppg.channel_minimum_output_au(channel);
                const double maximum = ppg.channel_maximum_output_au(channel);
                const unsigned int bits = ppg.channel_quantization_bits(channel);
                const double levels = bits ? static_cast<double>((1u << bits) - 1u) : 0.0;
                for (unsigned int sample = 0; sample < ppg.sample_count(); ++sample)
                {
                    double value = source[channel][sample];
                    if (other >= 0) value += ppg.channel_crosstalk_ratio(channel) * (source[static_cast<unsigned int>(other)][sample] - ppg.channel_dc_au(static_cast<unsigned int>(other)));
                    if (ppg.channel_noise_std_au(channel) > 0.0) value += ppg.channel_noise_std_au(channel) * signed_noise(noise_state);
                    value = value * ppg.channel_sensor_gain(channel) + ppg.channel_ambient_offset_au(channel);
                    if (value < minimum || value > maximum) ++fresh_clipping[channel];
                    value = std::max(minimum, std::min(maximum, value));
                    if (bits) value = minimum + std::floor((value - minimum) * levels / (maximum - minimum) + 0.5) * (maximum - minimum) / levels;
                    if (!is_finite(value)) return false;
                    fresh[channel][sample] = value;
                }
            }
        }
        catch (...) { return false; }
        waveforms.ppg_channels.swap(fresh);
        clipping_counts.swap(fresh_clipping);
        return true;
    }
}
