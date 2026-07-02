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
        return type != signal_synth::signal_quality_ppg_dropout;
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
            return true;
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

    void apply_ecg_artifact(std::vector<double>& samples, const signal_synth::signal_quality_artifact_config& config, unsigned int lead, unsigned int sampling_rate_hz, unsigned long long first, unsigned long long past)
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
        }
    }

    void apply_ppg_artifact(std::vector<double>& samples, const signal_synth::signal_quality_artifact_config& config, unsigned long long first, unsigned long long past)
    {
        if (samples.empty())
            return;
        const double baseline = *std::min_element(samples.begin(), samples.end());
        for (unsigned long long sample = first; sample < past; ++sample)
        {
            const std::size_t index = static_cast<std::size_t>(sample);
            const double weight = envelope(sample, first, past) * config.severity;
            samples[index] = samples[index] * (1.0 - weight) + baseline * weight;
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
        }
        return "unknown";
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
            }
            else if (!artifact.affects_ppg() || artifact.affects_ecg() || !ppg_enabled)
                return false;
        }
        return true;
    }

    bool apply_signal_quality_artifacts(const signal_quality_config& config, const clinical_ecg_record& ecg, const ppg_record& ppg, signal_quality_waveforms& output)
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
                const double* source = ppg.samples();
                if (!source || ppg.sample_count() != ecg.sample_count() || ppg.sampling_rate_hz() != ecg.sampling_rate_hz())
                    return false;
                fresh.ppg.assign(source, source + ppg.sample_count());
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
                    for (unsigned int lead = 0; lead < clinical_lead_count; ++lead)
                        if (artifact.ecg_leads[lead])
                            apply_ecg_artifact(fresh.ecg_leads[lead], artifact, lead, ecg.sampling_rate_hz(), first, past);
                }
                else
                    apply_ppg_artifact(fresh.ppg, artifact, first, past);

                signal_quality_artifact_interval interval;
                interval.type = artifact.type;
                interval.start_seconds = artifact.start_seconds;
                interval.end_seconds = artifact.start_seconds + artifact.duration_seconds;
                interval.start_sample_index = first;
                interval.end_sample_index = past - 1u;
                interval.severity = artifact.severity;
                interval.seed = artifact.seed;
                interval.ppg = artifact.ppg;
                for (unsigned int lead = 0; lead < clinical_lead_count; ++lead)
                    interval.ecg_leads[lead] = artifact.ecg_leads[lead];
                fresh.artifacts.push_back(interval);
            }
            for (unsigned int lead = 0; lead < clinical_lead_count; ++lead)
                for (std::size_t sample = 0; sample < fresh.ecg_leads[lead].size(); ++sample)
                    if (!is_finite(fresh.ecg_leads[lead][sample]))
                        return false;
            for (std::size_t sample = 0; sample < fresh.ppg.size(); ++sample)
                if (!is_finite(fresh.ppg[sample]))
                    return false;
        }
        catch (...)
        {
            return false;
        }
        output = fresh;
        return true;
    }
}
