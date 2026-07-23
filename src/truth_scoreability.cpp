#include "truth_scoreability.h"

#include "ecg_render.h"

#include <algorithm>
#include <cmath>

namespace
{
    const double pi = 3.141592653589793238462643383279502884;
    const double minimum_scoreable_retained_fraction = 0.05;

    bool all_ecg_leads(const signal_synth::signal_quality_artifact_interval& artifact)
    {
        for (unsigned int lead = 0; lead < signal_synth::clinical_lead_count; ++lead)
            if (!artifact.ecg_leads[lead]) return false;
        return true;
    }

    double artifact_envelope(const signal_synth::signal_quality_artifact_interval& artifact, double time_seconds, unsigned int sampling_rate_hz)
    {
        if (!sampling_rate_hz || time_seconds < artifact.start_seconds || time_seconds >= artifact.end_seconds)
            return 0.0;
        const unsigned long long first = artifact.start_sample_index;
        const unsigned long long past = artifact.end_sample_index + 1u;
        if (past <= first)
            return 0.0;
        long long rounded = static_cast<long long>(std::llround(time_seconds * sampling_rate_hz));
        if (rounded < static_cast<long long>(first)) rounded = static_cast<long long>(first);
        if (rounded >= static_cast<long long>(past)) rounded = static_cast<long long>(past - 1u);
        const unsigned long long index = static_cast<unsigned long long>(rounded);
        if (past <= first + 1u)
            return 1.0;
        const unsigned long long length = past - first;
        unsigned long long taper = length / 20u;
        if (taper < 1u) taper = 1u;
        if (taper > length / 2u) taper = length / 2u;
        const unsigned long long edge = std::min(index - first, past - 1u - index);
        if (edge >= taper)
            return 1.0;
        const double phase = static_cast<double>(edge) / static_cast<double>(taper);
        return 0.5 - 0.5 * std::cos(pi * phase);
    }
}

namespace signal_synth
{
    truth_event_scoreability::truth_event_scoreability()
        : scoreable(true), complete_support(true), retained_signal_fraction(1.0), exclusion_reason()
    {
    }

    truth_event_scoreability assess_ecg_qrs_scoreability(const ecg_render_bundle& render, const clinical_beat_annotation& beat)
    {
        truth_event_scoreability result;
        const double record_end = render.record.sampling_rate_hz()
            ? static_cast<double>(render.record.sample_count()) / render.record.sampling_rate_hz() : 0.0;
        if (beat.qrs_onset_time_seconds < 0.0 || beat.qrs_offset_time_seconds > record_end)
        {
            result.scoreable = false;
            result.complete_support = false;
            result.exclusion_reason = "record_boundary_truncated_qrs";
            return result;
        }
        for (std::size_t i = 0; i < render.signal_quality.artifacts.size(); ++i)
        {
            const signal_quality_artifact_interval& artifact = render.signal_quality.artifacts[i];
            if (artifact.type != signal_quality_ecg_dropout || !all_ecg_leads(artifact))
                continue;
            const double weight = artifact_envelope(artifact, beat.r_peak_time_seconds, render.record.sampling_rate_hz());
            if (weight <= 0.0)
                continue;
            const double retained = std::max(0.0, 1.0 - weight * artifact.severity * 0.98);
            result.retained_signal_fraction = std::min(result.retained_signal_fraction, retained);
        }
        if (result.retained_signal_fraction <= minimum_scoreable_retained_fraction)
        {
            result.scoreable = false;
            result.exclusion_reason = "near_total_all_lead_ecg_dropout";
        }
        return result;
    }

    truth_event_scoreability assess_ppg_event_scoreability(const ecg_render_bundle& render, double time_seconds)
    {
        truth_event_scoreability result;
        for (std::size_t i = 0; i < render.signal_quality.artifacts.size(); ++i)
        {
            const signal_quality_artifact_interval& artifact = render.signal_quality.artifacts[i];
            if (artifact.type != signal_quality_ppg_dropout || !artifact.ppg)
                continue;
            const double weight = artifact_envelope(artifact, time_seconds, render.record.sampling_rate_hz());
            if (weight <= 0.0)
                continue;
            const double retained = std::max(0.0, 1.0 - weight * artifact.severity);
            result.retained_signal_fraction = std::min(result.retained_signal_fraction, retained);
        }
        if (result.retained_signal_fraction <= minimum_scoreable_retained_fraction)
        {
            result.scoreable = false;
            result.exclusion_reason = "near_total_ppg_dropout";
        }
        return result;
    }
}
