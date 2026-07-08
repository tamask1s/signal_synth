#pragma once

#include "clinical_ecg.h"
#include "ecg_morphology.h"
#include "ecg_scenario_json.h"
#include "hrv_metrics.h"
#include "ppg_model.h"
#include "signal_quality.h"
#include "scenario_stress.h"

#include <string>
#include <vector>

namespace signal_synth
{
    struct ecg_ground_truth_metrics
    {
        ecg_ground_truth_metrics();

        unsigned int beat_count;
        unsigned int atrial_event_count;
        unsigned int fiducial_count;
        unsigned int episode_count;
        unsigned int artifact_count;
        unsigned int rr_clipping_count;
        double mean_rr_seconds;
        double mean_heart_rate_bpm;
        double sdnn_seconds;
        double rmssd_seconds;
        double pnn50_percent;
        unsigned int hrv_accepted_interval_count;
        unsigned int hrv_excluded_interval_count;
        unsigned int hrv_ectopic_interval_count;
        unsigned int hrv_artifact_overlap_interval_count;
        double sd1_seconds;
        double sd2_seconds;
        double sd1_sd2_ratio;
        double lf_power_seconds2;
        double hf_power_seconds2;
        double lf_hf_ratio;
        double total_power_seconds2;
        unsigned int ppg_pulse_count;
        unsigned int ppg_expected_pulse_count;
        unsigned int ppg_missing_pulse_count;
        unsigned int ppg_weak_pulse_count;
        unsigned int ppg_low_perfusion_pulse_count;
        unsigned int ppg_arrhythmia_linked_pulse_count;
        unsigned int ppg_arrhythmia_linked_missing_pulse_count;
        unsigned int ppg_out_of_record_pulse_count;
        double mean_ppg_onset_delay_seconds;
        double mean_ppg_peak_delay_seconds;
        double total_artifact_seconds;
        double ecg_artifact_seconds[clinical_lead_count];
        double ppg_artifact_seconds;
    };

    struct ecg_render_bundle
    {
        ecg_scenario_document document;
        ecg_scenario_json_result document_identity;
        ecg_scenario_document resolved_document;
        ecg_scenario_json_result resolved_document_identity;
        std::vector<scenario_parameter_draw> parameter_draws;
        std::string render_identity;
        clinical_ecg_record record;
        ecg_scenario_report scenario_report;
        ecg_morphology_report morphology;
        ppg_record ppg;
        signal_quality_waveforms signal_quality;
        hrv_analysis_result hrv;
        ecg_ground_truth_metrics metrics;
    };

    struct ecg_text_artifact
    {
        std::string name;
        std::string media_type;
        std::string content;
    };

    struct ecg_export_bundle
    {
        std::vector<ecg_text_artifact> artifacts;

        const ecg_text_artifact* find(const std::string& name) const;
    };

    struct ecg_export_result
    {
        ecg_export_result();

        bool success;
        std::vector<std::string> messages;
    };

    const char* signal_synth_generator_version();
    const char* signal_synth_generator_git_commit();
    const char* signal_synth_build_identity();
    const char* signal_synth_package_contract_version();
    const char* signal_synth_scoring_manifest_contract_version();
    const char* signal_synth_verifier_version();
    const char* signal_synth_engineering_claim_boundary_text();
    bool render_ecg_document(const ecg_scenario_document& document, ecg_render_bundle& output, ecg_export_result& result);
    bool build_ecg_export_bundle(const ecg_render_bundle& render, ecg_export_bundle& output, ecg_export_result& result);
}
