#include "../src/ecg_export.h"
#include "../src/ecg_pack.h"
#include "../src/ecg_scenario_json.h"

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

    std::string read_file(const std::string& path)
    {
        std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    bool close(double left, double right, double tolerance)
    {
        return std::fabs(left - right) <= tolerance;
    }
}

int main()
{
    bool ok = true;
    signal_synth::ecg_pack_manifest pack;
    signal_synth::ecg_pack_json_result pack_result;
    const std::string pack_json = read_file("examples/packs/hrv_robustness_v2.json");
    ok &= check(signal_synth::parse_ecg_pack_json(pack_json, pack, pack_result) && pack.pack_id == "hrv_robustness_v2" && pack.scenarios.size() == 10u, "hrv_pack_manifest");

    double high_sdnn = 0.0;
    double clean_sdnn = 0.0;
    for (std::size_t i = 0; i < pack.scenarios.size(); ++i)
    {
        const signal_synth::ecg_pack_scenario& entry = pack.scenarios[i];
        const std::string scenario_json = read_file(std::string("examples/packs/") + entry.path);
        signal_synth::ecg_scenario_document document;
        signal_synth::ecg_scenario_json_result parse_result;
        signal_synth::ecg_render_bundle first;
        signal_synth::ecg_render_bundle second;
        signal_synth::ecg_document_render_result first_result;
        signal_synth::ecg_document_render_result second_result;
        const bool rendered = signal_synth::parse_ecg_scenario_json(scenario_json, document, parse_result) && signal_synth::render_ecg_document(document, first, first_result) && signal_synth::render_ecg_document(document, second, second_result);
        ok &= check(rendered, "hrv_pack_case_renders");
        if (!rendered)
            continue;

        const signal_synth::hrv_metric_summary& metrics = first.hrv.metrics;
        const signal_synth::hrv_metric_summary& repeated = second.hrv.metrics;
        ok &= check(first.render_identity == second.render_identity && metrics.interval_count == repeated.interval_count && metrics.excluded_interval_count == repeated.excluded_interval_count && metrics.sdnn_seconds == repeated.sdnn_seconds && metrics.vlf_power_seconds2 == repeated.vlf_power_seconds2 && metrics.lf_hf_ratio == repeated.lf_hf_ratio, "hrv_pack_case_deterministic");
        ok &= check(metrics.clipped_interval_count == 0u && metrics.mean_heart_rate_bpm > 58.0 && metrics.mean_heart_rate_bpm < 62.0, "hrv_pack_case_rate_and_clipping");
        ok &= check(first.hrv.metric_definition_version == "synsigra_hrv_metrics_v2", "hrv_pack_metric_definition");

        if (entry.id == "clean_baseline")
        {
            clean_sdnn = metrics.sdnn_seconds;
            ok &= check(metrics.sdnn_seconds == 0.0 && metrics.rmssd_seconds == 0.0 && metrics.vlf_power_seconds2 == 0.0 && metrics.lf_hf_ratio == 0.0, "hrv_clean_expected_metrics");
        }
        else if (entry.id == "vlf_dominant")
        {
            ok &= check(close(metrics.sdnn_seconds, 0.060, 0.006) && metrics.vlf_power_seconds2 > 4.0 * (metrics.lf_power_seconds2 + metrics.hf_power_seconds2), "hrv_vlf_expected_metrics");
            ok &= check(metrics.lf_normalized_units > 0.0 && close(metrics.lf_normalized_units + metrics.hf_normalized_units, 100.0, 1e-9), "hrv_vlf_normalized_power");
        }
        else if (entry.id == "high_variability")
        {
            high_sdnn = metrics.sdnn_seconds;
            ok &= check(close(metrics.sdnn_seconds, 0.080, 0.005) && metrics.lf_hf_ratio > 0.8 && metrics.lf_hf_ratio < 2.0, "hrv_high_expected_metrics");
        }
        else if (entry.id == "lf_dominant")
            ok &= check(close(metrics.sdnn_seconds, 0.060, 0.005) && metrics.lf_hf_ratio > 3.0, "hrv_lf_expected_metrics");
        else if (entry.id == "hf_dominant")
            ok &= check(close(metrics.sdnn_seconds, 0.060, 0.005) && metrics.lf_hf_ratio > 0.0 && metrics.lf_hf_ratio < 0.6, "hrv_hf_expected_metrics");
        else if (entry.id == "respiratory_variation")
            ok &= check(close(metrics.sdnn_seconds, 0.070, 0.006) && metrics.rmssd_seconds > 0.075 && metrics.lf_hf_ratio < 1.0, "hrv_respiratory_expected_metrics");
        else if (entry.id == "ectopic_contamination")
            ok &= check(metrics.ectopic_interval_count >= 45u && metrics.excluded_interval_count == metrics.ectopic_interval_count && close(metrics.sdnn_seconds, 0.040, 0.006), "hrv_ectopic_exclusion_metrics");
        else if (entry.id == "baseline_wander_stress")
            ok &= check(metrics.artifact_overlap_interval_count >= 60u && metrics.excluded_interval_count == metrics.artifact_overlap_interval_count && close(metrics.sdnn_seconds, 0.040, 0.006), "hrv_baseline_exclusion_metrics");
        else if (entry.id == "powerline_emg_stress")
            ok &= check(metrics.artifact_overlap_interval_count >= 55u && metrics.excluded_interval_count == metrics.artifact_overlap_interval_count && close(metrics.sdnn_seconds, 0.040, 0.006), "hrv_noise_exclusion_metrics");
        else if (entry.id == "drift_dropout_stress")
            ok &= check(metrics.artifact_overlap_interval_count >= 45u && metrics.excluded_interval_count == metrics.artifact_overlap_interval_count && close(metrics.sdnn_seconds, 0.040, 0.006), "hrv_dropout_exclusion_metrics");
    }
    ok &= check(clean_sdnn == 0.0 && high_sdnn > 0.070, "hrv_variability_levels_ordered");

    signal_synth::ecg_scenario_document short_document;
    signal_synth::ecg_scenario_json_result short_result;
    ok &= check(!signal_synth::parse_ecg_scenario_json(read_file("examples/scenarios/hrv/hrv_short_window_rejected.json"), short_document, short_result) && !short_result.messages.empty() && short_result.messages[0].code == signal_synth::ecg_json_range && short_result.messages[0].path == "$.hrv", "hrv_short_window_rejected");

    const std::string expectations = read_file("examples/packs/hrv_robustness_v2_expectations.json");
    ok &= check(expectations.find("\"contract\":\"synsigra_verification_protocol_v2\"") != std::string::npos && expectations.find("\"pack_id\":\"hrv_robustness_v2\"") != std::string::npos && expectations.find("\"required_case_targets\"") != std::string::npos && expectations.find("\"acceptance_profile\"") != std::string::npos && expectations.find("hrv_short_window_rejected") != std::string::npos, "hrv_expectations_manifest");
    return ok ? 0 : 1;
}
