#include "ecg_export.h"
#include "scenario_stress.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    bool check(bool condition, const char* name)
    {
        if (!condition)
            std::cerr << "FAILED: " << name << '\n';
        return condition;
    }

    signal_synth::ecg_scenario_document stress_document()
    {
        signal_synth::ecg_scenario_document document;
        document.schema_version = 3;
        document.scenario_id = "wearable_stress_test";
        document.name = "Wearable stress test";
        document.duration_seconds = 60.0;
        document.ecg.set_sampling_rate_hz(250);
        document.ecg.set_heart_rate_bpm(72.0);
        document.ecg.set_rr_variability_seconds(0.02);
        document.ppg.enabled = true;
        document.ppg.pulse_delay_ms = 180.0;
        document.ppg.pulse_delay_variation_ms = 25.0;
        document.ppg.pulse_delay_variation_hz = 0.10;
        document.ppg.missing_pulse_every_n_beats = 7;
        document.ppg.clock_drift_ppm = 300.0;
        document.ppg.seed = 64001;
        document.randomization.enabled = true;
        document.randomization.seed = 64002;
        signal_synth::scenario_randomization_envelope heart_rate;
        heart_rate.parameter = "ecg.heart_rate_bpm";
        heart_rate.minimum = 65.0;
        heart_rate.maximum = 85.0;
        document.randomization.envelopes.push_back(heart_rate);
        signal_synth::scenario_randomization_envelope pulse_delay;
        pulse_delay.parameter = "ppg.pulse_delay_ms";
        pulse_delay.minimum = 150.0;
        pulse_delay.maximum = 230.0;
        document.randomization.envelopes.push_back(pulse_delay);
        signal_synth::scenario_randomization_envelope r_amplitude;
        r_amplitude.parameter = "ecg.morphology.r_amplitude_mv";
        r_amplitude.minimum = 0.85;
        r_amplitude.maximum = 1.35;
        document.randomization.envelopes.push_back(r_amplitude);
        signal_synth::scenario_randomization_envelope t_axis;
        t_axis.parameter = "ecg.morphology.t_axis_degrees";
        t_axis.minimum = 20.0;
        t_axis.maximum = 70.0;
        document.randomization.envelopes.push_back(t_axis);
        signal_synth::scenario_randomization_envelope qt_interval;
        qt_interval.parameter = "ecg.morphology.qt_interval_ms";
        qt_interval.minimum = 380.0;
        qt_interval.maximum = 440.0;
        document.randomization.envelopes.push_back(qt_interval);
        document.physiology.respiration_frequency_hz = 0.22;
        document.physiology.respiratory_rr_amplitude_seconds = 0.035;
        document.physiology.ecg_baseline_amplitude_mv = 0.08;
        document.physiology.ppg_amplitude_modulation_ratio = 0.18;
        document.physiology.activity_start_seconds = 20.0;
        document.physiology.activity_duration_seconds = 25.0;
        document.physiology.activity_intensity = 0.5;
        document.physiology.seed = 64003;
        return document;
    }

    bool same_draws(const std::vector<signal_synth::scenario_parameter_draw>& left, const std::vector<signal_synth::scenario_parameter_draw>& right)
    {
        if (left.size() != right.size())
            return false;
        for (std::size_t i = 0; i < left.size(); ++i)
            if (left[i].parameter != right[i].parameter || left[i].unit_draw != right[i].unit_draw || left[i].value != right[i].value)
                return false;
        return true;
    }
}

int main()
{
    bool ok = true;
    const signal_synth::ecg_scenario_document input = stress_document();
    signal_synth::ecg_scenario_document first;
    signal_synth::ecg_scenario_document second;
    std::vector<signal_synth::scenario_parameter_draw> first_draws;
    std::vector<signal_synth::scenario_parameter_draw> second_draws;
    std::vector<std::string> messages;
    ok &= check(signal_synth::resolve_scenario_controls(input, first, first_draws, messages)
        && signal_synth::resolve_scenario_controls(input, second, second_draws, messages)
        && same_draws(first_draws, second_draws)
        && first_draws.size() == 5u, "seeded_resolution_is_deterministic");
    ok &= check(first.ecg.heart_rate_bpm() >= 65.0 && first.ecg.heart_rate_bpm() <= 85.0
        && first.ppg.pulse_delay_ms >= 150.0 && first.ppg.pulse_delay_ms <= 230.0
        && first.ecg.morphology_control_enabled(signal_synth::ecg_morphology_r_amplitude_mv)
        && first.ecg.morphology_control_value(signal_synth::ecg_morphology_r_amplitude_mv) >= 0.85
        && first.ecg.morphology_control_value(signal_synth::ecg_morphology_r_amplitude_mv) <= 1.35
        && first.ecg.morphology_control_enabled(signal_synth::ecg_morphology_t_axis_degrees)
        && first.ecg.morphology_control_enabled(signal_synth::ecg_morphology_qt_interval_ms), "draws_stay_inside_envelopes");

    signal_synth::ecg_scenario_document changed_seed = input;
    ++changed_seed.randomization.seed;
    signal_synth::ecg_scenario_document changed;
    std::vector<signal_synth::scenario_parameter_draw> changed_draws;
    ok &= check(signal_synth::resolve_scenario_controls(changed_seed, changed, changed_draws, messages)
        && !same_draws(first_draws, changed_draws), "seed_changes_draws");

    signal_synth::ecg_scenario_json_result identity;
    signal_synth::ecg_scenario_document roundtrip;
    signal_synth::ecg_scenario_json_result repeated_identity;
    ok &= check(signal_synth::write_ecg_scenario_json(input, identity)
        && signal_synth::parse_ecg_scenario_json(identity.canonical_json, roundtrip, repeated_identity)
        && identity.document_fingerprint == repeated_identity.document_fingerprint
        && roundtrip.ppg.missing_pulse_every_n_beats == 7u, "schema_v3_roundtrip");
    signal_synth::ecg_scenario_document negative_delay = input;
    negative_delay.ppg.pulse_delay_variation_ms = 200.0;
    signal_synth::ecg_scenario_json_result negative_delay_result;
    ok &= check(!signal_synth::write_ecg_scenario_json(negative_delay, negative_delay_result), "negative_variable_pulse_delay_rejected");
    signal_synth::ecg_scenario_document invalid_morphology = input;
    invalid_morphology.randomization.envelopes[2].maximum = 20.0;
    signal_synth::ecg_scenario_json_result invalid_morphology_result;
    ok &= check(!signal_synth::write_ecg_scenario_json(invalid_morphology, invalid_morphology_result), "invalid_morphology_envelope_rejected");

    signal_synth::ecg_render_bundle render;
    signal_synth::ecg_document_render_result result;
    signal_synth::ecg_export_result export_result;
    ok &= check(signal_synth::render_ecg_document(input, render, result), "stress_render");
    unsigned int missing = 0;
    double minimum_delay = 1e9;
    double maximum_delay = -1e9;
    for (unsigned int i = 0; i < render.ppg.pulse_count(); ++i)
    {
        const signal_synth::ppg_pulse_annotation& pulse = render.ppg.pulses()[i];
        missing += pulse.intentionally_missing ? 1u : 0u;
        minimum_delay = std::min(minimum_delay, pulse.pulse_delay_seconds);
        maximum_delay = std::max(maximum_delay, pulse.pulse_delay_seconds);
    }
    ok &= check(render.ppg.pulse_count() == render.record.beat_count()
        && missing > 0u
        && missing == render.metrics.ppg_missing_pulse_count
        && maximum_delay - minimum_delay > 0.03, "ppg_sync_stress_ground_truth");
    ok &= check(render.parameter_draws.size() == 5u
        && render.resolved_document_identity.document_fingerprint != render.document_identity.document_fingerprint
        && signal_synth::scenario_parameter_draws_json(input, render.resolved_document, render.parameter_draws).find("\"ecg.morphology.r_amplitude_mv\"") != std::string::npos
        && render.resolved_document_identity.canonical_json.find("\"morphology\":{") != std::string::npos, "resolved_scenario_audit_trail");
    signal_synth::ecg_scenario_document resolved_roundtrip;
    signal_synth::ecg_scenario_json_result resolved_roundtrip_identity;
    ok &= check(signal_synth::parse_ecg_scenario_json(render.resolved_document_identity.canonical_json, resolved_roundtrip, resolved_roundtrip_identity)
        && resolved_roundtrip.ecg.morphology_control_enabled(signal_synth::ecg_morphology_r_amplitude_mv)
        && resolved_roundtrip_identity.document_fingerprint == render.resolved_document_identity.document_fingerprint, "resolved_morphology_roundtrip");

    for (unsigned int seed_offset = 0; seed_offset < 6; ++seed_offset)
    {
        signal_synth::ecg_scenario_document seeded = input;
        seeded.randomization.seed += seed_offset * 17u;
        signal_synth::ecg_render_bundle seeded_render;
        ok &= check(signal_synth::render_ecg_document(seeded, seeded_render, result)
            && seeded_render.record.beat_count() > 20u
            && seeded_render.record.fiducial_count() > seeded_render.record.beat_count() * 5u
            && seeded_render.morphology.entry_count() >= seeded_render.record.beat_count() * signal_synth::clinical_lead_count, "morphology_population_seed_integrity");
    }

    signal_synth::ecg_scenario_document compact = input;
    compact.scenario_id = "compact_stress_test";
    compact.output.compact = true;
    compact.output.retain_source_channels = false;
    compact.output.include_waveform_csv = false;
    compact.output.include_edf_bdf = false;
    signal_synth::ecg_render_bundle compact_render;
    signal_synth::ecg_export_bundle compact_bundle;
    ok &= check(signal_synth::render_ecg_document(compact, compact_render, result)
        && signal_synth::build_ecg_export_bundle(compact_render, compact_bundle, export_result), "compact_render_and_export");
    ok &= check(compact_render.record.source_data(0, 0) == 0
        && compact_render.record.vcg_data(0) == 0
        && compact_bundle.find("waveform.csv") == 0
        && compact_bundle.find("synsigra.edf") == 0
        && compact_bundle.find("synsigra.bdf") == 0
        && compact_bundle.find("synsigra.dat") != 0
        && compact_bundle.find("resolved_scenario.json") != 0
        && compact_bundle.find("randomization.json") != 0, "compact_output_contract");

    if (!ok)
        return 1;
    std::cout << "scenario_stress_test=passed\n";
    return 0;
}
