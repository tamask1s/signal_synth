#include "challenge_package.h"
#include "ecg_export.h"
#include "ecg_render.h"

#include <cmath>
#include <iostream>
#include <string>

namespace
{
    bool check(bool condition, const char* name)
    {
        if (!condition) std::cerr << "FAILED: " << name << '\n';
        return condition;
    }

    std::string fixture()
    {
        return "noise\n0\n0.4\n-0.2\n0.8\n-0.7\n0.1\n0.9\n-0.5\n0.3\n-0.8\n0.6\n-0.1\n0.7\n-0.9\n0.2\n0.5\n-0.4\n1\n-0.6\n0.15\n0.45\n";
    }

    signal_synth::ecg_scenario_document scenario(signal_synth::external_noise_redistribution redistribution)
    {
        signal_synth::ecg_scenario_document document;
        document.schema_version = 8;
        document.scenario_id = "external_noise_test";
        document.name = "External noise test";
        document.duration_seconds = 4.0;
        document.ecg.set_sampling_rate_hz(100u);
        document.ecg.set_seed(80001u);
        signal_synth::external_noise_asset_manifest asset;
        asset.id = "fixture";
        asset.source_uri = "project://synsigra/test-fixture";
        asset.license = "Synsigra project-owned test fixture";
        asset.content_sha256 = signal_synth::challenge_package_content_sha256(fixture());
        asset.sample_rate_hz = 10u;
        asset.channels.push_back("noise");
        asset.redistribution = redistribution;
        document.external_noise.assets.push_back(asset);
        signal_synth::external_noise_interval_config interval;
        interval.asset_id = "fixture";
        interval.asset_channel = "noise";
        interval.start_seconds = 1.0;
        interval.duration_seconds = 1.5;
        interval.asset_offset_seconds = 0.0;
        interval.target_snr_db = 6.0;
        interval.taper_seconds = 0.1;
        interval.ecg_leads[signal_synth::clinical_lead_ii] = true;
        document.external_noise.intervals.push_back(interval);
        return document;
    }
}

int main()
{
    bool ok = true;
    signal_synth::ecg_scenario_document document = scenario(signal_synth::external_noise_rendered_output);
    signal_synth::ecg_scenario_json_result json;
    signal_synth::ecg_scenario_document parsed;
    signal_synth::ecg_scenario_json_result parsed_json;
    ok &= check(signal_synth::write_ecg_scenario_json(document, json) && json.canonical_json.find("\"external_noise\"") != std::string::npos && signal_synth::parse_ecg_scenario_json(json.canonical_json, parsed, parsed_json), "schema_v8_roundtrip");

    signal_synth::ecg_render_bundle missing;
    signal_synth::ecg_document_render_result missing_result;
    ok &= check(!signal_synth::render_ecg_document(parsed, missing, missing_result) && !missing_result.messages.empty() && missing_result.messages[0].find("missing external noise asset") != std::string::npos, "missing_asset_fails_clearly");

    signal_synth::external_noise_asset_input input;
    input.id = "fixture";
    input.csv_content = fixture();
    std::vector<signal_synth::external_noise_asset_input> assets(1u, input);
    signal_synth::ecg_render_bundle render;
    signal_synth::ecg_document_render_result render_result;
    ok &= check(signal_synth::render_ecg_document(parsed, assets, render, render_result), "render");
    ok &= check(render.external_noise.release_allowed && render.external_noise.intervals.size() == 1u && render.external_noise.intervals[0].channels.size() == 1u, "truth_shape");
    if (!render.external_noise.intervals.empty() && !render.external_noise.intervals[0].channels.empty())
        ok &= check(std::fabs(render.external_noise.intervals[0].channels[0].achieved_snr_db - 6.0) < 1e-10 && render.external_noise.intervals[0].channels[0].clipping_count == 0u, "calibrated_snr");
    ok &= check(render.external_noise_clean_ecg_leads.size() == signal_synth::clinical_lead_count && render.external_noise_clean_ecg_leads[signal_synth::clinical_lead_ii][150] != render.signal_quality.ecg_leads[signal_synth::clinical_lead_ii][150], "clean_reference_preserved");
    ok &= check(render.signal_quality.artifacts.size() == 1u && render.signal_quality.artifacts[0].type == signal_synth::signal_quality_ecg_external_noise, "signal_quality_interval_truth");

    signal_synth::ecg_render_bundle repeated;
    ok &= check(signal_synth::render_ecg_document(parsed, assets, repeated, render_result) && repeated.signal_quality.ecg_leads == render.signal_quality.ecg_leads, "deterministic_render");
    signal_synth::ecg_export_bundle bundle;
    signal_synth::ecg_export_result export_result;
    ok &= check(signal_synth::build_ecg_export_bundle(render, bundle, export_result) && bundle.find("external_noise_truth.json") && bundle.find("external_noise_clean_ecg.csv") && bundle.find("external_noise_truth.json")->content.find("\"achieved_snr_db\":6") != std::string::npos, "truth_export");

    signal_synth::ecg_scenario_document local_document = scenario(signal_synth::external_noise_local_only);
    signal_synth::ecg_render_bundle local_render;
    ok &= check(signal_synth::render_ecg_document(local_document, assets, local_render, render_result) && !local_render.external_noise.release_allowed, "local_only_release_gate");
    signal_synth::ecg_scenario_document clipped_document = scenario(signal_synth::external_noise_rendered_output);
    clipped_document.external_noise.intervals[0].clip_limit_mv = 0.05;
    signal_synth::ecg_render_bundle clipped_render;
    ok &= check(signal_synth::render_ecg_document(clipped_document, assets, clipped_render, render_result) && clipped_render.external_noise.intervals[0].channels[0].clipping_count > 0u, "clipping_accounted");
    signal_synth::ecg_scenario_document overlap_document = scenario(signal_synth::external_noise_rendered_output);
    overlap_document.external_noise.intervals.push_back(overlap_document.external_noise.intervals[0]);
    ok &= check(!signal_synth::write_ecg_scenario_json(overlap_document, json), "overlap_rejected");
    assets[0].csv_content += "0\n";
    ok &= check(!signal_synth::render_ecg_document(parsed, assets, repeated, render_result) && !render_result.messages.empty() && render_result.messages[0].find("checksum mismatch") != std::string::npos, "checksum_mismatch_rejected");

    signal_synth::external_noise_config invalid_config = document.external_noise;
    invalid_config.intervals[0].asset_channel = "unknown";
    std::vector<std::vector<double> > direct_leads(signal_synth::clinical_lead_count, std::vector<double>(400u, 0.0));
    signal_synth::external_noise_result direct_truth;
    std::vector<std::string> direct_messages;
    ok &= check(!signal_synth::apply_external_noise(invalid_config, std::vector<signal_synth::external_noise_asset_input>(1u, input), 100u, direct_leads, direct_truth, direct_messages) && !direct_messages.empty(), "direct_api_validates_config");

    signal_synth::external_noise_config exact_config = document.external_noise;
    exact_config.intervals[0].start_seconds = 0.0;
    exact_config.intervals[0].duration_seconds = 2.1;
    exact_config.intervals[0].taper_seconds = 0.0;
    std::vector<std::vector<double> > exact_leads(signal_synth::clinical_lead_count, std::vector<double>(21u, 0.0));
    for (std::size_t sample = 0; sample < 21u; ++sample) exact_leads[signal_synth::clinical_lead_ii][sample] = std::sin(0.4 * sample);
    ok &= check(signal_synth::apply_external_noise(exact_config, std::vector<signal_synth::external_noise_asset_input>(1u, input), 10u, exact_leads, direct_truth, direct_messages), "exact_last_asset_sample_supported");
    return ok ? 0 : 1;
}
