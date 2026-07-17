#include "ecg_compare.h"
#include "ecg_export.h"
#include "ecg_scenario_json.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
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

    signal_synth::signal_quality_artifact_config ppg_artifact(signal_synth::signal_quality_artifact_type type, double start, double duration, double severity, unsigned long long seed)
    {
        signal_synth::signal_quality_artifact_config artifact;
        artifact.type = type;
        artifact.start_seconds = start;
        artifact.duration_seconds = duration;
        artifact.severity = severity;
        artifact.seed = seed;
        artifact.ppg = true;
        return artifact;
    }

    bool any_nonzero(const std::vector<double>& samples, unsigned int first, unsigned int past)
    {
        for (unsigned int sample = first; sample < past; ++sample)
            if (std::fabs(samples[sample]) > 1e-9)
                return true;
        return false;
    }

    unsigned int text_uint(const std::string& text, std::size_t offset, std::size_t width)
    {
        return static_cast<unsigned int>(std::strtoul(text.substr(offset, width).c_str(), 0, 10));
    }
}

int main()
{
    bool ok = true;
    signal_synth::ecg_scenario_document document;
    document.schema_version = 4;
    document.scenario_id = "ppg_motion_accelerometer";
    document.name = "PPG motion and accelerometer";
    document.duration_seconds = 40.0;
    document.ecg.set_sampling_rate_hz(250);
    document.ppg.enabled = true;
    document.signal_quality.artifacts.push_back(ppg_artifact(signal_synth::signal_quality_ppg_motion_periodic, 5.0, 5.0, 0.65, 51001));
    document.signal_quality.artifacts.push_back(ppg_artifact(signal_synth::signal_quality_ppg_motion_burst, 12.0, 5.0, 0.75, 51002));
    document.signal_quality.artifacts.push_back(ppg_artifact(signal_synth::signal_quality_ppg_motion_broadband, 19.0, 5.0, 0.55, 51003));
    document.signal_quality.artifacts.push_back(ppg_artifact(signal_synth::signal_quality_ppg_ambient_light, 26.0, 4.0, 0.45, 51004));
    document.signal_quality.artifacts.push_back(ppg_artifact(signal_synth::signal_quality_ppg_sensor_saturation, 32.0, 4.0, 0.80, 51005));

    signal_synth::ecg_scenario_json_result written;
    signal_synth::ecg_scenario_document parsed;
    signal_synth::ecg_scenario_json_result parsed_result;
    ok &= check(signal_synth::write_ecg_scenario_json(document, written)
        && signal_synth::parse_ecg_scenario_json(written.canonical_json, parsed, parsed_result)
        && parsed.signal_quality.artifacts.size() == 5u
        && written.document_fingerprint == parsed_result.document_fingerprint, "artifact_json_roundtrip");

    signal_synth::ecg_render_bundle first;
    signal_synth::ecg_render_bundle repeated;
    signal_synth::ecg_document_render_result result;
    signal_synth::ecg_export_result export_result;
    ok &= check(signal_synth::render_ecg_document(document, first, result)
        && signal_synth::render_ecg_document(document, repeated, result)
        && first.signal_quality.ppg_channels == repeated.signal_quality.ppg_channels
        && first.signal_quality.accelerometer == repeated.signal_quality.accelerometer, "deterministic_replay");
    signal_synth::ecg_scenario_document reordered_document = document;
    std::reverse(reordered_document.signal_quality.artifacts.begin(), reordered_document.signal_quality.artifacts.end());
    signal_synth::ecg_render_bundle reordered;
    ok &= check(signal_synth::render_ecg_document(reordered_document, reordered, result)
        && first.signal_quality.ppg_channels == reordered.signal_quality.ppg_channels
        && first.signal_quality.accelerometer == reordered.signal_quality.accelerometer, "nonoverlapping_artifact_order_independence");
    ok &= check(first.signal_quality.accelerometer.size() == first.record.sample_count()
        && first.signal_quality.artifacts.size() == 5u, "reference_channel_and_intervals");
    ok &= check(first.signal_quality.artifacts[0].start_sample_index == 1250u
        && first.signal_quality.artifacts[0].end_sample_index == 2499u
        && first.signal_quality.artifacts[0].accelerometer_reference
        && !first.signal_quality.artifacts[3].accelerometer_reference, "exact_interval_metadata");

    ok &= check(any_nonzero(first.signal_quality.accelerometer, 1250u, 2500u)
        && any_nonzero(first.signal_quality.accelerometer, 3000u, 4250u)
        && any_nonzero(first.signal_quality.accelerometer, 4750u, 6000u), "all_motion_classes_have_reference");
    bool zero_outside_motion = true;
    for (unsigned int sample = 0; sample < first.record.sample_count(); ++sample)
    {
        const bool in_motion = (sample >= 1250u && sample < 2500u) || (sample >= 3000u && sample < 4250u) || (sample >= 4750u && sample < 6000u);
        if (!in_motion && first.signal_quality.accelerometer[sample] != 0.0)
            zero_outside_motion = false;
    }
    ok &= check(zero_outside_motion, "reference_zero_outside_motion");

    signal_synth::ecg_scenario_document clean_document = document;
    clean_document.signal_quality.artifacts.clear();
    signal_synth::ecg_render_bundle clean;
    ok &= check(signal_synth::render_ecg_document(clean_document, clean, result), "clean_reference_render");
    for (std::size_t artifact = 0; artifact < first.signal_quality.artifacts.size(); ++artifact)
    {
        const signal_synth::signal_quality_artifact_interval& interval = first.signal_quality.artifacts[artifact];
        const std::size_t first_sample = static_cast<std::size_t>(interval.start_sample_index);
        const std::size_t last_sample = static_cast<std::size_t>(interval.end_sample_index);
        bool changed_inside = false;
        for (std::size_t sample = first_sample; sample <= last_sample; ++sample)
            changed_inside = changed_inside || first.signal_quality.ppg_channels[0][sample] != clean.signal_quality.ppg_channels[0][sample];
        ok &= changed_inside
            && first.signal_quality.ppg_channels[0][first_sample] == clean.signal_quality.ppg_channels[0][first_sample]
            && first.signal_quality.ppg_channels[0][last_sample] == clean.signal_quality.ppg_channels[0][last_sample];
    }
    ok &= check(ok, "all_artifacts_modify_interior_and_taper_boundaries");

    signal_synth::ecg_export_bundle bundle;
    ok &= check(signal_synth::build_ecg_export_bundle(first, bundle, export_result), "export_bundle");
    const signal_synth::ecg_text_artifact* waveform = bundle.find("waveform.csv");
    const signal_synth::ecg_text_artifact* annotations = bundle.find("annotations.json");
    const signal_synth::ecg_text_artifact* metadata = bundle.find("metadata.json");
    const signal_synth::ecg_text_artifact* wfdb_header = bundle.find("synsigra.hea");
    const signal_synth::ecg_text_artifact* wfdb_signal = bundle.find("synsigra.dat");
    const signal_synth::ecg_text_artifact* wfdb_metadata = bundle.find("wfdb_metadata.json");
    const signal_synth::ecg_text_artifact* edf = bundle.find("synsigra.edf");
    const signal_synth::ecg_text_artifact* bdf = bundle.find("synsigra.bdf");
    const signal_synth::ecg_text_artifact* edf_metadata = bundle.find("edf_bdf_metadata.json");
    ok &= check(waveform && waveform->content.find(",accel_motion_g\n") != std::string::npos
        && annotations && annotations->content.find("\"channels\":[\"ppg_green\",\"accel_motion\"]") != std::string::npos
        && metadata && metadata->content.find("\"name\":\"accel_motion\",\"unit\":\"g\",\"role\":\"motion_reference\"") != std::string::npos
        && wfdb_header && wfdb_header->content.find(" 14 250 10000\n") != std::string::npos
        && wfdb_header->content.find("(0)/g") != std::string::npos
        && wfdb_signal && wfdb_signal->content.size() == first.record.sample_count() * 14u * 2u
        && wfdb_metadata && wfdb_metadata->content.find("\"role\":\"motion_reference\"") != std::string::npos
        && edf && text_uint(edf->content, 252u, 4u) == 15u && text_uint(edf->content, 184u, 8u) == 256u + 15u * 256u
        && bdf && text_uint(bdf->content, 252u, 4u) == 15u && text_uint(bdf->content, 184u, 8u) == 256u + 15u * 256u
        && edf_metadata && edf_metadata->content.find("\"role\":\"motion_reference\"") != std::string::npos, "portable_reference_export");

    std::vector<signal_synth::ecg_detected_event> detections;
    for (unsigned int i = 0; i < first.ppg.annotation_count(); ++i)
    {
        const signal_synth::ppg_annotation& annotation = first.ppg.annotations()[i];
        if (annotation.kind == signal_synth::ppg_systolic_peak && annotation.source == signal_synth::ppg_fiducial_measurement)
        {
            signal_synth::ecg_detected_event detection;
            detection.time_seconds = annotation.time_seconds;
            detections.push_back(detection);
        }
    }
    signal_synth::ecg_compare_options options;
    options.target = signal_synth::ecg_compare_ppg_systolic_peak;
    signal_synth::ecg_compare_result comparison;
    ok &= check(signal_synth::compare_detections_to_render(first, detections, options, comparison)
        && comparison.motion.ground_truth_count > 0u
        && comparison.motion.true_positive_count == comparison.motion.ground_truth_count
        && comparison.motion.detection_count == comparison.motion.ground_truth_count
        && signal_synth::ecg_compare_result_json(first, comparison).find("\"motion\":") != std::string::npos, "motion_scoring_bin");

    if (!ok)
        return 1;
    std::cout << "ppg_motion_test=passed\n";
    return 0;
}
