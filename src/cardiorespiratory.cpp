#include "cardiorespiratory.h"

#include "ecg_scenario_json.h"
#include "ppg_model.h"
#include "scenario_stress.h"
#include "signal_quality.h"

#include <algorithm>
#include <cmath>

namespace
{
    bool intervals_overlap(double first_start, double first_end, double second_start, double second_end)
    {
        return first_start < second_end && second_start < first_end;
    }

    bool ppg_artifact_overlap(const signal_synth::signal_quality_waveforms* signal_quality, double start, double end)
    {
        if (!signal_quality) return false;
        for (std::size_t i = 0; i < signal_quality->artifacts.size(); ++i)
            if (signal_quality->artifacts[i].ppg && intervals_overlap(start, end, signal_quality->artifacts[i].start_seconds, signal_quality->artifacts[i].end_seconds)) return true;
        return false;
    }

    const signal_synth::ppg_annotation* measured_peak(const signal_synth::ppg_record& ppg, unsigned long long beat_index)
    {
        const signal_synth::ppg_annotation* annotations = ppg.annotations();
        for (unsigned int i = 0; annotations && i < ppg.annotation_count(); ++i)
            if (annotations[i].ecg_beat_index == beat_index && annotations[i].kind == signal_synth::ppg_systolic_peak && annotations[i].source == signal_synth::ppg_fiducial_measurement) return annotations + i;
        return 0;
    }

    bool respiration_enabled(const signal_synth::physiology_coupling_config& config)
    {
        return config.respiratory_rr_amplitude_seconds > 0.0 || config.ecg_baseline_amplitude_mv > 0.0 || config.ppg_amplitude_modulation_ratio > 0.0 || config.ppg_delay_modulation_ms > 0.0 || config.accelerometer_respiration_amplitude_g > 0.0;
    }
}

namespace signal_synth
{
    respiratory_reference_sample::respiratory_reference_sample() : time_seconds(0.0), phase_radians(0.0), waveform(0.0), respiratory_rate_bpm(0.0) {}

    cardiorespiratory_analysis_result::cardiorespiratory_analysis_result()
        : prv_available(false), respiration_available(false), respiration_sample_rate_hz(10u), respiration_phase_radians(0.0), respiratory_rate_bpm(0.0), prv(), respiration() {}

    bool analyze_cardiorespiratory(double duration_seconds, const physiology_coupling_config& physiology, const ppg_record& ppg, const signal_quality_waveforms* signal_quality, cardiorespiratory_analysis_result& output)
    {
        cardiorespiratory_analysis_result fresh;
        if (!std::isfinite(duration_seconds) || duration_seconds <= 0.0) return false;
        try
        {
            if (ppg.sample_count() && ppg.pulses())
            {
                std::vector<hrv_rr_interval> intervals;
                for (unsigned int i = 1u; i < ppg.pulse_count(); ++i)
                {
                    const ppg_pulse_annotation& previous = ppg.pulses()[i - 1u];
                    const ppg_pulse_annotation& current = ppg.pulses()[i];
                    const ppg_annotation* previous_peak = measured_peak(ppg, previous.ecg_beat_index);
                    const ppg_annotation* current_peak = measured_peak(ppg, current.ecg_beat_index);
                    hrv_rr_interval interval;
                    interval.beat_index = current.ecg_beat_index;
                    interval.beat_time_seconds = current_peak ? current_peak->time_seconds : current.expected_peak_time_seconds;
                    interval.rr_seconds = previous_peak && current_peak ? current_peak->time_seconds - previous_peak->time_seconds : 0.0;
                    interval.ectopic = previous.arrhythmia_linked || current.arrhythmia_linked;
                    const double start = previous_peak ? previous_peak->time_seconds : previous.expected_peak_time_seconds;
                    const double end = current_peak ? current_peak->time_seconds : current.expected_peak_time_seconds;
                    interval.artifact_overlap = end > start && ppg_artifact_overlap(signal_quality, start, end);
                    interval.excluded = !previous_peak || !current_peak || !previous.valid_for_peak_scoring || !current.valid_for_peak_scoring || previous.low_perfusion || current.low_perfusion || interval.ectopic || interval.artifact_overlap || interval.rr_seconds <= 0.0;
                    intervals.push_back(interval);
                }
                if (!analyze_variability_intervals(intervals, "synsigra_prv_metrics_v1", "exclude missing, weak/low-perfusion, arrhythmia-linked, nonpositive, and PPG-artifact-overlapped pulse intervals", fresh.prv)) return false;
                fresh.prv_available = true;
            }
            fresh.respiration_available = respiration_enabled(physiology);
            if (fresh.respiration_available)
            {
                fresh.respiration_phase_radians = physiology_respiration_phase_radians(physiology);
                fresh.respiratory_rate_bpm = 60.0 * physiology.respiration_frequency_hz;
                const unsigned int count = static_cast<unsigned int>(std::floor(duration_seconds * fresh.respiration_sample_rate_hz)) + 1u;
                fresh.respiration.reserve(count);
                for (unsigned int sample = 0; sample < count; ++sample)
                {
                    respiratory_reference_sample item;
                    item.time_seconds = std::min(duration_seconds, static_cast<double>(sample) / fresh.respiration_sample_rate_hz);
                    item.phase_radians = 2.0 * 3.14159265358979323846 * physiology.respiration_frequency_hz * item.time_seconds + fresh.respiration_phase_radians;
                    item.waveform = physiology_respiration_value(physiology, item.time_seconds);
                    item.respiratory_rate_bpm = fresh.respiratory_rate_bpm;
                    fresh.respiration.push_back(item);
                }
            }
        }
        catch (...)
        {
            return false;
        }
        output = fresh;
        return true;
    }
}
