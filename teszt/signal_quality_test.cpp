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

    signal_synth::signal_quality_artifact_config make_ecg_fault(signal_synth::signal_quality_artifact_type type, unsigned int first_lead, unsigned int second_lead = signal_synth::clinical_lead_count)
    {
        signal_synth::signal_quality_artifact_config artifact;
        artifact.type = type;
        artifact.start_seconds = 1.0;
        artifact.duration_seconds = 0.5;
        artifact.severity = 0.8;
        artifact.seed = 9001;
        artifact.ecg_leads[first_lead] = true;
        if (second_lead < signal_synth::clinical_lead_count)
            artifact.ecg_leads[second_lead] = true;
        return artifact;
    }

    bool changed_by_fault(signal_synth::signal_quality_artifact_type type, const signal_synth::clinical_ecg_record& clean, unsigned int lead)
    {
        signal_synth::signal_quality_config config;
        config.artifacts.push_back(make_ecg_fault(type, lead));
        signal_synth::ppg_record no_ppg;
        signal_synth::signal_quality_waveforms output;
        return signal_synth::apply_signal_quality_artifacts(config, clean, no_ppg, output) && any_difference(output.ecg_leads[lead], clean.lead_data(lead), 500, 750);
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

    signal_synth::ecg_scenario_document clean_document;
    clean_document.schema_version = 2;
    clean_document.scenario_id = "lead_fault_clean";
    clean_document.name = "Lead Fault Clean";
    signal_synth::ecg_render_bundle clean_render;
    ok &= check(signal_synth::render_ecg_document(clean_document, clean_render, export_result), "lead_fault_clean_render");
    signal_synth::ppg_record no_ppg;
    signal_synth::signal_quality_config reversal_config;
    reversal_config.artifacts.push_back(make_ecg_fault(signal_synth::signal_quality_ecg_lead_reversal, signal_synth::clinical_lead_i));
    signal_synth::signal_quality_waveforms reversal_output;
    ok &= check(signal_synth::apply_signal_quality_artifacts(reversal_config, clean_render.record, no_ppg, reversal_output) && std::fabs(reversal_output.ecg_leads[signal_synth::clinical_lead_i][550] + clean_render.record.lead_data(signal_synth::clinical_lead_i)[550]) < 1e-12, "lead_reversal_exact");
    signal_synth::signal_quality_config swap_config;
    swap_config.artifacts.push_back(make_ecg_fault(signal_synth::signal_quality_ecg_lead_swap, signal_synth::clinical_lead_i, signal_synth::clinical_lead_ii));
    signal_synth::signal_quality_waveforms swap_output;
    ok &= check(signal_synth::apply_signal_quality_artifacts(swap_config, clean_render.record, no_ppg, swap_output) && swap_output.ecg_leads[signal_synth::clinical_lead_i][550] == clean_render.record.lead_data(signal_synth::clinical_lead_ii)[550] && swap_output.ecg_leads[signal_synth::clinical_lead_ii][550] == clean_render.record.lead_data(signal_synth::clinical_lead_i)[550], "lead_swap_exact");
    signal_synth::signal_quality_config invalid_swap;
    invalid_swap.artifacts.push_back(make_ecg_fault(signal_synth::signal_quality_ecg_lead_swap, signal_synth::clinical_lead_i));
    ok &= check(!signal_synth::validate_signal_quality_config(invalid_swap, 10.0, clean_render.record.sampling_rate_hz(), false), "lead_swap_requires_two_leads");
    ok &= check(changed_by_fault(signal_synth::signal_quality_ecg_electrode_misplacement, clean_render.record, signal_synth::clinical_lead_ii), "electrode_misplacement_modifies_waveform");
    ok &= check(changed_by_fault(signal_synth::signal_quality_ecg_gain_mismatch, clean_render.record, signal_synth::clinical_lead_ii), "gain_mismatch_modifies_waveform");
    ok &= check(changed_by_fault(signal_synth::signal_quality_ecg_offset_drift, clean_render.record, signal_synth::clinical_lead_ii), "offset_drift_modifies_waveform");
    ok &= check(changed_by_fault(signal_synth::signal_quality_ecg_clock_drift, clean_render.record, signal_synth::clinical_lead_ii), "clock_drift_modifies_waveform");
    ok &= check(changed_by_fault(signal_synth::signal_quality_ecg_dropped_samples, clean_render.record, signal_synth::clinical_lead_ii), "dropped_samples_modify_waveform");
    ok &= check(changed_by_fault(signal_synth::signal_quality_ecg_quantization, clean_render.record, signal_synth::clinical_lead_ii), "quantization_modifies_waveform");
    ok &= check(changed_by_fault(signal_synth::signal_quality_ecg_adc_clipping, clean_render.record, signal_synth::clinical_lead_ii), "adc_clipping_modifies_waveform");

    signal_synth::ecg_scenario_document lead_fault_document = clean_document;
    lead_fault_document.scenario_id = "lead_fault_json";
    lead_fault_document.signal_quality.artifacts.push_back(make_ecg_fault(signal_synth::signal_quality_ecg_lead_swap, signal_synth::clinical_lead_i, signal_synth::clinical_lead_ii));
    lead_fault_document.signal_quality.artifacts.push_back(make_ecg_fault(signal_synth::signal_quality_ecg_quantization, signal_synth::clinical_lead_v2));
    signal_synth::ecg_scenario_json_result lead_fault_json;
    signal_synth::ecg_scenario_document lead_fault_roundtrip;
    signal_synth::ecg_scenario_json_result lead_fault_roundtrip_result;
    ok &= check(signal_synth::write_ecg_scenario_json(lead_fault_document, lead_fault_json) && lead_fault_json.canonical_json.find("\"ecg_lead_swap\"") != std::string::npos && lead_fault_json.canonical_json.find("\"ecg_quantization\"") != std::string::npos && signal_synth::parse_ecg_scenario_json(lead_fault_json.canonical_json, lead_fault_roundtrip, lead_fault_roundtrip_result) && lead_fault_roundtrip.signal_quality.artifacts.size() == 2, "lead_fault_json_roundtrip");

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

    std::ifstream lead_fault_example("../examples/scenarios/packs/sq_lead_faults.json");
    if (!lead_fault_example.good())
        lead_fault_example.open("../../examples/scenarios/packs/sq_lead_faults.json");
    const bool lead_fault_opened = lead_fault_example.good();
    std::stringstream lead_fault_stream;
    lead_fault_stream << lead_fault_example.rdbuf();
    signal_synth::ecg_scenario_document lead_fault_example_document;
    signal_synth::ecg_scenario_json_result lead_fault_example_json;
    signal_synth::ecg_render_bundle lead_fault_example_render;
    ok &= check(lead_fault_opened && signal_synth::parse_ecg_scenario_json(lead_fault_stream.str(), lead_fault_example_document, lead_fault_example_json) && signal_synth::render_ecg_document(lead_fault_example_document, lead_fault_example_render, export_result) && lead_fault_example_render.metrics.artifact_count == 9, "lead_fault_example_scenario_renders");

    return ok ? 0 : 1;
}
