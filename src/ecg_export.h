#pragma once

#include "ecg_render.h"

#include <string>
#include <vector>

namespace signal_synth
{
    struct ecg_text_artifact
    {
        std::string name;
        std::string media_type;
        std::string content;
    };

    struct ecg_export_bundle
    {
        std::vector<ecg_text_artifact> artifacts;

        const ecg_text_artifact* find(const std::string& name) const;
    };

    struct ecg_export_result
    {
        ecg_export_result();

        bool success;
        std::vector<std::string> messages;
    };

    const char* signal_synth_generator_version();
    const char* signal_synth_generator_git_commit();
    const char* signal_synth_build_identity();
    const char* signal_synth_package_contract_version();
    const char* signal_synth_scoring_manifest_contract_version();
    const char* signal_synth_verification_protocol_contract_version();
    const char* signal_synth_verifier_version();
    const char* signal_synth_engineering_claim_boundary_text();
    bool build_ecg_export_bundle(const ecg_render_bundle& render, ecg_export_bundle& output, ecg_export_result& result);
}
