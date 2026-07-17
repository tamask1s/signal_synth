#include "ecg_render.h"

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
            std::cerr << "FAILED: " << name << '\n';
        return condition;
    }

    std::string read_text(const std::string& path)
    {
        std::ifstream input(path.c_str(), std::ios::binary);
        if (!input)
            return std::string();
        std::ostringstream content;
        content << input.rdbuf();
        return content.str();
    }

    bool extract_scenario_json(const std::string& script, std::size_t& position, std::string& json, int& annotation_mode)
    {
        const std::string function = "GenerateECGScenarioJSON(";
        const std::size_t call = script.find(function, position);
        if (call == std::string::npos)
            return false;
        const std::size_t first = script.find('{', call + function.size());
        if (first == std::string::npos)
            return false;
        unsigned int depth = 0;
        bool in_string = false;
        bool escaped = false;
        for (std::size_t i = first; i < script.size(); ++i)
        {
            const char c = script[i];
            if (in_string)
            {
                if (escaped)
                    escaped = false;
                else if (c == '\\')
                    escaped = true;
                else if (c == '"')
                    in_string = false;
                continue;
            }
            if (c == '"')
                in_string = true;
            else if (c == '{')
                ++depth;
            else if (c == '}' && --depth == 0)
            {
                json = script.substr(first, i - first + 1);
                const std::size_t comma = script.find(',', i + 1);
                const std::size_t close = comma == std::string::npos ? std::string::npos : script.find(')', comma + 1);
                if (close == std::string::npos)
                    return false;
                std::istringstream mode(script.substr(comma + 1, close - comma - 1));
                mode >> annotation_mode;
                position = close + 1;
                return annotation_mode >= 1 && annotation_mode <= 3;
            }
        }
        return false;
    }

    bool verify_render(const signal_synth::ecg_scenario_document& document)
    {
        signal_synth::ecg_render_bundle render;
        signal_synth::ecg_document_render_result result;
        if (!signal_synth::render_ecg_document(document, render, result) || !result.success)
            return false;
        if (document.scenario_id.find("morph_population_") == 0)
            return render.parameter_draws.size() == 5u && render.record.beat_count() > 20u;
        if (document.scenario_id.find("dynamic_repolarization_") == 0)
            return render.record.dynamic_annotation_count() == render.record.beat_count() * 6u && render.record.episode_count() >= 1u;
        if (document.scenario_id == "ppg_multichannel_optical_v4")
            return render.ppg.channel_count() == 3u && std::string(render.ppg.channel_name(0)) == "ppg_green" && std::string(render.ppg.channel_name(1)) == "ppg_red" && std::string(render.ppg.channel_name(2)) == "ppg_infrared" && render.ppg.channel_annotation_count(0) > 0u && render.ppg.channel_annotation_count(1) > 0u && render.ppg.channel_annotation_count(2) > 0u;
        if (document.scenario_id.find("delineation_") == 0)
        {
            bool has_p = false;
            bool has_qrs = false;
            bool has_j = false;
            bool has_t = false;
            for (unsigned int i = 0; i < render.record.fiducial_count(); ++i)
            {
                const signal_synth::clinical_fiducial_annotation& item = render.record.fiducials()[i];
                if (!item.present || item.source != signal_synth::clinical_fiducial_construction)
                    continue;
                has_p = has_p || item.kind == signal_synth::clinical_p_peak;
                has_qrs = has_qrs || item.kind == signal_synth::clinical_qrs_onset || item.kind == signal_synth::clinical_qrs_offset;
                has_j = has_j || item.kind == signal_synth::clinical_j_point;
                has_t = has_t || item.kind == signal_synth::clinical_t_peak;
            }
            const bool expected_p = document.scenario_id != "delineation_afib";
            return render.record.beat_count() > 5u && has_qrs && has_j && has_t && has_p == expected_p;
        }
        if (document.scenario_id == "wearable_databrowser_v5")
            return render.wearable.streams.size() == 3u
                && render.wearable.stream(signal_synth::wearable_stream_ecg)->config.sample_rate_hz == 250u
                && render.wearable.stream(signal_synth::wearable_stream_ppg)->config.sample_rate_hz == 100u
                && render.wearable.stream(signal_synth::wearable_stream_accelerometer)->config.sample_rate_hz == 50u
                && !render.wearable.alignments.empty();
        return false;
    }

    bool verify_wearable_script(const char* path)
    {
        const std::string script = read_text(path);
        const std::string function = "GenerateWearableScenarioJSON(";
        const std::size_t call = script.find(function);
        const std::size_t first = call == std::string::npos ? std::string::npos : script.find('{', call + function.size());
        if (first == std::string::npos || script.find("SaveVarToFile") == std::string::npos || script.find("DisplayData") == std::string::npos || script.find("SaveVarToFile") > script.find("DisplayData") || script.find(",, A2,") == std::string::npos || script.find(",, C,") != std::string::npos)
            return false;
        unsigned int depth = 0;
        bool in_string = false;
        bool escaped = false;
        for (std::size_t i = first; i < script.size(); ++i)
        {
            const char c = script[i];
            if (in_string)
            {
                if (escaped) escaped = false;
                else if (c == '\\') escaped = true;
                else if (c == '"') in_string = false;
                continue;
            }
            if (c == '"') in_string = true;
            else if (c == '{') ++depth;
            else if (c == '}' && --depth == 0)
            {
                signal_synth::ecg_scenario_document document;
                signal_synth::ecg_scenario_json_result result;
                return signal_synth::parse_ecg_scenario_json(script.substr(first, i - first + 1), document, result) && verify_render(document);
            }
        }
        return false;
    }

    bool verify_script(const char* path, unsigned int expected_scenarios)
    {
        const std::string script = read_text(path);
        if (script.empty() || script.find("SaveVarToFile") == std::string::npos || script.find("DisplayData") == std::string::npos || script.find("SaveVarToFile") > script.find("DisplayData") || script.find(",, A2,") == std::string::npos || script.find(",, C,") != std::string::npos)
        {
            std::cerr << "Invalid DataBrowser script structure: " << path << '\n';
            return false;
        }
        std::size_t position = 0;
        unsigned int count = 0;
        std::string json;
        int annotation_mode = 0;
        while (extract_scenario_json(script, position, json, annotation_mode))
        {
            signal_synth::ecg_scenario_document document;
            signal_synth::ecg_scenario_json_result result;
            if (!signal_synth::parse_ecg_scenario_json(json, document, result))
            {
                std::cerr << "Invalid scenario JSON in " << path;
                if (!result.messages.empty())
                    std::cerr << ": " << result.messages[0].path << ": " << result.messages[0].message;
                std::cerr << '\n';
                return false;
            }
            if (!verify_render(document))
            {
                std::cerr << "Unexpected render in " << path << ": " << document.scenario_id << '\n';
                return false;
            }
            ++count;
        }
        if (count != expected_scenarios)
            std::cerr << "Unexpected scenario count in " << path << ": " << count << '\n';
        return count == expected_scenarios;
    }
}

int main()
{
    bool ok = true;
    ok &= check(verify_script("examples/databrowser/077_ECG_Morphology_Population.txt", 3), "morphology_population_script");
    ok &= check(verify_script("examples/databrowser/078_ECG_Dynamic_Repolarization.txt", 2), "dynamic_repolarization_script");
    ok &= check(verify_script("examples/databrowser/079_PPG_Multichannel_Optical.txt", 1), "ppg_multichannel_script");
    ok &= check(verify_script("examples/databrowser/080_ECG_Delineation_GroundTruth.txt", 4), "delineation_ground_truth_script");
    ok &= check(verify_wearable_script("examples/databrowser/081_Wearable_Multirate_Timebase.txt"), "wearable_timebase_script");

    const std::string adapter = read_text("integrations/databrowser/SignalProc_RSPT.cpp");
    ok &= check(adapter.find("#include \"ecg_render.h\"") != std::string::npos && adapter.find("#include \"wearable_timebase.h\"") != std::string::npos && adapter.find("#include \"ecg_export.h\"") == std::string::npos, "adapter_uses_render_layer");
    ok &= check(adapter.find("pacing_event_count()") != std::string::npos && adapter.find("dynamic_annotation_count()") != std::string::npos && adapter.find("channel_name(ppg_channel)") != std::string::npos, "adapter_enumerates_new_truth");
    ok &= check(adapter.find("ecg_document_render_result render_result") != std::string::npos, "adapter_render_result_contract");
    ok &= check(adapter.find("GenerateWearableScenarioJSON") != std::string::npos && adapter.find("timestamp error") != std::string::npos && adapter.find("packet availability") != std::string::npos, "adapter_wearable_api");

    const std::string project = read_text("integrations/databrowser/SignalProc_RSPT.cbp");
    ok &= check(project.find("ecg_render.cpp") != std::string::npos && project.find("hrv_metrics.cpp") != std::string::npos && project.find("scenario_stress.cpp") != std::string::npos && project.find("wearable_timebase.cpp") != std::string::npos, "codeblocks_generation_dependencies");
    ok &= check(project.find("ecg_export.cpp") == std::string::npos && project.find("challenge_package.cpp") == std::string::npos && project.find("ecg_compare.cpp") == std::string::npos && project.find("synsigra_api.cpp") == std::string::npos && project.find("ecg_pack.cpp") == std::string::npos, "codeblocks_excludes_distribution_stack");
    return ok ? 0 : 1;
}
