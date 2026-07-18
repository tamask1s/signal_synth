#include "ecg_render.h"
#include "wearable_profiles.h"

#include <sstream>

namespace
{
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
            if (ppg.pulses()[i].arrhythmia_linked)
            {
                ++metrics.ppg_arrhythmia_linked_pulse_count;
                if (ppg.pulses()[i].state == signal_synth::ppg_pulse_missing)
                    ++metrics.ppg_arrhythmia_linked_missing_pulse_count;
            }
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

    bool build_wearable_record(signal_synth::ecg_render_bundle& render)
    {
        if (render.resolved_document.schema_version < 5 || signal_synth::wearable_timebase_config_is_default(render.resolved_document.wearable))
            return true;
        signal_synth::wearable_timebase_record wearable;
        wearable.duration_seconds = render.resolved_document.duration_seconds;
        wearable.latent_sample_rate_hz = render.record.sampling_rate_hz();
        const unsigned int source_count = render.record.sample_count();
        const unsigned int source_rate = render.record.sampling_rate_hz();

        if (render.resolved_document.wearable.ecg.enabled)
        {
            std::vector<signal_synth::wearable_source_channel> channels;
            for (unsigned int lead = 0; lead < signal_synth::clinical_lead_count; ++lead)
            {
                const double* samples = lead < render.signal_quality.ecg_leads.size() && render.signal_quality.ecg_leads[lead].size() == source_count ? &render.signal_quality.ecg_leads[lead][0] : render.record.lead_data(lead);
                channels.push_back(signal_synth::wearable_source_channel(render.record.lead_name(lead), "mV", samples));
            }
            signal_synth::wearable_stream_record stream;
            if (!signal_synth::render_wearable_ecg_profile(render.resolved_document.wearable.ecg_profile_id.c_str(), render.resolved_document.wearable.ecg, wearable.duration_seconds, source_rate, source_count, channels, 4096u, stream))
                return false;
            wearable.streams.push_back(stream);
        }
        if (render.resolved_document.wearable.ppg.enabled)
        {
            std::vector<signal_synth::wearable_source_channel> channels;
            for (unsigned int channel = 0; channel < render.ppg.channel_count(); ++channel)
            {
                const double* samples = channel < render.signal_quality.ppg_channels.size() && render.signal_quality.ppg_channels[channel].size() == source_count ? &render.signal_quality.ppg_channels[channel][0] : render.ppg.channel_samples(channel);
                channels.push_back(signal_synth::wearable_source_channel(render.ppg.channel_name(channel), render.ppg.channel_unit(channel), samples));
            }
            signal_synth::wearable_stream_record stream;
            if (!signal_synth::render_wearable_stream(render.resolved_document.wearable.ppg, signal_synth::wearable_stream_ppg, wearable.duration_seconds, source_rate, source_count, channels, 4096u, stream))
                return false;
            stream.profile_id = render.ppg.optical_enabled() ? render.ppg.optical_config().profile_id : "green_generic_v1";
            stream.channel_clipping_counts.assign(stream.channel_samples.size(), 0u);
            for (std::size_t channel = 0; channel < stream.channel_clipping_counts.size() && channel < render.ppg_clipping_counts.size(); ++channel) stream.channel_clipping_counts[channel] = render.ppg_clipping_counts[channel];
            stream.fingerprint = signal_synth::wearable_stream_record_fingerprint(stream);
            wearable.streams.push_back(stream);
        }
        if (render.resolved_document.wearable.accelerometer.enabled)
        {
            std::vector<signal_synth::wearable_source_channel> channels;
            channels.push_back(signal_synth::wearable_source_channel("accel_motion", "g", render.signal_quality.accelerometer.empty() ? 0 : &render.signal_quality.accelerometer[0]));
            signal_synth::wearable_stream_record stream;
            if (!signal_synth::render_wearable_stream(render.resolved_document.wearable.accelerometer, signal_synth::wearable_stream_accelerometer, wearable.duration_seconds, source_rate, source_count, channels, 4096u, stream))
                return false;
            stream.profile_id = "synthetic_activity_v1";
            stream.fingerprint = signal_synth::wearable_stream_record_fingerprint(stream);
            wearable.streams.push_back(stream);
        }
        const signal_synth::wearable_stream_record* ecg = wearable.stream(signal_synth::wearable_stream_ecg);
        const signal_synth::wearable_stream_record* ppg = wearable.stream(signal_synth::wearable_stream_ppg);
        if (ecg && ppg && !signal_synth::build_wearable_alignment_truth(render.record, render.ppg, *ecg, *ppg, wearable.alignments))
            return false;
        wearable.fingerprint = signal_synth::wearable_timebase_record_fingerprint(wearable);
        render.wearable = wearable;
        render.render_identity += ":wearable-" + wearable.fingerprint;
        return true;
    }
}

namespace signal_synth
{
    ecg_ground_truth_metrics::ecg_ground_truth_metrics()
        : beat_count(0), atrial_event_count(0), fiducial_count(0), episode_count(0), artifact_count(0), rr_clipping_count(0), mean_rr_seconds(0.0), mean_heart_rate_bpm(0.0), sdnn_seconds(0.0), rmssd_seconds(0.0), pnn50_percent(0.0), hrv_accepted_interval_count(0), hrv_excluded_interval_count(0), hrv_ectopic_interval_count(0), hrv_artifact_overlap_interval_count(0), sd1_seconds(0.0), sd2_seconds(0.0), sd1_sd2_ratio(0.0), lf_power_seconds2(0.0), hf_power_seconds2(0.0), lf_hf_ratio(0.0), total_power_seconds2(0.0), ppg_pulse_count(0), ppg_expected_pulse_count(0), ppg_missing_pulse_count(0), ppg_weak_pulse_count(0), ppg_low_perfusion_pulse_count(0), ppg_arrhythmia_linked_pulse_count(0), ppg_arrhythmia_linked_missing_pulse_count(0), ppg_out_of_record_pulse_count(0), mean_ppg_onset_delay_seconds(0.0), mean_ppg_peak_delay_seconds(0.0), total_artifact_seconds(0.0), ppg_artifact_seconds(0.0)
    {
        for (unsigned int lead = 0; lead < clinical_lead_count; ++lead)
            ecg_artifact_seconds[lead] = 0.0;
    }

    ecg_document_render_result::ecg_document_render_result() : success(false)
    {
    }

    bool render_ecg_document(const ecg_scenario_document& document, ecg_render_bundle& output, ecg_document_render_result& result)
    {
        ecg_document_render_result fresh_result;
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
        if (!initialize_signal_quality_waveforms(fresh.record, fresh.ppg, fresh.signal_quality))
        {
            fresh_result.messages.push_back("observable waveform initialization failed");
            result = fresh_result;
            return false;
        }
        if (!apply_physiology_coupling(fresh.resolved_document.physiology, fresh.ppg, fresh.record.sampling_rate_hz(), fresh.signal_quality))
        {
            fresh_result.messages.push_back("physiology coupling failed");
            result = fresh_result;
            return false;
        }
        if (!apply_signal_quality_artifacts_in_place(fresh.resolved_document.signal_quality, fresh.record, fresh.ppg, fresh.signal_quality))
        {
            fresh_result.messages.push_back("signal quality artifact application failed");
            result = fresh_result;
            return false;
        }
        if (!finalize_ppg_sensor(fresh.ppg, fresh.signal_quality, fresh.ppg_clipping_counts))
        {
            fresh_result.messages.push_back("PPG sensor rendering failed");
            result = fresh_result;
            return false;
        }
        for (unsigned int channel = 0; channel < fresh.ppg.channel_count(); ++channel)
            if (!remeasure_ppg_channel_fiducials(channel, &fresh.signal_quality.ppg_channels[channel][0], static_cast<unsigned int>(fresh.signal_quality.ppg_channels[channel].size()), fresh.ppg))
            {
                fresh_result.messages.push_back("final PPG fiducial measurement failed");
                result = fresh_result;
                return false;
            }
        if (!build_wearable_record(fresh))
        {
            fresh_result.messages.push_back("wearable timebase rendering failed");
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
}
