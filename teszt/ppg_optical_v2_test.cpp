#include "challenge_assembly.h"
#include "ecg_export.h"
#include "ecg_scenario_json.h"
#include "measurement_scoring.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    bool check(bool condition, const char* name)
    {
        if (!condition) std::cerr << "FAILED: " << name << '\n';
        return condition;
    }

    signal_synth::signal_quality_artifact_config ppg_artifact(signal_synth::signal_quality_artifact_type type, double start, double duration, double severity, unsigned long long seed)
    {
        signal_synth::signal_quality_artifact_config artifact;
        artifact.type = type;
        artifact.start_seconds = start;
        artifact.duration_seconds = duration;
        artifact.severity = severity;
        artifact.seed = seed;
        artifact.ppg = true;
        return artifact;
    }

    signal_synth::ecg_scenario_document optical_document()
    {
        signal_synth::ecg_scenario_document document;
        document.schema_version = 6;
        document.scenario_id = "ppg_optical_v2_acceptance";
        document.name = "PPG optical v2 acceptance";
        document.duration_seconds = 30.0;
        document.ecg.set_sampling_rate_hz(250u);
        document.ecg.set_heart_rate_bpm(60.0);
        document.ppg.enabled = true;
        signal_synth::configure_ppg_optical_profile("wrist_reflectance_v1", document.ppg.optical);
        document.ppg.optical.red.sensor_gain = 1.05;
        document.ppg.optical.red.ambient_offset_au = 0.01;
        document.ppg.optical.red.crosstalk_ratio = 0.10;
        document.ppg.optical.red.maximum_output_au = 1.485;
        document.ppg.optical.red.quantization_bits = 8u;
        document.ppg.optical.infrared.crosstalk_ratio = 0.04;
        signal_synth::ppg_oxygenation_episode_config desaturation;
        desaturation.start_seconds = 8.0;
        desaturation.duration_seconds = 14.0;
        desaturation.transition_seconds = 3.0;
        desaturation.target_spo2_percent = 88.0;
        document.ppg.optical.oxygenation_episodes.push_back(desaturation);
        document.signal_quality.artifacts.push_back(ppg_artifact(signal_synth::signal_quality_ppg_motion_periodic, 12.0, 5.0, 0.6, 81001u));
        document.signal_quality.artifacts.push_back(ppg_artifact(signal_synth::signal_quality_ppg_ambient_light, 23.0, 4.0, 0.5, 81002u));
        document.physiology.respiration_frequency_hz = 0.2;
        document.physiology.ppg_amplitude_modulation_ratio = 0.12;
        document.wearable.ecg.enabled = true;
        document.wearable.ecg.sample_rate_hz = 250u;
        return document;
    }
}

int main()
{
    bool ok = true;
    signal_synth::ppg_optical_config profile;
    ok &= check(signal_synth::ppg_optical_profile_count() == 4u
        && signal_synth::configure_ppg_optical_profile("finger_transmissive_v1", profile)
        && profile.enabled && profile.profile_id == "finger_transmissive_v1" && profile.red.quantization_bits == 16u
        && !signal_synth::configure_ppg_optical_profile("unknown", profile), "site_device_profiles");

    signal_synth::ecg_scenario_document document = optical_document();
    signal_synth::ecg_scenario_json_result identity;
    signal_synth::ecg_scenario_document parsed;
    signal_synth::ecg_scenario_json_result parsed_identity;
    ok &= check(signal_synth::write_ecg_scenario_json(document, identity)
        && signal_synth::parse_ecg_scenario_json(identity.canonical_json, parsed, parsed_identity)
        && identity.canonical_json == parsed_identity.canonical_json
        && parsed.ppg.optical.profile_id == "wrist_reflectance_v1"
        && parsed.ppg.optical.oxygenation_episodes.size() == 1u, "schema_v6_roundtrip");

    signal_synth::ecg_render_bundle render, replay;
    signal_synth::ecg_document_render_result render_result;
    ok &= check(signal_synth::render_ecg_document(parsed, render, render_result)
        && signal_synth::render_ecg_document(parsed, replay, render_result)
        && render.signal_quality.ppg_channels == replay.signal_quality.ppg_channels
        && render.ppg.channel_count() == 3u && render.ppg.optical_state_count() == render.ppg.pulse_count(), "deterministic_multichannel_render");

    bool baseline_seen = false, target_seen = false, recovery_seen = false, smooth_change_seen = false, equations_exact = true;
    double previous_spo2 = 0.0;
    const signal_synth::ppg_optical_pulse_state* states = render.ppg.optical_states();
    for (unsigned int i = 0; states && i < render.ppg.optical_state_count(); ++i)
    {
        const signal_synth::ppg_optical_pulse_state& state = states[i];
        baseline_seen = baseline_seen || (state.time_seconds < 7.5 && std::fabs(state.spo2_percent - 97.0) < 1e-12);
        target_seen = target_seen || (state.time_seconds > 11.5 && state.time_seconds < 18.5 && std::fabs(state.spo2_percent - 88.0) < 1e-12);
        recovery_seen = recovery_seen || (state.time_seconds > 23.0 && std::fabs(state.spo2_percent - 97.0) < 1e-12);
        if (i)
        {
            const double change = std::fabs(state.spo2_percent - previous_spo2);
            smooth_change_seen = smooth_change_seen || change > 0.01;
            equations_exact = equations_exact && change < 6.0;
        }
        previous_spo2 = state.spo2_percent;
        if (state.valid_for_measurement)
            equations_exact = equations_exact
                && std::fabs(state.infrared_ac_au / state.infrared_dc_au * 100.0 - state.infrared_perfusion_index_percent) < 1e-12
                && std::fabs((state.red_ac_au / state.red_dc_au) / (state.infrared_ac_au / state.infrared_dc_au) - state.ratio_of_ratios) < 1e-12
                && std::fabs(document.ppg.optical.calibration_intercept_percent + document.ppg.optical.calibration_slope_percent * state.ratio_of_ratios - state.spo2_percent) < 1e-12;
    }
    ok &= check(baseline_seen && target_seen && recovery_seen && smooth_change_seen && equations_exact, "smooth_trajectory_and_optical_equations");

    const std::vector<double>& red = render.signal_quality.ppg_channels[1];
    const std::vector<double>& infrared = render.signal_quality.ppg_channels[2];
    ok &= check(render.ppg.channel_samples(1u)[0] == render.ppg.channel_dc_au(1u)
        && red[0] != render.ppg.channel_samples(1u)[0], "clean_latent_and_sensor_noise_layers");
    bool wavelength_difference = false, red_quantized = true;
    const double red_minimum = render.ppg.channel_minimum_output_au(1u);
    const double red_step = (render.ppg.channel_maximum_output_au(1u) - red_minimum) / 255.0;
    for (std::size_t i = 0; i < red.size(); ++i)
    {
        wavelength_difference = wavelength_difference || std::fabs(red[i] - infrared[i]) > 1e-6;
        red_quantized = red_quantized && std::fabs((red[i] - red_minimum) / red_step - std::floor((red[i] - red_minimum) / red_step + 0.5)) < 1e-9;
    }
    ok &= check(wavelength_difference, "wavelength_dependent_response");
    ok &= check(red_quantized, "sensor_quantization");
    ok &= check(render.ppg_clipping_counts.size() == 3u && render.ppg_clipping_counts[1] > 0u, "sensor_clipping_truth");

    signal_synth::ecg_export_bundle exports;
    signal_synth::ecg_export_result export_result;
    const signal_synth::ecg_text_artifact* latent = 0;
    const signal_synth::ecg_text_artifact* truth_artifact = 0;
    ok &= check(signal_synth::build_ecg_export_bundle(render, exports, export_result)
        && (latent = exports.find("ppg_optical_latent.csv"))
        && (truth_artifact = exports.find("ppg_optical_truth.json"))
        && latent->content.find("red_latent_au") != std::string::npos
        && truth_artifact->content.find("\"profile_id\":\"wrist_reflectance_v1\"") != std::string::npos
        && truth_artifact->content.find("ratio_of_ratios") != std::string::npos
        && truth_artifact->content.find("no clinical SpO2") != std::string::npos
        && signal_synth::challenge_file_role_for_export_artifact("ppg_optical_latent.csv") == signal_synth::challenge_file_ppg_optical_latent_csv
        && signal_synth::challenge_file_role_for_export_artifact("ppg_optical_truth.json") == signal_synth::challenge_file_ppg_optical_truth_json, "portable_optical_truth_contract");

    std::vector<signal_synth::measurement_truth> truth;
    std::vector<std::string> messages;
    signal_synth::measurement_output_document perfect;
    ok &= check(signal_synth::measurement_ground_truth_from_render(render, "ppg_optical", truth, messages)
        && truth.size() == render.ppg.optical_state_count() * 6u, "measurement_truth");
    for (std::size_t i = 0; i < truth.size(); ++i) perfect.measurements.push_back(truth[i].measurement);
    signal_synth::measurement_score_options options;
    signal_synth::measurement_score_result score;
    ok &= check(signal_synth::score_measurement_output_to_render(render, "ppg_optical", perfect, options, score)
        && score.total.missing_count == 0u && score.total.extra_count == 0u
        && score.total.tolerance_pass_count == score.total.numeric_pair_count, "perfect_local_scoring");

    signal_synth::ecg_scenario_document trailing = optical_document();
    trailing.duration_seconds = 3.0;
    trailing.ppg.optical.oxygenation_episodes.clear();
    trailing.ppg.optical.red.delay_ms = 1000.0;
    trailing.ppg.optical.infrared.delay_ms = 0.0;
    trailing.signal_quality.artifacts.clear();
    trailing.wearable = signal_synth::wearable_timebase_config();
    signal_synth::ecg_scenario_json_result trailing_identity;
    signal_synth::ecg_render_bundle trailing_render;
    bool rejected_partial_pair = false;
    ok &= check(signal_synth::write_ecg_scenario_json(trailing, trailing_identity)
        && trailing_identity.canonical_json.find("\"wearable\"") == std::string::npos
        && signal_synth::render_ecg_document(trailing, trailing_render, render_result)
        && trailing_render.wearable.streams.empty(), "schema_v6_optional_wearable");
    for (unsigned int i = 0; i < trailing_render.ppg.pulse_count() && i < trailing_render.ppg.optical_state_count(); ++i)
        if (trailing_render.ppg.pulses()[i].generated && !trailing_render.ppg.optical_states()[i].generated)
        {
            bool red_present = false, infrared_present = false;
            for (unsigned int annotation = 0; annotation < trailing_render.ppg.channel_annotation_count(1u); ++annotation)
                red_present = red_present || trailing_render.ppg.channel_annotations(1u)[annotation].ecg_beat_index == trailing_render.ppg.pulses()[i].ecg_beat_index;
            for (unsigned int annotation = 0; annotation < trailing_render.ppg.channel_annotation_count(2u); ++annotation)
                infrared_present = infrared_present || trailing_render.ppg.channel_annotations(2u)[annotation].ecg_beat_index == trailing_render.ppg.pulses()[i].ecg_beat_index;
            rejected_partial_pair = rejected_partial_pair || (!red_present && !infrared_present);
        }
    ok &= check(rejected_partial_pair, "atomic_optical_pair_at_record_boundary");

    signal_synth::ecg_scenario_document invalid = document;
    invalid.ppg.optical.profile_id = "unregistered_profile";
    signal_synth::ecg_scenario_json_result invalid_result;
    ok &= check(!signal_synth::write_ecg_scenario_json(invalid, invalid_result), "unknown_profile_rejected");
    return ok ? 0 : 1;
}
