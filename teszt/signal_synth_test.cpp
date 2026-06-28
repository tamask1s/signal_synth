#include "../src/signal_synth.h"
#include "../src/signal_synth.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
    struct signal_case
    {
        std::string name;
        std::function<std::vector<double>()> generate;
    };

    struct signal_summary
    {
        std::size_t size = 0;
        std::size_t finite = 0;
        double minimum = 0.0;
        double maximum = 0.0;
        double mean = 0.0;
        double rms = 0.0;
        double absolute_sum = 0.0;
        double weighted_checksum = 0.0;
        std::vector<std::pair<std::size_t, double> > anchors;
    };

    signal_synth::qrs_params script_qrs_params()
    {
        signal_synth::qrs_params params;
        params.amplitude_p = 0.1;
        params.amplitude_q = -0.1;
        params.amplitude_r = 1.0;
        params.amplitude_s = -0.1;
        params.amplitude_t = 0.3;
        params.st_elev = 0.01;
        params.st_diff = 0.005;
        params.curv_s = 3.0;
        params.curv_st = 0.15;
        params.len_p = 0.08;
        params.len_pq = 0.08;
        params.len_q = 0.008;
        params.len_r = 0.07;
        params.len_s = 0.005;
        params.len_st = 0.1;
        params.len_t = 0.16;
        return params;
    }

    signal_synth::qrs_params modulated_script_qrs_params()
    {
        signal_synth::qrs_params params;
        params.amplitude_p = 0.1;
        params.amplitude_q = -0.1;
        params.amplitude_r = 1.0;
        params.amplitude_s = -0.2;
        params.amplitude_t = 0.2;
        params.len_p = 0.08;
        params.len_pq = 0.08;
        params.len_q = 0.007;
        params.len_r = 0.07;
        params.len_s = 0.024;
        params.len_st = 0.1;
        params.len_t = 0.16;
        return params;
    }

    signal_synth::ecg_simulation_params modulated_script_params()
    {
        signal_synth::ecg_simulation_params params;
        params.heartbeat_frequency = 1.2;
        params.alteration_frequency_for_DC_component = 0.01;
        params.alteration_amplitude_for_DC_component = 0.2;
        params.alteration_phase_for_DC_component_in_radians = 0.0;
        params.frequency_HF = 0.2;
        params.frequency_LF = 0.08;
        params.amplitude_modulation_depth_for_QRS_by_HF = 0.04;
        params.frequency_modulation_depth_HF = 0.3;
        params.frequency_modulation_depth_LF = 0.3;
        params.phase_HF_radians = 0.0;
        params.phase_LF_radians = 0.0;
        params.extrasys_frequency = 0.0;
        params.extrasys_shift_after_last_QRS = 0.35;
        params.QRS_interval_standard_deviation = 0.0;
        params.skip_one_QRS_at_every = 0;
        return params;
    }

    std::vector<signal_case> make_cases()
    {
        std::vector<signal_case> cases;
        cases.push_back(signal_case{
            "script_ecg_500",
            [] {
                std::vector<double> data(500, 0.0);
                const signal_synth::qrs_params params = script_qrs_params();
                signal_synth::generate_ecg(
                    data.data(),
                    static_cast<unsigned int>(data.size()),
                    500,
                    1.2,
                    params);
                return data;
            }});
        cases.push_back(signal_case{
            "script_modulated_ecg_045",
            [] {
                std::vector<double> data(240000, 0.0);
                const signal_synth::ecg_simulation_params params = modulated_script_params();
                const signal_synth::qrs_params qrs = modulated_script_qrs_params();
                signal_synth::generate_modulated_ecg(
                    data.data(),
                    static_cast<unsigned int>(data.size()),
                    1000,
                    params,
                    qrs);
                return data;
            }});
        cases.push_back(signal_case{
            "script_modulated_sine_036",
            [] {
                std::vector<double> data(240000, 0.0);
                signal_synth::create_modulated_sine(
                    data.data(),
                    static_cast<unsigned int>(data.size()),
                    1000,
                    50.0,
                    1.0,
                    0.02,
                    50.0);
                return data;
            }});
        cases.push_back(signal_case{
            "script_noise_045",
            [] {
                std::vector<double> data(240000, 0.0);
                signal_synth::set_random_seed(0x5eedULL);
                signal_synth::add_noise_to_signal(
                    data.data(),
                    static_cast<unsigned int>(data.size()),
                    0.02,
                    1000,
                    50.0);
                signal_synth::add_noise_to_signal(
                    data.data(),
                    static_cast<unsigned int>(data.size()),
                    0.12,
                    1000);
                signal_synth::add_bandlimited_noise(
                    data.data(),
                    static_cast<unsigned int>(data.size()),
                    0.95,
                    1000.0,
                    30.0,
                    70.0);
                return data;
            }});
        cases.push_back(signal_case{
            "triangle_legacy_profile",
            [] {
                std::vector<double> data(1000, 0.0);
                signal_synth::create_triangle(
                    data.data(),
                    static_cast<unsigned int>(data.size()),
                    1000,
                    200.0,
                    100.0,
                    0.7);
                return data;
            }});
        cases.push_back(signal_case{
            "pulse_legacy_profile",
            [] {
                std::vector<double> data(16000, 0.0);
                signal_synth::create_pulse(
                    data.data(),
                    static_cast<unsigned int>(data.size()),
                    1000,
                    4000.0,
                    10000.0,
                    0.7);
                return data;
            }});
        return cases;
    }

    signal_summary summarize(const std::vector<double>& data)
    {
        signal_summary result;
        result.size = data.size();
        result.minimum = std::numeric_limits<double>::infinity();
        result.maximum = -std::numeric_limits<double>::infinity();

        long double sum = 0.0;
        long double square_sum = 0.0;
        long double absolute_sum = 0.0;
        long double weighted_checksum = 0.0;
        for (std::size_t i = 0; i < data.size(); ++i)
        {
            const double value = data[i];
            if (!std::isfinite(value))
                continue;
            ++result.finite;
            result.minimum = std::min(result.minimum, value);
            result.maximum = std::max(result.maximum, value);
            sum += value;
            square_sum += value * value;
            absolute_sum += std::fabs(value);
            weighted_checksum += value * static_cast<double>((i % 1021) + 1);
        }

        if (result.finite)
        {
            result.mean = static_cast<double>(sum / result.finite);
            result.rms = std::sqrt(static_cast<double>(square_sum / result.finite));
            result.absolute_sum = static_cast<double>(absolute_sum);
            result.weighted_checksum = static_cast<double>(weighted_checksum);
        }

        if (!data.empty())
        {
            const std::size_t indexes[] = {
                0,
                data.size() / 11,
                data.size() / 7,
                data.size() / 3,
                data.size() / 2,
                (data.size() * 2) / 3,
                (data.size() * 6) / 7,
                data.size() - 1
            };
            for (std::size_t index : indexes)
                result.anchors.push_back(std::make_pair(index, data[index]));
        }
        return result;
    }

    bool write_summary(const std::string& path, const signal_summary& summary)
    {
        std::ofstream output(path.c_str());
        if (!output)
            return false;
        output << std::setprecision(17);
        output << "size " << summary.size << '\n';
        output << "finite " << summary.finite << '\n';
        output << "minimum " << summary.minimum << '\n';
        output << "maximum " << summary.maximum << '\n';
        output << "mean " << summary.mean << '\n';
        output << "rms " << summary.rms << '\n';
        output << "absolute_sum " << summary.absolute_sum << '\n';
        output << "weighted_checksum " << summary.weighted_checksum << '\n';
        for (const std::pair<std::size_t, double>& anchor : summary.anchors)
            output << "anchor " << anchor.first << ' ' << anchor.second << '\n';
        return true;
    }

    bool read_summary(const std::string& path, signal_summary& summary)
    {
        std::ifstream input(path.c_str());
        if (!input)
            return false;

        std::string key;
        while (input >> key)
        {
            if (key == "size")
                input >> summary.size;
            else if (key == "finite")
                input >> summary.finite;
            else if (key == "minimum")
                input >> summary.minimum;
            else if (key == "maximum")
                input >> summary.maximum;
            else if (key == "mean")
                input >> summary.mean;
            else if (key == "rms")
                input >> summary.rms;
            else if (key == "absolute_sum")
                input >> summary.absolute_sum;
            else if (key == "weighted_checksum")
                input >> summary.weighted_checksum;
            else if (key == "anchor")
            {
                std::size_t index = 0;
                double value = 0.0;
                input >> index >> value;
                summary.anchors.push_back(std::make_pair(index, value));
            }
            else
                return false;
        }
        return input.eof();
    }

    bool close_enough(double actual, double expected)
    {
        const double scale = std::max(1.0, std::fabs(expected));
        return std::fabs(actual - expected) <= 1e-10 * scale;
    }

    bool compare_summary(
        const std::string& name,
        const signal_summary& actual,
        const signal_summary& expected)
    {
        bool ok = true;
        const auto check_integer = [&](const char* field, std::size_t lhs, std::size_t rhs) {
            if (lhs != rhs)
            {
                std::cerr << name << ": " << field << " actual=" << lhs
                          << " expected=" << rhs << '\n';
                ok = false;
            }
        };
        const auto check_double = [&](const char* field, double lhs, double rhs) {
            if (!close_enough(lhs, rhs))
            {
                std::cerr << std::setprecision(17) << name << ": " << field
                          << " actual=" << lhs << " expected=" << rhs << '\n';
                ok = false;
            }
        };

        check_integer("size", actual.size, expected.size);
        check_integer("finite", actual.finite, expected.finite);
        check_double("minimum", actual.minimum, expected.minimum);
        check_double("maximum", actual.maximum, expected.maximum);
        check_double("mean", actual.mean, expected.mean);
        check_double("rms", actual.rms, expected.rms);
        check_double("absolute_sum", actual.absolute_sum, expected.absolute_sum);
        check_double(
            "weighted_checksum", actual.weighted_checksum, expected.weighted_checksum);
        check_integer("anchor_count", actual.anchors.size(), expected.anchors.size());
        const std::size_t count = std::min(actual.anchors.size(), expected.anchors.size());
        for (std::size_t i = 0; i < count; ++i)
        {
            check_integer(
                "anchor_index", actual.anchors[i].first, expected.anchors[i].first);
            check_double(
                "anchor_value", actual.anchors[i].second, expected.anchors[i].second);
        }
        return ok;
    }

    bool all_finite(const std::vector<double>& data)
    {
        for (double value : data)
        {
            if (!std::isfinite(value))
                return false;
        }
        return true;
    }

    double maximum_absolute_value(const std::vector<double>& data)
    {
        double result = 0.0;
        for (double value : data)
            result = std::max(result, std::fabs(value));
        return result;
    }

    double bandpass_gain(double frequency)
    {
        const unsigned int sampling_rate = 1000;
        std::vector<double> data(20000, 0.0);
        for (std::size_t i = 0; i < data.size(); ++i)
        {
            data[i] = std::sin(
                2.0 * 3.14159265358979323846 *
                frequency *
                static_cast<double>(i) /
                sampling_rate);
        }
        signal_synth::butterworth_bandpass_filter(
            data.data(),
            static_cast<unsigned int>(data.size()),
            sampling_rate,
            10.0,
            20.0);
        long double square_sum = 0.0;
        const std::size_t first = 2000;
        const std::size_t last = data.size() - 2000;
        for (std::size_t i = first; i < last; ++i)
            square_sum += data[i] * data[i];
        const double rms = std::sqrt(
            static_cast<double>(square_sum / (last - first)));
        return rms * std::sqrt(2.0);
    }

    bool run_contract_tests()
    {
        bool ok = true;
        const auto check = [&](bool condition, const char* name) {
            if (condition)
                std::cout << "PASS " << name << '\n';
            else
            {
                std::cerr << "FAIL " << name << '\n';
                ok = false;
            }
        };

        double one_sample = 0.0;
        signal_synth::print_gauss(&one_sample, 1, 2.5);
        check(
            std::isfinite(one_sample) && close_enough(one_sample, 2.5),
            "one_sample_gauss");

        std::vector<double> guarded_qrs(66, 0.0);
        guarded_qrs.front() = 12345.0;
        guarded_qrs.back() = 67890.0;
        signal_synth::qrs_params qrs;
        signal_synth::simulate_qrs(
            guarded_qrs.data() + 1, 64, 1000, 1.0, qrs);
        check(
            guarded_qrs.front() == 12345.0 &&
                guarded_qrs.back() == 67890.0 &&
                all_finite(guarded_qrs),
            "qrs_respects_buffer");

        std::vector<double> one_interval(1000, 7.0);
        signal_synth::generate_ecg(
            one_interval.data(),
            static_cast<unsigned int>(one_interval.size()),
            1000,
            1.0,
            qrs);
        check(
            maximum_absolute_value(one_interval) > 0.9 &&
                std::find(
                    one_interval.begin(),
                    one_interval.end(),
                    7.0) == one_interval.end(),
            "ecg_one_interval_and_initialization");

        std::vector<double> invalid_frequency(1000, 7.0);
        signal_synth::generate_ecg(
            invalid_frequency.data(),
            static_cast<unsigned int>(invalid_frequency.size()),
            1000,
            2000.0,
            qrs);
        check(
            maximum_absolute_value(invalid_frequency) == 0.0,
            "ecg_zero_interval_rejected");

        signal_synth::qrs_params low_st = qrs;
        signal_synth::qrs_params high_st = qrs;
        low_st.st_elev = 0.1;
        high_st.st_elev = 0.5;
        low_st.curv_st = 1.0;
        high_st.curv_st = 1.0;
        std::vector<double> low_st_data(1000, 0.0);
        std::vector<double> high_st_data(1000, 0.0);
        signal_synth::simulate_qrs(
            low_st_data.data(),
            static_cast<unsigned int>(low_st_data.size()),
            1000,
            1.0,
            low_st);
        signal_synth::simulate_qrs(
            high_st_data.data(),
            static_cast<unsigned int>(high_st_data.size()),
            1000,
            1.0,
            high_st);
        double st_difference = 0.0;
        for (std::size_t i = 0; i < low_st_data.size(); ++i)
        {
            st_difference = std::max(
                st_difference,
                std::fabs(low_st_data[i] - high_st_data[i]));
        }
        check(st_difference > 0.1, "st_elevation_has_magnitude");

        signal_synth::ecg_simulation_params skip_params;
        skip_params.alteration_amplitude_for_DC_component = 0.0;
        skip_params.amplitude_modulation_depth_for_QRS_by_HF = 0.0;
        skip_params.frequency_modulation_depth_HF = 0.0;
        skip_params.frequency_modulation_depth_LF = 0.0;
        skip_params.extrasys_frequency = 0.0;
        skip_params.QRS_interval_standard_deviation = 0.0;
        skip_params.skip_one_QRS_at_every = 7;
        std::vector<double> skipped(9000, 0.0);
        signal_synth::generate_modulated_ecg(
            skipped.data(),
            static_cast<unsigned int>(skipped.size()),
            1000,
            skip_params,
            qrs);
        const double first_interval_peak = *std::max_element(
            skipped.begin(), skipped.begin() + 1000);
        const double seventh_interval_peak = *std::max_element(
            skipped.begin() + 6000, skipped.begin() + 7000);
        check(
            first_interval_peak > 0.9 &&
                seventh_interval_peak < 0.5,
            "skip_every_seventh_not_first");

        std::vector<double> noise_a(256, 0.0);
        std::vector<double> noise_b(256, 0.0);
        std::vector<double> noise_c(256, 0.0);
        signal_synth::set_random_seed(123456ULL);
        signal_synth::add_noise_to_signal(
            noise_a.data(),
            static_cast<unsigned int>(noise_a.size()),
            1.0,
            1000);
        signal_synth::add_noise_to_signal(
            noise_b.data(),
            static_cast<unsigned int>(noise_b.size()),
            1.0,
            1000);
        signal_synth::set_random_seed(123456ULL);
        signal_synth::add_noise_to_signal(
            noise_c.data(),
            static_cast<unsigned int>(noise_c.size()),
            1.0,
            1000);
        check(
            noise_a != noise_b && noise_a == noise_c,
            "random_state_and_seed");

        std::vector<double> zero_modulation(1000, 0.0);
        signal_synth::create_modulated_sine(
            zero_modulation.data(),
            static_cast<unsigned int>(zero_modulation.size()),
            1000,
            10.0,
            1.0,
            0.0,
            0.0);
        check(all_finite(zero_modulation), "zero_modulation_frequency");

        std::vector<double> zero_rate(1000, 7.0);
        signal_synth::create_sine(
            zero_rate.data(),
            static_cast<unsigned int>(zero_rate.size()),
            0,
            10.0,
            1.0);
        check(
            maximum_absolute_value(zero_rate) == 0.0,
            "zero_sampling_rate");

        std::vector<double> guarded_triangle(102, 7.0);
        guarded_triangle.front() = 12345.0;
        guarded_triangle.back() = 67890.0;
        signal_synth::create_triangle(
            guarded_triangle.data() + 1,
            100,
            1000,
            0.0,
            20.0,
            1.0);
        check(
            guarded_triangle.front() == 12345.0 &&
                guarded_triangle.back() == 67890.0,
            "triangle_zero_start_buffer");

        std::vector<double> guarded_pulse(102, 7.0);
        guarded_pulse.front() = 12345.0;
        guarded_pulse.back() = 67890.0;
        signal_synth::create_pulse(
            guarded_pulse.data() + 1,
            100,
            1000,
            200.0,
            20.0,
            1.0);
        check(
            guarded_pulse.front() == 12345.0 &&
                guarded_pulse.back() == 67890.0 &&
                maximum_absolute_value(
                    std::vector<double>(
                        guarded_pulse.begin() + 1,
                        guarded_pulse.end() - 1)) ==
                    0.0,
            "pulse_past_end_buffer");

        std::vector<double> empty;
        signal_synth::butterworth_bandpass_filter(
            empty.data(), 0, 1000.0, 10.0, 20.0);
        std::vector<double> one(1, 1.0);
        signal_synth::butterworth_bandpass_filter(
            one.data(), 1, 1000.0, 10.0, 20.0);
        check(one[0] == 0.0, "short_bandpass_input");

        const double gain_5 = bandpass_gain(5.0);
        const double gain_10 = bandpass_gain(10.0);
        const double gain_center =
            bandpass_gain(std::sqrt(10.0 * 20.0));
        const double gain_20 = bandpass_gain(20.0);
        const double gain_50 = bandpass_gain(50.0);
        check(
            gain_5 < 0.2 &&
                gain_10 > 0.65 &&
                gain_10 < 0.75 &&
                gain_center > 0.9 &&
                gain_20 > 0.65 &&
                gain_20 < 0.75 &&
                gain_50 < 0.2,
            "bandpass_cutoffs");

        signal_synth::ecg_simulation_params zero_extrasys =
            modulated_script_params();
        zero_extrasys.extrasys_frequency = 0.0;
        zero_extrasys.QRS_interval_standard_deviation = 0.0;
        std::vector<double> zero_extrasys_data(2000, 0.0);
        signal_synth::generate_modulated_ecg(
            zero_extrasys_data.data(),
            static_cast<unsigned int>(zero_extrasys_data.size()),
            1000,
            zero_extrasys,
            qrs);
        check(
            all_finite(zero_extrasys_data),
            "zero_extrasys_and_standard_deviation");

        return ok;
    }
}

int main(int argc, char** argv)
{
    const bool record = argc == 2 && std::string(argv[1]) == "--record";
    if (argc > 2 || (argc == 2 && !record))
    {
        std::cerr << "usage: signal_synth_test [--record]\n";
        return 2;
    }

    bool ok = true;
    const std::vector<signal_case> cases = make_cases();
    for (const signal_case& test_case : cases)
    {
        const std::vector<double> data = test_case.generate();
        const signal_summary actual = summarize(data);
        const std::string path = "teszt/templates/" + test_case.name + ".txt";

        if (record)
        {
            if (!write_summary(path, actual))
            {
                std::cerr << "cannot write template: " << path << '\n';
                ok = false;
            }
            else
                std::cout << "RECORDED " << test_case.name << '\n';
            continue;
        }

        signal_summary expected;
        if (!read_summary(path, expected))
        {
            std::cerr << "cannot read template: " << path << '\n';
            ok = false;
        }
        else if (!compare_summary(test_case.name, actual, expected))
            ok = false;
        else
            std::cout << "PASS " << test_case.name << '\n';
    }
    if (!record && !run_contract_tests())
        ok = false;
    return ok ? 0 : 1;
}
