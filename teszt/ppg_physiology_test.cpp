#include "ecg_export.h"
#include "ecg_compare.h"
#include "ecg_scenario_json.h"
#include "ppg_model.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace
{
    bool check(bool condition, const char* name)
    {
        if (!condition)
            std::cerr << "FAILED: " << name << '\n';
        return condition;
    }

    signal_synth::ppg_config physiology_config()
    {
        signal_synth::ppg_config config;
        config.enabled = true;
        config.pulse_delay_ms = 190.0;
        config.pulse_delay_variation_ms = 15.0;
        config.pulse_delay_variation_hz = 0.08;
        config.pulse_delay_jitter_ms = 12.0;
        config.low_frequency_amplitude_modulation_ratio = 0.25;
        config.low_frequency_amplitude_modulation_hz = 0.05;
        config.rise_time_variation_ratio = 0.20;
        config.decay_time_variation_ratio = 0.25;
        config.seed = 50052001;
        signal_synth::ppg_perfusion_episode_config episode;
        episode.start_seconds = 10.0;
        episode.duration_seconds = 20.0;
        episode.amplitude_scale = 0.45;
        episode.rise_time_scale = 1.35;
        episode.decay_time_scale = 1.20;
        episode.weak_pulse_every_n_beats = 3;
        episode.weak_pulse_amplitude_scale = 0.30;
        episode.missing_pulse_every_n_beats = 5;
        config.perfusion_episodes.push_back(episode);
        return config;
    }
}

int main()
{
    bool ok = true;
    signal_synth::clinical_ecg_config ecg_config;
    ecg_config.sampling_rate_hz = 250;
    ecg_config.rhythm.heart_rate_bpm = 60.0;
    signal_synth::clinical_ecg_record ecg;
    ok &= check(signal_synth::clinical_ecg_generator(ecg_config).generate(10000, ecg), "ecg_timeline");

    const signal_synth::ppg_config config = physiology_config();
    signal_synth::ppg_record first;
    signal_synth::ppg_record repeated;
    ok &= check(signal_synth::ppg_generator(config).generate(ecg, first)
        && signal_synth::ppg_generator(config).generate(ecg, repeated)
        && first.pulse_count() == repeated.pulse_count(), "deterministic_generation");

    unsigned int low_perfusion = 0;
    unsigned int weak = 0;
    unsigned int missing = 0;
    double minimum_delay = 1e9, maximum_delay = -1e9;
    double minimum_rise = 1e9, maximum_rise = -1e9;
    double minimum_decay = 1e9, maximum_decay = -1e9;
    double minimum_clean_amplitude = 1e9;
    double maximum_perfusion_amplitude = 0.0;
    for (unsigned int i = 0; i < first.pulse_count(); ++i)
    {
        const signal_synth::ppg_pulse_annotation& pulse = first.pulses()[i];
        const signal_synth::ppg_pulse_annotation& replay = repeated.pulses()[i];
        ok &= pulse.pulse_delay_seconds == replay.pulse_delay_seconds
            && pulse.effective_amplitude_au == replay.effective_amplitude_au
            && pulse.effective_rise_time_seconds == replay.effective_rise_time_seconds
            && pulse.effective_decay_time_seconds == replay.effective_decay_time_seconds
            && pulse.state == replay.state;
        minimum_delay = std::min(minimum_delay, pulse.pulse_delay_seconds);
        maximum_delay = std::max(maximum_delay, pulse.pulse_delay_seconds);
        minimum_rise = std::min(minimum_rise, pulse.effective_rise_time_seconds);
        maximum_rise = std::max(maximum_rise, pulse.effective_rise_time_seconds);
        minimum_decay = std::min(minimum_decay, pulse.effective_decay_time_seconds);
        maximum_decay = std::max(maximum_decay, pulse.effective_decay_time_seconds);
        if (pulse.low_perfusion)
        {
            ++low_perfusion;
            maximum_perfusion_amplitude = std::max(maximum_perfusion_amplitude, pulse.effective_amplitude_au);
        }
        else
            minimum_clean_amplitude = std::min(minimum_clean_amplitude, pulse.effective_amplitude_au);
        weak += pulse.state == signal_synth::ppg_pulse_weak ? 1u : 0u;
        missing += pulse.state == signal_synth::ppg_pulse_missing ? 1u : 0u;
        ok &= pulse.valid_for_peak_scoring == pulse.generated;
    }
    ok &= check(maximum_delay - minimum_delay > 0.020, "beat_to_beat_ptt_variation");
    ok &= check(maximum_rise - minimum_rise > 0.030 && maximum_decay - minimum_decay > 0.050, "morphology_variation");
    ok &= check(low_perfusion > 0u && weak > 0u && missing > 0u && maximum_perfusion_amplitude < minimum_clean_amplitude, "perfusion_weak_missing_states");
    signal_synth::ppg_config changed_seed_config = config;
    ++changed_seed_config.seed;
    signal_synth::ppg_record changed_seed_record;
    ok &= check(signal_synth::ppg_generator(changed_seed_config).generate(ecg, changed_seed_record)
        && changed_seed_record.pulses()[2].pulse_delay_seconds != first.pulses()[2].pulse_delay_seconds, "seed_changes_pulse_draws");

    for (unsigned int i = 0; i < first.pulse_count(); ++i)
        if (first.pulses()[i].state == signal_synth::ppg_pulse_missing)
            for (unsigned int annotation = 0; annotation < first.annotation_count(); ++annotation)
                ok &= first.annotations()[annotation].ecg_beat_index != first.pulses()[i].ecg_beat_index;
    ok &= check(ok, "missing_pulses_have_no_fabricated_fiducials");

    signal_synth::ppg_config overlap = config;
    signal_synth::ppg_perfusion_episode_config second_episode = overlap.perfusion_episodes[0];
    second_episode.start_seconds = 20.0;
    overlap.perfusion_episodes.push_back(second_episode);
    ok &= check(!signal_synth::ppg_generator(overlap).valid(), "overlapping_perfusion_rejected");

    signal_synth::ecg_scenario_document document;
    document.schema_version = 6;
    document.scenario_id = "ppg_physiology_v2";
    document.name = "PPG physiology v2";
    document.duration_seconds = 40.0;
    document.ecg.set_sampling_rate_hz(250);
    document.ppg = config;
    document.ppg.pac_pulse_amplitude_scale = 0.6;
    document.ppg.optical.enabled = true;
    document.ppg.optical.red.sensor_gain = 0.75;
    document.ppg.optical.red.dc_au = 0.12;
    document.ppg.optical.red.delay_ms = 10.0;
    document.ppg.optical.red.noise_std_au = 0.0005;
    document.ppg.optical.infrared.sensor_gain = 1.25;
    document.ppg.optical.infrared.dc_au = 0.18;
    document.ppg.optical.infrared.delay_ms = 16.0;
    document.physiology.respiration_frequency_hz = 0.22;
    document.physiology.ppg_amplitude_modulation_ratio = 0.15;
    document.wearable.ecg.enabled = true;
    document.wearable.ecg.sample_rate_hz = 250;
    signal_synth::ecg_scenario_json_result identity;
    signal_synth::ecg_scenario_document parsed;
    signal_synth::ecg_scenario_json_result repeated_identity;
    ok &= check(signal_synth::write_ecg_scenario_json(document, identity)
        && signal_synth::parse_ecg_scenario_json(identity.canonical_json, parsed, repeated_identity)
        && identity.document_fingerprint == repeated_identity.document_fingerprint
        && parsed.ppg.perfusion_episodes.size() == 1u
        && parsed.ppg.pac_pulse_amplitude_scale == 0.6
        && parsed.ppg.optical.enabled
        && parsed.ppg.optical.red.sensor_gain == 0.75
        && parsed.ppg.optical.infrared.sensor_gain == 1.25, "schema_v6_roundtrip");
    signal_synth::ecg_scenario_document changed_identity_document = document;
    ++changed_identity_document.ppg.seed;
    signal_synth::ecg_scenario_json_result changed_identity;
    ok &= check(signal_synth::write_ecg_scenario_json(changed_identity_document, changed_identity)
        && changed_identity.document_fingerprint != identity.document_fingerprint
        && changed_identity.generation_fingerprint == identity.generation_fingerprint, "ppg_seed_changes_document_identity");
    document.schema_version = 3;
    signal_synth::ecg_scenario_json_result old_schema;
    ok &= check(!signal_synth::write_ecg_scenario_json(document, old_schema), "older_schema_rejects_v2_controls");

    document.schema_version = 6;
    signal_synth::ecg_render_bundle render;
    signal_synth::ecg_document_render_result result;
    signal_synth::ecg_export_result export_result;
    ok &= check(signal_synth::render_ecg_document(document, render, result)
        && render.metrics.ppg_low_perfusion_pulse_count == low_perfusion
        && render.metrics.ppg_weak_pulse_count == weak
        && render.metrics.ppg_missing_pulse_count == missing
        && render.ppg.channel_count() == 3u, "rendered_state_metrics");
    signal_synth::ecg_scenario_document no_respiratory_amplitude = document;
    no_respiratory_amplitude.physiology.ppg_amplitude_modulation_ratio = 0.0;
    signal_synth::ecg_render_bundle no_respiratory_render;
    ok &= check(signal_synth::render_ecg_document(no_respiratory_amplitude, no_respiratory_render, result)
        && no_respiratory_render.ppg.pulse_count() == render.ppg.pulse_count(), "respiratory_reference_render");
    bool respiratory_waveform_difference = false;
    for (std::size_t i = 0; i < render.signal_quality.ppg_channels[0].size(); ++i)
        if (render.signal_quality.ppg_channels[0][i] != no_respiratory_render.signal_quality.ppg_channels[0][i])
        {
            respiratory_waveform_difference = true;
            break;
        }
    ok &= check(respiratory_waveform_difference, "respiratory_amplitude_modulation");
    signal_synth::ecg_export_bundle export_bundle;
    ok &= check(signal_synth::build_ecg_export_bundle(render, export_bundle, export_result)
        && export_bundle.find("synsigra.dat") && export_bundle.find("synsigra.edf") && export_bundle.find("synsigra.bdf")
        && export_bundle.find("waveform.csv")
        && export_bundle.find("annotations.json")
        && export_bundle.find("metadata.json")
        && export_bundle.find("wfdb_metadata.json")
        && export_bundle.find("edf_bdf_metadata.json")
        && export_bundle.find("waveform.csv")->content.find("ppg_red_au") != std::string::npos
        && export_bundle.find("waveform.csv")->content.find("ppg_infrared_au") != std::string::npos
        && export_bundle.find("annotations.json")->content.find("\"ppg_channel_fiducials\":[{") != std::string::npos
        && export_bundle.find("annotations.json")->content.find("\"channel\":\"ppg_red\"") != std::string::npos
        && export_bundle.find("metadata.json")->content.find("\"name\":\"ppg_infrared\"") != std::string::npos
        && export_bundle.find("wfdb_metadata.json")->content.find("\"name\":\"ppg_red\"") != std::string::npos
        && export_bundle.find("edf_bdf_metadata.json")->content.find("\"name\":\"ppg_infrared\"") != std::string::npos
        && export_bundle.find("annotations.json")->content.find("\"state\":\"weak\"") != std::string::npos
        && export_bundle.find("annotations.json")->content.find("\"state\":\"missing\"") != std::string::npos
        && export_bundle.find("ground_truth_metrics.json")->content.find("\"low_perfusion_pulse_count\":") != std::string::npos, "state_export_contract");
    unsigned int measured_onsets = 0, measured_peaks = 0, measured_offsets = 0;
    for (unsigned int i = 0; i < render.ppg.annotation_count(); ++i)
    {
        const signal_synth::ppg_annotation& annotation = render.ppg.annotations()[i];
        if (annotation.source == signal_synth::ppg_fiducial_measurement)
        {
            ok &= annotation.sample_index < render.signal_quality.ppg_channels[0].size()
                && annotation.value_au == render.signal_quality.ppg_channels[0][static_cast<std::size_t>(annotation.sample_index)];
            measured_onsets += annotation.kind == signal_synth::ppg_pulse_onset ? 1u : 0u;
            measured_peaks += annotation.kind == signal_synth::ppg_systolic_peak ? 1u : 0u;
            measured_offsets += annotation.kind == signal_synth::ppg_pulse_offset ? 1u : 0u;
        }
    }
    ok &= check(ok && measured_onsets == render.metrics.ppg_pulse_count
        && measured_peaks == render.metrics.ppg_pulse_count
        && measured_offsets == render.metrics.ppg_pulse_count, "final_waveform_fiducials");

    std::vector<signal_synth::ecg_detected_event> detections;
    for (unsigned int i = 0; i < render.ppg.annotation_count(); ++i)
    {
        const signal_synth::ppg_annotation& annotation = render.ppg.annotations()[i];
        if (annotation.kind == signal_synth::ppg_systolic_peak && annotation.source == signal_synth::ppg_fiducial_measurement)
        {
            signal_synth::ecg_detected_event event;
            event.time_seconds = annotation.time_seconds;
            detections.push_back(event);
        }
    }
    signal_synth::ecg_compare_options compare_options;
    compare_options.target = signal_synth::ecg_compare_ppg_systolic_peak;
    signal_synth::ecg_compare_result comparison;
    ok &= check(signal_synth::compare_detections_to_render(render, detections, compare_options, comparison)
        && comparison.total.false_positive_count == 0u && comparison.total.false_negative_count == 0u
        && comparison.low_perfusion.ground_truth_count > 0u
        && comparison.low_perfusion.true_positive_count == comparison.low_perfusion.ground_truth_count
        && comparison.weak.ground_truth_count > 0u
        && comparison.weak.true_positive_count == comparison.weak.ground_truth_count
        && comparison.missing_pulse_opportunity_count == missing
        && comparison.detections_in_missing_pulse_windows == 0u, "ppg_quality_scoring_bins");
    for (unsigned int i = 0; i < render.ppg.pulse_count(); ++i)
        if (render.ppg.pulses()[i].state == signal_synth::ppg_pulse_missing)
        {
            signal_synth::ecg_detected_event event;
            event.time_seconds = render.ppg.pulses()[i].expected_peak_time_seconds;
            detections.push_back(event);
            break;
        }
    ok &= check(signal_synth::compare_detections_to_render(render, detections, compare_options, comparison)
        && comparison.detections_in_missing_pulse_windows == 1u
        && comparison.total.false_positive_count == 1u, "missing_pulse_detection_scoring");

    signal_synth::ecg_scenario_document arrhythmia_document = document;
    arrhythmia_document.scenario_id = "ppg_arrhythmia_pulse_loss";
    arrhythmia_document.ecg.clear_conditions();
    arrhythmia_document.ecg.add_condition(signal_synth::ecg_condition_pvc, 0.8);
    arrhythmia_document.ecg.set_ectopic_every_n_beats(4);
    arrhythmia_document.ppg.perfusion_episodes.clear();
    arrhythmia_document.ppg.pac_pulse_amplitude_scale = 1.0;
    arrhythmia_document.ppg.pvc_pulse_amplitude_scale = 0.0;
    signal_synth::ecg_render_bundle arrhythmia_render;
    ok &= check(signal_synth::render_ecg_document(arrhythmia_document, arrhythmia_render, result)
        && arrhythmia_render.metrics.ppg_arrhythmia_linked_pulse_count > 0u
        && arrhythmia_render.metrics.ppg_arrhythmia_linked_missing_pulse_count == arrhythmia_render.metrics.ppg_arrhythmia_linked_pulse_count, "arrhythmia_linked_render_metrics");
    signal_synth::ecg_export_bundle arrhythmia_export;
    ok &= check(signal_synth::build_ecg_export_bundle(arrhythmia_render, arrhythmia_export, export_result)
        && arrhythmia_export.find("annotations.json")
        && arrhythmia_export.find("annotations.json")->content.find("\"arrhythmia_linked\":true") != std::string::npos
        && arrhythmia_export.find("annotations.json")->content.find("\"arrhythmia_amplitude_scale\":0") != std::string::npos
        && arrhythmia_export.find("ground_truth_metrics.json")->content.find("\"arrhythmia_linked_missing_pulse_count\":") != std::string::npos, "arrhythmia_linked_export_contract");

    if (!ok)
        return 1;
    std::cout << "ppg_physiology_test=passed\n";
    return 0;
}
