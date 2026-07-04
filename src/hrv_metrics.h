#pragma once

#include "clinical_ecg.h"
#include "signal_quality.h"

#include <string>
#include <vector>

namespace signal_synth
{
    struct hrv_rr_interval
    {
        hrv_rr_interval();

        unsigned long long beat_index;
        double beat_time_seconds;
        double rr_seconds;
        bool clipped;
        bool ectopic;
        bool artifact_overlap;
        bool excluded;
    };

    struct hrv_metric_summary
    {
        hrv_metric_summary();

        unsigned int interval_count;
        unsigned int accepted_interval_count;
        unsigned int excluded_interval_count;
        unsigned int clipped_interval_count;
        unsigned int ectopic_interval_count;
        unsigned int artifact_overlap_interval_count;
        double mean_rr_seconds;
        double mean_heart_rate_bpm;
        double sdnn_seconds;
        double rmssd_seconds;
        double pnn50_percent;
        double sd1_seconds;
        double sd2_seconds;
        double sd1_sd2_ratio;
        double lf_power_seconds2;
        double hf_power_seconds2;
        double lf_hf_ratio;
        double total_power_seconds2;
    };

    struct hrv_analysis_result
    {
        hrv_analysis_result();

        std::vector<hrv_rr_interval> intervals;
        hrv_metric_summary metrics;
        std::string metric_definition_version;
        std::string exclusion_policy;
        std::string spectral_method;
        double interpolation_rate_hz;
        double lf_low_hz;
        double lf_high_hz;
        double hf_low_hz;
        double hf_high_hz;
    };

    bool analyze_hrv_from_ecg(const clinical_ecg_record& record, const signal_quality_waveforms* signal_quality, hrv_analysis_result& output);
}
