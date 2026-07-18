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

    bool valid_optical_channel(const signal_synth::ppg_optical_channel_config& channel)
    {
        return finite(channel.dc_au) && channel.dc_au > 0.0 && channel.dc_au <= 1000.0
            && finite(channel.sensor_gain) && channel.sensor_gain > 0.0 && channel.sensor_gain <= 1000.0
            && finite(channel.delay_ms) && channel.delay_ms >= 0.0 && channel.delay_ms <= 2000.0
            && finite(channel.noise_std_au) && channel.noise_std_au >= 0.0 && channel.noise_std_au <= 100.0
            && finite(channel.ambient_offset_au) && std::fabs(channel.ambient_offset_au) <= 1000.0
            && finite(channel.motion_sensitivity) && channel.motion_sensitivity >= 0.0 && channel.motion_sensitivity <= 10.0
            && finite(channel.ambient_sensitivity) && channel.ambient_sensitivity >= 0.0 && channel.ambient_sensitivity <= 10.0
            && finite(channel.crosstalk_ratio) && channel.crosstalk_ratio >= 0.0 && channel.crosstalk_ratio <= 0.5
            && finite(channel.minimum_output_au) && finite(channel.maximum_output_au) && channel.minimum_output_au < channel.maximum_output_au
            && (channel.quantization_bits == 0u || (channel.quantization_bits >= 2u && channel.quantization_bits <= 24u));
    }

    bool valid_optical_config(const signal_synth::ppg_optical_config& optical)
    {
        bool known_profile = false;
        for (unsigned int i = 0; i < signal_synth::ppg_site_profile_count(); ++i)
            known_profile = known_profile || optical.profile_id == signal_synth::ppg_site_profile_id(i);
        if (!valid_optical_channel(optical.red) || !valid_optical_channel(optical.infrared)
            || !known_profile || optical.calibration_id.empty() || optical.calibration_id.size() > 128u
            || !finite(optical.calibration_intercept_percent) || !finite(optical.calibration_slope_percent) || optical.calibration_slope_percent >= -1e-12
            || !finite(optical.minimum_spo2_percent) || !finite(optical.maximum_spo2_percent) || optical.minimum_spo2_percent < 0.0 || optical.maximum_spo2_percent > 100.0 || optical.minimum_spo2_percent >= optical.maximum_spo2_percent
            || !finite(optical.baseline_spo2_percent) || optical.baseline_spo2_percent < optical.minimum_spo2_percent || optical.baseline_spo2_percent > optical.maximum_spo2_percent
            || !finite(optical.infrared_perfusion_index_percent) || optical.infrared_perfusion_index_percent <= 0.0 || optical.infrared_perfusion_index_percent > 20.0
            || optical.oxygenation_episodes.size() > 64u)
            return false;
        for (std::size_t i = 0; i < optical.oxygenation_episodes.size(); ++i)
        {
            const signal_synth::ppg_oxygenation_episode_config& episode = optical.oxygenation_episodes[i];
            if (!finite(episode.start_seconds) || !finite(episode.duration_seconds) || !finite(episode.transition_seconds) || !finite(episode.target_spo2_percent)
                || episode.start_seconds < 0.0 || episode.duration_seconds <= 0.0 || episode.transition_seconds < 0.0 || episode.transition_seconds > 0.5 * episode.duration_seconds
                || episode.target_spo2_percent < optical.minimum_spo2_percent || episode.target_spo2_percent > optical.maximum_spo2_percent)
                return false;
            const double end = episode.start_seconds + episode.duration_seconds;
            if (!finite(end)) return false;
            for (std::size_t previous = 0; previous < i; ++previous)
            {
                const signal_synth::ppg_oxygenation_episode_config& other = optical.oxygenation_episodes[previous];
                if (episode.start_seconds < other.start_seconds + other.duration_seconds && other.start_seconds < end) return false;
            }
        }
        return true;
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
        if (!valid_optical_config(config.optical) || (config.optical.enabled && !config.enabled)) return false;
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

    double smooth_transition(double position)
    {
        const double clipped = std::max(0.0, std::min(1.0, position));
        return 0.5 - 0.5 * std::cos(pi * clipped);
    }

    double oxygen_saturation_at(const signal_synth::ppg_optical_config& config, double time_seconds)
    {
        for (std::size_t i = 0; i < config.oxygenation_episodes.size(); ++i)
        {
            const signal_synth::ppg_oxygenation_episode_config& episode = config.oxygenation_episodes[i];
            const double relative = time_seconds - episode.start_seconds;
            if (relative < 0.0 || relative >= episode.duration_seconds) continue;
            double weight = 1.0;
            if (episode.transition_seconds > 0.0 && relative < episode.transition_seconds) weight = smooth_transition(relative / episode.transition_seconds);
            else if (episode.transition_seconds > 0.0 && relative > episode.duration_seconds - episode.transition_seconds) weight = smooth_transition((episode.duration_seconds - relative) / episode.transition_seconds);
            return config.baseline_spo2_percent + weight * (episode.target_spo2_percent - config.baseline_spo2_percent);
        }
        return config.baseline_spo2_percent;
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
            double dc_au;
            double sensor_gain;
            double delay_ms;
            double noise_std_au;
            double ambient_offset_au;
            double motion_sensitivity;
            double ambient_sensitivity;
            double crosstalk_ratio;
            double minimum_output_au;
            double maximum_output_au;
            unsigned int quantization_bits;
            unsigned long long seed;
            std::vector<double> samples;
            std::vector<ppg_annotation> annotations;

            channel_data()
                : kind(ppg_channel_green), dc_au(0.0), sensor_gain(1.0), delay_ms(0.0), noise_std_au(0.0), ambient_offset_au(0.0), motion_sensitivity(1.0), ambient_sensitivity(1.0), crosstalk_ratio(0.0), minimum_output_au(-1e9), maximum_output_au(1e9), quantization_bits(0), seed(0), samples(), annotations()
            {
            }
        };

        unsigned int sampling_rate_hz;
        ppg_optical_config optical;
        std::vector<channel_data> channels;
        std::vector<ppg_pulse_annotation> pulses;
        std::vector<ppg_optical_pulse_state> optical_states;

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
        : dc_au(1.0), sensor_gain(1.0), delay_ms(0.0), noise_std_au(0.0), ambient_offset_au(0.0), motion_sensitivity(1.0), ambient_sensitivity(1.0), crosstalk_ratio(0.0), minimum_output_au(0.0), maximum_output_au(5.0), quantization_bits(0), seed(0x5050475f4f505431ULL)
    {
    }

    ppg_oxygenation_episode_config::ppg_oxygenation_episode_config()
        : start_seconds(0.0), duration_seconds(10.0), transition_seconds(1.0), target_spo2_percent(90.0)
    {
    }

    ppg_optical_config::ppg_optical_config()
        : enabled(false), profile_id("custom"), calibration_id("engineering_linear_v1"), calibration_intercept_percent(110.0), calibration_slope_percent(-25.0), minimum_spo2_percent(70.0), maximum_spo2_percent(100.0), baseline_spo2_percent(97.0), infrared_perfusion_index_percent(2.0), red(), infrared(), oxygenation_episodes()
    {
        red.dc_au = 1.0;
        red.delay_ms = 8.0;
        red.motion_sensitivity = 1.2;
        red.ambient_sensitivity = 1.1;
        red.seed = 0x5050475f52454431ULL;
        infrared.dc_au = 1.2;
        infrared.delay_ms = 12.0;
        infrared.motion_sensitivity = 0.8;
        infrared.ambient_sensitivity = 0.7;
        infrared.seed = 0x5050475f49523131ULL;
    }

    ppg_perfusion_episode_config::ppg_perfusion_episode_config()
        : start_seconds(0.0), duration_seconds(1.0), amplitude_scale(0.35), rise_time_scale(1.0), decay_time_scale(1.0), weak_pulse_every_n_beats(0), weak_pulse_amplitude_scale(0.35), missing_pulse_every_n_beats(0)
    {
    }

    ppg_config::ppg_config()
        : enabled(false), pulse_delay_ms(180.0), rise_time_ms(120.0), decay_time_ms(300.0), amplitude_au(1.0), baseline_au(0.0), dicrotic_delay_ms(180.0), dicrotic_width_ms(80.0), dicrotic_amplitude_ratio(0.15), pulse_delay_variation_ms(0.0), pulse_delay_variation_hz(0.0), missing_pulse_every_n_beats(0), pulse_delay_jitter_ms(0.0), low_frequency_amplitude_modulation_ratio(0.0), low_frequency_amplitude_modulation_hz(0.1), rise_time_variation_ratio(0.0), decay_time_variation_ratio(0.0), pac_pulse_amplitude_scale(1.0), pvc_pulse_amplitude_scale(1.0), paced_pulse_amplitude_scale(1.0), seed(0x5050475f53545253ULL), optical(), perfusion_episodes()
    {
    }

    unsigned int ppg_site_profile_count()
    {
        return 4u;
    }

    const char* ppg_site_profile_id(unsigned int index)
    {
        const char* ids[] = {"custom", "finger_transmissive_v1", "wrist_reflectance_v1", "ear_reflectance_v1"};
        return index < sizeof(ids) / sizeof(ids[0]) ? ids[index] : 0;
    }

    bool configure_ppg_site_profile(const char* profile_id, ppg_config& output)
    {
        if (!profile_id) return false;
        const std::string id(profile_id);
        ppg_config fresh;
        fresh.enabled = true;
        fresh.optical.enabled = true;
        fresh.optical.profile_id = id;
        if (id == "custom")
        {
        }
        else if (id == "finger_transmissive_v1")
        {
            fresh.rise_time_ms = 115.0; fresh.decay_time_ms = 300.0; fresh.dicrotic_delay_ms = 180.0; fresh.dicrotic_width_ms = 80.0; fresh.dicrotic_amplitude_ratio = 0.15;
            fresh.optical.infrared_perfusion_index_percent = 2.0;
            fresh.optical.red.noise_std_au = 0.0005; fresh.optical.red.motion_sensitivity = 0.8; fresh.optical.red.ambient_sensitivity = 0.5; fresh.optical.red.crosstalk_ratio = 0.005; fresh.optical.red.maximum_output_au = 2.5; fresh.optical.red.quantization_bits = 16u;
            fresh.optical.infrared.noise_std_au = 0.0004; fresh.optical.infrared.motion_sensitivity = 0.6; fresh.optical.infrared.ambient_sensitivity = 0.4; fresh.optical.infrared.crosstalk_ratio = 0.005; fresh.optical.infrared.maximum_output_au = 2.5; fresh.optical.infrared.quantization_bits = 16u;
        }
        else if (id == "wrist_reflectance_v1")
        {
            fresh.pulse_delay_ms = 220.0; fresh.rise_time_ms = 145.0; fresh.decay_time_ms = 360.0; fresh.amplitude_au = 0.75; fresh.dicrotic_delay_ms = 210.0; fresh.dicrotic_width_ms = 100.0; fresh.dicrotic_amplitude_ratio = 0.10; fresh.pulse_delay_jitter_ms = 8.0;
            fresh.optical.infrared_perfusion_index_percent = 1.0;
            fresh.optical.red.dc_au = 1.4; fresh.optical.red.delay_ms = 6.0; fresh.optical.red.noise_std_au = 0.0015; fresh.optical.red.motion_sensitivity = 1.6; fresh.optical.red.ambient_sensitivity = 1.3; fresh.optical.red.crosstalk_ratio = 0.03; fresh.optical.red.maximum_output_au = 4.0; fresh.optical.red.quantization_bits = 14u;
            fresh.optical.infrared.dc_au = 1.6; fresh.optical.infrared.delay_ms = 10.0; fresh.optical.infrared.noise_std_au = 0.0012; fresh.optical.infrared.motion_sensitivity = 1.2; fresh.optical.infrared.ambient_sensitivity = 1.0; fresh.optical.infrared.crosstalk_ratio = 0.03; fresh.optical.infrared.maximum_output_au = 4.0; fresh.optical.infrared.quantization_bits = 14u;
        }
        else if (id == "ear_reflectance_v1")
        {
            fresh.pulse_delay_ms = 160.0; fresh.rise_time_ms = 100.0; fresh.decay_time_ms = 260.0; fresh.amplitude_au = 0.9; fresh.dicrotic_delay_ms = 155.0; fresh.dicrotic_width_ms = 70.0; fresh.dicrotic_amplitude_ratio = 0.18; fresh.pulse_delay_jitter_ms = 3.0;
            fresh.optical.infrared_perfusion_index_percent = 1.5;
            fresh.optical.red.dc_au = 1.1; fresh.optical.red.delay_ms = 7.0; fresh.optical.red.noise_std_au = 0.0008; fresh.optical.red.motion_sensitivity = 0.9; fresh.optical.red.ambient_sensitivity = 0.7; fresh.optical.red.crosstalk_ratio = 0.015; fresh.optical.red.maximum_output_au = 3.0; fresh.optical.red.quantization_bits = 16u;
            fresh.optical.infrared.dc_au = 1.3; fresh.optical.infrared.delay_ms = 10.0; fresh.optical.infrared.noise_std_au = 0.0007; fresh.optical.infrared.motion_sensitivity = 0.7; fresh.optical.infrared.ambient_sensitivity = 0.5; fresh.optical.infrared.crosstalk_ratio = 0.015; fresh.optical.infrared.maximum_output_au = 3.0; fresh.optical.infrared.quantization_bits = 16u;
        }
        else
            return false;
        output = fresh;
        return true;
    }

    ppg_optical_pulse_state::ppg_optical_pulse_state()
        : ecg_beat_index(0), time_seconds(0.0), spo2_percent(0.0), ratio_of_ratios(0.0), red_dc_au(0.0), red_ac_au(0.0), red_perfusion_index_percent(0.0), infrared_dc_au(0.0), infrared_ac_au(0.0), infrared_perfusion_index_percent(0.0), calibration_in_range(false), generated(false), valid_for_measurement(false)
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
    double ppg_record::channel_dc_au(unsigned int channel_index) const { return channel_index < implementation_->channels.size() ? implementation_->channels[channel_index].dc_au : 0.0; }
    double ppg_record::channel_sensor_gain(unsigned int channel_index) const { return channel_index < implementation_->channels.size() ? implementation_->channels[channel_index].sensor_gain : 0.0; }
    double ppg_record::channel_delay_ms(unsigned int channel_index) const { return channel_index < implementation_->channels.size() ? implementation_->channels[channel_index].delay_ms : 0.0; }
    double ppg_record::channel_noise_std_au(unsigned int channel_index) const { return channel_index < implementation_->channels.size() ? implementation_->channels[channel_index].noise_std_au : 0.0; }
    double ppg_record::channel_ambient_offset_au(unsigned int channel_index) const { return channel_index < implementation_->channels.size() ? implementation_->channels[channel_index].ambient_offset_au : 0.0; }
    double ppg_record::channel_motion_sensitivity(unsigned int channel_index) const { return channel_index < implementation_->channels.size() ? implementation_->channels[channel_index].motion_sensitivity : 0.0; }
    double ppg_record::channel_ambient_sensitivity(unsigned int channel_index) const { return channel_index < implementation_->channels.size() ? implementation_->channels[channel_index].ambient_sensitivity : 0.0; }
    double ppg_record::channel_crosstalk_ratio(unsigned int channel_index) const { return channel_index < implementation_->channels.size() ? implementation_->channels[channel_index].crosstalk_ratio : 0.0; }
    double ppg_record::channel_minimum_output_au(unsigned int channel_index) const { return channel_index < implementation_->channels.size() ? implementation_->channels[channel_index].minimum_output_au : 0.0; }
    double ppg_record::channel_maximum_output_au(unsigned int channel_index) const { return channel_index < implementation_->channels.size() ? implementation_->channels[channel_index].maximum_output_au : 0.0; }
    unsigned int ppg_record::channel_quantization_bits(unsigned int channel_index) const { return channel_index < implementation_->channels.size() ? implementation_->channels[channel_index].quantization_bits : 0u; }
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
    bool ppg_record::optical_enabled() const { return implementation_->optical.enabled; }
    const ppg_optical_config& ppg_record::optical_config() const { return implementation_->optical; }
    unsigned int ppg_record::optical_state_count() const { return static_cast<unsigned int>(implementation_->optical_states.size()); }
    const ppg_optical_pulse_state* ppg_record::optical_states() const { return implementation_->optical_states.empty() ? 0 : &implementation_->optical_states[0]; }

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
            fresh.implementation_->optical = config_.optical;
            ppg_record::implementation::channel_data green;
            green.kind = ppg_channel_green;
            green.dc_au = config_.baseline_au;
            green.sensor_gain = 1.0;
            green.delay_ms = 0.0;
            green.noise_std_au = 0.0;
            green.seed = config_.seed;
            green.samples.assign(timeline.sample_count(), config_.baseline_au);
            fresh.implementation_->channels.push_back(green);
            if (config_.optical.enabled)
            {
                ppg_record::implementation::channel_data red;
                red.kind = ppg_channel_red;
                red.dc_au = config_.optical.red.dc_au;
                red.sensor_gain = config_.optical.red.sensor_gain;
                red.delay_ms = config_.optical.red.delay_ms;
                red.noise_std_au = config_.optical.red.noise_std_au;
                red.ambient_offset_au = config_.optical.red.ambient_offset_au;
                red.motion_sensitivity = config_.optical.red.motion_sensitivity;
                red.ambient_sensitivity = config_.optical.red.ambient_sensitivity;
                red.crosstalk_ratio = config_.optical.red.crosstalk_ratio;
                red.minimum_output_au = config_.optical.red.minimum_output_au;
                red.maximum_output_au = config_.optical.red.maximum_output_au;
                red.quantization_bits = config_.optical.red.quantization_bits;
                red.seed = config_.optical.red.seed;
                red.samples.assign(timeline.sample_count(), red.dc_au);
                fresh.implementation_->channels.push_back(red);
                ppg_record::implementation::channel_data infrared;
                infrared.kind = ppg_channel_infrared;
                infrared.dc_au = config_.optical.infrared.dc_au;
                infrared.sensor_gain = config_.optical.infrared.sensor_gain;
                infrared.delay_ms = config_.optical.infrared.delay_ms;
                infrared.noise_std_au = config_.optical.infrared.noise_std_au;
                infrared.ambient_offset_au = config_.optical.infrared.ambient_offset_au;
                infrared.motion_sensitivity = config_.optical.infrared.motion_sensitivity;
                infrared.ambient_sensitivity = config_.optical.infrared.ambient_sensitivity;
                infrared.crosstalk_ratio = config_.optical.infrared.crosstalk_ratio;
                infrared.minimum_output_au = config_.optical.infrared.minimum_output_au;
                infrared.maximum_output_au = config_.optical.infrared.maximum_output_au;
                infrared.quantization_bits = config_.optical.infrared.quantization_bits;
                infrared.seed = config_.optical.infrared.seed;
                infrared.samples.assign(timeline.sample_count(), infrared.dc_au);
                fresh.implementation_->channels.push_back(infrared);
            }
            const double record_end = static_cast<double>(timeline.sample_count() - 1) / timeline.sampling_rate_hz();
            for (std::size_t i = 0; i < config_.perfusion_episodes.size(); ++i)
                if (config_.perfusion_episodes[i].start_seconds + config_.perfusion_episodes[i].duration_seconds > record_end + 1.0 / timeline.sampling_rate_hz())
                    return false;
            for (std::size_t i = 0; i < config_.optical.oxygenation_episodes.size(); ++i)
                if (config_.optical.oxygenation_episodes[i].start_seconds + config_.optical.oxygenation_episodes[i].duration_seconds > record_end + 1.0 / timeline.sampling_rate_hz())
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
                ppg_optical_pulse_state optical_state;
                bool optical_pair_generated = false;
                if (config_.optical.enabled)
                {
                    const double last_optical_delay_ms = std::max(config_.optical.red.delay_ms, config_.optical.infrared.delay_ms);
                    optical_pair_generated = pulse.generated && onset >= 0.0 && offset + last_optical_delay_ms / 1000.0 <= record_end;
                    optical_state.ecg_beat_index = beat.beat_index;
                    optical_state.time_seconds = beat.r_peak_time_seconds;
                    optical_state.spo2_percent = oxygen_saturation_at(config_.optical, beat.r_peak_time_seconds);
                    optical_state.ratio_of_ratios = (optical_state.spo2_percent - config_.optical.calibration_intercept_percent) / config_.optical.calibration_slope_percent;
                    optical_state.red_dc_au = config_.optical.red.dc_au;
                    optical_state.infrared_dc_au = config_.optical.infrared.dc_au;
                    const double pulse_scale = optical_pair_generated ? effective_amplitude / config_.amplitude_au : 0.0;
                    optical_state.infrared_perfusion_index_percent = config_.optical.infrared_perfusion_index_percent * pulse_scale;
                    optical_state.infrared_ac_au = optical_state.infrared_dc_au * optical_state.infrared_perfusion_index_percent / 100.0;
                    optical_state.red_perfusion_index_percent = optical_state.ratio_of_ratios * optical_state.infrared_perfusion_index_percent;
                    optical_state.red_ac_au = optical_state.red_dc_au * optical_state.red_perfusion_index_percent / 100.0;
                    optical_state.calibration_in_range = optical_state.spo2_percent >= config_.optical.minimum_spo2_percent && optical_state.spo2_percent <= config_.optical.maximum_spo2_percent;
                    optical_state.generated = optical_pair_generated;
                    optical_state.valid_for_measurement = optical_pair_generated && optical_state.calibration_in_range && optical_state.infrared_ac_au > 0.0 && optical_state.red_ac_au > 0.0;
                    fresh.implementation_->optical_states.push_back(optical_state);
                }
                if (!pulse.generated)
                    continue;
                for (std::size_t channel_index = 0; channel_index < fresh.implementation_->channels.size(); ++channel_index)
                {
                    ppg_record::implementation::channel_data& channel = fresh.implementation_->channels[channel_index];
                    if (channel.kind != ppg_channel_green && !optical_pair_generated) continue;
                    const double channel_onset = onset + channel.delay_ms / 1000.0;
                    const double channel_peak = channel_onset + rise_seconds;
                    const double channel_offset = channel_peak + decay_seconds;
                    if (channel_onset < 0.0 || channel_offset > record_end)
                        continue;
                    double channel_amplitude = effective_amplitude;
                    if (channel.kind == ppg_channel_red) channel_amplitude = optical_state.red_ac_au;
                    else if (channel.kind == ppg_channel_infrared) channel_amplitude = optical_state.infrared_ac_au;
                    const unsigned long long first = sample_at_or_after(channel_onset, timeline.sampling_rate_hz());
                    const unsigned long long last = sample_at_or_after(channel_offset, timeline.sampling_rate_hz());
                    for (unsigned long long sample = first; sample <= last && sample < timeline.sample_count(); ++sample)
                    {
                        const double time = static_cast<double>(sample) / timeline.sampling_rate_hz();
                        channel.samples[static_cast<std::size_t>(sample)] += pulse_value(time, channel_onset, channel_peak, channel_offset, channel_amplitude, config_.dicrotic_delay_ms / 1000.0, config_.dicrotic_width_ms / 1000.0, config_.dicrotic_amplitude_ratio);
                    }

                    add_annotation(channel.annotations, beat, ppg_pulse_onset, ppg_fiducial_construction, channel_onset, channel.dc_au, timeline.sampling_rate_hz());
                    add_annotation(channel.annotations, beat, ppg_systolic_peak, ppg_fiducial_construction, channel_peak, channel.dc_au + channel_amplitude, timeline.sampling_rate_hz());
                    if (config_.dicrotic_amplitude_ratio > 0.0)
                        add_annotation(channel.annotations, beat, ppg_dicrotic_feature, ppg_fiducial_construction, channel_peak + config_.dicrotic_delay_ms / 1000.0, channel.dc_au + channel_amplitude * config_.dicrotic_amplitude_ratio, timeline.sampling_rate_hz());
                    add_annotation(channel.annotations, beat, ppg_pulse_offset, ppg_fiducial_construction, channel_offset, channel.dc_au, timeline.sampling_rate_hz());
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
        return remeasure_ppg_channel_fiducials(0u, samples, sample_count, record);
    }

    bool remeasure_ppg_channel_fiducials(unsigned int channel_index, const double* samples, unsigned int sample_count, ppg_record& record)
    {
        if (!samples || sample_count != record.sample_count() || !record.sampling_rate_hz() || channel_index >= record.implementation_->channels.size()) return false;
        return remeasure_annotation_vector(samples, sample_count, record.sampling_rate_hz(), record.implementation_->channels[channel_index].annotations);
    }

    bool remeasure_ppg_systolic_peaks(const double* samples, unsigned int sample_count, ppg_record& record)
    {
        return remeasure_ppg_fiducials(samples, sample_count, record);
    }
}
