#include "signal_synth.h"

#include <cmath>
#include <cstring>
#include <random>
#include <vector>

namespace signal_synth
{
    const double PI = 3.14159265358979323846;

    void print_gauss(double* data, unsigned int data_samples, double amplitude)
    {
        double mean = static_cast<double>(data_samples - 1) / 2.0;
        double sigma = mean / 3.0;
        for (unsigned int i = 0; i < data_samples; ++i)
        {
            double x = static_cast<double>(i);
            data[i] = amplitude * exp(-0.5 * pow((x - mean) / sigma, 2));
        }
    }

    void print_line(double* data, unsigned int data_samples, double amplitude)
    {
        if (data_samples < 2)
            return;
        double step = amplitude / static_cast<double>(data_samples - 1);
        for (unsigned int i = 1; i < data_samples; ++i)
            data[i] = data[0] + step * i;
    }

    void print_curve(double* data, unsigned int data_samples, double amplitude, double curvature)
    {
        if (data_samples < 2)
            return;
        double factor = curvature / (data_samples - 1);
        for (unsigned int i = 0; i < data_samples; ++i)
        {
            double t = i * factor;
            data[i] = data[0] + amplitude * (1.0 - exp(-t));
        }
    }

    void print_semicircle(double* data, unsigned int data_samples, double amplitude, double curvature)
    {
        if (data_samples < 2)
            return;
        double y0 = data[0];
        for (unsigned int i = 0; i < data_samples; ++i)
        {
            double t = static_cast<double>(i) / (data_samples - 1);
            double base = y0 + amplitude * t;
            double offset = curvature * 4.0 * t * (1.0 - t);
            data[i] = base + offset;
        }
    }

    void simulate_qrs(double* data, unsigned int data_samples, unsigned int sampling_rate, double amp_mod, const qrs_params& params)
    {
        (void)data_samples;
        unsigned int samples_p = params.len_p * sampling_rate;
        unsigned int samples_pq = params.len_pq * sampling_rate;
        unsigned int samples_q = params.len_q * sampling_rate;
        unsigned int samples_r = params.len_r * sampling_rate;
        unsigned int samples_s = params.len_s * sampling_rate;
        unsigned int samples_st = params.len_st * sampling_rate;
        unsigned int samples_t = params.len_t * sampling_rate;
        unsigned int samples_diff = params.st_diff * sampling_rate;

        print_gauss(data, samples_p, params.amplitude_p * amp_mod);
        print_line(data + samples_p + samples_pq, samples_q, params.amplitude_q * amp_mod);
        print_line(data + samples_p + samples_pq + samples_q - 1, samples_r / 2 + 1, (params.amplitude_r - params.amplitude_q) * amp_mod);
        print_line(data + samples_p + samples_pq + samples_q + samples_r / 2 - 1, samples_r / 2 + 1, (-params.amplitude_r + params.amplitude_s) * amp_mod);
        print_curve(data + samples_p + samples_pq + samples_q + samples_r / 2 + samples_r / 2 - 1 - samples_diff, samples_s + 1 + samples_diff, -params.amplitude_s * amp_mod, params.curv_s);
        print_gauss(data + samples_p + samples_pq + samples_q + samples_r + samples_s + samples_st, samples_t, params.amplitude_t * amp_mod);
        double gauss_amp_idx = samples_p + samples_pq + samples_q + samples_r + samples_s + samples_st + samples_t / 2;
        double start_amp = *(data + samples_p + samples_pq + samples_q + samples_r + samples_s - 2);
        if (params.st_elev)
            print_semicircle(data + samples_p + samples_pq + samples_q + samples_r + samples_s - 2, gauss_amp_idx - (samples_p + samples_pq + samples_q + samples_r + samples_s - 2), params.amplitude_t * amp_mod - start_amp, params.curv_st);
    }

    void generate_ecg(double* data, unsigned int data_samples, unsigned int sampling_rate, double frequency, const qrs_params& params)
    {
        unsigned int interval_samples = sampling_rate / frequency;
        for (unsigned int j = 0; j < data_samples - interval_samples; j += interval_samples)
        {
            if (j + interval_samples >= data_samples) break;
            simulate_qrs(data + j, interval_samples, sampling_rate, 1, params);
        }
    }

    void generate_modulated_ecg(double* data, unsigned int data_samples, unsigned int sampling_rate, const ecg_simulation_params& params, const qrs_params& qrs_pars)
    {
        std::default_random_engine generator;
        std::normal_distribution<double> distribution(1.0, params.QRS_interval_standard_deviation);

        int interval_samples = sampling_rate / params.heartbeat_frequency;
        unsigned int extra_last_simulated_qrs_index = 0;
        int extrasys_samples = 1.0 / params.extrasys_frequency * sampling_rate;
        int extrasys_shift = params.extrasys_shift_after_last_QRS * sampling_rate;
        int QRS_count = 0;
        for (unsigned int j = 0; j < data_samples - interval_samples - extrasys_shift; j += interval_samples)
        {
            double HF_sin_coeff = sin(params.phase_HF_radians + j * 2 * PI * params.frequency_HF / sampling_rate);
            double HF_sin_coeff_plus_pi = sin(params.phase_HF_radians + j * 2 * PI * params.frequency_HF / sampling_rate + PI);
            double LF_sin_coeff = sin(params.phase_LF_radians + j * 2 * PI * params.frequency_LF / sampling_rate);
            interval_samples = sampling_rate / params.heartbeat_frequency;
            interval_samples += HF_sin_coeff * interval_samples * params.frequency_modulation_depth_HF;
            interval_samples += LF_sin_coeff * interval_samples * params.frequency_modulation_depth_LF;
            interval_samples *= distribution(generator);
            if ((!params.skip_one_QRS_at_every) || ((QRS_count++) % params.skip_one_QRS_at_every))
            {
                if (j + interval_samples >= data_samples) break;
                simulate_qrs(data + j, interval_samples, sampling_rate, 1 + HF_sin_coeff_plus_pi * params.amplitude_modulation_depth_for_QRS_by_HF, qrs_pars);
                if (j > extra_last_simulated_qrs_index + extrasys_samples)
                {
                    simulate_qrs(data + j + extrasys_shift, interval_samples, sampling_rate, 1 + HF_sin_coeff_plus_pi * params.amplitude_modulation_depth_for_QRS_by_HF, qrs_pars);
                    extra_last_simulated_qrs_index = j;
                }
            }
        }
        for (unsigned int j = 0; j < data_samples; j++)
            data[j] += sin(params.alteration_phase_for_DC_component_in_radians + j * 2 * PI * params.alteration_frequency_for_DC_component / sampling_rate) * params.alteration_amplitude_for_DC_component;
    }

    void add_noise_to_signal(double* data, unsigned int data_samples, double noise_amplitude, unsigned int sampling_rate, double noise_frequency)
    {
        std::default_random_engine generator;
        std::uniform_real_distribution<double> distribution(-noise_amplitude, noise_amplitude);
        for (unsigned int i = 0; i < data_samples; ++i)
        {
            double noise_value;
            if (noise_frequency > 0.0)
            {
                double phase = static_cast<double>(i) / sampling_rate;
                noise_value = noise_amplitude * sin(2 * PI * noise_frequency * phase);
            }
            else
                noise_value = distribution(generator);
            data[i] += noise_value;
        }
    }

    void butterworth_bandpass_filter(double* data, unsigned int data_samples, double sampling_rate, double low_freq, double high_freq)
    {
        double center_freq = PI * ((high_freq + low_freq) / sampling_rate);
        double bandwidth = PI * ((high_freq - low_freq) / sampling_rate);
        double alpha = sin(center_freq) * sinh(log(2.0) / 2.0 * bandwidth / sin(center_freq));
        double b0 = alpha;
        double b1 = 0;
        double b2 = -alpha;
        double a0 = 1 + alpha;
        double a1 = -2 * cos(center_freq);
        double a2 = 1 - alpha;
        b0 /= a0;
        b1 /= a0;
        b2 /= a0;
        a1 /= a0;
        a2 /= a0;

        std::vector<double> filtered(data_samples, 0.0);
        for (unsigned int i = 2; i < data_samples; ++i)
            filtered[i] = b0 * data[i] + b1 * data[i - 1] + b2 * data[i - 2] - a1 * filtered[i - 1] - a2 * filtered[i - 2];
        for (int i = data_samples - 3; i >= 0; --i)
            data[i] = b0 * filtered[i] + b1 * filtered[i + 1] + b2 * filtered[i + 2] - a1 * data[i + 1] - a2 * data[i + 2];
        data[0] = filtered[0];
        data[1] = filtered[1];
    }

    void butterworth_bandpass_filter_(double* data, unsigned int data_samples, unsigned int sampling_rate, double low_freq, double high_freq)
    {
        double low = low_freq / sampling_rate;
        double high = high_freq / sampling_rate;
        double Q = 0.707;
        double omega = PI * (high + low);
        double bw = PI * (high - low) / Q;
        double alpha = sin(omega) * sinh(log(2.0) / 2.0 * bw / sin(omega));
        double b0 = alpha, b1 = 0, b2 = -alpha;
        double a0 = 1 + alpha, a1 = -2 * cos(omega), a2 = 1 - alpha;
        b0 /= a0;
        b1 /= a0;
        b2 /= a0;
        a1 /= a0;
        a2 /= a0;
        std::vector<double> filtered(data_samples, 0.0);
        for (unsigned int i = 2; i < data_samples; ++i)
            filtered[i] = b0 * data[i] + b1 * data[i - 1] + b2 * data[i - 2] - a1 * filtered[i - 1] - a2 * filtered[i - 2];
        for (int i = data_samples - 3; i >= 0; --i)
            data[i] = b0 * filtered[i] + b1 * filtered[i + 1] + b2 * filtered[i + 2] - a1 * data[i + 1] - a2 * data[i + 2];
        data[0] = filtered[0];
        data[1] = filtered[1];
    }

    void add_bandlimited_noise(double* data, unsigned int data_samples, double noise_amplitude, double sampling_rate, double low_freq, double high_freq)
    {
        std::default_random_engine generator;
        std::uniform_real_distribution<double> distribution(-noise_amplitude, noise_amplitude);
        std::vector<double> noise(data_samples);
        for (unsigned int i = 0; i < data_samples; ++i)
            noise[i] = distribution(generator);
        butterworth_bandpass_filter(noise.data(), data_samples, sampling_rate, low_freq, high_freq);
        for (unsigned int i = 0; i < data_samples; ++i)
            data[i] += noise[i];
    }

    void add_bandlimited_noise_not_really_band_limited(double* data, unsigned int data_samples, double noise_amplitude, unsigned int sampling_rate, double low_freq, double high_freq)
    {
        std::default_random_engine generator;
        std::uniform_real_distribution<double> amplitude_distribution(0, noise_amplitude);
        std::uniform_real_distribution<double> phase_distribution(0, 2 * PI);
        int num_components = 10;
        double freq_step = (high_freq - low_freq) / num_components;
        for (unsigned int i = 0; i < data_samples; ++i)
        {
            double noise_value = 0.0;
            for (int j = 0; j < num_components; ++j)
            {
                double freq = low_freq + j * freq_step;
                double amplitude = amplitude_distribution(generator);
                double phase = phase_distribution(generator);
                noise_value += amplitude * sin(2 * PI * freq * i / sampling_rate + phase);
            }
            data[i] += noise_value;
        }
    }

    void create_modulated_sine(double* data, unsigned int data_len, unsigned int sampling_rate, double frequency, double amplitude, double modulation_frequency, double frequency_modulation_depth)
    {
        for (unsigned int j = 0; j < data_len; ++j)
        {
            double p2t = j / static_cast<double>(sampling_rate) * 2.0 * PI;
            double phi = p2t * frequency + frequency_modulation_depth / modulation_frequency * sin(p2t * modulation_frequency);
            data[j] = sin(phi) * amplitude;
        }
    }

    void create_sine(double* data, unsigned int data_len, int sampling_rate, double frequency, double amplitude)
    {
        double p = (2.0 * PI * frequency) / sampling_rate;
        amplitude /= 2.0;
        for (unsigned int i = 0; i < data_len; ++i)
            data[i] = (sin(i * p) + 1.0) * amplitude;
    }

    static void print_line2(double* data, unsigned int data_samples, double amplitude)
    {
        if (data_samples < 2)
            return;
        double step = amplitude / static_cast<double>(data_samples - 1);
        for (unsigned int i = 1; i < data_samples; ++i)
            data[i] = data[0] + step * i;
    }

    void create_triangle(double* data, unsigned int data_len, int sampling_rate, double silence_before_triangle_in_msec, double duration_in_msec, double amplitude)
    {
        memset(data, 0, sizeof(data[0] * data_len));
        unsigned int triangle_starting_point = (silence_before_triangle_in_msec * sampling_rate) / 1000.0;
        unsigned int duration_samples = (duration_in_msec * sampling_rate) / 1000.0;
        print_line2(data + triangle_starting_point - 1, duration_samples / 2 + 1, amplitude);
        print_line2(data + triangle_starting_point + duration_samples / 2 - 1, duration_samples / 2 + 2, -amplitude);
    }

    void create_pulse(double* data, unsigned int data_len, int sampling_rate, double silence_before_pulse_in_msec, double duration_in_msec, double amplitude)
    {
        memset(data, 0, sizeof(data[0] * data_len));
        unsigned int pulse_starting_point = (silence_before_pulse_in_msec * sampling_rate) / 1000.0;
        unsigned int duration_samples = (duration_in_msec * sampling_rate) / 1000.0;
        print_line2(data + pulse_starting_point - 1, 2, amplitude);
        print_line2(data + pulse_starting_point, duration_samples, 0);
    }

}
