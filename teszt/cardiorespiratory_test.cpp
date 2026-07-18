#include "challenge_assembly.h"
#include "ecg_export.h"
#include "measurement_scoring.h"

#include <cmath>
#include <iostream>
#include <vector>

namespace
{
    bool check(bool condition, const char* name)
    {
        if (!condition) std::cerr << "FAILED: " << name << '\n';
        return condition;
    }
}

int main()
{
    bool ok = true;
    signal_synth::ecg_scenario_document document;
    document.schema_version = 4;
    document.scenario_id = "cardiorespiratory_acceptance";
    document.name = "Cardiorespiratory acceptance";
    document.duration_seconds = 300.0;
    document.ecg.set_sampling_rate_hz(100u);
    document.ecg.set_heart_rate_bpm(64.0);
    document.ppg.enabled = true;
    document.ppg.missing_pulse_every_n_beats = 17u;
    document.physiology.respiration_frequency_hz = 0.25;
    document.physiology.respiratory_rr_amplitude_seconds = 0.04;
    document.physiology.ecg_baseline_amplitude_mv = 0.05;
    document.physiology.ppg_amplitude_modulation_ratio = 0.12;
    document.physiology.ppg_delay_modulation_ms = 30.0;
    document.physiology.accelerometer_respiration_amplitude_g = 0.03;
    signal_synth::ppg_perfusion_episode_config low_perfusion;
    low_perfusion.start_seconds = 120.0;
    low_perfusion.duration_seconds = 30.0;
    low_perfusion.amplitude_scale = 0.35;
    document.ppg.perfusion_episodes.push_back(low_perfusion);

    signal_synth::ecg_render_bundle render;
    signal_synth::ecg_document_render_result render_result;
    const bool rendered = signal_synth::render_ecg_document(document, render, render_result);
    if (!rendered)
        for (std::size_t i = 0; i < render_result.messages.size(); ++i) std::cerr << render_result.messages[i] << '\n';
    ok &= check(rendered, "render");
    ok &= check(render.cardiorespiratory.prv_available && render.cardiorespiratory.respiration_available
        && render.cardiorespiratory.prv.metrics.accepted_interval_count > 100u
        && render.cardiorespiratory.prv.metrics.excluded_interval_count > 0u
        && render.cardiorespiratory.respiration.size() == 3001u
        && std::fabs(render.cardiorespiratory.respiratory_rate_bpm - 15.0) < 1e-12
        && !render.signal_quality.accelerometer.empty(), "analysis_truth");
    ok &= check(std::fabs(render.cardiorespiratory.prv.metrics.rmssd_seconds - render.hrv.metrics.rmssd_seconds) > 1e-6, "prv_hrv_difference");
    bool shared_delay_phase = true;
    for (unsigned int i = 0; render.ppg.pulses() && i < render.ppg.pulse_count(); ++i)
    {
        const signal_synth::ppg_pulse_annotation& pulse = render.ppg.pulses()[i];
        const double expected_ms = document.ppg.pulse_delay_ms + document.physiology.ppg_delay_modulation_ms * signal_synth::physiology_respiration_value(document.physiology, pulse.ecg_r_time_seconds);
        shared_delay_phase = shared_delay_phase && std::fabs(1000.0 * pulse.pulse_delay_seconds - expected_ms) < 1e-9;
    }
    ok &= check(shared_delay_phase, "shared_respiratory_phase");

    std::vector<signal_synth::measurement_truth> prv_truth, respiration_truth;
    std::vector<std::string> messages;
    ok &= check(signal_synth::measurement_ground_truth_from_render(render, "prv", prv_truth, messages)
        && signal_synth::measurement_ground_truth_from_render(render, "respiratory_rate", respiration_truth, messages)
        && prv_truth.size() > render.cardiorespiratory.prv.intervals.size()
        && respiration_truth.size() == 302u, "measurement_truth");
    std::vector<signal_synth::measurement_value> predictions;
    for (std::size_t i = 0; i < respiration_truth.size(); ++i) predictions.push_back(respiration_truth[i].measurement);
    signal_synth::measurement_score_result score;
    signal_synth::measurement_score_options options;
    ok &= check(signal_synth::score_measurements("respiratory_rate", respiration_truth, predictions, options, score)
        && score.success && score.total.tolerance_pass_fraction == 1.0, "respiratory_scoring");

    signal_synth::ecg_export_bundle exports;
    signal_synth::ecg_export_result export_result;
    ok &= check(signal_synth::build_ecg_export_bundle(render, exports, export_result)
        && exports.find("cardiorespiratory_truth.json") && exports.find("prv_tachogram.csv") && exports.find("respiration_reference.csv")
        && exports.find("cardiorespiratory_truth.json")->content.find("synsigra_cardiorespiratory_truth_v1") != std::string::npos
        && exports.find("cardiorespiratory_truth.json")->content.find("hrv_prv_agreement") != std::string::npos
        && signal_synth::challenge_file_role_for_export_artifact("cardiorespiratory_truth.json") == signal_synth::challenge_file_cardiorespiratory_truth_json
        && signal_synth::challenge_file_role_for_export_artifact("prv_tachogram.csv") == signal_synth::challenge_file_prv_tachogram_csv
        && signal_synth::challenge_file_role_for_export_artifact("respiration_reference.csv") == signal_synth::challenge_file_respiration_reference_csv, "export_contract");

    if (ok) std::cout << "cardiorespiratory tests passed\n";
    return ok ? 0 : 1;
}
