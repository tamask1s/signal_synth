#include "../src/ecg_model.h"
#include "../src/ecg_model.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
    bool all_finite(const std::vector<double>& values)
    {
        for (double value : values)
        {
            if (!std::isfinite(value))
                return false;
        }
        return true;
    }

    bool check(bool condition, const char* name)
    {
        if (condition)
        {
            std::cout << "PASS " << name << '\n';
            return true;
        }
        std::cerr << "FAIL " << name << '\n';
        return false;
    }

    bool same_annotation(
        const signal_synth::ecg_model_annotation& left,
        const signal_synth::ecg_model_annotation& right)
    {
        return left.sample_index == right.sample_index &&
            left.beat_index == right.beat_index &&
            left.time_seconds == right.time_seconds &&
            left.phase_error_radians == right.phase_error_radians &&
            left.wave == right.wave &&
            left.present == right.present &&
            left.beat_kind == right.beat_kind &&
            left.rr_interval_seconds == right.rr_interval_seconds &&
            left.rr_was_clipped == right.rr_was_clipped;
    }

    double uneven_band_power(
        const std::vector<double>& times,
        const std::vector<double>& values,
        double mean,
        double low_frequency,
        double high_frequency)
    {
        const double pi = 3.14159265358979323846;
        const double frequency_step = 0.002;
        double power = 0.0;
        for (double frequency = low_frequency;
             frequency < high_frequency;
             frequency += frequency_step)
        {
            double real = 0.0;
            double imaginary = 0.0;
            for (std::size_t i = 0; i < values.size(); ++i)
            {
                const double phase =
                    2.0 * pi * frequency * times[i];
                real += (values[i] - mean) * std::cos(phase);
                imaginary +=
                    (values[i] - mean) * std::sin(phase);
            }
            power += real * real + imaginary * imaginary;
        }
        return power;
    }
}

int main()
{
    bool ok = true;

    signal_synth::ecg_model_config config;
    config.sampling_rate_hz = 500;
    config.heart_rate_bpm = 60.0;

    signal_synth::ecg_model whole_model(config);
    std::vector<double> whole(5000, 0.0);
    std::vector<signal_synth::ecg_model_annotation> whole_annotations(64);
    const signal_synth::ecg_render_result whole_result = whole_model.render(
        whole.data(),
        static_cast<unsigned int>(whole.size()),
        whole_annotations.data(),
        static_cast<unsigned int>(whole_annotations.size()));
    whole_annotations.resize(whole_result.annotations_written);

    ok &= check(whole_model.valid(), "default_config_valid");
    ok &= check(
        whole_result.samples_written == whole.size() &&
            whole_result.annotations_required ==
                whole_result.annotations_written &&
            whole_result.annotations_written == 50,
        "ten_beats_have_fifty_model_events");
    ok &= check(all_finite(whole), "all_samples_finite");

    double minimum = *std::min_element(whole.begin(), whole.end());
    double maximum = *std::max_element(whole.begin(), whole.end());
    ok &= check(
        minimum < -0.05 && maximum > 0.05,
        "bipolar_model_output");

    bool annotation_order = true;
    const double phase_step =
        2.0 * 3.14159265358979323846 * config.heart_rate_bpm /
        (60.0 * static_cast<double>(config.sampling_rate_hz));
    for (std::size_t i = 0; i < whole_annotations.size(); ++i)
    {
        const signal_synth::ecg_model_annotation& annotation =
            whole_annotations[i];
        annotation_order &=
            annotation.wave ==
                static_cast<signal_synth::ecg_wave>(
                    i % signal_synth::ecg_wave_count) &&
            annotation.beat_index ==
                static_cast<unsigned long long>(
                    i / signal_synth::ecg_wave_count) &&
            annotation.sample_index < whole.size() &&
            annotation.phase_error_radians >= 0.0 &&
            annotation.phase_error_radians < phase_step + 1e-12;
        if (i != 0)
        {
            annotation_order &=
                whole_annotations[i - 1].time_seconds <
                annotation.time_seconds;
        }
    }
    ok &= check(annotation_order, "annotations_are_ordered_and_quantified");

    signal_synth::ecg_model chunked_model(config);
    std::vector<double> chunked(whole.size(), 0.0);
    std::vector<signal_synth::ecg_model_annotation> chunked_annotations;
    const unsigned int chunk_sizes[] = {1, 7, 113, 2, 997, 31};
    std::size_t offset = 0;
    std::size_t chunk_index = 0;
    while (offset < chunked.size())
    {
        const unsigned int count = static_cast<unsigned int>(std::min(
            chunked.size() - offset,
            static_cast<std::size_t>(
                chunk_sizes[chunk_index %
                    (sizeof(chunk_sizes) / sizeof(chunk_sizes[0]))])));
        signal_synth::ecg_model_annotation annotations[16];
        const signal_synth::ecg_render_result result = chunked_model.render(
            chunked.data() + offset, count, annotations, 16);
        chunked_annotations.insert(
            chunked_annotations.end(),
            annotations,
            annotations + result.annotations_written);
        offset += count;
        ++chunk_index;
    }

    bool annotations_equal =
        chunked_annotations.size() == whole_annotations.size();
    for (std::size_t i = 0;
         annotations_equal && i < whole_annotations.size();
         ++i)
    {
        annotations_equal &=
            same_annotation(chunked_annotations[i], whole_annotations[i]);
    }
    ok &= check(
        chunked == whole && annotations_equal,
        "streaming_is_chunk_invariant");

    chunked_model.reset();
    std::vector<double> reset_output(whole.size(), 0.0);
    chunked_model.render(
        reset_output.data(),
        static_cast<unsigned int>(reset_output.size()));
    ok &= check(reset_output == whole, "reset_is_deterministic");

    signal_synth::ecg_model copied = whole_model;
    std::vector<double> continuation_a(1000, 0.0);
    std::vector<double> continuation_b(1000, 0.0);
    whole_model.render(
        continuation_a.data(),
        static_cast<unsigned int>(continuation_a.size()));
    copied.render(
        continuation_b.data(),
        static_cast<unsigned int>(continuation_b.size()));
    ok &= check(
        continuation_a == continuation_b,
        "copy_preserves_stream_state");

    std::vector<double> guarded(102, 0.0);
    guarded.front() = 12345.0;
    guarded.back() = 67890.0;
    signal_synth::ecg_model guarded_model(config);
    guarded_model.render(guarded.data() + 1, 100);
    ok &= check(
        guarded.front() == 12345.0 &&
            guarded.back() == 67890.0,
        "render_respects_buffer");

    signal_synth::ecg_model_config invalid = config;
    invalid.sampling_rate_hz = 0;
    const signal_synth::ecg_model_config old_config =
        guarded_model.config();
    ok &= check(
        !guarded_model.configure(invalid) &&
            guarded_model.valid() &&
            guarded_model.config().sampling_rate_hz ==
                old_config.sampling_rate_hz,
        "invalid_reconfigure_is_transactional");

    signal_synth::ecg_model_config invalid_order = config;
    invalid_order.waves[signal_synth::ecg_wave_q].phase_radians =
        invalid_order.waves[signal_synth::ecg_wave_r].phase_radians;
    signal_synth::ecg_model invalid_model(invalid_order);
    std::vector<double> invalid_output(16, 7.0);
    const signal_synth::ecg_render_result invalid_result =
        invalid_model.render(
            invalid_output.data(),
            static_cast<unsigned int>(invalid_output.size()));
    ok &= check(
        !invalid_model.valid() &&
            invalid_result.samples_written == 0 &&
            invalid_output.front() == 7.0,
        "invalid_constructor_does_not_render");

    signal_synth::ecg_model_annotation too_small[1];
    signal_synth::ecg_model capacity_model(config);
    std::vector<double> capacity_data(1000, 0.0);
    const signal_synth::ecg_render_result capacity_result =
        capacity_model.render(
            capacity_data.data(),
            static_cast<unsigned int>(capacity_data.size()),
            too_small,
            1);
    ok &= check(
        capacity_result.annotations_written == 1 &&
            capacity_result.annotations_required == 10,
        "annotation_capacity_is_reported");

    signal_synth::ecg_scenario_config scenario;
    scenario.premature_every_n_beats = 5;
    signal_synth::ecg_rr_generator scenario_rr(
        60.0, signal_synth::ecg_hrv_config(), scenario);
    std::vector<signal_synth::ecg_beat_plan> scenario_plans;
    double scenario_time = 0.0;
    for (unsigned long long beat = 0; beat < 12; ++beat)
    {
        const signal_synth::ecg_beat_plan plan =
            scenario_rr.next(beat, scenario_time);
        scenario_plans.push_back(plan);
        scenario_time += plan.rr_interval_seconds;
    }
    ok &= check(
        scenario_plans[3].kind == signal_synth::ecg_beat_sinus &&
            scenario_plans[4].kind ==
                signal_synth::ecg_beat_premature &&
            scenario_plans[4].rr_interval_seconds == 0.65 &&
            scenario_plans[5].kind ==
                signal_synth::ecg_beat_compensatory &&
            scenario_plans[5].rr_interval_seconds == 1.35 &&
            scenario_plans[6].kind == signal_synth::ecg_beat_sinus,
        "periodic_premature_and_compensatory_plan");

    signal_synth::ecg_hrv_config clipped_hrv;
    clipped_hrv.minimum_rr_seconds = 0.80;
    signal_synth::ecg_scenario_config clipped_scenario;
    clipped_scenario.premature_every_n_beats = 1;
    signal_synth::ecg_rr_generator clipped_rr(
        60.0, clipped_hrv, clipped_scenario);
    const signal_synth::ecg_beat_plan clipped_plan =
        clipped_rr.next(0, 0.0);
    ok &= check(
        clipped_plan.rr_interval_seconds == 0.80 &&
            clipped_plan.rr_was_clipped,
        "rr_clipping_is_annotated");

    signal_synth::ecg_hrv_config hrv;
    hrv.enabled = true;
    hrv.rr_standard_deviation_seconds = 0.06;
    hrv.seed = 123456789ULL;
    signal_synth::ecg_scenario_config no_scenario;
    signal_synth::ecg_rr_generator hrv_a(60.0, hrv, no_scenario);
    signal_synth::ecg_rr_generator hrv_b(60.0, hrv, no_scenario);
    signal_synth::ecg_hrv_config other_hrv = hrv;
    other_hrv.seed = 987654321ULL;
    signal_synth::ecg_rr_generator hrv_c(
        60.0, other_hrv, no_scenario);
    std::vector<double> rr_a;
    std::vector<double> rr_b;
    std::vector<double> rr_c;
    std::vector<double> rr_times;
    double time_a = 0.0;
    double time_b = 0.0;
    double time_c = 0.0;
    for (unsigned long long beat = 0; beat < 600; ++beat)
    {
        const signal_synth::ecg_beat_plan plan_a =
            hrv_a.next(beat, time_a);
        const signal_synth::ecg_beat_plan plan_b =
            hrv_b.next(beat, time_b);
        const signal_synth::ecg_beat_plan plan_c =
            hrv_c.next(beat, time_c);
        rr_a.push_back(plan_a.rr_interval_seconds);
        rr_b.push_back(plan_b.rr_interval_seconds);
        rr_c.push_back(plan_c.rr_interval_seconds);
        rr_times.push_back(time_a);
        time_a += plan_a.rr_interval_seconds;
        time_b += plan_b.rr_interval_seconds;
        time_c += plan_c.rr_interval_seconds;
    }
    double rr_mean = 0.0;
    for (double rr : rr_a)
        rr_mean += rr;
    rr_mean /= static_cast<double>(rr_a.size());
    double rr_variance = 0.0;
    for (double rr : rr_a)
        rr_variance += (rr - rr_mean) * (rr - rr_mean);
    rr_variance /= static_cast<double>(rr_a.size());
    const double rr_standard_deviation = std::sqrt(rr_variance);
    const double lf_power = uneven_band_power(
        rr_times, rr_a, rr_mean, 0.04, 0.15);
    const double hf_power = uneven_band_power(
        rr_times, rr_a, rr_mean, 0.15, 0.40);
    const double measured_lf_hf_ratio = lf_power / hf_power;
    ok &= check(
        rr_a == rr_b &&
            rr_a != rr_c &&
            rr_mean > 0.98 &&
            rr_mean < 1.02 &&
            rr_standard_deviation > 0.04 &&
            rr_standard_deviation < 0.08 &&
            measured_lf_hf_ratio > 0.35 &&
            measured_lf_hf_ratio < 0.70,
        "hrv_statistics_and_seed_reproducibility");

    signal_synth::ecg_model_config scenario_config;
    scenario_config.sampling_rate_hz = 500;
    scenario_config.heart_rate_bpm = 60.0;
    scenario_config.scenario = scenario;
    signal_synth::ecg_model scenario_model(scenario_config);
    std::vector<double> scenario_signal(4500, 0.0);
    std::vector<signal_synth::ecg_model_annotation>
        scenario_annotations(64);
    const signal_synth::ecg_render_result scenario_result =
        scenario_model.render(
            scenario_signal.data(),
            static_cast<unsigned int>(scenario_signal.size()),
            scenario_annotations.data(),
            static_cast<unsigned int>(scenario_annotations.size()));
    scenario_annotations.resize(scenario_result.annotations_written);
    std::vector<signal_synth::ecg_model_annotation> r_annotations;
    for (const signal_synth::ecg_model_annotation& annotation :
         scenario_annotations)
    {
        if (annotation.wave == signal_synth::ecg_wave_r)
            r_annotations.push_back(annotation);
    }
    bool rr_timing_exact = r_annotations.size() >= 7;
    for (std::size_t i = 1; rr_timing_exact && i < r_annotations.size(); ++i)
    {
        const double measured_rr =
            r_annotations[i].time_seconds -
            r_annotations[i - 1].time_seconds;
        rr_timing_exact &=
            std::fabs(
                measured_rr -
                r_annotations[i].rr_interval_seconds) < 1e-12;
    }
    if (r_annotations.size() >= 7)
    {
        rr_timing_exact &=
            r_annotations[4].beat_kind ==
                signal_synth::ecg_beat_premature &&
            r_annotations[5].beat_kind ==
                signal_synth::ecg_beat_compensatory;
    }
    ok &= check(rr_timing_exact, "model_r_intervals_match_beat_plan");

    bool premature_p_absent = false;
    for (const signal_synth::ecg_model_annotation& annotation :
         scenario_annotations)
    {
        if (annotation.beat_index == 4 &&
            annotation.wave == signal_synth::ecg_wave_p)
        {
            premature_p_absent =
                annotation.beat_kind ==
                    signal_synth::ecg_beat_premature &&
                !annotation.present;
        }
    }
    ok &= check(
        premature_p_absent,
        "absent_p_wave_is_explicitly_annotated");

    std::vector<signal_synth::ecg_measured_fiducial>
        scenario_fiducials(scenario_annotations.size());
    const signal_synth::ecg_fiducial_result scenario_fiducial_result =
        signal_synth::measure_ecg_fiducials(
            scenario_signal.data(),
            static_cast<unsigned int>(scenario_signal.size()),
            scenario_config.sampling_rate_hz,
            scenario_annotations.data(),
            static_cast<unsigned int>(scenario_annotations.size()),
            scenario_fiducials.data(),
            static_cast<unsigned int>(scenario_fiducials.size()));
    scenario_fiducials.resize(
        scenario_fiducial_result.fiducials_written);
    bool absent_p_not_measured = true;
    for (const signal_synth::ecg_measured_fiducial& fiducial :
         scenario_fiducials)
    {
        absent_p_not_measured &=
            !(fiducial.beat_index == 4 &&
                fiducial.wave == signal_synth::ecg_wave_p);
    }
    ok &= check(
        absent_p_not_measured &&
            scenario_fiducial_result.fiducials_required <
                scenario_annotations.size(),
        "absent_wave_has_no_measured_fiducial");

    signal_synth::ecg_model scenario_chunked_model(scenario_config);
    std::vector<double> scenario_chunked(scenario_signal.size(), 0.0);
    std::vector<signal_synth::ecg_model_annotation>
        scenario_chunked_annotations;
    offset = 0;
    chunk_index = 0;
    while (offset < scenario_chunked.size())
    {
        const unsigned int count = static_cast<unsigned int>(std::min(
            scenario_chunked.size() - offset,
            static_cast<std::size_t>(
                chunk_sizes[chunk_index %
                    (sizeof(chunk_sizes) / sizeof(chunk_sizes[0]))])));
        signal_synth::ecg_model_annotation annotations[16];
        const signal_synth::ecg_render_result result =
            scenario_chunked_model.render(
                scenario_chunked.data() + offset,
                count,
                annotations,
                16);
        scenario_chunked_annotations.insert(
            scenario_chunked_annotations.end(),
            annotations,
            annotations + result.annotations_written);
        offset += count;
        ++chunk_index;
    }
    bool scenario_annotations_equal =
        scenario_chunked_annotations.size() ==
            scenario_annotations.size();
    for (std::size_t i = 0;
         scenario_annotations_equal && i < scenario_annotations.size();
         ++i)
    {
        scenario_annotations_equal &= same_annotation(
            scenario_chunked_annotations[i],
            scenario_annotations[i]);
    }
    ok &= check(
        scenario_chunked == scenario_signal &&
            scenario_annotations_equal,
        "scenario_streaming_is_chunk_invariant");

    std::vector<signal_synth::ecg_measured_fiducial> measured(
        whole_annotations.size());
    const signal_synth::ecg_fiducial_result measured_result =
        signal_synth::measure_ecg_fiducials(
            whole.data(),
            static_cast<unsigned int>(whole.size()),
            config.sampling_rate_hz,
            whole_annotations.data(),
            static_cast<unsigned int>(whole_annotations.size()),
            measured.data(),
            static_cast<unsigned int>(measured.size()));
    measured.resize(measured_result.fiducials_written);
    bool measured_valid =
        measured_result.fiducials_required ==
            whole_annotations.size() &&
        measured.size() == whole_annotations.size();
    for (std::size_t i = 0; measured_valid && i < measured.size(); ++i)
    {
        const signal_synth::ecg_measured_fiducial& fiducial =
            measured[i];
        measured_valid &=
            fiducial.wave == whole_annotations[i].wave &&
            fiducial.beat_index == whole_annotations[i].beat_index &&
            fiducial.sample_index < whole.size() &&
            std::isfinite(fiducial.time_seconds) &&
            std::isfinite(fiducial.interpolated_value) &&
            std::fabs(
                fiducial.time_seconds -
                static_cast<double>(fiducial.sample_index) /
                    config.sampling_rate_hz) <=
                0.5 / config.sampling_rate_hz + 1e-12;
        if (fiducial.wave == signal_synth::ecg_wave_p ||
            fiducial.wave == signal_synth::ecg_wave_t)
        {
            measured_valid &=
                fiducial.has_onset_offset &&
                fiducial.onset_sample_index <
                    fiducial.sample_index &&
                fiducial.offset_sample_index >
                    fiducial.sample_index &&
                fiducial.onset_time_seconds <
                    fiducial.time_seconds &&
                fiducial.offset_time_seconds >
                    fiducial.time_seconds;
        }
        else
            measured_valid &= !fiducial.has_onset_offset;
        if (fiducial.wave == signal_synth::ecg_wave_r &&
            fiducial.sample_index > 0 &&
            fiducial.sample_index + 1 < whole.size())
        {
            measured_valid &=
                whole[fiducial.sample_index] >=
                    whole[fiducial.sample_index - 1] &&
                whole[fiducial.sample_index] >=
                    whole[fiducial.sample_index + 1];
        }
    }
    ok &= check(
        measured_valid,
        "measured_fiducials_are_exact_sample_extrema");

    signal_synth::ecg_validation_package package;
    const bool package_generated = package.generate(
        scenario_config,
        static_cast<unsigned int>(scenario_signal.size()));
    bool package_valid =
        package_generated &&
        package.sample_count() == scenario_signal.size() &&
        package.model_annotation_count() ==
            scenario_annotations.size() &&
        package.measured_fiducial_count() ==
            scenario_fiducials.size();
    for (int channel = 0;
         package_valid &&
             channel < signal_synth::ecg_validation_channel_count;
         ++channel)
    {
        package_valid &=
            package.channel(
                static_cast<signal_synth::ecg_validation_channel>(
                    channel)) != 0;
    }
    if (package_valid)
    {
        package_valid &= std::equal(
            scenario_signal.begin(),
            scenario_signal.end(),
            package.channel(signal_synth::ecg_validation_signal));
        const signal_synth::ecg_model_annotation* package_annotations =
            package.model_annotations();
        for (std::size_t i = 0;
             package_valid && i < scenario_annotations.size();
             ++i)
        {
            package_valid &= same_annotation(
                package_annotations[i],
                scenario_annotations[i]);
        }
        const double* measured_channel = package.channel(
            signal_synth::ecg_validation_measured_fiducials);
        for (std::size_t i = 0;
             package_valid && i < scenario_fiducials.size();
             ++i)
        {
            const signal_synth::ecg_measured_fiducial& fiducial =
                scenario_fiducials[i];
            if (fiducial.has_onset_offset)
            {
                package_valid &=
                    measured_channel[fiducial.onset_sample_index] !=
                        0.0 &&
                    measured_channel[fiducial.offset_sample_index] !=
                        0.0;
            }
        }
    }
    ok &= check(
        package_valid,
        "validation_package_channels_match_annotations");

    signal_synth::ecg_model_config invalid_package_config =
        scenario_config;
    invalid_package_config.hrv.minimum_rr_seconds = 0.0;
    ok &= check(
        !package.generate(invalid_package_config, 100) &&
            package.sample_count() == scenario_signal.size(),
        "validation_package_generation_is_transactional");

    return ok ? 0 : 1;
}
