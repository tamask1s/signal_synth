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
        "\"episode_type\":\"none\",\"episode_start_seconds\":2,\"episode_duration_seconds\":4,"
        "\"episode_rate_bpm\":170,"
        "\"flutter_conduction_pattern\":\"fixed\","
        "\"pacing_mode\":\"ventricular\",\"pacing_non_capture_every_n_beats\":0,"
        "\"fidelity_policy\":\"allow_parameterized\",\"conditions\":[{\"code\":\"NORM\",\"severity\":1}]}}";

    signal_synth::ecg_scenario_document document;
    signal_synth::ecg_scenario_json_result parsed;
    ok &= check(signal_synth::parse_ecg_scenario_json(input, document, parsed), "parse_valid_document");
    ok &= check(parsed.success && parsed.canonical_json == canonical && document.sample_count() == 5000, "canonicalization_and_sample_count");
    ok &= check(parsed.document_fingerprint == "sha256:bbf5e3e375d37782a11e4e1f96eea364ada9b762c33f0d7fe2638a2e10676467", "sha256_known_answer");
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
    ok &= check(rejects_without_mutation(std::string(input).replace(input.find("\"schema_version\":1"), 18, "\"schema_version\":4"), signal_synth::ecg_json_schema_version), "reject_schema_version");
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

    const std::string unsupported = std::string(input).replace(input.find("\"NORM\""), 6, "\"ABQRS\"");
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

    signal_synth::ecg_scenario_document flutter_document;
    flutter_document.schema_version = 2;
    flutter_document.scenario_id = "flutter_variable";
    flutter_document.name = "Flutter variable conduction";
    flutter_document.ecg.clear_conditions();
    flutter_document.ecg.add_condition(signal_synth::ecg_condition_aflt);
    flutter_document.ecg.set_flutter_conduction_pattern(signal_synth::ecg_flutter_alternate_2_3);
    signal_synth::ecg_scenario_json_result flutter_result;
    signal_synth::ecg_scenario_document flutter_roundtrip;
    signal_synth::ecg_scenario_json_result flutter_repeated;
    ok &= check(signal_synth::write_ecg_scenario_json(flutter_document, flutter_result) && flutter_result.canonical_json.find("\"flutter_conduction_pattern\":\"alternate_2_3\"") != std::string::npos && signal_synth::parse_ecg_scenario_json(flutter_result.canonical_json, flutter_roundtrip, flutter_repeated) && flutter_roundtrip.ecg.flutter_conduction_pattern() == signal_synth::ecg_flutter_alternate_2_3, "flutter_pattern_json_roundtrip");

    signal_synth::ecg_scenario_document pacing_document;
    pacing_document.schema_version = 2;
    pacing_document.scenario_id = "paced_dual_non_capture";
    pacing_document.name = "Paced dual chamber non-capture";
    pacing_document.ecg.clear_conditions();
    pacing_document.ecg.add_condition(signal_synth::ecg_condition_pace);
    pacing_document.ecg.set_pacing_mode(signal_synth::ecg_pacing_dual_chamber);
    pacing_document.ecg.set_pacing_non_capture_every_n_beats(4);
    signal_synth::ecg_scenario_json_result pacing_result;
    signal_synth::ecg_scenario_document pacing_roundtrip;
    signal_synth::ecg_scenario_json_result pacing_repeated;
    ok &= check(signal_synth::write_ecg_scenario_json(pacing_document, pacing_result) && pacing_result.canonical_json.find("\"pacing_mode\":\"dual_chamber\"") != std::string::npos && pacing_result.canonical_json.find("\"pacing_non_capture_every_n_beats\":4") != std::string::npos && signal_synth::parse_ecg_scenario_json(pacing_result.canonical_json, pacing_roundtrip, pacing_repeated) && pacing_roundtrip.ecg.pacing_mode() == signal_synth::ecg_pacing_dual_chamber && pacing_roundtrip.ecg.pacing_non_capture_every_n_beats() == 4, "pacing_json_roundtrip");

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
    multimodal_roundtrip.ppg.clock_drift_ppm = 10.0;
    signal_synth::ecg_scenario_json_result invalid_v2_stress;
    ok &= check(!signal_synth::write_ecg_scenario_json(multimodal_roundtrip, invalid_v2_stress), "schema_v2_rejects_v3_ppg_stress");

    const std::string hrv_input =
        "{"
        "\"schema_version\":2,\"scenario_id\":\"hrv_schema\",\"name\":\"HRV schema\",\"description\":\"\",\"author\":\"\","
        "\"tags\":[\"hrv\"],\"duration_seconds\":300,\"sample_rate_hz\":100,\"seed\":1,"
        "\"ecg\":{\"heart_rate_bpm\":60,\"rr_variability_seconds\":0,\"ectopic_every_n_beats\":0,"
        "\"second_degree_av_pattern\":\"unspecified\",\"q_wave_territory\":\"unspecified\","
        "\"episode_type\":\"none\",\"episode_start_seconds\":2,\"episode_duration_seconds\":4,"
        "\"episode_rate_bpm\":170,\"flutter_conduction_pattern\":\"fixed\","
        "\"pacing_mode\":\"ventricular\",\"pacing_non_capture_every_n_beats\":0,"
        "\"fidelity_policy\":\"allow_parameterized\","
        "\"conditions\":[{\"code\":\"NORM\",\"severity\":1}]},"
        "\"hrv\":{\"enabled\":true,\"target_mean_hr_bpm\":72,\"target_sdnn_seconds\":0.045,"
        "\"lf_hf_ratio\":1.8,\"lf_center_hz\":0.1,\"lf_bandwidth_hz\":0.04,"
        "\"hf_center_hz\":0.25,\"hf_bandwidth_hz\":0.12,"
        "\"respiratory_frequency_hz\":0.25,\"respiratory_amplitude_seconds\":0.02,"
        "\"minimum_rr_seconds\":0.45,\"maximum_rr_seconds\":1.6,\"seed\":4242},"
        "\"ppg\":{\"enabled\":false,\"pulse_delay_ms\":180,\"rise_time_ms\":120,\"decay_time_ms\":300,"
        "\"amplitude_au\":1,\"baseline_au\":0,\"dicrotic_delay_ms\":180,"
        "\"dicrotic_width_ms\":80,\"dicrotic_amplitude_ratio\":0.15}"
        "}";
    signal_synth::ecg_scenario_document hrv_document;
    signal_synth::ecg_scenario_json_result hrv_result;
    ok &= check(signal_synth::parse_ecg_scenario_json(hrv_input, hrv_document, hrv_result), "hrv_schema_parse");
    ok &= check(hrv_document.hrv.enabled && hrv_document.hrv.lf_hf_ratio == 1.8 && hrv_document.ecg.heart_rate_bpm() == 72.0 && hrv_document.ecg.rr_variability_seconds() == 0.045 && hrv_document.ecg.minimum_rr_seconds() == 0.45 && hrv_document.ecg.maximum_rr_seconds() == 1.6 && hrv_document.ecg.seed() == 4242, "hrv_schema_applies_timeline");
    ok &= check(hrv_result.canonical_json.find("\"hrv\":{\"enabled\":true") != std::string::npos && hrv_result.canonical_json.find("\"lf_hf_ratio\":1.8") != std::string::npos, "hrv_canonical");
    signal_synth::ecg_scenario_document hrv_changed = hrv_document;
    hrv_changed.hrv.lf_hf_ratio = 2.1;
    signal_synth::ecg_scenario_json_result hrv_changed_result;
    ok &= check(signal_synth::write_ecg_scenario_json(hrv_changed, hrv_changed_result) && hrv_changed_result.document_fingerprint != hrv_result.document_fingerprint, "hrv_parameter_changes_document_fingerprint");
    ok &= check(rejects_without_mutation(std::string(hrv_input).replace(hrv_input.find("\"lf_hf_ratio\":1.8"), std::string("\"lf_hf_ratio\":1.8").size(), "\"lf_hf_ratio\":-1"), signal_synth::ecg_json_range), "reject_invalid_hrv_lfhf");
    ok &= check(rejects_without_mutation(std::string(hrv_input).replace(hrv_input.find("\"duration_seconds\":300"), std::string("\"duration_seconds\":300").size(), "\"duration_seconds\":40"), signal_synth::ecg_json_range), "reject_short_hrv_window");
    ok &= check(rejects_without_mutation(std::string(hrv_input).replace(hrv_input.find("\"schema_version\":2"), std::string("\"schema_version\":2").size(), "\"schema_version\":1"), signal_synth::ecg_json_unknown_field), "schema_v1_rejects_hrv");

    signal_synth::ecg_scenario_document v1_with_ppg;
    v1_with_ppg.ppg.enabled = true;
    ok &= check(!signal_synth::write_ecg_scenario_json(v1_with_ppg, invalid_result) && invalid_result.messages[0].path == "$.ppg", "schema_v1_rejects_unrepresentable_ppg");

    return ok ? 0 : 1;
}
