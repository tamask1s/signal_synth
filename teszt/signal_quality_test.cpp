#include "../src/ecg_export.h"
#include "../src/ecg_scenario_json.h"
#include "../src/signal_quality.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace
{
    bool check(bool condition, const char* name)
    {
        if (!condition)
            std::cerr << "FAIL: " << name << '\n';
        return condition;
    }

    signal_synth::ecg_scenario_document make_artifact_document()
    {
        signal_synth::ecg_scenario_document document;
        document.schema_version = 2;
        document.scenario_id = "artifact_pack";
        document.name = "Artifact Pack";
        document.tags.push_back("artifact");
        document.ppg.enabled = true;

        signal_synth::signal_quality_artifact_config wander;
        wander.type = signal_synth::signal_quality_ecg_baseline_wander;
        wander.start_seconds = 2.0;
        wander.duration_seconds = 3.0;
        wander.severity = 0.8;
        wander.seed = 42;
        wander.ecg_leads[signal_synth::clinical_lead_ii] = true;
        document.signal_quality.artifacts.push_back(wander);

        signal_synth::signal_quality_artifact_config ppg_dropout;
        ppg_dropout.type = signal_synth::signal_quality_ppg_dropout;
        ppg_dropout.start_seconds = 3.0;
        ppg_dropout.duration_seconds = 1.0;
        ppg_dropout.severity = 0.7;
        ppg_dropout.seed = 43;
        ppg_dropout.ppg = true;
        document.signal_quality.artifacts.push_back(ppg_dropout);
        return document;
    }

    bool same_vector(const std::vector<double>& left, const double* right, unsigned int count)
    {
        if (left.size() != count || !right)
            return false;
        for (unsigned int i = 0; i < count; ++i)
            if (left[i] != right[i])
                return false;
        return true;
    }

    bool any_difference(const std::vector<double>& left, const double* right, unsigned int first, unsigned int past)
    {
        if (!right || left.size() < past)
            return false;
        for (unsigned int i = first; i < past; ++i)
            if (std::fabs(left[i] - right[i]) > 1e-9)
                return true;
        return false;
    }
}

int main()
{
    bool ok = true;

    signal_synth::ecg_scenario_document document = make_artifact_document();
    signal_synth::ecg_scenario_json_result json_result;
    ok &= check(signal_synth::write_ecg_scenario_json(document, json_result), "write_artifact_document");
    ok &= check(json_result.canonical_json.find("\"artifacts\":[") != std::string::npos && json_result.canonical_json.find("\"ecg_baseline_wander\"") != std::string::npos && json_result.canonical_json.find("\"ppg_dropout\"") != std::string::npos, "artifact_canonical_json");

    signal_synth::ecg_scenario_document roundtrip;
    signal_synth::ecg_scenario_json_result roundtrip_result;
    ok &= check(signal_synth::parse_ecg_scenario_json(json_result.canonical_json, roundtrip, roundtrip_result) && roundtrip.signal_quality.artifacts.size() == 2 && roundtrip_result.document_fingerprint == json_result.document_fingerprint, "artifact_json_roundtrip");

    signal_synth::ecg_scenario_document changed = document;
    changed.signal_quality.artifacts[0].severity = 0.4;
    signal_synth::ecg_scenario_json_result changed_result;
    ok &= check(signal_synth::write_ecg_scenario_json(changed, changed_result) && changed_result.document_fingerprint != json_result.document_fingerprint && changed_result.generation_fingerprint == json_result.generation_fingerprint, "artifact_changes_document_not_ecg_fingerprint");

    signal_synth::ecg_render_bundle render;
    signal_synth::ecg_export_result export_result;
    ok &= check(signal_synth::render_ecg_document(document, render, export_result) && render.signal_quality.artifacts.size() == 2 && render.metrics.artifact_count == 2, "render_artifact_document");
    ok &= check(render.signal_quality.artifacts[0].start_sample_index == 1000 && render.signal_quality.artifacts[0].end_sample_index == 2499 && std::fabs(render.signal_quality.artifacts[0].end_seconds - 5.0) < 1e-12, "artifact_interval_exactness");
    ok &= check(same_vector(render.signal_quality.ecg_leads[signal_synth::clinical_lead_i], render.record.lead_data(signal_synth::clinical_lead_i), render.record.sample_count()), "unselected_ecg_channel_unchanged");
    ok &= check(any_difference(render.signal_quality.ecg_leads[signal_synth::clinical_lead_ii], render.record.lead_data(signal_synth::clinical_lead_ii), 1000, 2500), "selected_ecg_channel_modified");
    ok &= check(any_difference(render.signal_quality.ppg, render.ppg.samples(), 1500, 2000), "ppg_artifact_modified");

    signal_synth::ecg_render_bundle repeated;
    signal_synth::ecg_export_result repeated_result;
    ok &= check(signal_synth::render_ecg_document(document, repeated, repeated_result) && repeated.signal_quality.ecg_leads == render.signal_quality.ecg_leads && repeated.signal_quality.ppg == render.signal_quality.ppg, "artifact_render_reproducible");

    signal_synth::ecg_export_bundle bundle;
    ok &= check(signal_synth::build_ecg_export_bundle(render, bundle, export_result), "artifact_export_bundle");
    const signal_synth::ecg_text_artifact* annotations = bundle.find("annotations.json");
    const signal_synth::ecg_text_artifact* metrics = bundle.find("ground_truth_metrics.json");
    const signal_synth::ecg_text_artifact* report = bundle.find("report.html");
    ok &= check(annotations && annotations->content.find("\"artifact_intervals\":[{\"type\":\"ecg_baseline_wander\"") != std::string::npos && annotations->content.find("\"channels\":[\"II\"]") != std::string::npos, "artifact_annotation_export");
    ok &= check(metrics && metrics->content.find("\"artifacts\":{\"count\":2") != std::string::npos && metrics->content.find("\"ppg_seconds\":1") != std::string::npos, "artifact_metrics_export");
    ok &= check(report && report->content.find("Artifact Intervals") != std::string::npos && report->content.find("Acquisition artifacts corrupt waveform samples") != std::string::npos, "artifact_report_export");

    signal_synth::ecg_scenario_document invalid_ppg = document;
    invalid_ppg.ppg.enabled = false;
    signal_synth::ecg_scenario_json_result invalid_result;
    ok &= check(!signal_synth::write_ecg_scenario_json(invalid_ppg, invalid_result) && !invalid_result.messages.empty() && invalid_result.messages[0].path == "$.artifacts", "disabled_ppg_artifact_rejected");

    const std::string duplicate_channel =
        "{"
        "\"schema_version\":2,\"scenario_id\":\"artifact_bad\",\"name\":\"bad\",\"description\":\"\",\"author\":\"\",\"tags\":[],"
        "\"duration_seconds\":10,\"sample_rate_hz\":500,\"seed\":12345,"
        "\"ecg\":{\"heart_rate_bpm\":70,\"rr_variability_seconds\":0,\"ectopic_every_n_beats\":0,\"second_degree_av_pattern\":\"unspecified\",\"q_wave_territory\":\"unspecified\",\"episode_type\":\"none\",\"episode_start_seconds\":2,\"episode_duration_seconds\":4,\"episode_rate_bpm\":170,\"fidelity_policy\":\"allow_parameterized\",\"conditions\":[{\"code\":\"NORM\",\"severity\":1}]},"
        "\"ppg\":{\"enabled\":false,\"pulse_delay_ms\":180,\"rise_time_ms\":120,\"decay_time_ms\":300,\"amplitude_au\":1,\"baseline_au\":0,\"dicrotic_delay_ms\":180,\"dicrotic_width_ms\":80,\"dicrotic_amplitude_ratio\":0.15},"
        "\"artifacts\":[{\"type\":\"ecg_powerline\",\"start_seconds\":1,\"duration_seconds\":1,\"severity\":0.5,\"seed\":1,\"channels\":[\"II\",\"II\"]}]"
        "}";
    signal_synth::ecg_scenario_document duplicate_document;
    ok &= check(!signal_synth::parse_ecg_scenario_json(duplicate_channel, duplicate_document, invalid_result) && !invalid_result.messages.empty(), "duplicate_artifact_channel_rejected");

    const std::string overlapping_channel =
        "{"
        "\"schema_version\":2,\"scenario_id\":\"artifact_bad_overlap\",\"name\":\"bad\",\"description\":\"\",\"author\":\"\",\"tags\":[],"
        "\"duration_seconds\":10,\"sample_rate_hz\":500,\"seed\":12345,"
        "\"ecg\":{\"heart_rate_bpm\":70,\"rr_variability_seconds\":0,\"ectopic_every_n_beats\":0,\"second_degree_av_pattern\":\"unspecified\",\"q_wave_territory\":\"unspecified\",\"episode_type\":\"none\",\"episode_start_seconds\":2,\"episode_duration_seconds\":4,\"episode_rate_bpm\":170,\"fidelity_policy\":\"allow_parameterized\",\"conditions\":[{\"code\":\"NORM\",\"severity\":1}]},"
        "\"ppg\":{\"enabled\":false,\"pulse_delay_ms\":180,\"rise_time_ms\":120,\"decay_time_ms\":300,\"amplitude_au\":1,\"baseline_au\":0,\"dicrotic_delay_ms\":180,\"dicrotic_width_ms\":80,\"dicrotic_amplitude_ratio\":0.15},"
        "\"artifacts\":[{\"type\":\"ecg_powerline\",\"start_seconds\":1,\"duration_seconds\":1,\"severity\":0.5,\"seed\":1,\"channels\":[\"all_ecg\",\"II\"]}]"
        "}";
    ok &= check(!signal_synth::parse_ecg_scenario_json(overlapping_channel, duplicate_document, invalid_result) && !invalid_result.messages.empty(), "overlapping_artifact_channel_rejected");

    std::ifstream script("../examples/databrowser/075_ECG_Artifact_Signal_Quality.txt");
    if (!script.good())
        script.open("../../examples/databrowser/075_ECG_Artifact_Signal_Quality.txt");
    ok &= check(script.good(), "databrowser_artifact_script_present");

    std::ifstream example("../examples/scenarios/ecg_artifact_signal_quality.json");
    if (!example.good())
        example.open("../../examples/scenarios/ecg_artifact_signal_quality.json");
    const bool example_opened = example.good();
    std::stringstream example_stream;
    example_stream << example.rdbuf();
    signal_synth::ecg_scenario_document example_document;
    signal_synth::ecg_scenario_json_result example_json;
    signal_synth::ecg_render_bundle example_render;
    ok &= check(example_opened && signal_synth::parse_ecg_scenario_json(example_stream.str(), example_document, example_json) && signal_synth::render_ecg_document(example_document, example_render, export_result) && example_render.metrics.artifact_count == 6, "example_artifact_scenario_renders");

    return ok ? 0 : 1;
}
