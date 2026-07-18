#pragma once

#include "clinical_ecg.h"
#include "ppg_model.h"

#include <vector>

namespace signal_synth
{
    enum signal_quality_artifact_type
    {
        signal_quality_ecg_baseline_wander = 0,
        signal_quality_ecg_powerline = 1,
        signal_quality_ecg_emg_noise = 2,
        signal_quality_ecg_dropout = 3,
        signal_quality_ecg_saturation = 4,
        signal_quality_ppg_dropout = 5,
        signal_quality_ecg_lead_reversal = 6,
        signal_quality_ecg_lead_swap = 7,
        signal_quality_ecg_electrode_misplacement = 8,
        signal_quality_ecg_gain_mismatch = 9,
        signal_quality_ecg_offset_drift = 10,
        signal_quality_ecg_clock_drift = 11,
        signal_quality_ecg_dropped_samples = 12,
        signal_quality_ecg_quantization = 13,
        signal_quality_ecg_adc_clipping = 14,
        signal_quality_ppg_motion_periodic = 15,
        signal_quality_ppg_motion_burst = 16,
        signal_quality_ppg_motion_broadband = 17,
        signal_quality_ppg_ambient_light = 18,
        signal_quality_ppg_sensor_saturation = 19,
        signal_quality_ecg_external_noise = 20
    };

    const char* signal_quality_artifact_type_name(signal_quality_artifact_type type);
    bool signal_quality_artifact_is_ppg(signal_quality_artifact_type type);
    bool signal_quality_artifact_is_motion(signal_quality_artifact_type type);

    struct signal_quality_artifact_config
    {
        signal_quality_artifact_config();

        signal_quality_artifact_type type;
        double start_seconds;
        double duration_seconds;
        double severity;
        unsigned long long seed;
        bool ecg_leads[clinical_lead_count];
        bool ppg;

        bool affects_ecg() const;
        bool affects_ppg() const;
    };

    struct signal_quality_config
    {
        std::vector<signal_quality_artifact_config> artifacts;
    };

    struct signal_quality_artifact_interval
    {
        signal_quality_artifact_interval();

        signal_quality_artifact_type type;
        double start_seconds;
        double end_seconds;
        unsigned long long start_sample_index;
        unsigned long long end_sample_index;
        double severity;
        unsigned long long seed;
        bool ecg_leads[clinical_lead_count];
        bool ppg;
        bool accelerometer_reference;
    };

    struct signal_quality_waveforms
    {
        std::vector<std::vector<double> > ecg_leads;
        std::vector<std::vector<double> > ppg_channels;
        std::vector<double> accelerometer;
        std::vector<signal_quality_artifact_interval> artifacts;
    };

    bool validate_signal_quality_config(const signal_quality_config& config, double duration_seconds, unsigned int sampling_rate_hz, bool ppg_enabled);
    bool initialize_signal_quality_waveforms(const clinical_ecg_record& ecg, const ppg_record& ppg, signal_quality_waveforms& output);
    bool apply_signal_quality_artifacts_in_place(const signal_quality_config& config, const clinical_ecg_record& ecg, const ppg_record& ppg, signal_quality_waveforms& output);
    bool apply_signal_quality_artifacts(const signal_quality_config& config, const clinical_ecg_record& ecg, const ppg_record& ppg, signal_quality_waveforms& output);
    bool finalize_ppg_sensor(const ppg_record& ppg, signal_quality_waveforms& waveforms, std::vector<unsigned long long>& clipping_counts);
}
