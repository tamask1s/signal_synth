#include "wearable_profiles.h"

#include "clinical_ecg.h"

#include <algorithm>
#include <cmath>

namespace
{
    signal_synth::wearable_ecg_profile_info make_profile(const char* id, const char* channel, const char* placement, bool preserve_12_lead, double highpass_hz, double lowpass_hz, double gain, double minimum_mv, double maximum_mv, unsigned int bits)
    {
        signal_synth::wearable_ecg_profile_info output;
        output.profile_id = id;
        output.channel_name = channel;
        output.placement = placement;
        output.preserve_standard_12_lead = preserve_12_lead;
        output.highpass_hz = highpass_hz;
        output.lowpass_hz = lowpass_hz;
        output.gain = gain;
        output.minimum_output_mv = minimum_mv;
        output.maximum_output_mv = maximum_mv;
        output.quantization_bits = bits;
        return output;
    }

    std::vector<signal_synth::wearable_ecg_profile_info> make_profiles()
    {
        std::vector<signal_synth::wearable_ecg_profile_info> output;
        signal_synth::wearable_ecg_profile_info clinical = make_profile("clinical_12lead_reference_v1", "standard_12_lead_ecg", "reference_12_lead", true, 0.0, 0.0, 1.0, -1000000.0, 1000000.0, 0u);
        output.push_back(clinical);

        signal_synth::wearable_ecg_profile_info holter = make_profile("holter_lead_ii_v1", "ecg_holter_ii", "torso_lead_ii_approximation", false, 0.05, 40.0, 1.0, -5.0, 5.0, 12u);
        holter.lead_weights[signal_synth::clinical_lead_ii] = 1.0;
        output.push_back(holter);

        signal_synth::wearable_ecg_profile_info patch = make_profile("patch_left_chest_vector_v1", "ecg_patch_left_chest", "left_chest_patch_engineering_vector", false, 0.5, 40.0, 1.15, -4.0, 4.0, 12u);
        patch.lead_weights[signal_synth::clinical_lead_ii] = 0.65;
        patch.lead_weights[signal_synth::clinical_lead_v2] = 0.35;
        output.push_back(patch);

        signal_synth::wearable_ecg_profile_info watch = make_profile("watch_lead_i_v1", "ecg_watch_i", "wrist_to_opposite_finger_lead_i_approximation", false, 0.5, 40.0, 0.9, -4.0, 4.0, 12u);
        watch.lead_weights[signal_synth::clinical_lead_i] = 1.0;
        output.push_back(watch);
        return output;
    }

    const std::vector<signal_synth::wearable_ecg_profile_info>& profiles()
    {
        static const std::vector<signal_synth::wearable_ecg_profile_info> values = make_profiles();
        return values;
    }

    void apply_bandwidth(std::vector<double>& samples, unsigned int sample_rate_hz, double highpass_hz, double lowpass_hz)
    {
        if (samples.empty()) return;
        const double dt = 1.0 / sample_rate_hz;
        if (highpass_hz > 0.0)
        {
            const double rc = 1.0 / (2.0 * 3.14159265358979323846 * highpass_hz);
            const double alpha = rc / (rc + dt);
            double previous_input = samples[0];
            double previous_output = 0.0;
            for (std::size_t i = 0; i < samples.size(); ++i)
            {
                const double input = samples[i];
                const double filtered = alpha * (previous_output + input - previous_input);
                samples[i] = filtered;
                previous_input = input;
                previous_output = filtered;
            }
        }
        if (lowpass_hz > 0.0)
        {
            const double rc = 1.0 / (2.0 * 3.14159265358979323846 * lowpass_hz);
            const double alpha = dt / (rc + dt);
            double filtered = samples[0];
            for (std::size_t i = 0; i < samples.size(); ++i)
            {
                filtered += alpha * (samples[i] - filtered);
                samples[i] = filtered;
            }
        }
    }

    bool apply_electronics(const signal_synth::wearable_ecg_profile_info& profile, signal_synth::wearable_stream_record& stream)
    {
        if (!stream.config.sample_rate_hz || profile.minimum_output_mv >= profile.maximum_output_mv) return false;
        const double levels = profile.quantization_bits ? static_cast<double>((1u << profile.quantization_bits) - 1u) : 0.0;
        stream.channel_clipping_counts.assign(stream.channel_samples.size(), 0u);
        for (std::size_t channel = 0; channel < stream.channel_samples.size(); ++channel)
        {
            apply_bandwidth(stream.channel_samples[channel], stream.config.sample_rate_hz, profile.highpass_hz, profile.lowpass_hz);
            for (std::size_t sample = 0; sample < stream.channel_samples[channel].size(); ++sample)
            {
                double value = stream.channel_samples[channel][sample] * profile.gain;
                if (value < profile.minimum_output_mv || value > profile.maximum_output_mv) ++stream.channel_clipping_counts[channel];
                value = std::max(profile.minimum_output_mv, std::min(profile.maximum_output_mv, value));
                if (profile.quantization_bits) value = profile.minimum_output_mv + std::floor((value - profile.minimum_output_mv) * levels / (profile.maximum_output_mv - profile.minimum_output_mv) + 0.5) * (profile.maximum_output_mv - profile.minimum_output_mv) / levels;
                if (!std::isfinite(value)) return false;
                stream.channel_samples[channel][sample] = value;
            }
        }
        return true;
    }
}

namespace signal_synth
{
    wearable_ecg_profile_info::wearable_ecg_profile_info()
        : profile_id(), channel_name(), placement(), preserve_standard_12_lead(false), highpass_hz(0.0), lowpass_hz(0.0), gain(1.0), minimum_output_mv(-5.0), maximum_output_mv(5.0), quantization_bits(0u)
    {
        for (unsigned int lead = 0; lead < 12u; ++lead) lead_weights[lead] = 0.0;
    }

    unsigned int wearable_ecg_profile_count()
    {
        return static_cast<unsigned int>(profiles().size());
    }

    const wearable_ecg_profile_info* wearable_ecg_profile(unsigned int index)
    {
        return index < profiles().size() ? &profiles()[index] : 0;
    }

    const wearable_ecg_profile_info* find_wearable_ecg_profile(const char* profile_id)
    {
        if (!profile_id) return 0;
        for (std::size_t i = 0; i < profiles().size(); ++i) if (profiles()[i].profile_id == profile_id) return &profiles()[i];
        return 0;
    }

    bool validate_wearable_ecg_profile(const char* profile_id, unsigned int sample_rate_hz)
    {
        const wearable_ecg_profile_info* profile = find_wearable_ecg_profile(profile_id);
        return profile && sample_rate_hz && (profile->lowpass_hz <= 0.0 || profile->lowpass_hz < 0.5 * sample_rate_hz);
    }

    bool render_wearable_ecg_profile(const char* profile_id, const wearable_stream_config& config, double duration_seconds, unsigned int source_sample_rate_hz, unsigned int source_sample_count, const std::vector<wearable_source_channel>& standard_leads, unsigned int chunk_size_samples, wearable_stream_record& output)
    {
        const wearable_ecg_profile_info* profile = find_wearable_ecg_profile(profile_id);
        if (!profile || !source_sample_count || standard_leads.size() != clinical_lead_count || !validate_wearable_ecg_profile(profile_id, config.sample_rate_hz)) return false;
        wearable_stream_record fresh;
        if (profile->preserve_standard_12_lead)
        {
            if (!render_wearable_stream(config, wearable_stream_ecg, duration_seconds, source_sample_rate_hz, source_sample_count, standard_leads, chunk_size_samples, fresh)) return false;
        }
        else
        {
            std::vector<double> projected(source_sample_count, 0.0);
            for (unsigned int lead = 0; lead < clinical_lead_count; ++lead)
            {
                if (!standard_leads[lead].samples) return false;
                for (unsigned int sample = 0; sample < source_sample_count; ++sample) projected[sample] += profile->lead_weights[lead] * standard_leads[lead].samples[sample];
            }
            std::vector<wearable_source_channel> channels;
            channels.push_back(wearable_source_channel(profile->channel_name, "mV", &projected[0]));
            if (!render_wearable_stream(config, wearable_stream_ecg, duration_seconds, source_sample_rate_hz, source_sample_count, channels, chunk_size_samples, fresh)) return false;
        }
        fresh.profile_id = profile->profile_id;
        if (!apply_electronics(*profile, fresh)) return false;
        fresh.fingerprint = wearable_stream_record_fingerprint(fresh);
        output = fresh;
        return true;
    }
}
