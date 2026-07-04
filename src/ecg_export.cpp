#include "ecg_export.h"
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
        if (!record.beat_count())
            return metrics;

        const signal_synth::clinical_beat_annotation* beats = record.beats();
        double sum = 0.0;
        for (unsigned int i = 0; i < record.beat_count(); ++i)
        {
            sum += beats[i].rr_interval_seconds;
            metrics.rr_clipping_count += beats[i].rr_was_clipped ? 1u : 0u;
        }
        metrics.mean_rr_seconds = sum / record.beat_count();
        metrics.mean_heart_rate_bpm = 60.0 / metrics.mean_rr_seconds;

        double variance = 0.0;
        double squared_differences = 0.0;
        unsigned int nn50 = 0;
        for (unsigned int i = 0; i < record.beat_count(); ++i)
        {
            const double difference = beats[i].rr_interval_seconds - metrics.mean_rr_seconds;
            variance += difference * difference;
            if (i)
            {
                const double successive = beats[i].rr_interval_seconds - beats[i - 1].rr_interval_seconds;
                squared_differences += successive * successive;
                nn50 += std::fabs(successive) > 0.05 ? 1u : 0u;
            }
        }
        metrics.sdnn_seconds = normalized_zero(std::sqrt(variance / record.beat_count()));
        if (record.beat_count() > 1)
        {
            metrics.rmssd_seconds = normalized_zero(std::sqrt(squared_differences / (record.beat_count() - 1)));
            metrics.pnn50_percent = 100.0 * nn50 / (record.beat_count() - 1);
        }
        return metrics;
    }

    void add_ppg_metrics(const signal_synth::ppg_record& ppg, signal_synth::ecg_ground_truth_metrics& metrics)
    {
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
        output << '\n';
        for (unsigned int sample = 0; sample < render.record.sample_count(); ++sample)
        {
            output << sample << ',' << static_cast<double>(sample) / render.record.sampling_rate_hz();
            for (unsigned int lead = 0; lead < render.record.lead_count(); ++lead)
                output << ',' << normalized_zero(rendered_ecg_lead(render, lead)[sample]);
            if (render.ppg.sample_count())
                output << ',' << normalized_zero(rendered_ppg(render)[sample]);
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
        }
        output << ']';
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
        for (unsigned int i = 0; i < render.record.fiducial_count(); ++i)
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
        output << "],\"episodes\":[";
        for (unsigned int i = 0; i < render.record.episode_count(); ++i)
        {
            if (i)
                output << ',';
            const signal_synth::clinical_episode_annotation& episode = render.record.episodes()[i];
            output << "{\"kind\":" << json_string(episode_kind_name(episode.kind))
                   << ",\"start_seconds\":" << episode.start_time_seconds
                   << ",\"end_seconds\":" << episode.end_time_seconds
                   << ",\"first_beat_index\":" << episode.first_beat_index
                   << ",\"last_beat_index\":" << episode.last_beat_index
                   << ",\"start_sample_index\":" << episode.start_sample_index
                   << ",\"end_sample_index\":" << episode.end_sample_index
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
            output << ']';
        }
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
               << "},\"hrv\":{\"mean_rr_seconds\":" << metrics.mean_rr_seconds
               << ",\"mean_heart_rate_bpm\":" << metrics.mean_heart_rate_bpm
               << ",\"sdnn_seconds\":" << metrics.sdnn_seconds
               << ",\"rmssd_seconds\":" << metrics.rmssd_seconds
               << ",\"pnn50_percent\":" << metrics.pnn50_percent
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
               << ",\"render_identity\":" << json_string(render.render_identity)
               << ",\"ecg_run_fingerprint\":" << render.scenario_report.run_fingerprint()
               << ",\"scenario_schema_version\":" << render.document.schema_version
               << ",\"engine_version\":" << render.scenario_report.engine_version()
               << ",\"seed\":" << render.document.ecg.seed()
               << "},\"render\":{\"sample_rate_hz\":" << render.record.sampling_rate_hz()
               << ",\"sample_count\":" << render.record.sample_count()
               << ",\"duration_seconds\":" << std::setprecision(std::numeric_limits<double>::max_digits10) << render.document.duration_seconds
               << ",\"channel_count\":" << render.record.lead_count() + (render.ppg.sample_count() ? 1u : 0u)
               << ",\"channels\":[";
        for (unsigned int lead = 0; lead < render.record.lead_count(); ++lead)
        {
            if (lead)
                output << ',';
            output << "{\"name\":" << json_string(render.record.lead_name(lead)) << ",\"unit\":\"mV\"}";
        }
        if (render.ppg.sample_count())
            output << ",{\"name\":\"ppg_green\",\"unit\":\"a.u.\"}";
        output << "],\"timestamp_policy\":\"not_recorded_for_deterministic_local_export\""
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
               << " %</td></tr><tr><th>Artifact intervals</th><td>" << render.metrics.artifact_count
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
               << "ground_truth_metrics.json, warnings.json, report.html, README.txt, synsigra.hea, synsigra.dat, synsigra.atr, wfdb_metadata.json</p></body></html>";
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
        : beat_count(0), atrial_event_count(0), fiducial_count(0), episode_count(0), artifact_count(0), rr_clipping_count(0), mean_rr_seconds(0.0), mean_heart_rate_bpm(0.0), sdnn_seconds(0.0), rmssd_seconds(0.0), pnn50_percent(0.0), ppg_pulse_count(0), mean_ppg_onset_delay_seconds(0.0), mean_ppg_peak_delay_seconds(0.0), total_artifact_seconds(0.0), ppg_artifact_seconds(0.0)
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
        return "0.1.0-dev";
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
        if (!ecg_scenario_engine().generate(document.ecg, document.sample_count(), fresh.record, fresh.scenario_report))
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
            identity << fresh.document_identity.document_fingerprint << ":ecg-run-" << fresh.scenario_report.run_fingerprint();
            fresh.render_identity = identity.str();
        }
        if (!measure_ecg_morphology(fresh.record, fresh.morphology))
        {
            fresh_result.messages.push_back("ECG morphology measurement failed");
            result = fresh_result;
            return false;
        }
        fresh.metrics = calculate_metrics(fresh.record);
        if (!ppg_generator(document.ppg).generate(fresh.record, fresh.ppg))
        {
            fresh_result.messages.push_back("PPG generation failed");
            result = fresh_result;
            return false;
        }
        add_ppg_metrics(fresh.ppg, fresh.metrics);
        if (!apply_signal_quality_artifacts(document.signal_quality, fresh.record, fresh.ppg, fresh.signal_quality))
        {
            fresh_result.messages.push_back("signal quality artifact application failed");
            result = fresh_result;
            return false;
        }
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
        add_artifact(fresh, "metadata.json", "application/json", metadata_json(render));
        add_artifact(fresh, "waveform.csv", "text/csv", waveform_csv(render));
        add_artifact(fresh, "annotations.json", "application/json", annotations_json(render));
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
        fresh_result.success = true;
        output = fresh;
        result = fresh_result;
        return true;
    }
}
