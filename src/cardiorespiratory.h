#pragma once

#include "hrv_metrics.h"

#include <vector>

namespace signal_synth
{
    struct physiology_coupling_config;
    class ppg_record;
    struct signal_quality_waveforms;

    struct respiratory_reference_sample
    {
        respiratory_reference_sample();

        double time_seconds;
        double phase_radians;
        double waveform;
        double respiratory_rate_bpm;
    };

    struct cardiorespiratory_analysis_result
    {
        cardiorespiratory_analysis_result();

        bool prv_available;
        bool respiration_available;
        unsigned int respiration_sample_rate_hz;
        double respiration_phase_radians;
        double respiratory_rate_bpm;
        hrv_analysis_result prv;
        std::vector<respiratory_reference_sample> respiration;
    };

    bool analyze_cardiorespiratory(double duration_seconds, const physiology_coupling_config& physiology, const ppg_record& ppg, const signal_quality_waveforms* signal_quality, cardiorespiratory_analysis_result& output);
}
