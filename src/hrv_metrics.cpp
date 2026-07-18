#include "hrv_metrics.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
    const double PI = 3.141592653589793238462643383279502884;

    double normalized_zero(double value)
    {
        return std::fabs(value) < 1e-15 ? 0.0 : value;
    }

    bool finite_positive(double value)
    {
        return std::isfinite(value) && value > 0.0;
    }

    bool any_ecg_channel(const signal_synth::signal_quality_artifact_interval& artifact)
    {
        for (unsigned int lead = 0; lead < signal_synth::clinical_lead_count; ++lead)
            if (artifact.ecg_leads[lead])
                return true;
        return false;
    }

    bool intervals_overlap(double first_start, double first_end, double second_start, double second_end)
    {
        return first_start < second_end && second_start < first_end;
    }

    bool artifact_overlaps_rr(const signal_synth::signal_quality_waveforms* signal_quality, double start_seconds, double end_seconds)
    {
        if (!signal_quality)
            return false;
        for (std::size_t i = 0; i < signal_quality->artifacts.size(); ++i)
        {
            const signal_synth::signal_quality_artifact_interval& artifact = signal_quality->artifacts[i];
            if (any_ecg_channel(artifact) && intervals_overlap(start_seconds, end_seconds, artifact.start_seconds, artifact.end_seconds))
                return true;
        }
        return false;
    }

    void compute_time_metrics(signal_synth::hrv_analysis_result& analysis)
    {
        std::vector<double> rr;
        rr.reserve(analysis.intervals.size());
        signal_synth::hrv_metric_summary& metrics = analysis.metrics;
        metrics.interval_count = static_cast<unsigned int>(analysis.intervals.size());
        for (std::size_t i = 0; i < analysis.intervals.size(); ++i)
        {
            const signal_synth::hrv_rr_interval& interval = analysis.intervals[i];
            metrics.clipped_interval_count += interval.clipped ? 1u : 0u;
            metrics.ectopic_interval_count += interval.ectopic ? 1u : 0u;
            metrics.artifact_overlap_interval_count += interval.artifact_overlap ? 1u : 0u;
            if (interval.excluded)
                ++metrics.excluded_interval_count;
            else
                rr.push_back(interval.rr_seconds);
        }
        metrics.accepted_interval_count = static_cast<unsigned int>(rr.size());
        if (rr.empty())
            return;

        double sum = 0.0;
        for (std::size_t i = 0; i < rr.size(); ++i)
            sum += rr[i];
        metrics.mean_rr_seconds = sum / rr.size();
        metrics.mean_heart_rate_bpm = metrics.mean_rr_seconds > 0.0 ? 60.0 / metrics.mean_rr_seconds : 0.0;

        double variance = 0.0;
        for (std::size_t i = 0; i < rr.size(); ++i)
        {
            const double difference = rr[i] - metrics.mean_rr_seconds;
            variance += difference * difference;
        }
        metrics.sdnn_seconds = rr.size() > 1u ? normalized_zero(std::sqrt(variance / (rr.size() - 1u))) : 0.0;

        if (rr.size() < 2u)
            return;

        std::vector<double> successive;
        successive.reserve(analysis.intervals.size() - 1u);
        double successive_squared_sum = 0.0;
        unsigned int nn50 = 0;
        for (std::size_t i = 1; i < analysis.intervals.size(); ++i)
        {
            if (analysis.intervals[i - 1u].excluded || analysis.intervals[i].excluded)
                continue;
            const double difference = analysis.intervals[i].rr_seconds - analysis.intervals[i - 1u].rr_seconds;
            successive.push_back(difference);
            successive_squared_sum += difference * difference;
            nn50 += std::fabs(difference) > 0.05 ? 1u : 0u;
        }
        if (successive.empty())
            return;
        metrics.rmssd_seconds = normalized_zero(std::sqrt(successive_squared_sum / successive.size()));
        metrics.pnn50_percent = 100.0 * nn50 / successive.size();

        double successive_mean = 0.0;
        for (std::size_t i = 0; i < successive.size(); ++i)
            successive_mean += successive[i];
        successive_mean /= successive.size();
        double successive_variance = 0.0;
        for (std::size_t i = 0; i < successive.size(); ++i)
        {
            const double centered = successive[i] - successive_mean;
            successive_variance += centered * centered;
        }
        successive_variance = successive.size() > 1u ? successive_variance / (successive.size() - 1u) : 0.0;
        metrics.sd1_seconds = normalized_zero(std::sqrt(0.5 * successive_variance));
        const double sd2_squared = std::max(0.0, 2.0 * metrics.sdnn_seconds * metrics.sdnn_seconds - metrics.sd1_seconds * metrics.sd1_seconds);
        metrics.sd2_seconds = normalized_zero(std::sqrt(sd2_squared));
        metrics.sd1_sd2_ratio = metrics.sd2_seconds > 0.0 ? metrics.sd1_seconds / metrics.sd2_seconds : 0.0;
    }

    void compute_frequency_metrics(signal_synth::hrv_analysis_result& analysis)
    {
        std::vector<double> times;
        std::vector<double> values;
        times.reserve(analysis.intervals.size());
        values.reserve(analysis.intervals.size());
        for (std::size_t i = 0; i < analysis.intervals.size(); ++i)
        {
            const signal_synth::hrv_rr_interval& interval = analysis.intervals[i];
            if (!interval.excluded)
            {
                times.push_back(interval.beat_time_seconds);
                values.push_back(interval.rr_seconds);
            }
        }
        if (times.size() < 8u || times.back() <= times.front())
            return;

        const double duration = times.back() - times.front();
        const double interpolation_rate = analysis.interpolation_rate_hz;
        if (!finite_positive(interpolation_rate) || duration < 60.0)
            return;

        const unsigned int sample_count = static_cast<unsigned int>(std::floor(duration * interpolation_rate)) + 1u;
        if (sample_count < 8u)
            return;

        std::vector<double> sampled(sample_count, 0.0);
        std::size_t segment = 0;
        double mean = 0.0;
        for (unsigned int i = 0; i < sample_count; ++i)
        {
            const double time = times.front() + static_cast<double>(i) / interpolation_rate;
            while (segment + 1u < times.size() && times[segment + 1u] < time)
                ++segment;
            double value = values[segment];
            if (segment + 1u < times.size() && times[segment + 1u] > times[segment])
            {
                const double alpha = (time - times[segment]) / (times[segment + 1u] - times[segment]);
                value = values[segment] + alpha * (values[segment + 1u] - values[segment]);
            }
            sampled[i] = value;
            mean += value;
        }
        mean /= sample_count;

        double window_power = 0.0;
        for (unsigned int i = 0; i < sample_count; ++i)
        {
            const double window = sample_count > 1u ? 0.5 - 0.5 * std::cos(2.0 * PI * i / (sample_count - 1u)) : 1.0;
            sampled[i] = (sampled[i] - mean) * window;
            window_power += window * window;
        }
        if (window_power <= 0.0)
            return;

        const double bin_width = interpolation_rate / sample_count;
        const unsigned int first_bin = std::max(1u, static_cast<unsigned int>(std::ceil(analysis.vlf_low_hz / bin_width)));
        const unsigned int last_bin = std::min(sample_count / 2u, static_cast<unsigned int>(std::floor(analysis.hf_high_hz / bin_width)));
        if (last_bin < first_bin)
            return;

        for (unsigned int bin = first_bin; bin <= last_bin; ++bin)
        {
            const double frequency = bin * bin_width;
            double real = 0.0;
            double imag = 0.0;
            for (unsigned int n = 0; n < sample_count; ++n)
            {
                const double angle = 2.0 * PI * bin * n / sample_count;
                real += sampled[n] * std::cos(angle);
                imag -= sampled[n] * std::sin(angle);
            }
            const double power = 2.0 * (real * real + imag * imag) / (interpolation_rate * window_power);
            const double band_power = power * bin_width;
            if (frequency >= analysis.vlf_low_hz && frequency < analysis.vlf_high_hz)
                analysis.metrics.vlf_power_seconds2 += band_power;
            if (frequency >= analysis.lf_low_hz && frequency < analysis.lf_high_hz)
                analysis.metrics.lf_power_seconds2 += band_power;
            if (frequency >= analysis.hf_low_hz && frequency <= analysis.hf_high_hz)
                analysis.metrics.hf_power_seconds2 += band_power;
            if (frequency >= analysis.vlf_low_hz && frequency <= analysis.hf_high_hz)
                analysis.metrics.total_power_seconds2 += band_power;
        }
        analysis.metrics.vlf_power_seconds2 = normalized_zero(analysis.metrics.vlf_power_seconds2);
        analysis.metrics.lf_power_seconds2 = normalized_zero(analysis.metrics.lf_power_seconds2);
        analysis.metrics.hf_power_seconds2 = normalized_zero(analysis.metrics.hf_power_seconds2);
        analysis.metrics.total_power_seconds2 = normalized_zero(analysis.metrics.total_power_seconds2);
        analysis.metrics.lf_hf_ratio = analysis.metrics.hf_power_seconds2 > 0.0 ? analysis.metrics.lf_power_seconds2 / analysis.metrics.hf_power_seconds2 : 0.0;
        const double normalized_power = analysis.metrics.lf_power_seconds2 + analysis.metrics.hf_power_seconds2;
        if (normalized_power > 0.0)
        {
            analysis.metrics.lf_normalized_units = 100.0 * analysis.metrics.lf_power_seconds2 / normalized_power;
            analysis.metrics.hf_normalized_units = 100.0 * analysis.metrics.hf_power_seconds2 / normalized_power;
        }
    }
}

namespace signal_synth
{
    hrv_rr_interval::hrv_rr_interval()
        : beat_index(0), beat_time_seconds(0.0), rr_seconds(0.0), clipped(false), ectopic(false), artifact_overlap(false), excluded(false)
    {
    }

    hrv_metric_summary::hrv_metric_summary()
        : interval_count(0), accepted_interval_count(0), excluded_interval_count(0), clipped_interval_count(0), ectopic_interval_count(0), artifact_overlap_interval_count(0), mean_rr_seconds(0.0), mean_heart_rate_bpm(0.0), sdnn_seconds(0.0), rmssd_seconds(0.0), pnn50_percent(0.0), sd1_seconds(0.0), sd2_seconds(0.0), sd1_sd2_ratio(0.0), vlf_power_seconds2(0.0), lf_power_seconds2(0.0), hf_power_seconds2(0.0), lf_hf_ratio(0.0), lf_normalized_units(0.0), hf_normalized_units(0.0), total_power_seconds2(0.0)
    {
    }

    hrv_analysis_result::hrv_analysis_result()
        : metric_definition_version("synsigra_hrv_metrics_v2"), exclusion_policy("exclude clipped, ectopic or ectopic-adjacent, missing-QRS, nonpositive, and ECG-artifact-overlapped RR intervals"), spectral_method("linear interpolation to 4 Hz, mean removal, Hann window, direct deterministic periodogram, VLF 0.0033-0.04 Hz, LF 0.04-0.15 Hz, HF 0.15-0.40 Hz"), interpolation_rate_hz(4.0), vlf_low_hz(0.0033), vlf_high_hz(0.04), lf_low_hz(0.04), lf_high_hz(0.15), hf_low_hz(0.15), hf_high_hz(0.40)
    {
    }

    bool analyze_variability_intervals(const std::vector<hrv_rr_interval>& intervals, const std::string& definition_version, const std::string& exclusion_policy, hrv_analysis_result& output)
    {
        hrv_analysis_result fresh;
        if (definition_version.empty() || exclusion_policy.empty()) return false;
        try
        {
            fresh.intervals = intervals;
            fresh.metric_definition_version = definition_version;
            fresh.exclusion_policy = exclusion_policy;
            compute_time_metrics(fresh);
            compute_frequency_metrics(fresh);
        }
        catch (...)
        {
            return false;
        }
        output = fresh;
        return true;
    }

    bool analyze_hrv_from_ecg(const clinical_ecg_record& record, const signal_quality_waveforms* signal_quality, hrv_analysis_result& output)
    {
        hrv_analysis_result fresh;
        const clinical_beat_annotation* beats = record.beats();
        if (!beats && record.beat_count())
            return false;
        try
        {
            fresh.intervals.reserve(record.beat_count());
            for (unsigned int i = 0; i < record.beat_count(); ++i)
            {
                const clinical_beat_annotation& beat = beats[i];
                hrv_rr_interval interval;
                interval.beat_index = beat.beat_index;
                interval.beat_time_seconds = beat.r_peak_time_seconds;
                interval.rr_seconds = beat.rr_interval_seconds;
                interval.clipped = beat.rr_was_clipped;
                interval.ectopic = beat.origin != clinical_origin_conducted || (i > 0u && beats[i - 1u].origin != clinical_origin_conducted);
                const double interval_start = i ? beats[i - 1u].r_peak_time_seconds : std::max(0.0, beat.r_peak_time_seconds - beat.rr_interval_seconds);
                interval.artifact_overlap = artifact_overlaps_rr(signal_quality, interval_start, beat.r_peak_time_seconds);
                interval.excluded = !beat.qrs_present || !finite_positive(beat.rr_interval_seconds) || interval.clipped || interval.ectopic || interval.artifact_overlap;
                fresh.intervals.push_back(interval);
            }
        }
        catch (...)
        {
            return false;
        }
        return analyze_variability_intervals(fresh.intervals, fresh.metric_definition_version, fresh.exclusion_policy, output);
    }
}
