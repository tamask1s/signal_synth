#pragma once

#include "clinical_ecg.h"

#include <string>

namespace signal_synth
{
    struct ecg_render_bundle;

    struct truth_event_scoreability
    {
        truth_event_scoreability();

        bool scoreable;
        bool complete_support;
        double retained_signal_fraction;
        std::string exclusion_reason;
    };

    truth_event_scoreability assess_ecg_qrs_scoreability(const ecg_render_bundle& render, const clinical_beat_annotation& beat);
    truth_event_scoreability assess_ppg_event_scoreability(const ecg_render_bundle& render, double time_seconds);
}
