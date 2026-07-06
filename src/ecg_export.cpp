#include "ecg_export.h"
#include "ecg_beat_classification.h"
#include "ecg_edf_bdf_export.h"
#include "ecg_wfdb_export.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace
{
    std::string json_string(const std::string& value)
    {
        static const char hex[] = "0123456789abcdef";
        std::string output("\"");
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            const unsigned char c = static_cast<unsigned char>(value[i]);
            switch (c)
            {
            case '"': output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if (c < 0x20u)
                {
                    output += "\\u00";
                    output.push_back(hex[c >> 4]);
                    output.push_back(hex[c & 0x0fu]);
                }
                else
                    output.push_back(static_cast<char>(c));
            }
        }
        output.push_back('"');
        return output;
    }

    std::string html_text(const std::string& value)
    {
        std::string output;
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            switch (value[i])
            {
            case '&': output += "&amp;"; break;
            case '<': output += "&lt;"; break;
            case '>': output += "&gt;"; break;
            case '"': output += "&quot;"; break;
            case '\'': output += "&#39;"; break;
            default: output.push_back(value[i]);
            }
        }
        return output;
    }

    const char* boolean(bool value)
    {
        return value ? "true" : "false";
    }

    double normalized_zero(double value)
    {
        return std::fabs(value) < 1e-15 ? 0.0 : value;
    }

    const char* origin_name(signal_synth::clinical_ventricular_origin value)
    {
        switch (value)
        {
        case signal_synth::clinical_origin_conducted: return "conducted";
        case signal_synth::clinical_origin_pac: return "pac";
        case signal_synth::clinical_origin_pvc: return "pvc";
        case signal_synth::clinical_origin_junctional_escape: return "junctional_escape";
        case signal_synth::clinical_origin_ventricular_escape: return "ventricular_escape";
        case signal_synth::clinical_origin_paced: return "paced";
        case signal_synth::clinical_origin_vt: return "ventricular_tachycardia";
        case signal_synth::clinical_origin_atrial_paced: return "atrial_paced";
        }
        return "unknown";
    }

    const char* pacing_event_name(signal_synth::clinical_pacing_event_kind value)
    {
        switch (value)
        {
        case signal_synth::clinical_pacing_event_atrial: return "atrial";
        case signal_synth::clinical_pacing_event_ventricular: return "ventricular";
        }
        return "unknown";
    }

    const char* fiducial_name(signal_synth::clinical_fiducial_kind value)
    {
        switch (value)
        {
        case signal_synth::clinical_p_onset: return "p_onset";
        case signal_synth::clinical_p_peak: return "p_peak";
        case signal_synth::clinical_p_offset: return "p_offset";
        case signal_synth::clinical_qrs_onset: return "qrs_onset";
        case signal_synth::clinical_q_peak: return "q_peak";
        case signal_synth::clinical_r_peak: return "r_peak";
        case signal_synth::clinical_s_peak: return "s_peak";
        case signal_synth::clinical_j_point: return "j_point";
        case signal_synth::clinical_qrs_offset: return "qrs_offset";
        case signal_synth::clinical_t_onset: return "t_onset";
        case signal_synth::clinical_t_peak: return "t_peak";
        case signal_synth::clinical_t_offset: return "t_offset";
        case signal_synth::clinical_pacing_spike: return "pacing_spike";
        }
        return "unknown";
    }

    const char* episode_kind_name(signal_synth::clinical_episode_kind value)
    {
        switch (value)
        {
        case signal_synth::clinical_episode_none: return "none";
        case signal_synth::clinical_episode_psvt: return "psvt";
        case signal_synth::clinical_episode_svarr: return "svarr";
        }
        return "unknown";
    }

    const char* clinical_lead_name(unsigned int lead)
    {
        static const char* names[signal_synth::clinical_lead_count] = {"I","II","III","aVR","aVL","aVF","V1","V2","V3","V4","V5","V6"};
        return lead < signal_synth::clinical_lead_count ? names[lead] : "";
    }

    const char* assertion_status_name(signal_synth::ecg_phenotype_assertion_status value)
    {
        switch (value)
        {
        case signal_synth::ecg_assertion_not_evaluated: return "not_evaluated";
        case signal_synth::ecg_assertion_passed: return "passed";
        case signal_synth::ecg_assertion_failed: return "failed";
        }
        return "unknown";
    }

    signal_synth::ecg_ground_truth_metrics calculate_metrics(const signal_synth::clinical_ecg_record& record)
    {
        signal_synth::ecg_ground_truth_metrics metrics;
        metrics.beat_count = record.beat_count();
        metrics.atrial_event_count = record.atrial_event_count();
        metrics.fiducial_count = record.fiducial_count();
        metrics.episode_count = record.episode_count();
        return metrics;
    }

    void add_hrv_metrics(const signal_synth::hrv_analysis_result& hrv, signal_synth::ecg_ground_truth_metrics& metrics)
    {
        metrics.rr_clipping_count = hrv.metrics.clipped_interval_count;
        metrics.mean_rr_seconds = hrv.metrics.mean_rr_seconds;
        metrics.mean_heart_rate_bpm = hrv.metrics.mean_heart_rate_bpm;
        metrics.sdnn_seconds = hrv.metrics.sdnn_seconds;
        metrics.rmssd_seconds = hrv.metrics.rmssd_seconds;
        metrics.pnn50_percent = hrv.metrics.pnn50_percent;
        metrics.hrv_accepted_interval_count = hrv.metrics.accepted_interval_count;
        metrics.hrv_excluded_interval_count = hrv.metrics.excluded_interval_count;
        metrics.hrv_ectopic_interval_count = hrv.metrics.ectopic_interval_count;
        metrics.hrv_artifact_overlap_interval_count = hrv.metrics.artifact_overlap_interval_count;
        metrics.sd1_seconds = hrv.metrics.sd1_seconds;
        metrics.sd2_seconds = hrv.metrics.sd2_seconds;
        metrics.sd1_sd2_ratio = hrv.metrics.sd1_sd2_ratio;
        metrics.lf_power_seconds2 = hrv.metrics.lf_power_seconds2;
        metrics.hf_power_seconds2 = hrv.metrics.hf_power_seconds2;
        metrics.lf_hf_ratio = hrv.metrics.lf_hf_ratio;
        metrics.total_power_seconds2 = hrv.metrics.total_power_seconds2;
    }

    void add_ppg_metrics(const signal_synth::ppg_record& ppg, signal_synth::ecg_ground_truth_metrics& metrics)
    {
        metrics.ppg_expected_pulse_count = ppg.pulse_count();
        for (unsigned int i = 0; i < ppg.pulse_count(); ++i)
        {
            if (ppg.pulses()[i].state == signal_synth::ppg_pulse_missing)
                ++metrics.ppg_missing_pulse_count;
            if (ppg.pulses()[i].state == signal_synth::ppg_pulse_weak)
                ++metrics.ppg_weak_pulse_count;
            if (ppg.pulses()[i].low_perfusion)
                ++metrics.ppg_low_perfusion_pulse_count;
            if (ppg.pulses()[i].state == signal_synth::ppg_pulse_out_of_record)
                ++metrics.ppg_out_of_record_pulse_count;
        }
        double onset_delay = 0.0;
        double peak_delay = 0.0;
        unsigned int peak_count = 0;
        for (unsigned int i = 0; i < ppg.annotation_count(); ++i)
        {
            const signal_synth::ppg_annotation& annotation = ppg.annotations()[i];
            if (annotation.kind == signal_synth::ppg_pulse_onset && annotation.source == signal_synth::ppg_fiducial_construction)
            {
                ++metrics.ppg_pulse_count;
                onset_delay += annotation.time_seconds - annotation.ecg_r_time_seconds;
            }
            if (annotation.kind == signal_synth::ppg_systolic_peak && annotation.source == signal_synth::ppg_fiducial_measurement)
            {
                ++peak_count;
                peak_delay += annotation.time_seconds - annotation.ecg_r_time_seconds;
            }
        }
        if (metrics.ppg_pulse_count)
            metrics.mean_ppg_onset_delay_seconds = onset_delay / metrics.ppg_pulse_count;
        if (peak_count)
            metrics.mean_ppg_peak_delay_seconds = peak_delay / peak_count;
    }

    void add_artifact_metrics(const signal_synth::signal_quality_waveforms& waveforms, signal_synth::ecg_ground_truth_metrics& metrics)
    {
        metrics.artifact_count = static_cast<unsigned int>(waveforms.artifacts.size());
        for (std::size_t i = 0; i < waveforms.artifacts.size(); ++i)
        {
            const signal_synth::signal_quality_artifact_interval& artifact = waveforms.artifacts[i];
            const double duration = artifact.end_seconds - artifact.start_seconds;
            metrics.total_artifact_seconds += duration;
            for (unsigned int lead = 0; lead < signal_synth::clinical_lead_count; ++lead)
                if (artifact.ecg_leads[lead])
                    metrics.ecg_artifact_seconds[lead] += duration;
            if (artifact.ppg)
                metrics.ppg_artifact_seconds += duration;
        }
    }

    const double* rendered_ecg_lead(const signal_synth::ecg_render_bundle& render, unsigned int lead)
    {
        if (lead < render.signal_quality.ecg_leads.size() && render.signal_quality.ecg_leads[lead].size() == render.record.sample_count())
            return render.signal_quality.ecg_leads[lead].empty() ? 0 : &render.signal_quality.ecg_leads[lead][0];
        return render.record.lead_data(lead);
    }

    const double* rendered_ppg(const signal_synth::ecg_render_bundle& render)
    {
        if (render.signal_quality.ppg.size() == render.ppg.sample_count())
            return render.signal_quality.ppg.empty() ? 0 : &render.signal_quality.ppg[0];
        return render.ppg.samples();
    }

    std::string waveform_csv(const signal_synth::ecg_render_bundle& render)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10);
        output << "sample_index,time_seconds";
        for (unsigned int lead = 0; lead < render.record.lead_count(); ++lead)
            output << ',' << render.record.lead_name(lead) << "_mv";
        if (render.ppg.sample_count())
            output << ",ppg_green_au";
        if (!render.signal_quality.accelerometer.empty())
            output << ",accel_motion_g";
        output << '\n';
        for (unsigned int sample = 0; sample < render.record.sample_count(); ++sample)
        {
            output << sample << ',' << static_cast<double>(sample) / render.record.sampling_rate_hz();
            for (unsigned int lead = 0; lead < render.record.lead_count(); ++lead)
                output << ',' << normalized_zero(rendered_ecg_lead(render, lead)[sample]);
            if (render.ppg.sample_count())
                output << ',' << normalized_zero(rendered_ppg(render)[sample]);
            if (!render.signal_quality.accelerometer.empty())
                output << ',' << normalized_zero(render.signal_quality.accelerometer[sample]);
            output << '\n';
        }
        return output.str();
    }

    void write_artifact_channels(std::ostringstream& output, const signal_synth::signal_quality_artifact_interval& artifact)
    {
        output << '[';
        bool first = true;
        for (unsigned int lead = 0; lead < signal_synth::clinical_lead_count; ++lead)
        {
            if (artifact.ecg_leads[lead])
            {
                if (!first)
                    output << ',';
                output << json_string(clinical_lead_name(lead));
                first = false;
            }
        }
        if (artifact.ppg)
        {
            if (!first)
                output << ',';
            output << "\"ppg_green\"";
            first = false;
        }
        if (artifact.accelerometer_reference)
        {
            if (!first)
                output << ',';
            output << "\"accel_motion\"";
        }
        output << ']';
    }

    void write_rr_tachogram_json(std::ostringstream& output, const signal_synth::hrv_analysis_result& hrv)
    {
        output << '[';
        for (std::size_t i = 0; i < hrv.intervals.size(); ++i)
        {
            if (i)
                output << ',';
            const signal_synth::hrv_rr_interval& interval = hrv.intervals[i];
            output << "{\"beat_index\":" << interval.beat_index
                   << ",\"beat_time_seconds\":" << interval.beat_time_seconds
                   << ",\"rr_seconds\":" << interval.rr_seconds
                   << ",\"clipped\":" << boolean(interval.clipped)
                   << ",\"ectopic\":" << boolean(interval.ectopic)
                   << ",\"artifact_overlap\":" << boolean(interval.artifact_overlap)
                   << ",\"excluded\":" << boolean(interval.excluded) << '}';
        }
        output << ']';
    }

    std::string rr_tachogram_csv(const signal_synth::ecg_render_bundle& render)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10);
        output << "beat_index,beat_time_seconds,rr_seconds,clipped,ectopic,artifact_overlap,excluded\n";
        for (std::size_t i = 0; i < render.hrv.intervals.size(); ++i)
        {
            const signal_synth::hrv_rr_interval& interval = render.hrv.intervals[i];
            output << interval.beat_index << ',' << interval.beat_time_seconds << ',' << interval.rr_seconds
                   << ',' << (interval.clipped ? 1 : 0)
                   << ',' << (interval.ectopic ? 1 : 0)
                   << ',' << (interval.artifact_overlap ? 1 : 0)
                   << ',' << (interval.excluded ? 1 : 0) << '\n';
        }
        return output.str();
    }

    std::string hrv_metrics_json(const signal_synth::ecg_render_bundle& render)
    {
        const signal_synth::hrv_analysis_result& hrv = render.hrv;
        const signal_synth::hrv_metric_summary& metrics = hrv.metrics;
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10);
        output << "{\"schema_version\":1"
               << ",\"definition_version\":" << json_string(hrv.metric_definition_version)
               << ",\"units\":{\"time\":\"seconds\",\"power\":\"seconds_squared\",\"frequency\":\"hertz\"}"
               << ",\"exclusion_policy\":" << json_string(hrv.exclusion_policy)
               << ",\"spectral_method\":" << json_string(hrv.spectral_method)
               << ",\"analysis_window\":{\"start_seconds\":";
        if (hrv.intervals.empty())
            output << "0,\"end_seconds\":0,\"duration_seconds\":0";
        else
            output << hrv.intervals.front().beat_time_seconds
                   << ",\"end_seconds\":" << hrv.intervals.back().beat_time_seconds
                   << ",\"duration_seconds\":" << hrv.intervals.back().beat_time_seconds - hrv.intervals.front().beat_time_seconds;
        output << ",\"interpolation_rate_hz\":" << hrv.interpolation_rate_hz
               << ",\"lf_band_hz\":[" << hrv.lf_low_hz << ',' << hrv.lf_high_hz << ']'
               << ",\"hf_band_hz\":[" << hrv.hf_low_hz << ',' << hrv.hf_high_hz << "]}"
               << ",\"counts\":{\"intervals\":" << metrics.interval_count
               << ",\"accepted\":" << metrics.accepted_interval_count
               << ",\"excluded\":" << metrics.excluded_interval_count
               << ",\"clipped\":" << metrics.clipped_interval_count
               << ",\"ectopic\":" << metrics.ectopic_interval_count
               << ",\"artifact_overlap\":" << metrics.artifact_overlap_interval_count << '}'
               << ",\"time_domain\":{\"mean_rr_seconds\":" << metrics.mean_rr_seconds
               << ",\"mean_heart_rate_bpm\":" << metrics.mean_heart_rate_bpm
               << ",\"sdnn_seconds\":" << metrics.sdnn_seconds
               << ",\"rmssd_seconds\":" << metrics.rmssd_seconds
               << ",\"pnn50_percent\":" << metrics.pnn50_percent
               << ",\"sd1_seconds\":" << metrics.sd1_seconds
               << ",\"sd2_seconds\":" << metrics.sd2_seconds
               << ",\"sd1_sd2_ratio\":" << metrics.sd1_sd2_ratio << '}'
               << ",\"frequency_domain\":{\"lf_power_seconds2\":" << metrics.lf_power_seconds2
               << ",\"hf_power_seconds2\":" << metrics.hf_power_seconds2
               << ",\"lf_hf_ratio\":" << metrics.lf_hf_ratio
               << ",\"total_power_seconds2\":" << metrics.total_power_seconds2 << '}'
               << ",\"tachogram\":";
        write_rr_tachogram_json(output, hrv);
        output << '}';
        return output.str();
    }

    std::string annotations_json(const signal_synth::ecg_render_bundle& render)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10);
        output << "{\"schema_version\":1,\"document_fingerprint\":" << json_string(render.document_identity.document_fingerprint)
               << ",\"generation_fingerprint\":" << render.document_identity.generation_fingerprint
               << ",\"render_identity\":" << json_string(render.render_identity)
               << ",\"beats\":[";
        for (unsigned int i = 0; i < render.record.beat_count(); ++i)
        {
            if (i)
                output << ',';
            const signal_synth::clinical_beat_annotation& beat = render.record.beats()[i];
            output << "{\"beat_index\":" << beat.beat_index
                   << ",\"linked_atrial_index\":" << beat.linked_atrial_index
                   << ",\"origin\":" << json_string(origin_name(beat.origin))
                   << ",\"beat_class\":" << json_string(signal_synth::ecg_beat_class_name(signal_synth::ecg_beat_class_from_origin(beat.origin)))
                   << ",\"rr_seconds\":" << beat.rr_interval_seconds
                   << ",\"pr_seconds\":" << beat.pr_interval_seconds
                   << ",\"qrs_seconds\":" << beat.qrs_duration_seconds
                   << ",\"qt_seconds\":" << beat.qt_interval_seconds
                   << ",\"qtc_seconds\":" << beat.qtc_interval_seconds
                   << ",\"qrs_onset_seconds\":" << beat.qrs_onset_time_seconds
                   << ",\"r_peak_seconds\":" << beat.r_peak_time_seconds
                   << ",\"p_present\":" << boolean(beat.p_present)
                   << ",\"qrs_present\":" << boolean(beat.qrs_present)
                   << ",\"t_present\":" << boolean(beat.t_present)
                   << ",\"rr_was_clipped\":" << boolean(beat.rr_was_clipped) << '}';
        }
        output << "],\"atrial_events\":[";
        for (unsigned int i = 0; i < render.record.atrial_event_count(); ++i)
        {
            if (i)
                output << ',';
            const signal_synth::clinical_atrial_event& event = render.record.atrial_events()[i];
            output << "{\"atrial_index\":" << event.atrial_index
                   << ",\"onset_seconds\":" << event.onset_time_seconds
                   << ",\"peak_seconds\":" << event.peak_time_seconds
                   << ",\"offset_seconds\":" << event.offset_time_seconds
                   << ",\"visible\":" << boolean(event.visible)
                   << ",\"conducted\":" << boolean(event.conducted)
                   << ",\"linked_ventricular_index\":" << event.linked_ventricular_index << '}';
        }
        output << "],\"fiducials\":[";
        const bool detailed_annotations = !render.resolved_document.output.compact;
        for (unsigned int i = 0; detailed_annotations && i < render.record.fiducial_count(); ++i)
        {
            if (i)
                output << ',';
            const signal_synth::clinical_fiducial_annotation& fiducial = render.record.fiducials()[i];
            output << "{\"beat_index\":" << fiducial.beat_index
                   << ",\"atrial_index\":" << fiducial.atrial_index
                   << ",\"lead_index\":" << fiducial.lead_index
                   << ",\"kind\":" << json_string(fiducial_name(fiducial.kind))
                   << ",\"source\":" << json_string(fiducial.source == signal_synth::clinical_fiducial_construction ? "construction" : "lead_measurement")
                   << ",\"sample_index\":" << fiducial.sample_index
                   << ",\"time_seconds\":" << fiducial.time_seconds
                   << ",\"amplitude_mv\":" << fiducial.amplitude_mv
                   << ",\"present\":" << boolean(fiducial.present) << '}';
        }
        output << "],\"fiducial_detail\":" << json_string(detailed_annotations ? "full" : "omitted_in_compact_output")
               << ",\"pacing_events\":[";
        for (unsigned int i = 0; i < render.record.pacing_event_count(); ++i)
        {
            if (i)
                output << ',';
            const signal_synth::clinical_pacing_event& event = render.record.pacing_events()[i];
            output << "{\"pacing_index\":" << event.pacing_index
                   << ",\"kind\":" << json_string(pacing_event_name(event.kind))
                   << ",\"time_seconds\":" << event.time_seconds
                   << ",\"sample_index\":" << event.sample_index
                   << ",\"captured\":" << boolean(event.captured)
                   << ",\"linked_atrial_index\":" << event.linked_atrial_index
                   << ",\"linked_ventricular_index\":" << event.linked_ventricular_index << '}';
        }
        output << "],\"episodes\":[";
        for (unsigned int i = 0; i < render.record.episode_count(); ++i)
        {
            if (i)
                output << ',';
            const signal_synth::clinical_episode_annotation& episode = render.record.episodes()[i];
            output << "{\"kind\":" << json_string(episode_kind_name(episode.kind))
                   << ",\"start_seconds\":" << episode.start_time_seconds
                   << ",\"end_seconds\":" << episode.end_time_seconds
                   << ",\"onset_transition_start_seconds\":" << episode.onset_transition_start_seconds
                   << ",\"onset_transition_end_seconds\":" << episode.onset_transition_end_seconds
                   << ",\"offset_transition_start_seconds\":" << episode.offset_transition_start_seconds
                   << ",\"offset_transition_end_seconds\":" << episode.offset_transition_end_seconds
                   << ",\"first_beat_index\":" << episode.first_beat_index
                   << ",\"last_beat_index\":" << episode.last_beat_index
                   << ",\"start_sample_index\":" << episode.start_sample_index
                   << ",\"end_sample_index\":" << episode.end_sample_index
                   << ",\"onset_transition_start_sample_index\":" << episode.onset_transition_start_sample_index
                   << ",\"onset_transition_end_sample_index\":" << episode.onset_transition_end_sample_index
                   << ",\"offset_transition_start_sample_index\":" << episode.offset_transition_start_sample_index
                   << ",\"offset_transition_end_sample_index\":" << episode.offset_transition_end_sample_index
                   << ",\"present\":" << boolean(episode.present) << '}';
        }
        output << ']';
        if (render.document.schema_version >= 2)
        {
            output << ",\"ppg_fiducials\":[";
            for (unsigned int i = 0; i < render.ppg.annotation_count(); ++i)
            {
                if (i)
                    output << ',';
                const signal_synth::ppg_annotation& annotation = render.ppg.annotations()[i];
                const char* kind = annotation.kind == signal_synth::ppg_pulse_onset ? "pulse_onset"
                    : annotation.kind == signal_synth::ppg_systolic_peak ? "systolic_peak"
                    : annotation.kind == signal_synth::ppg_dicrotic_feature ? "dicrotic_feature" : "pulse_offset";
                output << "{\"ecg_beat_index\":" << annotation.ecg_beat_index
                       << ",\"ecg_r_time_seconds\":" << annotation.ecg_r_time_seconds
                       << ",\"kind\":" << json_string(kind)
                       << ",\"source\":" << json_string(annotation.source == signal_synth::ppg_fiducial_construction ? "construction" : "measurement")
                       << ",\"sample_index\":" << annotation.sample_index
                       << ",\"time_seconds\":" << annotation.time_seconds
                       << ",\"value_au\":" << annotation.value_au << '}';
            }
            output << "],\"ppg_pulses\":[";
            for (unsigned int i = 0; i < render.ppg.pulse_count(); ++i)
            {
                if (i)
                    output << ',';
                const signal_synth::ppg_pulse_annotation& pulse = render.ppg.pulses()[i];
                output << "{\"ecg_beat_index\":" << pulse.ecg_beat_index
                       << ",\"ecg_r_time_seconds\":" << pulse.ecg_r_time_seconds
                       << ",\"pulse_delay_seconds\":" << pulse.pulse_delay_seconds
                       << ",\"expected_onset_time_seconds\":" << pulse.expected_onset_time_seconds
                       << ",\"expected_peak_time_seconds\":" << pulse.expected_peak_time_seconds
                       << ",\"expected_offset_time_seconds\":" << pulse.expected_offset_time_seconds
                       << ",\"effective_amplitude_au\":" << pulse.effective_amplitude_au
                       << ",\"effective_rise_time_seconds\":" << pulse.effective_rise_time_seconds
                       << ",\"effective_decay_time_seconds\":" << pulse.effective_decay_time_seconds
                       << ",\"state\":" << json_string(signal_synth::ppg_pulse_state_name(pulse.state))
                       << ",\"low_perfusion\":" << boolean(pulse.low_perfusion)
                       << ",\"valid_for_peak_scoring\":" << boolean(pulse.valid_for_peak_scoring)
                       << ",\"generated\":" << boolean(pulse.generated)
                       << ",\"intentionally_missing\":" << boolean(pulse.intentionally_missing) << '}';
            }
            output << ']';
        }
        if (render.document.schema_version >= 3)
            output << ",\"randomization\":{\"seed\":" << render.document.randomization.seed
                   << ",\"draw_count\":" << render.parameter_draws.size()
                   << ",\"resolved_document_fingerprint\":" << json_string(render.resolved_document_identity.document_fingerprint)
                   << "},\"physiology\":{\"respiration_frequency_hz\":" << render.resolved_document.physiology.respiration_frequency_hz
                   << ",\"respiratory_rr_amplitude_seconds\":" << render.resolved_document.physiology.respiratory_rr_amplitude_seconds
                   << ",\"ecg_baseline_amplitude_mv\":" << render.resolved_document.physiology.ecg_baseline_amplitude_mv
                   << ",\"ppg_amplitude_modulation_ratio\":" << render.resolved_document.physiology.ppg_amplitude_modulation_ratio
                   << ",\"activity_start_seconds\":" << render.resolved_document.physiology.activity_start_seconds
                   << ",\"activity_duration_seconds\":" << render.resolved_document.physiology.activity_duration_seconds
                   << ",\"activity_intensity\":" << render.resolved_document.physiology.activity_intensity << '}';
        output << ",\"rr_tachogram\":";
        if (detailed_annotations)
            write_rr_tachogram_json(output, render.hrv);
        else
            output << "[]";
        output << ",\"rr_tachogram_reference\":\"rr_tachogram.csv\"";
        output << ",\"artifact_intervals\":[";
        for (std::size_t i = 0; i < render.signal_quality.artifacts.size(); ++i)
        {
            if (i)
                output << ',';
            const signal_synth::signal_quality_artifact_interval& artifact = render.signal_quality.artifacts[i];
            output << "{\"type\":" << json_string(signal_synth::signal_quality_artifact_type_name(artifact.type))
                   << ",\"start_seconds\":" << artifact.start_seconds
                   << ",\"end_seconds\":" << artifact.end_seconds
                   << ",\"start_sample_index\":" << artifact.start_sample_index
                   << ",\"end_sample_index\":" << artifact.end_sample_index
                   << ",\"severity\":" << artifact.severity
                   << ",\"seed\":" << artifact.seed
                   << ",\"channels\":";
            write_artifact_channels(output, artifact);
            output << '}';
        }
        output << "]}";
        return output.str();
    }

    std::string metrics_json(const signal_synth::ecg_render_bundle& render)
    {
        const signal_synth::ecg_ground_truth_metrics& metrics = render.metrics;
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10);
        output << "{\"schema_version\":1,\"beats\":{\"count\":" << metrics.beat_count
               << ",\"atrial_event_count\":" << metrics.atrial_event_count
               << ",\"fiducial_count\":" << metrics.fiducial_count
               << ",\"episode_count\":" << metrics.episode_count
               << ",\"rr_clipping_count\":" << metrics.rr_clipping_count
               << "},\"hrv\":{\"definition_version\":" << json_string(render.hrv.metric_definition_version)
               << ",\"exclusion_policy\":" << json_string(render.hrv.exclusion_policy)
               << ",\"spectral_method\":" << json_string(render.hrv.spectral_method)
               << ",\"interval_count\":" << render.hrv.metrics.interval_count
               << ",\"accepted_interval_count\":" << metrics.hrv_accepted_interval_count
               << ",\"excluded_interval_count\":" << metrics.hrv_excluded_interval_count
               << ",\"ectopic_interval_count\":" << metrics.hrv_ectopic_interval_count
               << ",\"artifact_overlap_interval_count\":" << metrics.hrv_artifact_overlap_interval_count
               << ",\"mean_rr_seconds\":" << metrics.mean_rr_seconds
               << ",\"mean_heart_rate_bpm\":" << metrics.mean_heart_rate_bpm
               << ",\"sdnn_seconds\":" << metrics.sdnn_seconds
               << ",\"rmssd_seconds\":" << metrics.rmssd_seconds
               << ",\"pnn50_percent\":" << metrics.pnn50_percent
               << ",\"sd1_seconds\":" << metrics.sd1_seconds
               << ",\"sd2_seconds\":" << metrics.sd2_seconds
               << ",\"sd1_sd2_ratio\":" << metrics.sd1_sd2_ratio
               << ",\"lf_power_seconds2\":" << metrics.lf_power_seconds2
               << ",\"hf_power_seconds2\":" << metrics.hf_power_seconds2
               << ",\"lf_hf_ratio\":" << metrics.lf_hf_ratio
               << ",\"total_power_seconds2\":" << metrics.total_power_seconds2
               << "},\"artifacts\":{\"count\":" << metrics.artifact_count
               << ",\"total_artifact_seconds\":" << metrics.total_artifact_seconds
               << ",\"ecg_channel_seconds\":{";
        for (unsigned int lead = 0; lead < signal_synth::clinical_lead_count; ++lead)
        {
            if (lead)
                output << ',';
            output << json_string(clinical_lead_name(lead)) << ':' << metrics.ecg_artifact_seconds[lead];
        }
        output << "},\"ppg_seconds\":" << metrics.ppg_artifact_seconds
               << "},\"phenotype_passed\":" << boolean(render.scenario_report.phenotype_passed())
               << ",\"assertions\":[";
        for (unsigned int i = 0; i < render.scenario_report.assertion_count(); ++i)
        {
            if (i)
                output << ',';
            const signal_synth::ecg_condition_info* condition = signal_synth::find_ecg_condition(render.scenario_report.assertion_condition(i));
            output << "{\"condition\":" << json_string(condition ? condition->scp_code : "")
                   << ",\"name\":" << json_string(render.scenario_report.assertion_name(i))
                   << ",\"status\":" << json_string(assertion_status_name(render.scenario_report.assertion_status(i)))
                   << ",\"measured\":" << render.scenario_report.assertion_measured_value(i)
                   << ",\"minimum\":" << render.scenario_report.assertion_minimum(i)
                   << ",\"maximum\":" << render.scenario_report.assertion_maximum(i)
                   << ",\"unit\":" << json_string(render.scenario_report.assertion_unit(i)) << '}';
        }
        output << ']';
        if (render.document.schema_version >= 2)
            output << ",\"ppg\":{\"pulse_count\":" << metrics.ppg_pulse_count
                   << ",\"expected_pulse_count\":" << metrics.ppg_expected_pulse_count
                   << ",\"intentionally_missing_pulse_count\":" << metrics.ppg_missing_pulse_count
                   << ",\"weak_pulse_count\":" << metrics.ppg_weak_pulse_count
                   << ",\"low_perfusion_pulse_count\":" << metrics.ppg_low_perfusion_pulse_count
                   << ",\"out_of_record_pulse_count\":" << metrics.ppg_out_of_record_pulse_count
                   << ",\"mean_onset_delay_seconds\":" << metrics.mean_ppg_onset_delay_seconds
                   << ",\"mean_measured_peak_delay_seconds\":" << metrics.mean_ppg_peak_delay_seconds << '}';
        output << '}';
        return output.str();
    }

    std::string warnings_json(const signal_synth::ecg_render_bundle& render)
    {
        std::ostringstream output;
        output << "{\"schema_version\":1,\"issues\":[";
        for (unsigned int i = 0; i < render.scenario_report.issue_count(); ++i)
        {
            if (i)
                output << ',';
            output << "{\"severity\":" << json_string(render.scenario_report.issue_severity(i) == signal_synth::ecg_issue_error ? "error" : "warning")
                   << ",\"message\":" << json_string(render.scenario_report.issue_message(i)) << '}';
        }
        output << "]}";
        return output.str();
    }

    std::string metadata_json(const signal_synth::ecg_render_bundle& render)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << "{\"schema_version\":1,\"generator\":{\"name\":\"signal_synth\",\"product\":\"Synsigra Testbench\",\"version\":"
               << json_string(signal_synth::signal_synth_generator_version())
               << "},\"scenario\":{\"id\":" << json_string(render.document.scenario_id)
               << ",\"document_fingerprint\":" << json_string(render.document_identity.document_fingerprint)
               << ",\"generation_fingerprint\":" << render.document_identity.generation_fingerprint
               << ",\"resolved_document_fingerprint\":" << json_string(render.resolved_document_identity.document_fingerprint)
               << ",\"resolved_generation_fingerprint\":" << render.resolved_document_identity.generation_fingerprint
               << ",\"render_identity\":" << json_string(render.render_identity)
               << ",\"ecg_run_fingerprint\":" << render.scenario_report.run_fingerprint()
               << ",\"scenario_schema_version\":" << render.document.schema_version
               << ",\"engine_version\":" << render.scenario_report.engine_version()
               << ",\"seed\":" << render.resolved_document.ecg.seed()
               << "},\"render\":{\"sample_rate_hz\":" << render.record.sampling_rate_hz()
               << ",\"sample_count\":" << render.record.sample_count()
               << ",\"duration_seconds\":" << std::setprecision(std::numeric_limits<double>::max_digits10) << render.document.duration_seconds
               << ",\"channel_count\":" << render.record.lead_count() + (render.ppg.sample_count() ? 1u : 0u) + (render.signal_quality.accelerometer.empty() ? 0u : 1u)
               << ",\"channels\":[";
        for (unsigned int lead = 0; lead < render.record.lead_count(); ++lead)
        {
            if (lead)
                output << ',';
            output << "{\"name\":" << json_string(render.record.lead_name(lead)) << ",\"unit\":\"mV\"}";
        }
        if (render.ppg.sample_count())
            output << ",{\"name\":\"ppg_green\",\"unit\":\"a.u.\"}";
        if (!render.signal_quality.accelerometer.empty())
            output << ",{\"name\":\"accel_motion\",\"unit\":\"g\",\"role\":\"motion_reference\"}";
        output << "],\"timestamp_policy\":\"not_recorded_for_deterministic_local_export\""
               << ",\"compact_output\":" << (render.resolved_document.output.compact ? "true" : "false")
               << ",\"source_channels_retained\":" << (render.resolved_document.output.retain_source_channels ? "true" : "false")
               << "},\"intended_use\":\"synthetic engineering algorithm testing and QA\","
               << "\"not_for\":\"diagnosis, patient monitoring, clinical validation certificate, or standalone conformity assessment\"}";
        return output.str();
    }

    std::string svg_preview(const signal_synth::ecg_render_bundle& render)
    {
        const double* samples = rendered_ecg_lead(render, signal_synth::clinical_lead_ii);
        if (!samples || !render.record.sample_count())
            return "";
        const unsigned int point_count = std::min(1000u, render.record.sample_count());
        double minimum = samples[0], maximum = samples[0];
        for (unsigned int i = 0; i < point_count; ++i)
        {
            const unsigned int sample = point_count == 1 ? 0 : static_cast<unsigned int>((static_cast<unsigned long long>(i) * (render.record.sample_count() - 1)) / (point_count - 1));
            minimum = std::min(minimum, samples[sample]);
            maximum = std::max(maximum, samples[sample]);
        }
        const double span = maximum > minimum ? maximum - minimum : 1.0;
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::fixed << std::setprecision(2)
               << "<svg viewBox=\"0 0 1000 240\" role=\"img\" aria-label=\"Lead II waveform preview\"><rect width=\"1000\" height=\"240\" fill=\"#fff\"/><path d=\"M0 120H1000\" stroke=\"#d1d5db\"/><polyline fill=\"none\" stroke=\"#b42318\" stroke-width=\"1.5\" points=\"";
        for (unsigned int i = 0; i < point_count; ++i)
        {
            const unsigned int sample = point_count == 1 ? 0 : static_cast<unsigned int>((static_cast<unsigned long long>(i) * (render.record.sample_count() - 1)) / (point_count - 1));
            const double x = point_count == 1 ? 0.0 : 1000.0 * i / (point_count - 1);
            const double y = 220.0 - 200.0 * (samples[sample] - minimum) / span;
            if (i)
                output << ' ';
            output << x << ',' << y;
        }
        output << "\"/></svg>";
        return output.str();
    }

    std::string ppg_svg_preview(const signal_synth::ecg_render_bundle& render)
    {
        const double* samples = rendered_ppg(render);
        if (!samples || !render.ppg.sample_count())
            return "";
        const unsigned int point_count = std::min(1000u, render.ppg.sample_count());
        double minimum = samples[0], maximum = samples[0];
        for (unsigned int i = 0; i < point_count; ++i)
        {
            const unsigned int sample = point_count == 1 ? 0 : static_cast<unsigned int>((static_cast<unsigned long long>(i) * (render.ppg.sample_count() - 1)) / (point_count - 1));
            minimum = std::min(minimum, samples[sample]);
            maximum = std::max(maximum, samples[sample]);
        }
        const double span = maximum > minimum ? maximum - minimum : 1.0;
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::fixed << std::setprecision(2)
               << "<svg viewBox=\"0 0 1000 240\" role=\"img\" aria-label=\"PPG waveform preview\"><rect width=\"1000\" height=\"240\" fill=\"#fff\"/><path d=\"M0 220H1000\" stroke=\"#d1d5db\"/><polyline fill=\"none\" stroke=\"#067647\" stroke-width=\"1.5\" points=\"";
        for (unsigned int i = 0; i < point_count; ++i)
        {
            const unsigned int sample = point_count == 1 ? 0 : static_cast<unsigned int>((static_cast<unsigned long long>(i) * (render.ppg.sample_count() - 1)) / (point_count - 1));
            const double x = point_count == 1 ? 0.0 : 1000.0 * i / (point_count - 1);
            const double y = 220.0 - 200.0 * (samples[sample] - minimum) / span;
            if (i)
                output << ' ';
            output << x << ',' << y;
        }
        output << "\"/></svg>";
        return output.str();
    }

    std::string report_html(const signal_synth::ecg_render_bundle& render)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(6);
        output << "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\"><title>"
               << html_text(render.document.name)
               << " - Synthetic Scenario Performance Report</title><style>"
               << "body{font:14px Arial,sans-serif;color:#202124;max-width:1100px;margin:32px auto;padding:0 20px}"
               << "h1,h2{color:#111827}table{border-collapse:collapse;width:100%;margin:12px 0 24px}"
               << "th,td{border:1px solid #d1d5db;padding:7px;text-align:left}th{background:#f3f4f6}"
               << ".notice{border-left:4px solid #b42318;padding:10px 14px;background:#fef3f2}svg{width:100%;height:auto;border:1px solid #d1d5db}"
               << "</style></head><body><h1>Synthetic Scenario Performance Report</h1>"
               << "<p class=\"notice\">This report describes synthetic engineering test signals generated from the specified scenario. "
               << "It is intended for research, development, software testing and algorithm QA. It is not a clinical validation certificate, "
               << "not a diagnostic result, and not standalone evidence of medical-device conformity.</p>"
               << "<h2>Identity</h2><table><tr><th>Scenario</th><td>" << html_text(render.document.scenario_id)
               << "</td></tr><tr><th>Document fingerprint</th><td>" << html_text(render.document_identity.document_fingerprint)
               << "</td></tr><tr><th>Generation fingerprint</th><td>" << render.document_identity.generation_fingerprint
               << "</td></tr><tr><th>Render identity</th><td>" << html_text(render.render_identity)
               << "</td></tr><tr><th>ECG run fingerprint</th><td>" << render.scenario_report.run_fingerprint()
               << "</td></tr><tr><th>Generator</th><td>" << signal_synth::signal_synth_generator_version()
               << "</td></tr></table><h2>Lead II Preview</h2>" << svg_preview(render)
               << (render.ppg.sample_count() ? "<h2>PPG Preview</h2>" + ppg_svg_preview(render) : "")
               << "<h2>Ground Truth Summary</h2><table><tr><th>Beats</th><td>" << render.metrics.beat_count
               << "</td></tr><tr><th>Mean HR</th><td>" << render.metrics.mean_heart_rate_bpm
               << " bpm</td></tr><tr><th>Mean RR</th><td>" << render.metrics.mean_rr_seconds
               << " s</td></tr><tr><th>SDNN</th><td>" << render.metrics.sdnn_seconds
               << " s</td></tr><tr><th>RMSSD</th><td>" << render.metrics.rmssd_seconds
               << " s</td></tr><tr><th>pNN50</th><td>" << render.metrics.pnn50_percent
               << " %</td></tr><tr><th>SD1 / SD2</th><td>" << render.metrics.sd1_seconds << " s / " << render.metrics.sd2_seconds
               << " s</td></tr><tr><th>LF/HF</th><td>" << render.metrics.lf_hf_ratio
               << "</td></tr><tr><th>HRV accepted / excluded intervals</th><td>" << render.metrics.hrv_accepted_interval_count << " / " << render.metrics.hrv_excluded_interval_count
               << "</td></tr><tr><th>Artifact intervals</th><td>" << render.metrics.artifact_count
               << "</td></tr><tr><th>Total artifact seconds</th><td>" << render.metrics.total_artifact_seconds
               << " s</td></tr></table><h2>Phenotype Assertions</h2><table><tr><th>Condition</th><th>Assertion</th><th>Status</th><th>Measured</th><th>Range</th><th>Unit</th></tr>";
        for (unsigned int i = 0; i < render.scenario_report.assertion_count(); ++i)
        {
            const signal_synth::ecg_condition_info* condition = signal_synth::find_ecg_condition(render.scenario_report.assertion_condition(i));
            output << "<tr><td>" << html_text(condition ? condition->scp_code : "")
                   << "</td><td>" << html_text(render.scenario_report.assertion_name(i))
                   << "</td><td>" << assertion_status_name(render.scenario_report.assertion_status(i))
                   << "</td><td>" << render.scenario_report.assertion_measured_value(i)
                   << "</td><td>" << render.scenario_report.assertion_minimum(i) << " to " << render.scenario_report.assertion_maximum(i)
                   << "</td><td>" << html_text(render.scenario_report.assertion_unit(i)) << "</td></tr>";
        }
        output << "</table><h2>Artifact Intervals</h2><table><tr><th>Type</th><th>Start</th><th>End</th><th>Severity</th><th>Channels</th></tr>";
        for (std::size_t i = 0; i < render.signal_quality.artifacts.size(); ++i)
        {
            const signal_synth::signal_quality_artifact_interval& artifact = render.signal_quality.artifacts[i];
            std::ostringstream channels;
            write_artifact_channels(channels, artifact);
            output << "<tr><td>" << signal_synth::signal_quality_artifact_type_name(artifact.type)
                   << "</td><td>" << artifact.start_seconds
                   << "</td><td>" << artifact.end_seconds
                   << "</td><td>" << artifact.severity
                   << "</td><td>" << html_text(channels.str()) << "</td></tr>";
        }
        if (render.signal_quality.artifacts.empty())
            output << "<tr><td colspan=\"5\">No acquisition artifacts requested.</td></tr>";
        output << "</table><h2>Warnings and Limitations</h2><ul>";
        for (unsigned int i = 0; i < render.scenario_report.issue_count(); ++i)
            output << "<li>" << html_text(render.scenario_report.issue_message(i)) << "</li>";
        output << "<li>The compact cardiac phantom is not population-fitted clinical evidence.</li>"
               << (render.signal_quality.artifacts.empty() ? "<li>No acquisition artifacts are present in this scenario.</li>" : "<li>Acquisition artifacts corrupt waveform samples but do not change construction ground truth.</li>") << "</ul>"
               << "<h2>Artifacts</h2><p>scenario.json, metadata.json, waveform.csv, annotations.json, "
               << "rr_tachogram.csv, hrv_metrics.json, ground_truth_metrics.json, warnings.json, report.html, README.txt, synsigra.hea, synsigra.dat, synsigra.atr, wfdb_metadata.json, synsigra.edf, synsigra.bdf, edf_bdf_metadata.json</p></body></html>";
        return output.str();
    }

    void add_artifact(signal_synth::ecg_export_bundle& bundle, const char* name, const char* media_type, const std::string& content)
    {
        signal_synth::ecg_text_artifact artifact;
        artifact.name = name;
        artifact.media_type = media_type;
        artifact.content = content;
        bundle.artifacts.push_back(artifact);
    }
}

namespace signal_synth
{
    ecg_ground_truth_metrics::ecg_ground_truth_metrics()
        : beat_count(0), atrial_event_count(0), fiducial_count(0), episode_count(0), artifact_count(0), rr_clipping_count(0), mean_rr_seconds(0.0), mean_heart_rate_bpm(0.0), sdnn_seconds(0.0), rmssd_seconds(0.0), pnn50_percent(0.0), hrv_accepted_interval_count(0), hrv_excluded_interval_count(0), hrv_ectopic_interval_count(0), hrv_artifact_overlap_interval_count(0), sd1_seconds(0.0), sd2_seconds(0.0), sd1_sd2_ratio(0.0), lf_power_seconds2(0.0), hf_power_seconds2(0.0), lf_hf_ratio(0.0), total_power_seconds2(0.0), ppg_pulse_count(0), ppg_expected_pulse_count(0), ppg_missing_pulse_count(0), ppg_weak_pulse_count(0), ppg_low_perfusion_pulse_count(0), ppg_out_of_record_pulse_count(0), mean_ppg_onset_delay_seconds(0.0), mean_ppg_peak_delay_seconds(0.0), total_artifact_seconds(0.0), ppg_artifact_seconds(0.0)
    {
        for (unsigned int lead = 0; lead < clinical_lead_count; ++lead)
            ecg_artifact_seconds[lead] = 0.0;
    }

    const ecg_text_artifact* ecg_export_bundle::find(const std::string& name) const
    {
        for (std::size_t i = 0; i < artifacts.size(); ++i)
            if (artifacts[i].name == name)
                return &artifacts[i];
        return 0;
    }

    ecg_export_result::ecg_export_result() : success(false)
    {
    }

    const char* signal_synth_generator_version()
    {
        return "0.4.0-dev";
    }

    bool render_ecg_document(const ecg_scenario_document& document, ecg_render_bundle& output, ecg_export_result& result)
    {
        ecg_export_result fresh_result;
        ecg_render_bundle fresh;
        fresh.document = document;
        if (!write_ecg_scenario_json(document, fresh.document_identity))
        {
            fresh_result.messages.push_back("scenario document validation failed");
            result = fresh_result;
            return false;
        }
        std::vector<std::string> resolution_messages;
        if (!resolve_scenario_controls(document, fresh.resolved_document, fresh.parameter_draws, resolution_messages))
        {
            fresh_result.messages = resolution_messages;
            result = fresh_result;
            return false;
        }
        if (!write_ecg_scenario_json(fresh.resolved_document, fresh.resolved_document_identity))
        {
            fresh_result.messages.push_back("resolved scenario validation failed");
            result = fresh_result;
            return false;
        }
        if (!ecg_scenario_engine().generate(fresh.resolved_document.ecg, fresh.resolved_document.sample_count(), fresh.record, fresh.scenario_report))
        {
            if (fresh.scenario_report.issue_count())
                fresh_result.messages.push_back(std::string("ECG scenario generation failed: ") + fresh.scenario_report.issue_message(0));
            else
                fresh_result.messages.push_back("ECG scenario generation failed");
            result = fresh_result;
            return false;
        }
        {
            std::ostringstream identity;
            identity << fresh.document_identity.document_fingerprint;
            if (fresh.document.schema_version >= 3)
                identity << ":resolved-" << fresh.resolved_document_identity.document_fingerprint;
            identity << ":ecg-run-" << fresh.scenario_report.run_fingerprint();
            fresh.render_identity = identity.str();
        }
        if (!fresh.resolved_document.output.compact && !measure_ecg_morphology(fresh.record, fresh.morphology))
        {
            fresh_result.messages.push_back("ECG morphology measurement failed");
            result = fresh_result;
            return false;
        }
        if (!ppg_generator(fresh.resolved_document.ppg).generate(fresh.record, fresh.ppg))
        {
            fresh_result.messages.push_back("PPG generation failed");
            result = fresh_result;
            return false;
        }
        if (!apply_signal_quality_artifacts(fresh.resolved_document.signal_quality, fresh.record, fresh.ppg, fresh.signal_quality))
        {
            fresh_result.messages.push_back("signal quality artifact application failed");
            result = fresh_result;
            return false;
        }
        if (!apply_physiology_coupling(fresh.resolved_document.physiology, fresh.resolved_document.ppg.baseline_au, fresh.record.sampling_rate_hz(), fresh.signal_quality))
        {
            fresh_result.messages.push_back("physiology coupling failed");
            result = fresh_result;
            return false;
        }
        if (fresh.ppg.sample_count() && !remeasure_ppg_fiducials(fresh.signal_quality.ppg.data(), static_cast<unsigned int>(fresh.signal_quality.ppg.size()), fresh.ppg))
        {
            fresh_result.messages.push_back("final PPG peak measurement failed");
            result = fresh_result;
            return false;
        }
        fresh.metrics = calculate_metrics(fresh.record);
        add_ppg_metrics(fresh.ppg, fresh.metrics);
        if (!analyze_hrv_from_ecg(fresh.record, &fresh.signal_quality, fresh.hrv))
        {
            fresh_result.messages.push_back("HRV analysis failed");
            result = fresh_result;
            return false;
        }
        add_hrv_metrics(fresh.hrv, fresh.metrics);
        add_artifact_metrics(fresh.signal_quality, fresh.metrics);
        fresh_result.success = true;
        output = fresh;
        result = fresh_result;
        return true;
    }

    bool build_ecg_export_bundle(const ecg_render_bundle& render, ecg_export_bundle& output, ecg_export_result& result)
    {
        ecg_export_result fresh_result;
        ecg_export_bundle fresh;
        if (!render.document_identity.success || !render.scenario_report.success() || !render.record.sample_count() || render.record.lead_count() != clinical_lead_count)
        {
            fresh_result.messages.push_back("render bundle is incomplete");
            result = fresh_result;
            return false;
        }
        add_artifact(fresh, "scenario.json", "application/json", render.document_identity.canonical_json);
        if (render.document.schema_version >= 3)
        {
            add_artifact(fresh, "resolved_scenario.json", "application/json", render.resolved_document_identity.canonical_json);
            add_artifact(fresh, "randomization.json", "application/json", scenario_parameter_draws_json(render.document, render.resolved_document, render.parameter_draws));
        }
        add_artifact(fresh, "metadata.json", "application/json", metadata_json(render));
        if (render.resolved_document.output.include_waveform_csv)
            add_artifact(fresh, "waveform.csv", "text/csv", waveform_csv(render));
        add_artifact(fresh, "annotations.json", "application/json", annotations_json(render));
        add_artifact(fresh, "rr_tachogram.csv", "text/csv", rr_tachogram_csv(render));
        add_artifact(fresh, "hrv_metrics.json", "application/json", hrv_metrics_json(render));
        add_artifact(fresh, "ground_truth_metrics.json", "application/json", metrics_json(render));
        add_artifact(fresh, "warnings.json", "application/json", warnings_json(render));
        add_artifact(fresh, "report.html", "text/html", report_html(render));
        add_artifact(fresh, "README.txt", "text/plain",
            "Synsigra deterministic synthetic ECG engineering evidence package.\n"
            "Intended for research, development, software testing, and algorithm QA.\n"
            "Not for diagnosis, patient monitoring, clinical validation certification, or standalone conformity assessment.\n");
        wfdb_export_bundle wfdb;
        if (!build_wfdb_export_bundle(render, "synsigra", wfdb, fresh_result))
            return false;
        for (std::size_t i = 0; i < wfdb.artifacts.size(); ++i)
            add_artifact(fresh, wfdb.artifacts[i].name.c_str(), wfdb.artifacts[i].media_type.c_str(), wfdb.artifacts[i].content);
        if (render.resolved_document.output.include_edf_bdf)
        {
            edf_bdf_export_bundle edf_bdf;
            if (!build_edf_bdf_export_bundle(render, "synsigra", edf_bdf, fresh_result))
                return false;
            for (std::size_t i = 0; i < edf_bdf.artifacts.size(); ++i)
                add_artifact(fresh, edf_bdf.artifacts[i].name.c_str(), edf_bdf.artifacts[i].media_type.c_str(), edf_bdf.artifacts[i].content);
        }
        fresh_result.success = true;
        output = fresh;
        result = fresh_result;
        return true;
    }
}
