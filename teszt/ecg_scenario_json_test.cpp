#include "../src/ecg_scenario_json.h"

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

    bool rejects_without_mutation(const std::string& json, signal_synth::ecg_scenario_json_message_code code)
    {
        signal_synth::ecg_scenario_document document;
        document.scenario_id = "preserved";
        signal_synth::ecg_scenario_json_result result;
        return !signal_synth::parse_ecg_scenario_json(json, document, result)
            && document.scenario_id == "preserved"
            && !result.success
            && !result.messages.empty()
            && result.messages[0].code == code;
    }
}

int main()
{
    bool ok = true;
    const std::string input =
        "{"
        "\"ecg\":{\"conditions\":[{\"severity\":1,\"code\":\"NORM\"}],\"fidelity_policy\":\"allow_parameterized\","
        "\"q_wave_territory\":\"unspecified\",\"second_degree_av_pattern\":\"unspecified\","
        "\"ectopic_every_n_beats\":0,\"rr_variability_seconds\":0,\"heart_rate_bpm\":70},"
        "\"seed\":12345,\"sample_rate_hz\":500,\"duration_seconds\":10,\"tags\":[\"z\",\"a\"],"
        "\"author\":\"\",\"description\":\"test\",\"name\":\"Clean ECG\","
        "\"scenario_id\":\"ecg_clean_001\",\"schema_version\":1"
        "}";
    const std::string canonical =
        "{\"schema_version\":1,\"scenario_id\":\"ecg_clean_001\",\"name\":\"Clean ECG\","
        "\"description\":\"test\",\"author\":\"\",\"tags\":[\"a\",\"z\"],\"duration_seconds\":10,"
        "\"sample_rate_hz\":500,\"seed\":12345,\"ecg\":{\"heart_rate_bpm\":70,"
        "\"rr_variability_seconds\":0,\"ectopic_every_n_beats\":0,"
        "\"second_degree_av_pattern\":\"unspecified\",\"q_wave_territory\":\"unspecified\","
        "\"fidelity_policy\":\"allow_parameterized\",\"conditions\":[{\"code\":\"NORM\",\"severity\":1}]}}";

    signal_synth::ecg_scenario_document document;
    signal_synth::ecg_scenario_json_result parsed;
    ok &= check(signal_synth::parse_ecg_scenario_json(input, document, parsed), "parse_valid_document");
    ok &= check(parsed.success && parsed.canonical_json == canonical && document.sample_count() == 5000, "canonicalization_and_sample_count");
    ok &= check(parsed.document_fingerprint == "sha256:24c704e559da1d32b07b0ea951d4a264b7dd0e35ef0342b6a5cf2bfd58544e07", "sha256_known_answer");
    ok &= check(parsed.generation_fingerprint == document.ecg.fingerprint(), "generation_fingerprint_is_explicit");

    signal_synth::ecg_scenario_document roundtrip;
    signal_synth::ecg_scenario_json_result repeated;
    ok &= check(signal_synth::parse_ecg_scenario_json(parsed.canonical_json, roundtrip, repeated), "parse_canonical_document");
    ok &= check(repeated.canonical_json == parsed.canonical_json && repeated.document_fingerprint == parsed.document_fingerprint, "canonical_roundtrip_is_stable");

    roundtrip.name = "Renamed";
    signal_synth::ecg_scenario_json_result renamed;
    ok &= check(signal_synth::write_ecg_scenario_json(roundtrip, renamed), "write_typed_document");
    ok &= check(renamed.document_fingerprint != parsed.document_fingerprint && renamed.generation_fingerprint == parsed.generation_fingerprint, "document_and_generation_identity_are_distinct");

    const std::string unicode = std::string(input).replace(input.find("\"description\":\"test\""), 20, "\"description\":\"\\uD834\\uDD1E\"");
    signal_synth::ecg_scenario_document unicode_document;
    signal_synth::ecg_scenario_json_result unicode_result;
    ok &= check(signal_synth::parse_ecg_scenario_json(unicode, unicode_document, unicode_result) && unicode_document.description.size() == 4, "unicode_surrogate_pair");

    ok &= check(rejects_without_mutation("[]", signal_synth::ecg_json_type), "reject_root_type");
    ok &= check(rejects_without_mutation("{", signal_synth::ecg_json_syntax), "reject_truncated_json");
    ok &= check(rejects_without_mutation(input + "x", signal_synth::ecg_json_syntax), "reject_trailing_data");
    ok &= check(rejects_without_mutation(std::string(input).replace(input.find("\"schema_version\":1"), 18, "\"schema_version\":3"), signal_synth::ecg_json_schema_version), "reject_schema_version");
    ok &= check(rejects_without_mutation(std::string(input).replace(input.find("\"schema_version\":1"), 18, "\"schema_version\":1,\"schema_version\":1"), signal_synth::ecg_json_duplicate_key), "reject_duplicate_key");
    ok &= check(rejects_without_mutation(std::string(input).replace(input.find("\"schema_version\":1"), 18, "\"schema_version\":1,\"unknown\":0"), signal_synth::ecg_json_unknown_field), "reject_unknown_field");
    ok &= check(rejects_without_mutation(std::string(input).replace(input.find("\"name\":\"Clean ECG\","), 19, ""), signal_synth::ecg_json_missing_field), "reject_missing_field");
    ok &= check(rejects_without_mutation(std::string(input).replace(input.find("\"tags\":[\"z\",\"a\"]"), 16, "\"tags\":[\"a\",\"a\"]"), signal_synth::ecg_json_duplicate_tag), "reject_duplicate_tag");
    ok &= check(rejects_without_mutation(std::string(input).replace(input.find("\"duration_seconds\":10"), 21, "\"duration_seconds\":0.001"), signal_synth::ecg_json_range), "reject_non_integral_sample_count");
    ok &= check(rejects_without_mutation(std::string(input).replace(input.find("\"seed\":12345"), 12, "\"seed\":1e999"), signal_synth::ecg_json_range), "reject_number_overflow");
    ok &= check(rejects_without_mutation(std::string(input).replace(input.find("ecg_clean_001"), 13, "bad_\\uD800"), signal_synth::ecg_json_syntax), "reject_unpaired_surrogate");
    std::string invalid_utf8 = input;
    invalid_utf8.replace(input.find("ecg_clean_001"), 13, std::string("bad_") + static_cast<char>(0xc0) + static_cast<char>(0xaf));
    ok &= check(rejects_without_mutation(invalid_utf8, signal_synth::ecg_json_syntax), "reject_invalid_utf8");
    ok &= check(rejects_without_mutation(std::string(input).replace(input.find("\"seed\":12345"), 12, "\"seed\":\"12345\""), signal_synth::ecg_json_type), "reject_wrong_scalar_type");

    const std::string duplicate_condition = std::string(input).replace(
        input.find("[{\"severity\":1,\"code\":\"NORM\"}]"), 30,
        "[{\"severity\":1,\"code\":\"NORM\"},{\"severity\":1,\"code\":\"NORM\"}]");
    ok &= check(rejects_without_mutation(duplicate_condition, signal_synth::ecg_json_duplicate_condition), "reject_duplicate_condition");

    const std::string unsupported = std::string(input).replace(input.find("\"NORM\""), 6, "\"PSVT\"");
    ok &= check(rejects_without_mutation(unsupported, signal_synth::ecg_json_semantic), "reject_unsupported_condition");

    signal_synth::ecg_scenario_document territorial;
    territorial.scenario_id = "territorial_imi";
    territorial.ecg.clear_conditions();
    territorial.ecg.add_condition(signal_synth::ecg_condition_imi, 0.5);
    signal_synth::ecg_scenario_json_result territorial_result;
    signal_synth::ecg_scenario_document territorial_roundtrip;
    signal_synth::ecg_scenario_json_result territorial_repeated;
    ok &= check(signal_synth::write_ecg_scenario_json(territorial, territorial_result) && signal_synth::parse_ecg_scenario_json(territorial_result.canonical_json, territorial_roundtrip, territorial_repeated) && territorial_roundtrip.ecg.has_condition(signal_synth::ecg_condition_imi) && territorial_roundtrip.ecg.condition_severity(0) == 0.5 && territorial_result.document_fingerprint == territorial_repeated.document_fingerprint, "territorial_condition_json_roundtrip");

    signal_synth::ecg_scenario_document ischemia;
    ischemia.scenario_id = "ischemia_iscal";
    ischemia.ecg.clear_conditions();
    ischemia.ecg.add_condition(signal_synth::ecg_condition_iscal, 0.6);
    signal_synth::ecg_scenario_json_result ischemia_result;
    signal_synth::ecg_scenario_document ischemia_roundtrip;
    signal_synth::ecg_scenario_json_result ischemia_repeated;
    ok &= check(signal_synth::write_ecg_scenario_json(ischemia, ischemia_result) && signal_synth::parse_ecg_scenario_json(ischemia_result.canonical_json, ischemia_roundtrip, ischemia_repeated) && ischemia_roundtrip.ecg.has_condition(signal_synth::ecg_condition_iscal) && ischemia_roundtrip.ecg.condition_severity(0) == 0.6 && ischemia_result.document_fingerprint == ischemia_repeated.document_fingerprint, "ischemia_condition_json_roundtrip");

    signal_synth::ecg_scenario_document invalid_document;
    invalid_document.scenario_id.clear();
    signal_synth::ecg_scenario_json_result invalid_result;
    ok &= check(!signal_synth::write_ecg_scenario_json(invalid_document, invalid_result) && invalid_result.messages[0].path == "$.scenario_id", "typed_document_validation");

    signal_synth::ecg_scenario_document multimodal;
    multimodal.schema_version = 2;
    multimodal.scenario_id = "ecg_ppg";
    multimodal.ppg.enabled = true;
    signal_synth::ecg_scenario_json_result multimodal_result;
    ok &= check(signal_synth::write_ecg_scenario_json(multimodal, multimodal_result) && multimodal_result.canonical_json.find("\"ppg\":{\"enabled\":true") != std::string::npos, "schema_v2_ppg_serialization");
    signal_synth::ecg_scenario_document multimodal_roundtrip;
    signal_synth::ecg_scenario_json_result multimodal_repeated;
    ok &= check(signal_synth::parse_ecg_scenario_json(multimodal_result.canonical_json, multimodal_roundtrip, multimodal_repeated) && multimodal_roundtrip.ppg.enabled && multimodal_repeated.document_fingerprint == multimodal_result.document_fingerprint, "schema_v2_ppg_roundtrip");
    multimodal_roundtrip.ppg.pulse_delay_ms += 1.0;
    signal_synth::ecg_scenario_json_result changed_ppg;
    ok &= check(signal_synth::write_ecg_scenario_json(multimodal_roundtrip, changed_ppg) && changed_ppg.document_fingerprint != multimodal_result.document_fingerprint && changed_ppg.generation_fingerprint == multimodal_result.generation_fingerprint, "ppg_changes_document_not_ecg_fingerprint");

    signal_synth::ecg_scenario_document v1_with_ppg;
    v1_with_ppg.ppg.enabled = true;
    ok &= check(!signal_synth::write_ecg_scenario_json(v1_with_ppg, invalid_result) && invalid_result.messages[0].path == "$.ppg", "schema_v1_rejects_unrepresentable_ppg");

    return ok ? 0 : 1;
}
