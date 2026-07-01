#include "ecg_morphology.h"

#include "clinical_ecg.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace signal_synth
{
    namespace
    {
        unsigned int sample_at(double time_seconds, const clinical_ecg_record& record)
        {
            if (time_seconds <= 0.0)
                return 0;
            const unsigned long long sample = static_cast<unsigned long long>(std::llround(time_seconds * record.sampling_rate_hz()));
            return static_cast<unsigned int>(std::min<unsigned long long>(sample, record.sample_count() - 1));
        }

        double mean_between(const double* signal, unsigned int first, unsigned int last)
        {
            if (!signal || last < first)
                return 0.0;
            double sum = 0.0;
            for (unsigned int sample = first; sample <= last; ++sample)
                sum += signal[sample];
            return sum / (last - first + 1);
        }

        double isoelectric_baseline(const double* signal, const clinical_beat_annotation& beat, const clinical_ecg_record& record)
        {
            const double first_time = std::max(0.0, beat.qrs_onset_time_seconds - 0.050);
            const double last_time = std::max(first_time, beat.qrs_onset_time_seconds - 0.010);
            return mean_between(signal, sample_at(first_time, record), sample_at(last_time, record));
        }

        double strongest_linear_residual(const double* signal, unsigned int first, unsigned int last, unsigned int& peak_sample, double& duration_seconds, unsigned int sampling_rate)
        {
            peak_sample = first;
            duration_seconds = 0.0;
            if (!signal || last <= first)
                return 0.0;
            const double first_value = signal[first];
            const double last_value = signal[last];
            double strongest = 0.0;
            for (unsigned int sample = first; sample <= last; ++sample)
            {
                const double position = static_cast<double>(sample - first) / (last - first);
                const double baseline = first_value + position * (last_value - first_value);
                const double residual = signal[sample] - baseline;
                if (std::fabs(residual) > std::fabs(strongest))
                {
                    strongest = residual;
                    peak_sample = sample;
                }
            }
            const double threshold = std::fabs(strongest) * 0.05;
            if (threshold == 0.0)
                return 0.0;
            unsigned int onset = peak_sample;
            unsigned int offset = peak_sample;
            while (onset > first)
            {
                const double position = static_cast<double>(onset - 1 - first) / (last - first);
                const double baseline = first_value + position * (last_value - first_value);
                if (std::fabs(signal[onset - 1] - baseline) < threshold)
                    break;
                --onset;
            }
            while (offset < last)
            {
                const double position = static_cast<double>(offset + 1 - first) / (last - first);
                const double baseline = first_value + position * (last_value - first_value);
                if (std::fabs(signal[offset + 1] - baseline) < threshold)
                    break;
                ++offset;
            }
            duration_seconds = static_cast<double>(offset - onset) / sampling_rate;
            return strongest;
        }

        ecg_lead_morphology measure_lead(const clinical_ecg_record& record, const clinical_beat_annotation& beat, unsigned int lead_index)
        {
            ecg_lead_morphology result = {};
            result.beat_index = beat.beat_index;
            result.lead_index = lead_index;
            const double* signal = record.lead_data(lead_index);
            const unsigned int qrs_onset = sample_at(beat.qrs_onset_time_seconds, record);
            const unsigned int r_peak = sample_at(beat.r_peak_time_seconds, record);
            const unsigned int qrs_offset = sample_at(beat.qrs_offset_time_seconds, record);
            const double baseline = isoelectric_baseline(signal, beat, record);

            double minimum = signal[qrs_onset];
            double maximum = signal[qrs_onset];
            double square_sum = 0.0;
            for (unsigned int sample = qrs_onset; sample <= qrs_offset; ++sample)
            {
                minimum = std::min(minimum, signal[sample]);
                maximum = std::max(maximum, signal[sample]);
                const double value = signal[sample] - baseline;
                square_sum += value * value;
            }
            result.qrs_peak_to_peak_mv = maximum - minimum;
            result.qrs_rms_mv = std::sqrt(square_sum / (qrs_offset - qrs_onset + 1));

            unsigned int q_sample = qrs_onset;
            for (unsigned int sample = qrs_onset; sample <= r_peak; ++sample)
                if (signal[sample] < signal[q_sample])
                    q_sample = sample;
            result.q_amplitude_mv = signal[q_sample] - baseline;
            const double q_threshold = std::fabs(result.q_amplitude_mv) * 0.05;
            if (result.q_amplitude_mv < 0.0 && q_threshold > 0.0)
            {
                unsigned int q_first = q_sample;
                unsigned int q_last = q_sample;
                while (q_first > qrs_onset && signal[q_first - 1] - baseline < -q_threshold)
                    --q_first;
                while (q_last < r_peak && signal[q_last + 1] - baseline < -q_threshold)
                    ++q_last;
                result.q_duration_seconds = static_cast<double>(q_last - q_first) / record.sampling_rate_hz();
            }

            unsigned int r_sample = qrs_onset;
            for (unsigned int sample = qrs_onset; sample <= qrs_offset; ++sample)
                if (signal[sample] > signal[r_sample])
                    r_sample = sample;
            result.r_amplitude_mv = signal[r_sample] - baseline;
            unsigned int s_sample = r_sample;
            for (unsigned int sample = r_sample; sample <= qrs_offset; ++sample)
                if (signal[sample] < signal[s_sample])
                    s_sample = sample;
            result.s_amplitude_mv = signal[s_sample] - baseline;
            result.st_j_mv = signal[sample_at(beat.j_point_time_seconds, record)] - baseline;
            result.st_j60_mv = signal[sample_at(beat.j_point_time_seconds + 0.060, record)] - baseline;

            if (beat.linked_atrial_index >= 0 && static_cast<unsigned long long>(beat.linked_atrial_index) < record.atrial_event_count())
            {
                const clinical_atrial_event& atrial = record.atrial_events()[beat.linked_atrial_index];
                if (atrial.visible)
                {
                    unsigned int peak_sample = 0;
                    result.p_amplitude_mv = strongest_linear_residual(signal, sample_at(atrial.onset_time_seconds, record), sample_at(atrial.offset_time_seconds, record), peak_sample, result.p_duration_seconds, record.sampling_rate_hz());
                    result.p_present = std::fabs(result.p_amplitude_mv) > 0.0;
                }
            }
            if (beat.t_present)
            {
                unsigned int peak_sample = 0;
                result.t_amplitude_mv = strongest_linear_residual(signal, sample_at(beat.t_onset_time_seconds, record), sample_at(beat.t_offset_time_seconds, record), peak_sample, result.t_duration_seconds, record.sampling_rate_hz());
                result.t_present = std::fabs(result.t_amplitude_mv) > 0.0;
            }
            return result;
        }
    }

    bool ecg_lead_is_in_region(unsigned int lead_index, ecg_lead_region region)
    {
        if (lead_index >= clinical_lead_count)
            return false;
        switch (region)
        {
        case ecg_region_all:
            return true;
        case ecg_region_limb:
            return lead_index <= clinical_lead_avf;
        case ecg_region_precordial:
            return lead_index >= clinical_lead_v1;
        case ecg_region_inferior:
            return lead_index == clinical_lead_ii || lead_index == clinical_lead_iii || lead_index == clinical_lead_avf;
        case ecg_region_anterior:
            return lead_index >= clinical_lead_v1 && lead_index <= clinical_lead_v4;
        case ecg_region_septal:
            return lead_index == clinical_lead_v1 || lead_index == clinical_lead_v2;
        case ecg_region_lateral:
            return lead_index == clinical_lead_i || lead_index == clinical_lead_avl || lead_index == clinical_lead_v5 || lead_index == clinical_lead_v6;
        default:
            return false;
        }
    }

    const char* ecg_lead_region_name(ecg_lead_region region)
    {
        switch (region)
        {
        case ecg_region_all:
            return "all";
        case ecg_region_limb:
            return "limb";
        case ecg_region_precordial:
            return "precordial";
        case ecg_region_inferior:
            return "inferior";
        case ecg_region_anterior:
            return "anterior";
        case ecg_region_septal:
            return "septal";
        case ecg_region_lateral:
            return "lateral";
        default:
            return "";
        }
    }

    struct ecg_morphology_report::implementation
    {
        std::vector<ecg_lead_morphology> entries;
    };

    ecg_morphology_report::ecg_morphology_report()
        : implementation_(new implementation)
    {
    }

    ecg_morphology_report::ecg_morphology_report(const ecg_morphology_report& other)
        : implementation_(new implementation(*other.implementation_))
    {
    }

    ecg_morphology_report& ecg_morphology_report::operator=(const ecg_morphology_report& other)
    {
        if (this != &other)
            *implementation_ = *other.implementation_;
        return *this;
    }

    ecg_morphology_report::~ecg_morphology_report()
    {
        delete implementation_;
    }

    unsigned int ecg_morphology_report::entry_count() const
    {
        return static_cast<unsigned int>(implementation_->entries.size());
    }

    const ecg_lead_morphology* ecg_morphology_report::entries() const
    {
        return implementation_->entries.empty() ? 0 : implementation_->entries.data();
    }

    const ecg_lead_morphology* ecg_morphology_report::find(unsigned long long beat_index, unsigned int lead_index) const
    {
        if (lead_index >= clinical_lead_count || beat_index > std::numeric_limits<unsigned long long>::max() / clinical_lead_count)
            return 0;
        const unsigned long long index = beat_index * clinical_lead_count + lead_index;
        return index < implementation_->entries.size() && implementation_->entries[index].beat_index == beat_index ? &implementation_->entries[index] : 0;
    }

    bool measure_ecg_morphology(const clinical_ecg_record& record, ecg_morphology_report& output)
    {
        if (record.sample_count() == 0 || record.sampling_rate_hz() == 0)
            return false;
        try
        {
            ecg_morphology_report::implementation measured;
            measured.entries.reserve(record.beat_count() * clinical_lead_count);
            for (unsigned int beat = 0; beat < record.beat_count(); ++beat)
                for (unsigned int lead = 0; lead < clinical_lead_count; ++lead)
                    measured.entries.push_back(measure_lead(record, record.beats()[beat], lead));
            std::swap(*output.implementation_, measured);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }
}
