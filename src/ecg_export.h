#pragma once

#include "clinical_ecg.h"
#include "ecg_morphology.h"
#include "ecg_scenario_json.h"
#include "ppg_model.h"

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
        unsigned int rr_clipping_count;
        double mean_rr_seconds;
        double mean_heart_rate_bpm;
        double sdnn_seconds;
        double rmssd_seconds;
        double pnn50_percent;
        unsigned int ppg_pulse_count;
        double mean_ppg_onset_delay_seconds;
        double mean_ppg_peak_delay_seconds;
    };

    struct ecg_render_bundle
    {
        ecg_scenario_document document;
        ecg_scenario_json_result document_identity;
        std::string render_identity;
        clinical_ecg_record record;
        ecg_scenario_report scenario_report;
        ecg_morphology_report morphology;
        ppg_record ppg;
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
    bool render_ecg_document(const ecg_scenario_document& document, ecg_render_bundle& output, ecg_export_result& result);
    bool build_ecg_export_bundle(const ecg_render_bundle& render, ecg_export_bundle& output, ecg_export_result& result);
}
