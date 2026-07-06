#pragma once

#include "ecg_pack.h"
#include "ecg_scenario_json.h"

#include <string>
#include <vector>

namespace signal_synth
{
    enum scenario_target_support
    {
        scenario_target_local_scoring = 0,
        scenario_target_reference_only = 1,
        scenario_target_unsupported = 2
    };

    struct scenario_pack_analysis_message
    {
        scenario_pack_analysis_message();

        bool error;
        std::string code;
        std::string path;
        std::string message;
    };

    struct scenario_pack_case_analysis
    {
        scenario_pack_case_analysis();

        std::string case_id;
        std::string scenario_id;
        double duration_seconds;
        unsigned int sampling_rate_hz;
        unsigned int sample_count;
        unsigned int channel_count;
        unsigned long long estimated_waveform_csv_bytes;
        unsigned long long estimated_binary_signal_bytes;
        unsigned long long estimated_package_bytes;
        unsigned long long estimated_peak_memory_bytes;
        std::vector<std::string> targets;
    };

    struct scenario_pack_target_analysis
    {
        scenario_pack_target_analysis();

        std::string target;
        scenario_target_support support;
        unsigned int case_count;
    };

    struct scenario_pack_analysis
    {
        scenario_pack_analysis();

        bool success;
        std::string pack_id;
        std::string pack_version;
        unsigned int case_count;
        double total_duration_seconds;
        unsigned long long total_sample_count;
        unsigned long long estimated_package_bytes;
        unsigned long long estimated_peak_memory_bytes;
        std::vector<scenario_pack_case_analysis> cases;
        std::vector<scenario_pack_target_analysis> targets;
        std::vector<scenario_pack_analysis_message> messages;
    };

    const char* scenario_authoring_metadata_version();
    const char* scenario_template_catalog_version();
    const char* scenario_target_support_name(scenario_target_support support);
    scenario_target_support scenario_target_support_for_name(const std::string& target);
    std::string scenario_authoring_metadata_json();
    std::string scenario_template_catalog_json();
    bool analyze_scenario_pack(const ecg_pack_manifest& manifest, const std::vector<ecg_scenario_document>& scenarios, scenario_pack_analysis& analysis);
    std::string scenario_pack_analysis_json(const scenario_pack_analysis& analysis);
}
