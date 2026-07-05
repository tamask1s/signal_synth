#pragma once

#include "challenge_package.h"

#include <string>
#include <vector>

namespace signal_synth
{
    struct challenge_package_input_file
    {
        challenge_package_input_file();

        std::string path;
        challenge_file_role role;
        std::string media_type;
        std::string content;
        bool required;
    };

    struct challenge_package_case_input
    {
        std::string id;
        std::string scenario_id;
        std::string scenario_path;
        std::string document_fingerprint;
        std::string render_identity;
        std::vector<challenge_package_input_file> files;
    };

    struct challenge_package_build_options
    {
        challenge_package_build_options();

        std::string package_id;
        std::string name;
        std::string version;
        std::string description;
        challenge_package_type package_type;
        std::vector<std::string> waveform_formats;
        std::string generator_version;
        std::string usage_restrictions;
        std::string not_for;
        std::vector<challenge_package_input_file> package_files;
    };

    struct challenge_package_build_result
    {
        challenge_package_build_result();

        bool success;
        challenge_package_manifest manifest;
        challenge_package_json_result manifest_json;
    };

    const char* challenge_package_default_usage_restrictions();
    const char* challenge_package_default_not_for();
    challenge_file_role challenge_file_role_for_export_artifact(const std::string& artifact_name);
    bool build_challenge_package_manifest(const challenge_package_build_options& options, const std::vector<challenge_package_case_input>& cases, challenge_package_build_result& result);
}
