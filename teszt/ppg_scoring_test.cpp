#include "detection_io.h"
#include "ecg_compare.h"
#include "ecg_export.h"

#include <iostream>
#include <string>
#include <vector>

namespace
{
    bool check(bool condition, const char* name)
    {
        if (!condition)
            std::cerr << "FAILED: " << name << '\n';
        return condition;
    }

    signal_synth::signal_quality_artifact_config artifact(signal_synth::signal_quality_artifact_type type, double start, double duration, unsigned long long seed)
    {
        signal_synth::signal_quality_artifact_config value;
        value.type = type;
        value.start_seconds = start;
        value.duration_seconds = duration;
        value.severity = 0.75;
        value.seed = seed;
        value.ppg = true;
        return value;
    }

    std::vector<signal_synth::ecg_detected_event> detections(const signal_synth::ppg_record& ppg, signal_synth::ppg_fiducial_kind kind)
    {
        std::vector<signal_synth::ecg_detected_event> output;
        for (unsigned int i = 0; i < ppg.annotation_count(); ++i)
        {
            const signal_synth::ppg_annotation& annotation = ppg.annotations()[i];
            if (annotation.kind == kind && annotation.source == signal_synth::ppg_fiducial_measurement)
            {
                signal_synth::ecg_detected_event event;
                event.time_seconds = annotation.time_seconds;
                output.push_back(event);
            }
        }
        return output;
    }
}

int main()
{
    bool ok = true;
    signal_synth::ecg_scenario_document document;
    document.schema_version = 4;
    document.scenario_id = "ppg_scoring";
    document.duration_seconds = 30.0;
    document.ecg.set_sampling_rate_hz(250);
    document.ppg.enabled = true;
    signal_synth::ppg_perfusion_episode_config perfusion;
    perfusion.start_seconds = 5.0;
    perfusion.duration_seconds = 5.0;
    perfusion.amplitude_scale = 0.45;
    document.ppg.perfusion_episodes.push_back(perfusion);
    document.signal_quality.artifacts.push_back(artifact(signal_synth::signal_quality_ppg_motion_periodic, 12.0, 4.0, 53001));
    document.signal_quality.artifacts.push_back(artifact(signal_synth::signal_quality_ppg_dropout, 20.0, 4.0, 53002));

    signal_synth::ecg_render_bundle render;
    signal_synth::ecg_export_result export_result;
    ok &= check(signal_synth::render_ecg_document(document, render, export_result), "render");

    signal_synth::ecg_compare_options options;
    options.target = signal_synth::ecg_compare_ppg_pulse_onset;
    std::vector<signal_synth::ecg_detected_event> onset_detections = detections(render.ppg, signal_synth::ppg_pulse_onset);
    signal_synth::ecg_compare_result onset;
    ok &= check(signal_synth::compare_detections_to_render(render, onset_detections, options, onset)
        && onset.total.f1_score == 1.0
        && onset.low_perfusion.ground_truth_count > 0u
        && onset.motion.ground_truth_count > 0u
        && onset.dropout.ground_truth_count > 0u
        && onset.pulse_timing.ground_truth_interval_count + 1u == onset.total.ground_truth_count
        && onset.pulse_timing.matched_interval_count == onset.pulse_timing.ground_truth_interval_count
        && onset.pulse_timing.mean_absolute_interval_error_seconds == 0.0
        && onset.pulse_timing.absolute_pulse_rate_error_bpm == 0.0, "perfect_onset_scoring");

    std::vector<signal_synth::ecg_detected_event> drifted = onset_detections;
    for (std::size_t i = 0; i < drifted.size(); ++i)
        drifted[i].time_seconds += 0.0002 * i;
    signal_synth::ecg_compare_result drifted_result;
    ok &= check(signal_synth::compare_detections_to_render(render, drifted, options, drifted_result)
        && drifted_result.total.f1_score == 1.0
        && drifted_result.pulse_timing.mean_absolute_interval_error_seconds > 0.0
        && drifted_result.pulse_timing.absolute_pulse_rate_error_bpm > 0.0, "interval_and_rate_error");

    options.target = signal_synth::ecg_compare_ppg_systolic_peak;
    signal_synth::ecg_compare_result peak;
    ok &= check(signal_synth::compare_detections_to_render(render, detections(render.ppg, signal_synth::ppg_systolic_peak), options, peak)
        && peak.pulse_timing.matched_interval_count > 0u
        && peak.dropout.ground_truth_count > 0u, "peak_timing_and_dropout");

    signal_synth::ecg_compare_target parsed_target;
    ok &= check(signal_synth::detection_compare_target_from_name("ppg_pulse_onset", parsed_target)
        && parsed_target == signal_synth::ecg_compare_ppg_pulse_onset
        && signal_synth::ecg_compare_result_json(render, onset).find("\"pulse_timing\":") != std::string::npos
        && signal_synth::ecg_compare_result_json(render, onset).find("\"dropout\":") != std::string::npos, "public_contract");

    if (!ok)
        return 1;
    std::cout << "ppg_scoring_test=passed\n";
    return 0;
}
