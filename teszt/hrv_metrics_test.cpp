#include "../src/hrv_metrics.h"

#include <cmath>
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

    bool generate_record(signal_synth::clinical_ecg_config config, double duration_seconds, signal_synth::clinical_ecg_record& record)
    {
        const unsigned int sample_count = static_cast<unsigned int>(duration_seconds * config.sampling_rate_hz);
        return signal_synth::clinical_ecg_generator(config).generate(sample_count, record);
    }

    signal_synth::clinical_ecg_config base_config()
    {
        signal_synth::clinical_ecg_config config;
        config.sampling_rate_hz = 100;
        config.rhythm.heart_rate_bpm = 60.0;
        config.rhythm.minimum_rr_seconds = 0.35;
        config.rhythm.maximum_rr_seconds = 2.0;
        config.rhythm.seed = 1234567;
        return config;
    }

    signal_synth::signal_quality_waveforms artifact_window()
    {
        signal_synth::signal_quality_waveforms waveforms;
        signal_synth::signal_quality_artifact_interval artifact;
        artifact.type = signal_synth::signal_quality_ecg_baseline_wander;
        artifact.start_seconds = 2.0;
        artifact.end_seconds = 5.0;
        artifact.start_sample_index = 200;
        artifact.end_sample_index = 500;
        artifact.severity = 0.8;
        artifact.seed = 42;
        for (unsigned int lead = 0; lead < signal_synth::clinical_lead_count; ++lead)
            artifact.ecg_leads[lead] = false;
        artifact.ecg_leads[signal_synth::clinical_lead_ii] = true;
        artifact.ppg = false;
        waveforms.artifacts.push_back(artifact);
        return waveforms;
    }
}

int main()
{
    bool ok = true;

    signal_synth::clinical_ecg_config variable_config = base_config();
    variable_config.rhythm.rr_variability_seconds = 0.045;
    signal_synth::clinical_ecg_record variable_record;
    signal_synth::hrv_analysis_result variable_hrv;
    ok &= check(generate_record(variable_config, 300.0, variable_record) && signal_synth::analyze_hrv_from_ecg(variable_record, 0, variable_hrv), "variable_record_analysis");
    ok &= check(variable_hrv.metrics.interval_count == variable_record.beat_count() && variable_hrv.metrics.accepted_interval_count == variable_hrv.metrics.interval_count && variable_hrv.metrics.excluded_interval_count == 0, "clean_accepts_all_intervals");
    ok &= check(variable_hrv.metrics.sdnn_seconds > 0.0 && variable_hrv.metrics.rmssd_seconds > 0.0 && variable_hrv.metrics.sd2_seconds > 0.0, "time_domain_metrics_nonzero");
    ok &= check(variable_hrv.metrics.total_power_seconds2 > 0.0 && variable_hrv.metrics.vlf_power_seconds2 >= 0.0 && variable_hrv.metrics.lf_power_seconds2 >= 0.0 && variable_hrv.metrics.hf_power_seconds2 >= 0.0, "frequency_domain_metrics_present");
    ok &= check(std::fabs(variable_hrv.metrics.lf_normalized_units + variable_hrv.metrics.hf_normalized_units - 100.0) < 1e-9, "normalized_frequency_power");
    ok &= check(variable_hrv.spectral_method.find("VLF 0.0033-0.04 Hz") != std::string::npos && variable_hrv.metric_definition_version == "synsigra_hrv_metrics_v2", "method_metadata");

    signal_synth::clinical_ecg_config constant_config = base_config();
    signal_synth::clinical_ecg_record constant_record;
    signal_synth::hrv_analysis_result constant_hrv;
    ok &= check(generate_record(constant_config, 20.0, constant_record) && signal_synth::analyze_hrv_from_ecg(constant_record, 0, constant_hrv), "constant_record_analysis");
    ok &= check(constant_hrv.metrics.sdnn_seconds == 0.0 && constant_hrv.metrics.rmssd_seconds == 0.0 && constant_hrv.metrics.sd1_seconds == 0.0 && constant_hrv.metrics.sd2_seconds == 0.0, "constant_rr_metrics_zero");

    signal_synth::clinical_ecg_config clipped_config = base_config();
    clipped_config.rhythm.rr_variability_seconds = 0.5;
    clipped_config.rhythm.minimum_rr_seconds = 0.9;
    clipped_config.rhythm.maximum_rr_seconds = 1.1;
    signal_synth::clinical_ecg_record clipped_record;
    signal_synth::hrv_analysis_result clipped_hrv;
    ok &= check(generate_record(clipped_config, 60.0, clipped_record) && signal_synth::analyze_hrv_from_ecg(clipped_record, 0, clipped_hrv), "clipped_record_analysis");
    ok &= check(clipped_hrv.metrics.clipped_interval_count > 0 && clipped_hrv.metrics.excluded_interval_count >= clipped_hrv.metrics.clipped_interval_count, "clipped_intervals_excluded");

    signal_synth::clinical_ecg_config ectopic_config = base_config();
    ectopic_config.scenario.premature_every_n_beats = 5;
    ectopic_config.scenario.premature_origin = signal_synth::clinical_origin_pvc;
    ectopic_config.scenario.premature_coupling_ratio = 0.65;
    signal_synth::clinical_ecg_record ectopic_record;
    signal_synth::hrv_analysis_result ectopic_hrv;
    ok &= check(generate_record(ectopic_config, 60.0, ectopic_record) && signal_synth::analyze_hrv_from_ecg(ectopic_record, 0, ectopic_hrv), "ectopic_record_analysis");
    ok &= check(ectopic_hrv.metrics.ectopic_interval_count > 0 && ectopic_hrv.metrics.excluded_interval_count >= ectopic_hrv.metrics.ectopic_interval_count, "ectopic_intervals_excluded");

    signal_synth::signal_quality_waveforms artifacts = artifact_window();
    signal_synth::hrv_analysis_result artifact_hrv;
    ok &= check(signal_synth::analyze_hrv_from_ecg(constant_record, &artifacts, artifact_hrv), "artifact_analysis");
    ok &= check(artifact_hrv.metrics.artifact_overlap_interval_count > 0 && artifact_hrv.metrics.excluded_interval_count >= artifact_hrv.metrics.artifact_overlap_interval_count, "artifact_overlap_intervals_excluded");

    std::vector<signal_synth::hrv_rr_interval> separated(4);
    separated[0].rr_seconds = 1.0;
    separated[1].rr_seconds = 1.1;
    separated[1].excluded = true;
    separated[2].rr_seconds = 1.2;
    separated[3].rr_seconds = 1.3;
    signal_synth::hrv_analysis_result separated_hrv;
    ok &= check(signal_synth::analyze_variability_intervals(separated, "test_hrv_v2", "exclude marked intervals", separated_hrv), "separated_intervals_analysis");
    ok &= check(std::fabs(separated_hrv.metrics.rmssd_seconds - 0.1) < 1e-12 && std::fabs(separated_hrv.metrics.pnn50_percent - 100.0) < 1e-12, "excluded_gap_not_bridged");

    signal_synth::clinical_ecg_record empty_record;
    signal_synth::hrv_analysis_result empty_hrv;
    ok &= check(signal_synth::analyze_hrv_from_ecg(empty_record, 0, empty_hrv) && empty_hrv.metrics.interval_count == 0, "empty_record_analysis");

    return ok ? 0 : 1;
}
