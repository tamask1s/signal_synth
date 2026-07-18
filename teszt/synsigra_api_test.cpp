#include "../src/synsigra_api.h"

#include <cmath>
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

    std::string clean_scenario_json()
    {
        return "{"
            "\"schema_version\":2,"
            "\"scenario_id\":\"facade_clean\","
            "\"name\":\"Facade clean\","
            "\"description\":\"Facade API smoke\","
            "\"author\":\"Synsigra\","
            "\"tags\":[\"facade\"],"
            "\"duration_seconds\":8,"
            "\"sample_rate_hz\":500,"
            "\"seed\":12345,"
            "\"ecg\":{"
                "\"heart_rate_bpm\":60,"
                "\"rr_variability_seconds\":0,"
                "\"ectopic_every_n_beats\":0,"
                "\"second_degree_av_pattern\":\"unspecified\","
                "\"q_wave_territory\":\"unspecified\","
                "\"rhythm_episodes\":[],"
                "\"fidelity_policy\":\"allow_parameterized\","
                "\"conditions\":[{\"code\":\"NORM\",\"severity\":1}]"
            "},"
            "\"ppg\":{"
                "\"enabled\":true,"
                "\"pulse_delay_ms\":180,"
                "\"rise_time_ms\":120,"
                "\"decay_time_ms\":300,"
                "\"amplitude_au\":1,"
                "\"baseline_au\":0,"
                "\"dicrotic_delay_ms\":180,"
                "\"dicrotic_width_ms\":80,"
                "\"dicrotic_amplitude_ratio\":0.15"
            "}"
        "}";
    }

    bool append_detection_times_from_annotations(const std::string& annotations_json, std::vector<signal_synth::synsigra_detection_event>& detections)
    {
        detections.clear();
        const std::string key = "\"r_peak_seconds\":";
        std::string::size_type position = 0;
        while ((position = annotations_json.find(key, position)) != std::string::npos)
        {
            position += key.size();
            std::string::size_type end = position;
            while (end < annotations_json.size() && (annotations_json[end] == '-' || annotations_json[end] == '+' || annotations_json[end] == '.' || annotations_json[end] == 'e' || annotations_json[end] == 'E' || (annotations_json[end] >= '0' && annotations_json[end] <= '9')))
                ++end;
            std::istringstream input(annotations_json.substr(position, end - position));
            double time = 0.0;
            input >> time;
            if (!input)
                return false;
            signal_synth::synsigra_detection_event event;
            event.time_seconds = time;
            event.label = "r";
            detections.push_back(event);
            position = end;
        }
        return !detections.empty();
    }
}

int main()
{
    bool ok = true;
    const std::string scenario = clean_scenario_json();

    signal_synth::synsigra_validation_result validation;
    ok &= check(signal_synth::synsigra_validate_scenario_json(scenario, validation) && validation.success, "validate_success");
    ok &= check(validation.identity.scenario_id == "facade_clean" && validation.identity.sample_count == 4000, "validate_identity");
    ok &= check(validation.canonical_scenario_json.find("\"scenario_id\":\"facade_clean\"") != std::string::npos, "validate_canonical_json");

    signal_synth::synsigra_render_result render;
    ok &= check(signal_synth::synsigra_render_scenario_json(scenario, render) && render.success, "render_success");
    ok &= check(render.identity.render_identity.find(render.identity.document_fingerprint + ":ecg-run-") == 0, "render_identity");
    ok &= check(render.find_artifact("waveform.csv") && render.find_artifact("annotations.json") && render.find_artifact("rr_tachogram.csv") && render.find_artifact("hrv_metrics.json") && render.find_artifact("cardiorespiratory_truth.json") && render.find_artifact("prv_tachogram.csv") && render.find_artifact("realism_metrics.json") && render.find_artifact("realism_report.html") && render.find_artifact("provenance.json") && render.find_artifact("ENGINEERING_CLAIM_BOUNDARY.txt") && render.find_artifact("report.html") && render.find_artifact("synsigra.hea") && render.find_artifact("synsigra.edf") && render.find_artifact("synsigra.bdf"), "render_artifacts");
    ok &= check(render.find_artifact("waveform.csv")->content.find("ppg_green_au") != std::string::npos, "render_ppg_channel");

    std::vector<signal_synth::synsigra_detection_event> detections;
    ok &= check(append_detection_times_from_annotations(render.find_artifact("annotations.json")->content, detections), "derive_detection_events");

    signal_synth::synsigra_compare_options options;
    options.target = signal_synth::synsigra_compare_r_peak;
    signal_synth::synsigra_compare_result compare;
    ok &= check(signal_synth::synsigra_compare_scenario_detections(scenario, detections, options, compare) && compare.success, "compare_success");
    ok &= check(compare.identity.render_identity == render.identity.render_identity && compare.target_name == "r_peak", "compare_identity");
    ok &= check(compare.total.ground_truth_count == detections.size() && compare.total.true_positive_count == detections.size() && compare.total.f1_score == 1.0, "compare_metrics");
    ok &= check(compare.find_artifact("comparison.json") && compare.find_artifact("comparison.csv") && compare.find_artifact("comparison_report.html"), "compare_artifacts");
    ok &= check(std::fabs(signal_synth::synsigra_default_compare_tolerance_seconds(signal_synth::synsigra_compare_r_peak) - 0.050) < 1e-12, "default_r_tolerance");
    ok &= check(std::fabs(signal_synth::synsigra_default_compare_tolerance_seconds(signal_synth::synsigra_compare_ppg_systolic_peak) - 0.080) < 1e-12, "default_ppg_tolerance");
    ok &= check(std::fabs(signal_synth::synsigra_default_compare_tolerance_seconds(signal_synth::synsigra_compare_ppg_pulse_onset) - 0.050) < 1e-12
        && std::string(signal_synth::synsigra_compare_target_name(signal_synth::synsigra_compare_ppg_pulse_onset)) == "ppg_pulse_onset", "ppg_onset_contract");
    ok &= check(std::string(signal_synth::synsigra_compare_target_name(static_cast<signal_synth::synsigra_compare_target>(99))).empty()
        && signal_synth::synsigra_default_compare_tolerance_seconds(static_cast<signal_synth::synsigra_compare_target>(99)) == 0.0, "invalid_target_has_no_fallback");
    const std::string integration_contract = signal_synth::synsigra_integration_contract_json();
    ok &= check(std::string(signal_synth::synsigra_integration_contract_version()) == "synsigra_core_integration_v5"
        && integration_contract.find("\"contract\":\"synsigra_core_integration_v5\"") != std::string::npos
        && integration_contract.find("\"cpp_facade\":\"1.3.0\"") != std::string::npos
        && integration_contract.find("\"latest_schema_version\":9") != std::string::npos
        && integration_contract.find("\"metric_definition\":\"synsigra_hrv_metrics_v2\"") != std::string::npos
        && integration_contract.find("\"sd1_sd2_ratio\"") != std::string::npos
        && integration_contract.find("\"challenge_package\":\"synsigra_challenge_package_v2\"") != std::string::npos
        && integration_contract.find("\"submission\":\"synsigra_submission_v1\"") != std::string::npos
        && integration_contract.find("\"comparison_targets\":[\"r_peak\",\"ppg_systolic_peak\",\"ppg_pulse_onset\",\"ecg_beat_classification\"]") != std::string::npos
        && integration_contract.find("\"measurement_targets\":[\"rr_interval\",\"qtc\",\"morphology_assertions\",\"ecg_ppg_alignment\",\"ppg_optical\",\"prv\",\"respiratory_rate\",\"rhythm_burden\"]") != std::string::npos, "integration_contract");

    signal_synth::synsigra_validation_result invalid;
    ok &= check(!signal_synth::synsigra_validate_scenario_json("{\"schema_version\":2}", invalid) && !invalid.success && !invalid.messages.empty(), "invalid_has_messages");
    ok &= check(std::string(signal_synth::synsigra_api_version()) == "1.3.0", "api_version");

    return ok ? 0 : 1;
}
