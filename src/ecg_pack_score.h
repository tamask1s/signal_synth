#pragma once

#include "ecg_compare.h"

#include <string>
#include <vector>

namespace signal_synth
{
    struct ecg_pack_manifest;

    struct ecg_pack_score_case
    {
        ecg_pack_score_case();

        std::string case_id;
        std::string scenario_id;
        std::string scenario_path;
        std::string document_fingerprint;
        std::string render_identity;
        std::string detection_input_id;
        std::string detection_algorithm_name;
        std::string detection_algorithm_version;
        ecg_compare_result comparison;
    };

    struct ecg_pack_score_target
    {
        ecg_pack_score_target();

        std::string target_name;
        ecg_compare_bin_metrics total;
        ecg_compare_bin_metrics clean;
        ecg_compare_bin_metrics artifact;
        unsigned int case_count;
    };

    struct ecg_pack_score_summary
    {
        ecg_pack_score_summary();

        bool success;
        std::string pack_id;
        std::string pack_name;
        std::string pack_version;
        std::string pack_fingerprint;
        std::string scoring_version;
        std::vector<ecg_pack_score_case> cases;
        std::vector<ecg_pack_score_target> targets;
        std::vector<std::string> messages;
    };

    bool build_ecg_pack_score_summary(const ecg_pack_manifest& manifest, const std::string& pack_fingerprint, const std::vector<ecg_pack_score_case>& cases, ecg_pack_score_summary& summary);
    std::string ecg_pack_score_summary_json(const ecg_pack_score_summary& summary);
    std::string ecg_pack_score_summary_csv(const ecg_pack_score_summary& summary);
    std::string ecg_pack_score_report_html(const ecg_pack_score_summary& summary);
}
