#include "scenario_authoring.h"

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

    signal_synth::ecg_pack_manifest one_case_pack(const std::string& target)
    {
        signal_synth::ecg_pack_manifest pack;
        pack.pack_id = "authoring_test";
        pack.name = "Authoring test";
        pack.version = "1";
        pack.description = "Authoring unit test.";
        pack.targets.push_back(target);
        signal_synth::ecg_pack_scenario item;
        item.id = "case_1";
        item.path = "case_1.json";
        item.targets.push_back(target);
        pack.scenarios.push_back(item);
        return pack;
    }
}

int main()
{
    bool ok = true;
    const std::string metadata = signal_synth::scenario_authoring_metadata_json();
    const std::string templates = signal_synth::scenario_template_catalog_json();
    ok &= check(std::string(signal_synth::scenario_authoring_metadata_version()) == "synsigra_authoring_v2"
        && metadata.find("\"condition_count\"") == std::string::npos
        && metadata.find("\"code\":\"NORM\"") != std::string::npos
        && metadata.find("\"path\":\"$.hrv.target_sdnn_seconds\"") != std::string::npos
        && metadata.find("\"path\":\"$.randomization.envelopes\"") != std::string::npos
        && metadata.find("\"path\":\"$.output.compact\"") != std::string::npos
        && metadata.find("\"name\":\"ppg_systolic_peak\"") != std::string::npos, "metadata_contract");
    ok &= check(std::string(signal_synth::scenario_template_catalog_version()) == "synsigra_templates_v2"
        && templates.find("\"template_id\":\"ecg_rpeak_clean\"") != std::string::npos
        && templates.find("\"template_id\":\"ecg_hrv_benchmark\"") != std::string::npos
        && templates.find("\"template_id\":\"wearable_ecg_ppg_stress\"") != std::string::npos
        && templates.find("\"difficulty_values\"") != std::string::npos, "template_contract");
    ok &= check(signal_synth::scenario_target_support_for_name("r_peak") == signal_synth::scenario_target_local_scoring
        && signal_synth::scenario_target_support_for_name("signal_quality") == signal_synth::scenario_target_reference_only
        && signal_synth::scenario_target_support_for_name("unknown") == signal_synth::scenario_target_unsupported, "target_support_contract");

    signal_synth::ecg_scenario_document clean;
    clean.schema_version = 2;
    clean.scenario_id = "case_1";
    clean.duration_seconds = 10.0;
    clean.ecg.set_heart_rate_bpm(70.0);
    clean.ecg.add_condition(signal_synth::ecg_condition_norm);
    std::vector<signal_synth::ecg_scenario_document> scenarios(1, clean);

    signal_synth::scenario_pack_analysis analysis;
    ok &= check(signal_synth::analyze_scenario_pack(one_case_pack("r_peak"), scenarios, analysis)
        && analysis.success && analysis.case_count == 1 && analysis.total_sample_count == 5000
        && analysis.estimated_package_bytes > analysis.cases[0].estimated_waveform_csv_bytes
        && analysis.estimated_peak_memory_bytes == analysis.cases[0].estimated_peak_memory_bytes
        && analysis.targets[0].support == signal_synth::scenario_target_local_scoring, "valid_pack_analysis");

    clean.schema_version = 3;
    clean.output.compact = true;
    clean.output.retain_source_channels = false;
    clean.output.include_waveform_csv = false;
    clean.output.include_edf_bdf = false;
    scenarios[0] = clean;
    signal_synth::scenario_pack_analysis compact_analysis;
    ok &= check(signal_synth::analyze_scenario_pack(one_case_pack("r_peak"), scenarios, compact_analysis)
        && compact_analysis.cases[0].estimated_waveform_csv_bytes == 0
        && compact_analysis.cases[0].estimated_binary_signal_bytes < analysis.cases[0].estimated_binary_signal_bytes
        && compact_analysis.cases[0].estimated_peak_memory_bytes < analysis.cases[0].estimated_peak_memory_bytes, "compact_resource_estimates");

    ok &= check(!signal_synth::analyze_scenario_pack(one_case_pack("ppg_systolic_peak"), scenarios, analysis)
        && !analysis.success && !analysis.messages.empty() && analysis.messages[0].code == "TARGET_INCOMPATIBLE", "ppg_incompatibility");
    ok &= check(!signal_synth::analyze_scenario_pack(one_case_pack("unknown"), scenarios, analysis)
        && !analysis.success && analysis.messages[0].code == "UNSUPPORTED_TARGET", "unknown_target_rejected");

    const std::string analysis_json = signal_synth::scenario_pack_analysis_json(analysis);
    ok &= check(analysis_json.find("\"success\":false") != std::string::npos
        && analysis_json.find("\"estimated_package_bytes\"") != std::string::npos
        && analysis_json.find("\"estimated_peak_memory_bytes\"") != std::string::npos
        && analysis_json.find("\"UNSUPPORTED_TARGET\"") != std::string::npos, "analysis_json");

    if (!ok)
        return 1;
    std::cout << "scenario_authoring_test=passed\n";
    return 0;
}
