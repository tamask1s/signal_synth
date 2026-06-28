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
            left.wave == right.wave;
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

    return ok ? 0 : 1;
}
