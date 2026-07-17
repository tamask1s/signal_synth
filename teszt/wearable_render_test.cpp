#include "challenge_assembly.h"
#include "ecg_export.h"
#include "measurement_scoring.h"

#include <iostream>
#include <string>

namespace
{
    bool check(bool condition, const char* name)
    {
        if (!condition)
            std::cerr << "FAILED: " << name << '\n';
        return condition;
    }

    void configure_stream(signal_synth::wearable_stream_config& stream, unsigned int rate, double offset, double drift, double jitter, unsigned int packet_size, double loss, unsigned int burst, unsigned long long seed)
    {
        stream.enabled = true;
        stream.sample_rate_hz = rate;
        stream.clock_offset_ms = offset;
        stream.clock_drift_ppm = drift;
        stream.timestamp_jitter_ms = jitter;
        stream.packet_size_samples = packet_size;
        stream.packet_loss_probability = loss;
        stream.packet_loss_burst_packets = burst;
        stream.seed = seed;
    }

    bool has_measurement(const std::vector<signal_synth::measurement_truth>& truth, const char* name)
    {
        for (std::size_t i = 0; i < truth.size(); ++i)
            if (truth[i].measurement.name == name)
                return true;
        return false;
    }

    unsigned int line_count(const std::string& text)
    {
        unsigned int count = 0;
        for (std::size_t i = 0; i < text.size(); ++i)
            count += text[i] == '\n';
        return count;
    }
}

int main()
{
    bool ok = true;
    signal_synth::ecg_scenario_document document;
    document.schema_version = 6;
    document.scenario_id = "wearable_render_test_v6";
    document.name = "Wearable render test";
    document.duration_seconds = 12.0;
    document.ecg.set_sampling_rate_hz(250u);
    document.ppg.enabled = true;
    document.ppg.optical.enabled = true;
    document.physiology.activity_start_seconds = 2.0;
    document.physiology.activity_duration_seconds = 8.0;
    document.physiology.activity_intensity = 0.4;
    configure_stream(document.wearable.ecg, 200u, 4.0, 40.0, 0.2, 25u, 0.01, 2u, 77011u);
    configure_stream(document.wearable.ppg, 100u, 18.0, -120.0, 0.8, 10u, 0.08, 2u, 77012u);
    configure_stream(document.wearable.accelerometer, 50u, -12.0, 80.0, 1.5, 10u, 0.03, 3u, 77013u);

    signal_synth::ecg_scenario_json_result identity;
    ok &= check(signal_synth::write_ecg_scenario_json(document, identity) && identity.canonical_json.find("\"wearable\"") != std::string::npos && identity.canonical_json.find("\"optical\":{\"enabled\":true") != std::string::npos, "schema_v6_write");
    signal_synth::ecg_scenario_document parsed;
    signal_synth::ecg_scenario_json_result parsed_identity;
    ok &= check(signal_synth::parse_ecg_scenario_json(identity.canonical_json, parsed, parsed_identity) && parsed_identity.document_fingerprint == identity.document_fingerprint && parsed.wearable.ppg.clock_drift_ppm == -120.0, "schema_v6_roundtrip");

    signal_synth::ecg_render_bundle render;
    signal_synth::ecg_document_render_result render_result;
    ok &= check(signal_synth::render_ecg_document(parsed, render, render_result) && render.wearable.streams.size() == 3u && !render.wearable.alignments.empty() && render.render_identity.find(":wearable-fnv1a64:") != std::string::npos, "wearable_render");
    const signal_synth::wearable_stream_record* ecg = render.wearable.stream(signal_synth::wearable_stream_ecg);
    const signal_synth::wearable_stream_record* ppg = render.wearable.stream(signal_synth::wearable_stream_ppg);
    const signal_synth::wearable_stream_record* accel = render.wearable.stream(signal_synth::wearable_stream_accelerometer);
    ok &= check(ecg && ppg && accel && ecg->config.sample_rate_hz == 200u && ppg->config.sample_rate_hz == 100u && accel->config.sample_rate_hz == 50u && ppg->channel_count() == 3u, "independent_streams");

    signal_synth::ecg_export_bundle exports;
    signal_synth::ecg_export_result export_result;
    ok &= check(signal_synth::build_ecg_export_bundle(render, exports, export_result), "wearable_export");
    const signal_synth::ecg_text_artifact* ppg_samples = exports.find("wearable_ppg_samples.csv");
    const signal_synth::ecg_text_artifact* timestamps = exports.find("wearable_timestamp_truth.csv");
    const signal_synth::ecg_text_artifact* timebase = exports.find("wearable_timebase_truth.json");
    const signal_synth::ecg_text_artifact* alignment = exports.find("wearable_alignment_truth.json");
    ok &= check(ppg_samples && timestamps && timebase && alignment && line_count(ppg_samples->content) == ppg->received_sample_count() + 1u && timebase->content.find("linear_interpolation") != std::string::npos && alignment->content.find("observed_minus_physiological") != std::string::npos, "wearable_export_contract");
    ok &= check(signal_synth::challenge_file_role_for_export_artifact("wearable_ppg_samples.csv") == signal_synth::challenge_file_wearable_samples_csv && signal_synth::challenge_file_role_for_export_artifact("wearable_timestamp_truth.csv") == signal_synth::challenge_file_wearable_timestamp_truth_csv, "challenge_roles");

    std::vector<signal_synth::measurement_truth> measurements;
    std::vector<std::string> messages;
    ok &= check(signal_synth::measurement_ground_truth_from_render(render, "ecg_ppg_alignment", measurements, messages) && has_measurement(measurements, "device_timestamp_onset_delta") && has_measurement(measurements, "clock_and_sampling_peak_error"), "device_measurement_truth");

    signal_synth::ecg_scenario_document invalid = document;
    invalid.wearable.accelerometer.enabled = false;
    invalid.wearable.accelerometer.sample_rate_hz = 50u;
    signal_synth::ecg_scenario_json_result invalid_result;
    ok &= check(!signal_synth::write_ecg_scenario_json(invalid, invalid_result), "disabled_stream_must_be_default");
    return ok ? 0 : 1;
}
