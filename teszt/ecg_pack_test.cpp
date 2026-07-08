#include "../src/ecg_export.h"
#include "../src/ecg_compare.h"
#include "../src/ecg_pack.h"
#include "../src/ecg_scenario_json.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

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

    std::string dirname(const std::string& path)
    {
        const std::string::size_type slash = path.find_last_of("/\\");
        return slash == std::string::npos ? "." : path.substr(0, slash);
    }

    std::string join(const std::string& left, const std::string& right)
    {
        if (left.empty() || left == ".")
            return right;
        return left + "/" + right;
    }
}

int main()
{
    bool ok = true;
    signal_synth::ecg_pack_manifest typed;
    typed.pack_id = "typed_pack";
    typed.name = "Typed Pack";
    typed.version = "1";
    typed.description = "Typed pack manifest";
    typed.targets.push_back("r_peak");
    signal_synth::ecg_pack_scenario scenario;
    scenario.id = "clean";
    scenario.path = "../scenarios/ecg_clean.json";
    scenario.targets.push_back("r_peak");
    typed.scenarios.push_back(scenario);

    signal_synth::ecg_pack_json_result typed_result;
    signal_synth::ecg_pack_manifest roundtrip;
    signal_synth::ecg_pack_json_result roundtrip_result;
    ok &= check(signal_synth::write_ecg_pack_json(typed, typed_result) && typed_result.pack_fingerprint.find("sha256:") == 0, "typed_pack_write");
    ok &= check(signal_synth::parse_ecg_pack_json(typed_result.canonical_json, roundtrip, roundtrip_result) && roundtrip.pack_id == typed.pack_id && roundtrip_result.pack_fingerprint == typed_result.pack_fingerprint, "typed_pack_roundtrip");

    signal_synth::ecg_pack_manifest duplicate = typed;
    duplicate.scenarios.push_back(scenario);
    signal_synth::ecg_pack_json_result duplicate_result;
    ok &= check(!signal_synth::write_ecg_pack_json(duplicate, duplicate_result) && !duplicate_result.messages.empty() && duplicate_result.messages[0].code == signal_synth::ecg_pack_json_duplicate_id, "duplicate_scenario_id_rejected");

    signal_synth::ecg_pack_manifest invalid_target = typed;
    invalid_target.targets.push_back("");
    invalid_target.scenarios[0].targets.push_back("r_peak");
    signal_synth::ecg_pack_json_result invalid_target_result;
    ok &= check(!signal_synth::write_ecg_pack_json(invalid_target, invalid_target_result) && !invalid_target_result.messages.empty(), "invalid_or_duplicate_targets_rejected");

    const char* packs[] = {
        "../examples/packs/r_peak_stress_v1.json",
        "../examples/packs/hrv_v1.json",
        "../examples/packs/ppg_alignment_v1.json",
        "../examples/packs/signal_quality_v1.json",
        "../examples/packs/combined_worst_case_v1.json",
        "../examples/packs/ppg_benchmark_v1.json"
    };
    const unsigned int expected_scenario_counts[] = {4, 9, 4, 5, 4, 9};
    unsigned int total_pack_scenarios = 0;
    unsigned int rendered_scenarios = 0;
    unsigned int artifact_scenarios = 0;
    unsigned int ppg_scenarios = 0;
    for (unsigned int pack_index = 0; pack_index < sizeof(packs) / sizeof(packs[0]); ++pack_index)
    {
        std::string pack_path = packs[pack_index];
        if (read_file(pack_path).empty())
            pack_path = std::string("../../") + std::string(packs[pack_index]).substr(3);
        const std::string pack_json = read_file(pack_path);
        signal_synth::ecg_pack_manifest pack;
        signal_synth::ecg_pack_json_result pack_result;
        ok &= check(!pack_json.empty() && signal_synth::parse_ecg_pack_json(pack_json, pack, pack_result) && pack.scenarios.size() == expected_scenario_counts[pack_index], "curated_pack_parses");
        total_pack_scenarios += static_cast<unsigned int>(pack.scenarios.size());
        for (std::size_t scenario_index = 0; scenario_index < pack.scenarios.size(); ++scenario_index)
        {
            const std::string scenario_path = join(dirname(pack_path), pack.scenarios[scenario_index].path);
            signal_synth::ecg_scenario_document document;
            signal_synth::ecg_scenario_json_result scenario_result;
            signal_synth::ecg_render_bundle render;
            signal_synth::ecg_export_result export_result;
            const std::string scenario_json = read_file(scenario_path);
            ok &= check(!scenario_json.empty() && signal_synth::parse_ecg_scenario_json(scenario_json, document, scenario_result) && signal_synth::render_ecg_document(document, render, export_result), "curated_pack_scenario_renders");
            if (render.record.sample_count())
                ++rendered_scenarios;
            if (render.metrics.artifact_count)
                ++artifact_scenarios;
            if (render.metrics.ppg_pulse_count)
                ++ppg_scenarios;
            if (pack.pack_id == "ppg_benchmark_v1")
            {
                const signal_synth::ppg_fiducial_kind kinds[] = {signal_synth::ppg_systolic_peak, signal_synth::ppg_pulse_onset};
                const signal_synth::ecg_compare_target targets[] = {signal_synth::ecg_compare_ppg_systolic_peak, signal_synth::ecg_compare_ppg_pulse_onset};
                for (unsigned int target_index = 0; target_index < 2u; ++target_index)
                {
                    std::vector<signal_synth::ecg_detected_event> detections;
                    for (unsigned int annotation_index = 0; annotation_index < render.ppg.annotation_count(); ++annotation_index)
                    {
                        const signal_synth::ppg_annotation& annotation = render.ppg.annotations()[annotation_index];
                        if (annotation.kind == kinds[target_index] && annotation.source == signal_synth::ppg_fiducial_measurement)
                        {
                            signal_synth::ecg_detected_event event;
                            event.time_seconds = annotation.time_seconds;
                            detections.push_back(event);
                        }
                    }
                    signal_synth::ecg_compare_options options;
                    options.target = targets[target_index];
                    signal_synth::ecg_compare_result comparison;
                    ok &= check(signal_synth::compare_detections_to_render(render, detections, options, comparison)
                        && comparison.total.f1_score == 1.0
                        && comparison.pulse_timing.matched_interval_count == comparison.pulse_timing.ground_truth_interval_count, "ppg_benchmark_case_scores");
                }
            }
        }
    }
    ok &= check(total_pack_scenarios >= 35 && rendered_scenarios == total_pack_scenarios, "curated_pack_has_35_rendered_scenarios");
    ok &= check(artifact_scenarios >= 5 && ppg_scenarios >= 5, "curated_pack_covers_artifacts_and_ppg");

    std::ifstream script("../examples/databrowser/076_ECG_Scenario_Pack_Batch_QA.txt");
    if (!script.good())
    {
        script.clear();
        script.open("../../examples/databrowser/076_ECG_Scenario_Pack_Batch_QA.txt");
    }
    std::stringstream script_stream;
    script_stream << script.rdbuf();
    ok &= check(script.good() && script_stream.str().find("GenerateECGScenarioJSON") != std::string::npos && script_stream.str().find("pack_combined_quality") != std::string::npos, "databrowser_pack_preview_script_present");

    return ok ? 0 : 1;
}
