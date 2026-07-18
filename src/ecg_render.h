#pragma once

#include "cardiorespiratory.h"
#include "clinical_ecg.h"
#include "ecg_morphology.h"
#include "external_noise.h"
#include "ecg_scenario_json.h"
#include "hrv_metrics.h"
#include "ppg_model.h"
#include "scenario_stress.h"
#include "signal_quality.h"
#include "wearable_timebase.h"

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
        std::vector<std::vector<double> > external_noise_clean_ecg_leads;
        external_noise_result external_noise;
        std::vector<unsigned long long> ppg_clipping_counts;
        wearable_timebase_record wearable;
        hrv_analysis_result hrv;
        cardiorespiratory_analysis_result cardiorespiratory;
        ecg_ground_truth_metrics metrics;
    };

    struct ecg_document_render_result
    {
        ecg_document_render_result();

        bool success;
        std::vector<std::string> messages;
    };

    bool render_ecg_document(const ecg_scenario_document& document, ecg_render_bundle& output, ecg_document_render_result& result);
    bool render_ecg_document(const ecg_scenario_document& document, const std::vector<external_noise_asset_input>& external_noise_assets, ecg_render_bundle& output, ecg_document_render_result& result);
}
