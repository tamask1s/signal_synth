#include "../src/challenge_package.h"

#include <iostream>
#include <string>

namespace
{
    bool check(bool condition, const char* name)
    {
        if (!condition)
            std::cerr << "FAIL: " << name << '\n';
        return condition;
    }

    const char* fake_sha()
    {
        return "sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    }

    signal_synth::challenge_package_manifest example_manifest()
    {
        signal_synth::challenge_package_manifest manifest;
        manifest.package_id = "rpeak_challenge_demo";
        manifest.name = "R-peak Challenge Demo";
        manifest.version = "1.0";
        manifest.description = "Deterministic offline challenge package.";
        manifest.package_type = signal_synth::challenge_package_single_scenario;
        manifest.ground_truth_included = true;
        manifest.waveform_formats.push_back("csv");
        manifest.generator_version = "0.1.0-dev";
        manifest.usage_restrictions = "engineering algorithm QA only";
        manifest.not_for = "diagnosis, patient monitoring, clinical validation certificate, or standalone conformity assessment";

        signal_synth::challenge_package_file scenario;
        scenario.path = "cases/clean/scenario.json";
        scenario.role = signal_synth::challenge_file_scenario_json;
        scenario.media_type = "application/json";
        scenario.sha256 = fake_sha();
        scenario.size_bytes = 1200;
        scenario.required = true;
        manifest.files.push_back(scenario);

        signal_synth::challenge_package_file waveform;
        waveform.path = "cases/clean/waveform.csv";
        waveform.role = signal_synth::challenge_file_waveform_csv;
        waveform.media_type = "text/csv";
        waveform.sha256 = fake_sha();
        waveform.size_bytes = 9000;
        waveform.required = true;
        manifest.files.push_back(waveform);

        signal_synth::challenge_package_file annotations;
        annotations.path = "cases/clean/annotations.json";
        annotations.role = signal_synth::challenge_file_annotations_json;
        annotations.media_type = "application/json";
        annotations.sha256 = fake_sha();
        annotations.size_bytes = 2400;
        annotations.required = true;
        manifest.files.push_back(annotations);

        signal_synth::challenge_package_case item;
        item.id = "clean";
        item.scenario_id = "ecg_clean_001";
        item.scenario_path = "cases/clean/scenario.json";
        item.document_fingerprint = fake_sha();
        item.render_identity = std::string(fake_sha()) + ":ecg-run-1234";
        item.files.push_back("cases/clean/scenario.json");
        item.files.push_back("cases/clean/waveform.csv");
        item.files.push_back("cases/clean/annotations.json");
        manifest.cases.push_back(item);
        return manifest;
    }
}

int main()
{
    bool ok = true;
    signal_synth::challenge_package_manifest manifest = example_manifest();
    signal_synth::challenge_package_json_result result;
    ok &= check(signal_synth::write_challenge_package_json(manifest, result) && result.success, "write_valid_manifest");
    ok &= check(result.canonical_json.find("\"package_id\":\"rpeak_challenge_demo\"") != std::string::npos && result.package_fingerprint.find("sha256:") == 0, "canonical_and_fingerprint");
    const std::string canonical_json = result.canonical_json;

    signal_synth::challenge_package_manifest parsed;
    signal_synth::challenge_package_json_result parsed_result;
    ok &= check(signal_synth::parse_challenge_package_json(result.canonical_json, parsed, parsed_result) && parsed_result.success, "parse_canonical_manifest");
    ok &= check(parsed.package_id == manifest.package_id && parsed.files.size() == 3 && parsed.cases.size() == 1, "parsed_shape");
    ok &= check(parsed_result.canonical_json == result.canonical_json && parsed_result.package_fingerprint == result.package_fingerprint, "roundtrip_identity");
    ok &= check(signal_synth::challenge_file_role_from_name("waveform_csv", parsed.files[0].role) && parsed.files[0].role == signal_synth::challenge_file_waveform_csv, "role_parse");
    ok &= check(std::string(signal_synth::challenge_file_role_name(signal_synth::challenge_file_annotations_json)) == "annotations_json", "role_name");

    signal_synth::challenge_package_manifest no_ground_truth = manifest;
    no_ground_truth.ground_truth_included = false;
    ok &= check(!signal_synth::write_challenge_package_json(no_ground_truth, result) && !result.messages.empty(), "reject_without_ground_truth_flag");

    signal_synth::challenge_package_manifest no_truth_file = manifest;
    no_truth_file.files.pop_back();
    no_truth_file.cases[0].files.pop_back();
    ok &= check(!signal_synth::write_challenge_package_json(no_truth_file, result) && !result.messages.empty(), "reject_without_truth_file_role");

    signal_synth::challenge_package_manifest duplicate_path = manifest;
    duplicate_path.files[1].path = duplicate_path.files[0].path;
    ok &= check(!signal_synth::write_challenge_package_json(duplicate_path, result) && !result.messages.empty(), "reject_duplicate_file_path");

    signal_synth::challenge_package_manifest missing_reference = manifest;
    missing_reference.cases[0].files.push_back("missing/file.json");
    ok &= check(!signal_synth::write_challenge_package_json(missing_reference, result) && !result.messages.empty(), "reject_missing_case_file_reference");

    signal_synth::challenge_package_manifest bad_sha = manifest;
    bad_sha.files[0].sha256 = "sha256:BAD";
    ok &= check(!signal_synth::write_challenge_package_json(bad_sha, result) && !result.messages.empty(), "reject_bad_sha");

    const std::string unknown_field = canonical_json.substr(0, canonical_json.size() - 1) + ",\"unexpected\":1}";
    ok &= check(!signal_synth::parse_challenge_package_json(unknown_field, parsed, parsed_result) && !parsed_result.messages.empty(), "reject_unknown_field");

    const std::string duplicate_key = "{\"schema_version\":1,\"schema_version\":1}";
    ok &= check(!signal_synth::parse_challenge_package_json(duplicate_key, parsed, parsed_result) && !parsed_result.messages.empty(), "reject_duplicate_key");

    const std::string invalid_number = "{\"schema_version\":01}";
    ok &= check(!signal_synth::parse_challenge_package_json(invalid_number, parsed, parsed_result) && !parsed_result.messages.empty(), "reject_invalid_json_number");

    return ok ? 0 : 1;
}
