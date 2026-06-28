#include "signal_synth.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>

namespace signal_synth
{
    namespace
    {
        const double PI = 3.14159265358979323846;
        const double TWO_PI = 2.0 * PI;

        std::uint64_t initial_random_seed()
        {
            std::random_device device;
            const std::uint64_t high = static_cast<std::uint64_t>(device()) << 32;
            return high ^ static_cast<std::uint64_t>(device());
        }

        std::mt19937_64& random_engine()
        {
            static thread_local std::mt19937_64 engine(initial_random_seed());
            return engine;
        }

        double uniform_unit()
        {
            const std::uint64_t value = random_engine()() >> 11;
            return static_cast<double>(value) * (1.0 / 9007199254740992.0);
        }

        double uniform_random(double low, double high)
        {
            return low + (high - low) * uniform_unit();
        }

        double normal_interval_factor(double standard_deviation)
        {
            if (!(standard_deviation > 0.0) || !std::isfinite(standard_deviation))
                return 1.0;
            const double u1 = std::max(
                uniform_unit(), std::numeric_limits<double>::min());
            const double u2 = uniform_unit();
            const double normal =
                std::sqrt(-2.0 * std::log(u1)) * std::cos(TWO_PI * u2);
            return 1.0 + standard_deviation * normal;
        }

        void clear_signal(double* data, std::size_t data_samples)
        {
            if (data && data_samples)
                std::fill(data, data + data_samples, 0.0);
        }

        std::size_t sample_count(double seconds, unsigned int sampling_rate)
        {
            if (!std::isfinite(seconds) || seconds <= 0.0 || sampling_rate == 0)
                return 0;
            const double samples = seconds * static_cast<double>(sampling_rate);
            const double maximum =
                static_cast<double>(std::numeric_limits<std::size_t>::max());
            if (!std::isfinite(samples) || samples >= maximum)
                return std::numeric_limits<std::size_t>::max();
            return static_cast<std::size_t>(samples);
        }

        std::size_t saturated_add(
            std::size_t left, std::size_t right, std::size_t limit)
        {
            if (left >= limit || right >= limit - left)
                return limit;
            return left + right;
        }

        std::size_t writable_count(
            std::size_t data_samples,
            std::size_t offset,
            std::size_t requested)
        {
            if (offset >= data_samples)
                return 0;
            return std::min(requested, data_samples - offset);
        }

        bool valid_qrs_params(const qrs_params& params)
        {
            const double values[] = {
                params.amplitude_p,
                params.amplitude_q,
                params.amplitude_r,
                params.amplitude_s,
                params.amplitude_t,
                params.st_elev,
                params.st_diff,
                params.curv_s,
                params.curv_st,
                params.len_p,
                params.len_pq,
                params.len_q,
                params.len_r,
                params.len_s,
                params.len_st,
                params.len_t
            };
            for (double value : values)
            {
                if (!std::isfinite(value))
                    return false;
            }
            return params.st_diff >= 0.0 &&
                   params.len_p >= 0.0 &&
                   params.len_pq >= 0.0 &&
                   params.len_q >= 0.0 &&
                   params.len_r >= 0.0 &&
                   params.len_s >= 0.0 &&
                   params.len_st >= 0.0 &&
                   params.len_t >= 0.0;
        }

        void write_gauss(
            double* data,
            std::size_t data_samples,
            std::size_t offset,
            std::size_t shape_samples,
            double amplitude)
        {
            const std::size_t count =
                writable_count(data_samples, offset, shape_samples);
            if (!count || !std::isfinite(amplitude))
                return;
            if (shape_samples == 1)
            {
                data[offset] = amplitude;
                return;
            }

            const double mean = static_cast<double>(shape_samples - 1) / 2.0;
            const double sigma = mean / 3.0;
            for (std::size_t i = 0; i < count; ++i)
            {
                const double normalized =
                    (static_cast<double>(i) - mean) / sigma;
                data[offset + i] =
                    amplitude * std::exp(-0.5 * normalized * normalized);
            }
        }

        void write_line(
            double* data,
            std::size_t data_samples,
            std::size_t offset,
            std::size_t shape_samples,
            double amplitude)
        {
            const std::size_t count =
                writable_count(data_samples, offset, shape_samples);
            if (count < 2 || !std::isfinite(amplitude))
                return;
            const double start = data[offset];
            const double step =
                amplitude / static_cast<double>(shape_samples - 1);
            for (std::size_t i = 1; i < count; ++i)
                data[offset + i] = start + step * static_cast<double>(i);
        }

        void write_curve(
            double* data,
            std::size_t data_samples,
            std::size_t offset,
            std::size_t shape_samples,
            double amplitude,
            double curvature)
        {
            const std::size_t count =
                writable_count(data_samples, offset, shape_samples);
            if (count < 2 ||
                !std::isfinite(amplitude) ||
                !std::isfinite(curvature))
                return;
            const double start = data[offset];
            const double factor =
                curvature / static_cast<double>(shape_samples - 1);
            for (std::size_t i = 0; i < count; ++i)
            {
                const double t = static_cast<double>(i) * factor;
                data[offset + i] =
                    start + amplitude * (1.0 - std::exp(-t));
            }
        }

        void write_semicircle(
            double* data,
            std::size_t data_samples,
            std::size_t offset,
            std::size_t shape_samples,
            double amplitude,
            double curvature)
        {
            const std::size_t count =
                writable_count(data_samples, offset, shape_samples);
            if (count < 2 ||
                !std::isfinite(amplitude) ||
                !std::isfinite(curvature))
                return;
            const double start = data[offset];
            for (std::size_t i = 0; i < count; ++i)
            {
                const double t =
                    static_cast<double>(i) /
                    static_cast<double>(shape_samples - 1);
                const double base = start + amplitude * t;
                const double offset_value =
                    curvature * 4.0 * t * (1.0 - t);
                data[offset + i] = base + offset_value;
            }
        }

        bool valid_bandpass(
            double sampling_rate, double low_freq, double high_freq)
        {
            return std::isfinite(sampling_rate) &&
                   std::isfinite(low_freq) &&
                   std::isfinite(high_freq) &&
                   sampling_rate > 0.0 &&
                   low_freq > 0.0 &&
                   high_freq > low_freq &&
                   high_freq < sampling_rate / 2.0;
        }

        struct biquad_coefficients
        {
            double b0;
            double b1;
            double b2;
            double a1;
            double a2;
        };

        biquad_coefficients design_bandpass(
            double sampling_rate, double low_freq, double high_freq)
        {
            const double k = 2.0 * sampling_rate;
            const double omega_low =
                k * std::tan(PI * low_freq / sampling_rate);
            const double omega_high =
                k * std::tan(PI * high_freq / sampling_rate);
            const double forward_backward_correction =
                std::sqrt(std::sqrt(2.0) - 1.0);
            const double bandwidth =
                (omega_high - omega_low) /
                forward_backward_correction;
            const double center_squared = omega_low * omega_high;
            const double a0 =
                k * k + bandwidth * k + center_squared;

            biquad_coefficients coefficients;
            coefficients.b0 = bandwidth * k / a0;
            coefficients.b1 = 0.0;
            coefficients.b2 = -coefficients.b0;
            coefficients.a1 =
                2.0 * (center_squared - k * k) / a0;
            coefficients.a2 =
                (k * k - bandwidth * k + center_squared) / a0;
            return coefficients;
        }

        void apply_biquad(
            const std::vector<double>& input,
            std::vector<double>& output,
            const biquad_coefficients& coefficients)
        {
            output.assign(input.size(), 0.0);
            double x1 = 0.0;
            double x2 = 0.0;
            double y1 = 0.0;
            double y2 = 0.0;
            for (std::size_t i = 0; i < input.size(); ++i)
            {
                const double x = input[i];
                const double y =
                    coefficients.b0 * x +
                    coefficients.b1 * x1 +
                    coefficients.b2 * x2 -
                    coefficients.a1 * y1 -
                    coefficients.a2 * y2;
                output[i] = y;
                x2 = x1;
                x1 = x;
                y2 = y1;
                y1 = y;
            }
        }
    }

    void print_gauss(
        double* data, unsigned int data_samples, double amplitude)
    {
        if (!data || data_samples == 0)
            return;
        write_gauss(data, data_samples, 0, data_samples, amplitude);
    }

    void print_line(
        double* data, unsigned int data_samples, double amplitude)
    {
        if (!data || data_samples == 0)
            return;
        write_line(data, data_samples, 0, data_samples, amplitude);
    }

    void print_curve(
        double* data,
        unsigned int data_samples,
        double amplitude,
        double curvature)
    {
        if (!data || data_samples == 0)
            return;
        write_curve(
            data, data_samples, 0, data_samples, amplitude, curvature);
    }

    void print_semicircle(
        double* data,
        unsigned int data_samples,
        double amplitude,
        double curvature)
    {
        if (!data || data_samples == 0)
            return;
        write_semicircle(
            data, data_samples, 0, data_samples, amplitude, curvature);
    }

    void simulate_qrs(
        double* data,
        unsigned int data_samples,
        unsigned int sampling_rate,
        double amp_mod,
        const qrs_params& params)
    {
        if (!data ||
            data_samples == 0 ||
            sampling_rate == 0 ||
            !std::isfinite(amp_mod) ||
            !valid_qrs_params(params))
            return;

        const std::size_t limit = data_samples;
        const std::size_t samples_p =
            sample_count(params.len_p, sampling_rate);
        const std::size_t samples_pq =
            sample_count(params.len_pq, sampling_rate);
        const std::size_t samples_q =
            sample_count(params.len_q, sampling_rate);
        const std::size_t samples_r =
            sample_count(params.len_r, sampling_rate);
        const std::size_t samples_s =
            sample_count(params.len_s, sampling_rate);
        const std::size_t samples_st =
            sample_count(params.len_st, sampling_rate);
        const std::size_t samples_t =
            sample_count(params.len_t, sampling_rate);
        const std::size_t samples_diff =
            sample_count(params.st_diff, sampling_rate);

        const std::size_t q_start =
            saturated_add(samples_p, samples_pq, limit);
        const std::size_t q_end =
            saturated_add(q_start, samples_q, limit);
        const std::size_t r_half = samples_r / 2;
        const std::size_t r_up_start = q_end ? q_end - 1 : 0;
        const std::size_t r_mid =
            saturated_add(q_end, r_half, limit);
        const std::size_t r_down_start = r_mid ? r_mid - 1 : 0;
        const std::size_t r_end =
            saturated_add(r_mid, r_half, limit);
        const std::size_t s_base_start = r_end ? r_end - 1 : 0;
        const std::size_t s_start =
            samples_diff < s_base_start
                ? s_base_start - samples_diff
                : 0;
        const std::size_t t_start = saturated_add(
            saturated_add(
                saturated_add(q_end, samples_r, limit),
                samples_s,
                limit),
            samples_st,
            limit);

        write_gauss(
            data,
            limit,
            0,
            samples_p,
            params.amplitude_p * amp_mod);
        write_line(
            data,
            limit,
            q_start,
            samples_q,
            params.amplitude_q * amp_mod);
        write_line(
            data,
            limit,
            r_up_start,
            r_half + 1,
            (params.amplitude_r - params.amplitude_q) * amp_mod);
        write_line(
            data,
            limit,
            r_down_start,
            r_half + 1,
            (-params.amplitude_r + params.amplitude_s) * amp_mod);
        const std::size_t s_curve_samples = saturated_add(
            saturated_add(
                samples_s,
                1,
                std::numeric_limits<std::size_t>::max()),
            samples_diff,
            std::numeric_limits<std::size_t>::max());
        write_curve(
            data,
            limit,
            s_start,
            s_curve_samples,
            -params.amplitude_s * amp_mod,
            params.curv_s);
        write_gauss(
            data,
            limit,
            t_start,
            samples_t,
            params.amplitude_t * amp_mod);

        if (params.st_elev != 0.0)
        {
            const std::size_t st_source_end = saturated_add(
                saturated_add(q_end, samples_r, limit),
                samples_s,
                limit);
            const std::size_t st_start =
                st_source_end >= 2 ? st_source_end - 2 : 0;
            const std::size_t t_peak =
                saturated_add(t_start, samples_t / 2, limit);
            if (st_start < t_peak)
            {
                const double start_amplitude = data[st_start];
                write_semicircle(
                    data,
                    limit,
                    st_start,
                    t_peak - st_start,
                    params.amplitude_t * amp_mod - start_amplitude,
                    params.st_elev * params.curv_st * amp_mod);
            }
        }
    }

    void generate_ecg(
        double* data,
        unsigned int data_samples,
        unsigned int sampling_rate,
        double frequency,
        const qrs_params& params)
    {
        clear_signal(data, data_samples);
        if (!data ||
            data_samples == 0 ||
            sampling_rate == 0 ||
            !std::isfinite(frequency) ||
            frequency <= 0.0)
            return;

        const double interval_value =
            static_cast<double>(sampling_rate) / frequency;
        if (!std::isfinite(interval_value) || interval_value < 1.0)
            return;
        const std::size_t interval_samples =
            interval_value >= static_cast<double>(data_samples)
                ? static_cast<std::size_t>(data_samples)
                : static_cast<std::size_t>(interval_value);
        if (!interval_samples)
            return;

        std::size_t position = 0;
        while (position < data_samples)
        {
            const std::size_t remaining = data_samples - position;
            const std::size_t span =
                std::min(interval_samples, remaining);
            simulate_qrs(
                data + position,
                static_cast<unsigned int>(span),
                sampling_rate,
                1.0,
                params);
            if (interval_samples >= remaining)
                break;
            position += interval_samples;
        }
    }

    void generate_modulated_ecg(
        double* data,
        unsigned int data_samples,
        unsigned int sampling_rate,
        const ecg_simulation_params& params,
        const qrs_params& qrs_pars)
    {
        clear_signal(data, data_samples);
        if (!data ||
            data_samples == 0 ||
            sampling_rate == 0 ||
            !std::isfinite(params.heartbeat_frequency) ||
            params.heartbeat_frequency <= 0.0)
            return;

        const double base_interval =
            static_cast<double>(sampling_rate) /
            params.heartbeat_frequency;
        if (!std::isfinite(base_interval) || base_interval < 1.0)
            return;

        const bool extrasys_enabled =
            std::isfinite(params.extrasys_frequency) &&
            params.extrasys_frequency > 0.0;
        const std::size_t extrasys_period =
            extrasys_enabled
                ? std::max<std::size_t>(
                      1,
                      sample_count(
                          1.0 / params.extrasys_frequency,
                          sampling_rate))
                : 0;
        const std::size_t extrasys_shift =
            sample_count(
                params.extrasys_shift_after_last_QRS,
                sampling_rate);
        std::size_t next_extrasys = extrasys_period;

        std::size_t position = 0;
        unsigned int qrs_count = 0;
        while (position < data_samples)
        {
            const double time =
                static_cast<double>(position) /
                static_cast<double>(sampling_rate);
            const double hf =
                std::sin(
                    params.phase_HF_radians +
                    TWO_PI * params.frequency_HF * time);
            const double lf =
                std::sin(
                    params.phase_LF_radians +
                    TWO_PI * params.frequency_LF * time);
            const double amplitude_hf =
                std::sin(
                    params.phase_HF_radians +
                    TWO_PI * params.frequency_HF * time +
                    PI);

            double interval_value = std::floor(base_interval);
            interval_value +=
                hf *
                interval_value *
                params.frequency_modulation_depth_HF;
            interval_value = std::floor(interval_value);
            interval_value +=
                lf *
                interval_value *
                params.frequency_modulation_depth_LF;
            interval_value = std::floor(interval_value);
            interval_value *= normal_interval_factor(
                params.QRS_interval_standard_deviation);
            if (!std::isfinite(interval_value))
                break;
            const std::size_t interval_samples =
                interval_value >= static_cast<double>(data_samples)
                    ? static_cast<std::size_t>(data_samples)
                    : std::max<std::size_t>(
                          1,
                          static_cast<std::size_t>(
                              std::max(1.0, interval_value)));

            ++qrs_count;
            const bool skip =
                params.skip_one_QRS_at_every > 0 &&
                qrs_count %
                    static_cast<unsigned int>(
                        params.skip_one_QRS_at_every) ==
                    0;
            if (!skip)
            {
                const std::size_t remaining =
                    data_samples - position;
                const std::size_t span =
                    std::min(interval_samples, remaining);
                const double amplitude_modulation =
                    1.0 +
                    amplitude_hf *
                        params.amplitude_modulation_depth_for_QRS_by_HF;
                simulate_qrs(
                    data + position,
                    static_cast<unsigned int>(span),
                    sampling_rate,
                    amplitude_modulation,
                    qrs_pars);

                if (extrasys_enabled &&
                    position >= next_extrasys &&
                    extrasys_shift < data_samples - position)
                {
                    const std::size_t extra_position =
                        position + extrasys_shift;
                    const std::size_t extra_remaining =
                        data_samples - extra_position;
                    const std::size_t extra_span =
                        std::min(interval_samples, extra_remaining);
                    simulate_qrs(
                        data + extra_position,
                        static_cast<unsigned int>(extra_span),
                        sampling_rate,
                        amplitude_modulation,
                        qrs_pars);
                    next_extrasys = saturated_add(
                        position,
                        extrasys_period,
                        data_samples);
                }
            }

            const std::size_t remaining =
                data_samples - position;
            if (interval_samples >= remaining)
                break;
            position += interval_samples;
        }

        if (std::isfinite(
                params.alteration_frequency_for_DC_component) &&
            std::isfinite(
                params.alteration_amplitude_for_DC_component) &&
            std::isfinite(
                params.alteration_phase_for_DC_component_in_radians))
        {
            for (std::size_t i = 0; i < data_samples; ++i)
            {
                const double time =
                    static_cast<double>(i) /
                    static_cast<double>(sampling_rate);
                data[i] +=
                    std::sin(
                        params
                            .alteration_phase_for_DC_component_in_radians +
                        TWO_PI *
                            params
                                .alteration_frequency_for_DC_component *
                            time) *
                    params.alteration_amplitude_for_DC_component;
            }
        }
    }

    void set_random_seed(unsigned long long seed)
    {
        random_engine().seed(static_cast<std::uint64_t>(seed));
    }

    void add_noise_to_signal(
        double* data,
        unsigned int data_samples,
        double noise_amplitude,
        unsigned int sampling_rate,
        double noise_frequency)
    {
        if (!data ||
            data_samples == 0 ||
            !std::isfinite(noise_amplitude) ||
            !std::isfinite(noise_frequency))
            return;
        const double amplitude = std::fabs(noise_amplitude);
        if (noise_frequency > 0.0)
        {
            if (sampling_rate == 0)
                return;
            for (std::size_t i = 0; i < data_samples; ++i)
            {
                const double time =
                    static_cast<double>(i) /
                    static_cast<double>(sampling_rate);
                data[i] +=
                    amplitude *
                    std::sin(TWO_PI * noise_frequency * time);
            }
            return;
        }

        for (std::size_t i = 0; i < data_samples; ++i)
            data[i] += uniform_random(-amplitude, amplitude);
    }

    void butterworth_bandpass_filter(
        double* data,
        unsigned int data_samples,
        double sampling_rate,
        double low_freq,
        double high_freq)
    {
        if (!data || data_samples == 0)
            return;
        if (data_samples < 3)
        {
            clear_signal(data, data_samples);
            return;
        }
        if (!valid_bandpass(sampling_rate, low_freq, high_freq))
            return;

        const biquad_coefficients coefficients =
            design_bandpass(sampling_rate, low_freq, high_freq);
        const std::vector<double> input(data, data + data_samples);
        std::vector<double> forward;
        std::vector<double> backward;
        apply_biquad(input, forward, coefficients);
        std::reverse(forward.begin(), forward.end());
        apply_biquad(forward, backward, coefficients);
        std::reverse(backward.begin(), backward.end());
        std::copy(backward.begin(), backward.end(), data);
    }

    void butterworth_bandpass_filter_(
        double* data,
        unsigned int data_samples,
        unsigned int sampling_rate,
        double low_freq,
        double high_freq)
    {
        butterworth_bandpass_filter(
            data,
            data_samples,
            static_cast<double>(sampling_rate),
            low_freq,
            high_freq);
    }

    void add_bandlimited_noise(
        double* data,
        unsigned int data_samples,
        double noise_amplitude,
        double sampling_rate,
        double low_freq,
        double high_freq)
    {
        if (!data ||
            data_samples == 0 ||
            !std::isfinite(noise_amplitude) ||
            !valid_bandpass(sampling_rate, low_freq, high_freq))
            return;
        const double amplitude = std::fabs(noise_amplitude);
        std::vector<double> noise(data_samples);
        for (std::size_t i = 0; i < noise.size(); ++i)
            noise[i] = uniform_random(-amplitude, amplitude);
        butterworth_bandpass_filter(
            noise.data(),
            data_samples,
            sampling_rate,
            low_freq,
            high_freq);
        for (std::size_t i = 0; i < noise.size(); ++i)
            data[i] += noise[i];
    }

    void add_bandlimited_noise_not_really_band_limited(
        double* data,
        unsigned int data_samples,
        double noise_amplitude,
        unsigned int sampling_rate,
        double low_freq,
        double high_freq)
    {
        if (!data ||
            data_samples == 0 ||
            sampling_rate == 0 ||
            !std::isfinite(noise_amplitude) ||
            !std::isfinite(low_freq) ||
            !std::isfinite(high_freq) ||
            low_freq < 0.0 ||
            high_freq <= low_freq ||
            high_freq >= sampling_rate / 2.0)
            return;

        const std::size_t component_count = 10;
        const double amplitude = std::fabs(noise_amplitude);
        const double frequency_step =
            (high_freq - low_freq) / component_count;
        std::vector<double> amplitudes(component_count);
        std::vector<double> phases(component_count);
        for (std::size_t component = 0;
             component < component_count;
             ++component)
        {
            amplitudes[component] =
                uniform_random(0.0, amplitude);
            phases[component] =
                uniform_random(0.0, TWO_PI);
        }

        for (std::size_t i = 0; i < data_samples; ++i)
        {
            const double time =
                static_cast<double>(i) /
                static_cast<double>(sampling_rate);
            double noise_value = 0.0;
            for (std::size_t component = 0;
                 component < component_count;
                 ++component)
            {
                const double frequency =
                    low_freq +
                    static_cast<double>(component) * frequency_step;
                noise_value +=
                    amplitudes[component] *
                    std::sin(
                        TWO_PI * frequency * time +
                        phases[component]);
            }
            data[i] += noise_value;
        }
    }

    void create_modulated_sine(
        double* data,
        unsigned int data_len,
        unsigned int sampling_rate,
        double frequency,
        double amplitude,
        double modulation_frequency,
        double frequency_modulation_depth)
    {
        clear_signal(data, data_len);
        if (!data ||
            data_len == 0 ||
            sampling_rate == 0 ||
            !std::isfinite(frequency) ||
            !std::isfinite(amplitude) ||
            !std::isfinite(modulation_frequency) ||
            !std::isfinite(frequency_modulation_depth))
            return;

        for (std::size_t i = 0; i < data_len; ++i)
        {
            const double phase_time =
                static_cast<double>(i) /
                static_cast<double>(sampling_rate) *
                TWO_PI;
            double phase = 0.0;
            if (modulation_frequency == 0.0)
                phase =
                    phase_time *
                    (frequency + frequency_modulation_depth);
            else
                phase =
                    phase_time * frequency +
                    frequency_modulation_depth /
                        modulation_frequency *
                        std::sin(
                            phase_time * modulation_frequency);
            data[i] = std::sin(phase) * amplitude;
        }
    }

    void create_sine(
        double* data,
        unsigned int data_len,
        int sampling_rate,
        double frequency,
        double amplitude)
    {
        clear_signal(data, data_len);
        if (!data ||
            data_len == 0 ||
            sampling_rate <= 0 ||
            !std::isfinite(frequency) ||
            !std::isfinite(amplitude))
            return;
        const double phase_step =
            TWO_PI * frequency /
            static_cast<double>(sampling_rate);
        const double half_amplitude = amplitude / 2.0;
        for (std::size_t i = 0; i < data_len; ++i)
            data[i] =
                (std::sin(static_cast<double>(i) * phase_step) + 1.0) *
                half_amplitude;
    }

    void create_triangle(
        double* data,
        unsigned int data_len,
        int sampling_rate,
        double silence_before_triangle_in_msec,
        double duration_in_msec,
        double amplitude)
    {
        clear_signal(data, data_len);
        if (!data ||
            data_len == 0 ||
            sampling_rate <= 0 ||
            !std::isfinite(silence_before_triangle_in_msec) ||
            !std::isfinite(duration_in_msec) ||
            !std::isfinite(amplitude) ||
            silence_before_triangle_in_msec < 0.0 ||
            duration_in_msec <= 0.0)
            return;

        const std::size_t start = sample_count(
            silence_before_triangle_in_msec / 1000.0,
            static_cast<unsigned int>(sampling_rate));
        const std::size_t duration = sample_count(
            duration_in_msec / 1000.0,
            static_cast<unsigned int>(sampling_rate));
        if (start >= data_len || duration == 0)
            return;

        const std::size_t first_start = start ? start - 1 : 0;
        write_line(
            data,
            data_len,
            first_start,
            duration / 2 + 1,
            amplitude);
        const std::size_t midpoint =
            saturated_add(start, duration / 2, data_len);
        const std::size_t second_start =
            midpoint ? midpoint - 1 : 0;
        write_line(
            data,
            data_len,
            second_start,
            duration / 2 + 2,
            -amplitude);
    }

    void create_pulse(
        double* data,
        unsigned int data_len,
        int sampling_rate,
        double silence_before_pulse_in_msec,
        double duration_in_msec,
        double amplitude)
    {
        clear_signal(data, data_len);
        if (!data ||
            data_len == 0 ||
            sampling_rate <= 0 ||
            !std::isfinite(silence_before_pulse_in_msec) ||
            !std::isfinite(duration_in_msec) ||
            !std::isfinite(amplitude) ||
            silence_before_pulse_in_msec < 0.0 ||
            duration_in_msec <= 0.0)
            return;

        const std::size_t start = sample_count(
            silence_before_pulse_in_msec / 1000.0,
            static_cast<unsigned int>(sampling_rate));
        const std::size_t duration = sample_count(
            duration_in_msec / 1000.0,
            static_cast<unsigned int>(sampling_rate));
        if (start >= data_len || duration == 0)
            return;

        if (start)
            write_line(
                data,
                data_len,
                start - 1,
                2,
                amplitude);
        else
            data[0] = amplitude;
        write_line(
            data,
            data_len,
            start,
            duration,
            0.0);
    }
}
