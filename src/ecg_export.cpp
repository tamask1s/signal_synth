#include "ecg_export.h"
#include "challenge_package.h"
#include "ecg_beat_classification.h"
#include "ecg_edf_bdf_export.h"
#include "ecg_wfdb_export.h"
#include "realism_validation.h"
#include "wearable_profiles.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

#ifndef SIGNAL_SYNTH_GIT_COMMIT
#define SIGNAL_SYNTH_GIT_COMMIT "unknown"
#endif

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

    std::string json_u64_string(unsigned long long value)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << '"' << value << '"';
        return output.str();
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
        case signal_synth::clinical_origin_fusion: return "fusion";
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
        case signal_synth::clinical_p_secondary_peak: return "p_secondary_peak";
        case signal_synth::clinical_p_notch: return "p_notch";
        case signal_synth::clinical_r_prime: return "r_prime";
        case signal_synth::clinical_qrs_fragment: return "qrs_fragment";
        case signal_synth::clinical_t_secondary_peak: return "t_secondary_peak";
        case signal_synth::clinical_t_notch: return "t_notch";
        case signal_synth::clinical_u_onset: return "u_onset";
        case signal_synth::clinical_u_peak: return "u_peak";
        case signal_synth::clinical_u_offset: return "u_offset";
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
        case signal_synth::clinical_episode_repolarization: return "dynamic_repolarization";
        case signal_synth::clinical_episode_afib: return "afib";
        case signal_synth::clinical_episode_vt: return "vt";
        case signal_synth::clinical_episode_vf: return "vf";
        case signal_synth::clinical_episode_asystole: return "asystole";
        }
        return "unknown";
    }

    const char* dynamic_annotation_kind_name(signal_synth::clinical_dynamic_annotation_kind value)
    {
        switch (value)
        {
        case signal_synth::clinical_dynamic_repolarization_severity: return "repolarization_severity";
        case signal_synth::clinical_dynamic_qt_interval_ms: return "qt_interval_ms";
        case signal_synth::clinical_dynamic_qtc_ms: return "qtc_ms";
        case signal_synth::clinical_dynamic_st_j_amplitude_mv: return "st_j_amplitude_mv";
        case signal_synth::clinical_dynamic_st_slope_mv_per_second: return "st_slope_mv_per_second";
        case signal_synth::clinical_dynamic_t_amplitude_mv: return "t_amplitude_mv";
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

    const double* rendered_ecg_lead(const signal_synth::ecg_render_bundle& render, unsigned int lead)
    {
        if (lead < render.signal_quality.ecg_leads.size() && render.signal_quality.ecg_leads[lead].size() == render.record.sample_count())
            return render.signal_quality.ecg_leads[lead].empty() ? 0 : &render.signal_quality.ecg_leads[lead][0];
        return render.record.lead_data(lead);
    }

    const double* rendered_ppg(const signal_synth::ecg_render_bundle& render)
    {
        if (!render.signal_quality.ppg_channels.empty() && render.signal_quality.ppg_channels[0].size() == render.ppg.sample_count())
            return render.signal_quality.ppg_channels[0].empty() ? 0 : &render.signal_quality.ppg_channels[0][0];
        return render.ppg.samples();
    }

    const double* rendered_ppg_channel(const signal_synth::ecg_render_bundle& render, unsigned int channel)
    {
        if (channel < render.signal_quality.ppg_channels.size() && render.signal_quality.ppg_channels[channel].size() == render.ppg.sample_count()) return render.signal_quality.ppg_channels[channel].empty() ? 0 : &render.signal_quality.ppg_channels[channel][0];
        return render.ppg.channel_samples(channel);
    }

    std::string waveform_csv(const signal_synth::ecg_render_bundle& render)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10);
        output << "sample_index,time_seconds";
        for (unsigned int lead = 0; lead < render.record.lead_count(); ++lead)
            output << ',' << render.record.lead_name(lead) << "_mv";
        for (unsigned int channel = 0; channel < render.ppg.channel_count(); ++channel)
            output << ',' << render.ppg.channel_name(channel) << "_au";
        if (!render.signal_quality.accelerometer.empty())
            output << ",accel_motion_g";
        output << '\n';
        for (unsigned int sample = 0; sample < render.record.sample_count(); ++sample)
        {
            output << sample << ',' << static_cast<double>(sample) / render.record.sampling_rate_hz();
            for (unsigned int lead = 0; lead < render.record.lead_count(); ++lead)
                output << ',' << normalized_zero(rendered_ecg_lead(render, lead)[sample]);
            for (unsigned int channel = 0; channel < render.ppg.channel_count(); ++channel)
                output << ',' << normalized_zero(rendered_ppg_channel(render, channel)[sample]);
            if (!render.signal_quality.accelerometer.empty())
                output << ',' << normalized_zero(render.signal_quality.accelerometer[sample]);
            output << '\n';
        }
        return output.str();
    }

    std::string external_noise_clean_ecg_csv(const signal_synth::ecg_render_bundle& render)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10) << "sample_index,time_seconds";
        for (unsigned int lead = 0; lead < render.record.lead_count(); ++lead) output << ',' << render.record.lead_name(lead) << "_mv";
        output << '\n';
        for (unsigned int sample = 0; sample < render.record.sample_count(); ++sample)
        {
            output << sample << ',' << static_cast<double>(sample) / render.record.sampling_rate_hz();
            for (unsigned int lead = 0; lead < render.record.lead_count(); ++lead) output << ',' << normalized_zero(render.external_noise_clean_ecg_leads[lead][sample]);
            output << '\n';
        }
        return output.str();
    }

    int ppg_channel_index(const signal_synth::ppg_record& record, signal_synth::ppg_channel_kind kind)
    {
        for (unsigned int channel = 0; channel < record.channel_count(); ++channel) if (record.channel_kind(channel) == kind) return static_cast<int>(channel);
        return -1;
    }

    std::string ppg_optical_latent_csv(const signal_synth::ecg_render_bundle& render)
    {
        const int red = ppg_channel_index(render.ppg, signal_synth::ppg_channel_red);
        const int infrared = ppg_channel_index(render.ppg, signal_synth::ppg_channel_infrared);
        if (red < 0 || infrared < 0) return std::string();
        std::ostringstream output; output.imbue(std::locale::classic()); output << std::setprecision(std::numeric_limits<double>::max_digits10)
            << "sample_index,time_seconds,red_latent_au,infrared_latent_au,red_sensor_au,infrared_sensor_au\n";
        const double* red_latent = render.ppg.channel_samples(static_cast<unsigned int>(red));
        const double* infrared_latent = render.ppg.channel_samples(static_cast<unsigned int>(infrared));
        const double* red_sensor = rendered_ppg_channel(render, static_cast<unsigned int>(red));
        const double* infrared_sensor = rendered_ppg_channel(render, static_cast<unsigned int>(infrared));
        for (unsigned int sample = 0; sample < render.ppg.sample_count(); ++sample)
            output << sample << ',' << static_cast<double>(sample) / render.ppg.sampling_rate_hz() << ',' << red_latent[sample] << ',' << infrared_latent[sample] << ',' << red_sensor[sample] << ',' << infrared_sensor[sample] << '\n';
        return output.str();
    }

    std::string ppg_optical_truth_json(const signal_synth::ecg_render_bundle& render)
    {
        const signal_synth::ppg_optical_config& config = render.ppg.optical_config();
        std::ostringstream output; output.imbue(std::locale::classic()); output << std::setprecision(std::numeric_limits<double>::max_digits10)
            << "{\"schema_version\":1,\"contract\":\"synsigra_ppg_optical_truth_v2\",\"profile_id\":" << json_string(config.profile_id)
            << ",\"resolved_site_parameters\":{\"pulse_delay_ms\":" << render.resolved_document.ppg.pulse_delay_ms << ",\"rise_time_ms\":" << render.resolved_document.ppg.rise_time_ms << ",\"decay_time_ms\":" << render.resolved_document.ppg.decay_time_ms << ",\"amplitude_au\":" << render.resolved_document.ppg.amplitude_au << ",\"dicrotic_delay_ms\":" << render.resolved_document.ppg.dicrotic_delay_ms << ",\"dicrotic_width_ms\":" << render.resolved_document.ppg.dicrotic_width_ms << ",\"dicrotic_amplitude_ratio\":" << render.resolved_document.ppg.dicrotic_amplitude_ratio << "}"
            << ",\"calibration\":{\"id\":" << json_string(config.calibration_id)
            << ",\"equation\":\"spo2_percent = intercept_percent + slope_percent * ratio_of_ratios\",\"intercept_percent\":" << config.calibration_intercept_percent << ",\"slope_percent\":" << config.calibration_slope_percent
            << ",\"minimum_spo2_percent\":" << config.minimum_spo2_percent << ",\"maximum_spo2_percent\":" << config.maximum_spo2_percent << "},\"units\":{\"optical_amplitude\":\"a.u.\",\"perfusion_index\":\"%\",\"oxygen_saturation\":\"%\",\"time\":\"s\"},\"optical_equations\":{\"perfusion_index\":\"100 * AC / DC\",\"ratio_of_ratios\":\"(AC_red / DC_red) / (AC_infrared / DC_infrared)\"},\"channels\":[";
        for (unsigned int channel = 1u; channel < render.ppg.channel_count(); ++channel)
            output << (channel == 1u ? "" : ",") << "{\"name\":" << json_string(render.ppg.channel_name(channel)) << ",\"dc_au\":" << render.ppg.channel_dc_au(channel) << ",\"sensor_gain\":" << render.ppg.channel_sensor_gain(channel) << ",\"delay_ms\":" << render.ppg.channel_delay_ms(channel) << ",\"noise_std_au\":" << render.ppg.channel_noise_std_au(channel) << ",\"ambient_offset_au\":" << render.ppg.channel_ambient_offset_au(channel) << ",\"motion_sensitivity\":" << render.ppg.channel_motion_sensitivity(channel) << ",\"ambient_sensitivity\":" << render.ppg.channel_ambient_sensitivity(channel) << ",\"crosstalk_ratio\":" << render.ppg.channel_crosstalk_ratio(channel) << ",\"minimum_output_au\":" << render.ppg.channel_minimum_output_au(channel) << ",\"maximum_output_au\":" << render.ppg.channel_maximum_output_au(channel) << ",\"quantization_bits\":" << render.ppg.channel_quantization_bits(channel) << ",\"seed\":" << json_u64_string(render.ppg.channel_seed(channel)) << ",\"clipping_count\":" << (channel < render.ppg_clipping_counts.size() ? render.ppg_clipping_counts[channel] : 0u) << '}';
        output << "],\"pulses\":[";
        const signal_synth::ppg_optical_pulse_state* states = render.ppg.optical_states();
        for (unsigned int i = 0; states && i < render.ppg.optical_state_count(); ++i)
            output << (i ? "," : "") << "{\"ecg_beat_index\":\"" << states[i].ecg_beat_index << "\",\"time_seconds\":" << states[i].time_seconds << ",\"spo2_percent\":" << states[i].spo2_percent << ",\"ratio_of_ratios\":" << states[i].ratio_of_ratios << ",\"red_dc_au\":" << states[i].red_dc_au << ",\"red_ac_au\":" << states[i].red_ac_au << ",\"red_perfusion_index_percent\":" << states[i].red_perfusion_index_percent << ",\"infrared_dc_au\":" << states[i].infrared_dc_au << ",\"infrared_ac_au\":" << states[i].infrared_ac_au << ",\"infrared_perfusion_index_percent\":" << states[i].infrared_perfusion_index_percent << ",\"calibration_in_range\":" << (states[i].calibration_in_range ? "true" : "false") << ",\"generated\":" << (states[i].generated ? "true" : "false") << ",\"valid_for_measurement\":" << (states[i].valid_for_measurement ? "true" : "false") << '}';
        output << "],\"claim_boundary\":\"Engineering optical simulation only; no clinical SpO2 or pulse-oximeter validation claim.\"}";
        return output.str();
    }

    const char* wearable_sample_artifact_name(signal_synth::wearable_stream_kind kind)
    {
        switch (kind)
        {
        case signal_synth::wearable_stream_ecg: return "wearable_ecg_samples.csv";
        case signal_synth::wearable_stream_ppg: return "wearable_ppg_samples.csv";
        case signal_synth::wearable_stream_accelerometer: return "wearable_accelerometer_samples.csv";
        }
        return "wearable_samples.csv";
    }

    std::string wearable_samples_csv(const signal_synth::wearable_stream_record& stream)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10) << "sample_index,packet_index,device_timestamp_seconds";
        for (std::size_t channel = 0; channel < stream.channel_names.size(); ++channel)
            output << ',' << stream.channel_names[channel];
        output << '\n';
        for (std::size_t sample = 0; sample < stream.samples.size(); ++sample)
        {
            if (!stream.samples[sample].received)
                continue;
            output << stream.samples[sample].sample_index << ',' << stream.samples[sample].packet_index << ',' << stream.samples[sample].reported_device_time_seconds;
            for (std::size_t channel = 0; channel < stream.channel_samples.size(); ++channel)
                output << ',' << normalized_zero(stream.channel_samples[channel][sample]);
            output << '\n';
        }
        return output.str();
    }

    std::string wearable_timestamp_truth_csv(const signal_synth::wearable_timebase_record& wearable)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10)
               << "stream,sample_index,packet_index,latent_time_seconds,ideal_device_time_seconds,reported_device_time_seconds,received\n";
        for (std::size_t stream = 0; stream < wearable.streams.size(); ++stream)
            for (std::size_t sample = 0; sample < wearable.streams[stream].samples.size(); ++sample)
            {
                const signal_synth::wearable_sample_mapping& item = wearable.streams[stream].samples[sample];
                output << signal_synth::wearable_stream_kind_name(wearable.streams[stream].kind) << ',' << item.sample_index << ',' << item.packet_index
                       << ',' << item.latent_time_seconds << ',' << item.ideal_device_time_seconds << ',' << item.reported_device_time_seconds << ',' << (item.received ? 1 : 0) << '\n';
            }
        return output.str();
    }

    void write_wearable_event_json(std::ostringstream& output, const signal_synth::wearable_event_mapping& event)
    {
        output << "{\"present\":" << boolean(event.present);
        if (event.present)
            output << ",\"latent_time_seconds\":" << event.latent_time_seconds
                   << ",\"sample_index\":" << json_u64_string(event.sample_index)
                   << ",\"reported_device_time_seconds\":" << event.reported_device_time_seconds
                   << ",\"received\":" << boolean(event.received);
        output << '}';
    }

    void write_optional_number(std::ostringstream& output, bool present, double value)
    {
        if (present) output << value;
        else output << "null";
    }

    std::string wearable_timebase_truth_json(const signal_synth::wearable_timebase_record& wearable)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10)
               << "{\"schema_version\":2,\"contract\":\"synsigra_wearable_timebase_v3\",\"fingerprint\":" << json_string(wearable.fingerprint)
               << ",\"latent_reference\":{\"duration_seconds\":" << wearable.duration_seconds << ",\"sample_rate_hz\":" << wearable.latent_sample_rate_hz << "}"
               << ",\"mapping\":{\"clock_scale\":\"1 + clock_drift_ppm * 1e-6\",\"latent_time_seconds\":\"sample_index / (sample_rate_hz * clock_scale)\",\"ideal_device_time_seconds\":\"clock_offset_ms / 1000 + sample_index / sample_rate_hz\",\"reported_device_time_seconds\":\"ideal_device_time_seconds + deterministic_timestamp_jitter\",\"resampling\":\"linear_interpolation\"},\"streams\":[";
        for (std::size_t stream_index = 0; stream_index < wearable.streams.size(); ++stream_index)
        {
            const signal_synth::wearable_stream_record& stream = wearable.streams[stream_index];
            if (stream_index) output << ',';
            output << "{\"kind\":" << json_string(signal_synth::wearable_stream_kind_name(stream.kind))
                   << ",\"profile_id\":" << json_string(stream.profile_id)
                   << ",\"sample_rate_hz\":" << stream.config.sample_rate_hz
                   << ",\"clock_offset_ms\":" << stream.config.clock_offset_ms
                   << ",\"clock_drift_ppm\":" << stream.config.clock_drift_ppm
                   << ",\"timestamp_jitter_ms\":" << stream.config.timestamp_jitter_ms
                   << ",\"packet_size_samples\":" << stream.config.packet_size_samples
                   << ",\"packet_loss_probability\":" << stream.config.packet_loss_probability
                   << ",\"packet_loss_burst_packets\":" << stream.config.packet_loss_burst_packets
                   << ",\"seed\":" << json_u64_string(stream.config.seed)
                   << ",\"sample_count\":" << stream.sample_count() << ",\"received_sample_count\":" << stream.received_sample_count()
                   << ",\"fingerprint\":" << json_string(stream.fingerprint) << ",\"channels\":[";
            for (std::size_t channel = 0; channel < stream.channel_names.size(); ++channel)
                output << (channel ? "," : "") << "{\"name\":" << json_string(stream.channel_names[channel]) << ",\"unit\":" << json_string(stream.channel_units[channel]) << ",\"clipping_count\":" << (channel < stream.channel_clipping_counts.size() ? stream.channel_clipping_counts[channel] : 0u) << '}';
            output << ']';
            const signal_synth::wearable_ecg_profile_info* profile = stream.kind == signal_synth::wearable_stream_ecg ? signal_synth::find_wearable_ecg_profile(stream.profile_id.c_str()) : 0;
            if (profile)
            {
                output << ",\"resolved_profile\":{\"placement\":" << json_string(profile->placement) << ",\"channel_name\":" << json_string(profile->channel_name) << ",\"preserve_standard_12_lead\":" << boolean(profile->preserve_standard_12_lead) << ",\"lead_order\":[\"I\",\"II\",\"III\",\"aVR\",\"aVL\",\"aVF\",\"V1\",\"V2\",\"V3\",\"V4\",\"V5\",\"V6\"],\"lead_weights\":[";
                for (unsigned int lead = 0; lead < 12u; ++lead) output << (lead ? "," : "") << profile->lead_weights[lead];
                output << "],\"highpass_hz\":" << profile->highpass_hz << ",\"lowpass_hz\":" << profile->lowpass_hz << ",\"gain\":" << profile->gain << ",\"minimum_output_mv\":" << profile->minimum_output_mv << ",\"maximum_output_mv\":" << profile->maximum_output_mv << ",\"quantization_bits\":" << profile->quantization_bits << '}';
            }
            output << ",\"packets\":[";
            for (std::size_t packet = 0; packet < stream.packets.size(); ++packet)
            {
                const signal_synth::wearable_packet_annotation& item = stream.packets[packet];
                output << (packet ? "," : "") << "{\"packet_index\":" << json_u64_string(item.packet_index)
                       << ",\"first_sample_index\":" << json_u64_string(item.first_sample_index) << ",\"sample_count\":" << item.sample_count
                       << ",\"first_latent_time_seconds\":" << item.first_latent_time_seconds << ",\"last_latent_time_seconds\":" << item.last_latent_time_seconds
                       << ",\"first_reported_device_time_seconds\":" << item.first_reported_device_time_seconds << ",\"last_reported_device_time_seconds\":" << item.last_reported_device_time_seconds
                       << ",\"dropped\":" << boolean(item.dropped) << '}';
            }
            output << "]}";
        }
        output << "]}";
        return output.str();
    }

    std::string wearable_alignment_truth_json(const signal_synth::wearable_timebase_record& wearable)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10)
               << "{\"schema_version\":1,\"contract\":\"synsigra_wearable_alignment_v1\",\"time_domains\":{\"physiological\":\"latent_reference\",\"observed\":\"reported_device_timestamps\"},\"events\":[";
        for (std::size_t i = 0; i < wearable.alignments.size(); ++i)
        {
            const signal_synth::wearable_alignment_annotation& item = wearable.alignments[i];
            if (i) output << ',';
            output << "{\"ecg_beat_index\":" << json_u64_string(item.ecg_beat_index) << ",\"intentionally_missing\":" << boolean(item.intentionally_missing) << ",\"ecg_r\":";
            write_wearable_event_json(output, item.ecg_r);
            output << ",\"ppg_onset\":"; write_wearable_event_json(output, item.ppg_onset);
            output << ",\"ppg_peak\":"; write_wearable_event_json(output, item.ppg_peak);
            output << ",\"physiological_onset_delay_seconds\":"; write_optional_number(output, item.has_physiological_onset_delay, item.physiological_onset_delay_seconds);
            output << ",\"physiological_peak_delay_seconds\":"; write_optional_number(output, item.has_physiological_peak_delay, item.physiological_peak_delay_seconds);
            output << ",\"observed_onset_device_delta_seconds\":"; write_optional_number(output, item.has_observed_onset_device_delta, item.observed_onset_device_delta_seconds);
            output << ",\"onset_observed_minus_physiological_seconds\":"; write_optional_number(output, item.has_observed_onset_device_delta, item.onset_observed_minus_physiological_seconds);
            output << ",\"observed_peak_device_delta_seconds\":"; write_optional_number(output, item.has_observed_peak_device_delta, item.observed_peak_device_delta_seconds);
            output << ",\"peak_observed_minus_physiological_seconds\":"; write_optional_number(output, item.has_observed_peak_device_delta, item.peak_observed_minus_physiological_seconds);
            output << '}';
        }
        output << "]}";
        return output.str();
    }

    void write_artifact_channels(std::ostringstream& output, const signal_synth::signal_quality_artifact_interval& artifact, const signal_synth::ppg_record& ppg)
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
            for (unsigned int channel = 0; channel < ppg.channel_count(); ++channel)
            {
                if (!first) output << ',';
                output << json_string(ppg.channel_name(channel));
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
               << ",\"vlf_band_hz\":[" << hrv.vlf_low_hz << ',' << hrv.vlf_high_hz << ']'
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
               << ",\"frequency_domain\":{\"vlf_power_seconds2\":" << metrics.vlf_power_seconds2
               << ",\"lf_power_seconds2\":" << metrics.lf_power_seconds2
               << ",\"hf_power_seconds2\":" << metrics.hf_power_seconds2
               << ",\"lf_hf_ratio\":" << metrics.lf_hf_ratio
               << ",\"lf_normalized_units\":" << metrics.lf_normalized_units
               << ",\"hf_normalized_units\":" << metrics.hf_normalized_units
               << ",\"total_power_seconds2\":" << metrics.total_power_seconds2 << '}'
               << ",\"tachogram\":";
        write_rr_tachogram_json(output, hrv);
        output << '}';
        return output.str();
    }

    void append_variability_summary(std::ostringstream& output, const signal_synth::hrv_metric_summary& metrics)
    {
        output << "{\"interval_count\":" << metrics.interval_count << ",\"accepted_interval_count\":" << metrics.accepted_interval_count << ",\"excluded_interval_count\":" << metrics.excluded_interval_count
               << ",\"mean_interval_seconds\":" << metrics.mean_rr_seconds << ",\"mean_rate_bpm\":" << metrics.mean_heart_rate_bpm << ",\"sdnn_seconds\":" << metrics.sdnn_seconds << ",\"rmssd_seconds\":" << metrics.rmssd_seconds
               << ",\"pnn50_percent\":" << metrics.pnn50_percent << ",\"sd1_seconds\":" << metrics.sd1_seconds << ",\"sd2_seconds\":" << metrics.sd2_seconds << ",\"sd1_sd2_ratio\":" << metrics.sd1_sd2_ratio
               << ",\"vlf_power_seconds2\":" << metrics.vlf_power_seconds2 << ",\"lf_power_seconds2\":" << metrics.lf_power_seconds2 << ",\"hf_power_seconds2\":" << metrics.hf_power_seconds2 << ",\"lf_hf_ratio\":" << metrics.lf_hf_ratio << ",\"lf_normalized_units\":" << metrics.lf_normalized_units << ",\"hf_normalized_units\":" << metrics.hf_normalized_units << ",\"total_power_seconds2\":" << metrics.total_power_seconds2 << '}';
    }

    std::string cardiorespiratory_truth_json(const signal_synth::ecg_render_bundle& render)
    {
        const signal_synth::cardiorespiratory_analysis_result& analysis = render.cardiorespiratory;
        const signal_synth::hrv_metric_summary& hrv = render.hrv.metrics;
        const signal_synth::hrv_metric_summary& prv = analysis.prv.metrics;
        std::ostringstream output; output.imbue(std::locale::classic()); output << std::setprecision(std::numeric_limits<double>::max_digits10)
            << "{\"schema_version\":1,\"contract\":\"synsigra_cardiorespiratory_truth_v1\",\"prv_available\":" << boolean(analysis.prv_available) << ",\"respiration_available\":" << boolean(analysis.respiration_available);
        if (analysis.prv_available)
        {
            output << ",\"prv\":{\"definition_version\":" << json_string(analysis.prv.metric_definition_version) << ",\"exclusion_policy\":" << json_string(analysis.prv.exclusion_policy) << ",\"metrics\":";
            append_variability_summary(output, prv);
            output << "},\"hrv_prv_agreement\":{\"signed_difference_definition\":\"prv_minus_hrv\",\"mean_interval_seconds\":" << prv.mean_rr_seconds - hrv.mean_rr_seconds << ",\"mean_rate_bpm\":" << prv.mean_heart_rate_bpm - hrv.mean_heart_rate_bpm
                   << ",\"sdnn_seconds\":" << prv.sdnn_seconds - hrv.sdnn_seconds << ",\"rmssd_seconds\":" << prv.rmssd_seconds - hrv.rmssd_seconds << ",\"pnn50_percent\":" << prv.pnn50_percent - hrv.pnn50_percent
                   << ",\"sd1_seconds\":" << prv.sd1_seconds - hrv.sd1_seconds << ",\"sd2_seconds\":" << prv.sd2_seconds - hrv.sd2_seconds << ",\"sd1_sd2_ratio\":" << prv.sd1_sd2_ratio - hrv.sd1_sd2_ratio
                   << ",\"vlf_power_seconds2\":" << prv.vlf_power_seconds2 - hrv.vlf_power_seconds2 << ",\"lf_power_seconds2\":" << prv.lf_power_seconds2 - hrv.lf_power_seconds2 << ",\"hf_power_seconds2\":" << prv.hf_power_seconds2 - hrv.hf_power_seconds2 << ",\"lf_hf_ratio\":" << prv.lf_hf_ratio - hrv.lf_hf_ratio << ",\"lf_normalized_units\":" << prv.lf_normalized_units - hrv.lf_normalized_units << ",\"hf_normalized_units\":" << prv.hf_normalized_units - hrv.hf_normalized_units << '}';
        }
        if (analysis.respiration_available)
            output << ",\"respiration\":{\"reference_sample_rate_hz\":" << analysis.respiration_sample_rate_hz << ",\"frequency_hz\":" << render.resolved_document.physiology.respiration_frequency_hz << ",\"rate_bpm\":" << analysis.respiratory_rate_bpm << ",\"phase_radians\":" << analysis.respiration_phase_radians
                   << ",\"couplings\":{\"rr_amplitude_seconds\":" << render.resolved_document.physiology.respiratory_rr_amplitude_seconds << ",\"ecg_baseline_amplitude_mv\":" << render.resolved_document.physiology.ecg_baseline_amplitude_mv
                   << ",\"ppg_amplitude_modulation_ratio\":" << render.resolved_document.physiology.ppg_amplitude_modulation_ratio << ",\"ppg_delay_modulation_ms\":" << render.resolved_document.physiology.ppg_delay_modulation_ms
                   << ",\"accelerometer_amplitude_g\":" << render.resolved_document.physiology.accelerometer_respiration_amplitude_g << "}}";
        output << ",\"claim_boundary\":\"Engineering cardiorespiratory coupling and algorithm QA only; no clinical respiration or autonomic validation claim.\"}";
        return output.str();
    }

    std::string prv_tachogram_csv(const signal_synth::cardiorespiratory_analysis_result& analysis)
    {
        std::ostringstream output; output.imbue(std::locale::classic()); output << std::setprecision(std::numeric_limits<double>::max_digits10) << "beat_index,peak_time_seconds,pulse_interval_seconds,excluded,low_quality_or_missing,arrhythmia_linked,artifact_overlap\n";
        for (std::size_t i = 0; i < analysis.prv.intervals.size(); ++i)
        {
            const signal_synth::hrv_rr_interval& interval = analysis.prv.intervals[i];
            output << interval.beat_index << ',' << interval.beat_time_seconds << ',' << interval.rr_seconds << ',' << (interval.excluded ? 1 : 0) << ',' << (interval.excluded && !interval.ectopic && !interval.artifact_overlap ? 1 : 0) << ',' << (interval.ectopic ? 1 : 0) << ',' << (interval.artifact_overlap ? 1 : 0) << '\n';
        }
        return output.str();
    }

    std::string respiration_reference_csv(const signal_synth::cardiorespiratory_analysis_result& analysis)
    {
        std::ostringstream output; output.imbue(std::locale::classic()); output << std::setprecision(std::numeric_limits<double>::max_digits10) << "time_seconds,phase_radians,respiration_reference,respiratory_rate_bpm\n";
        for (std::size_t i = 0; i < analysis.respiration.size(); ++i) output << analysis.respiration[i].time_seconds << ',' << analysis.respiration[i].phase_radians << ',' << analysis.respiration[i].waveform << ',' << analysis.respiration[i].respiratory_rate_bpm << '\n';
        return output.str();
    }

    std::string annotations_json(const signal_synth::ecg_render_bundle& render)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10);
        output << "{\"schema_version\":1,\"document_fingerprint\":" << json_string(render.document_identity.document_fingerprint)
               << ",\"generation_fingerprint\":" << json_u64_string(render.document_identity.generation_fingerprint)
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
                   << ",\"fusion_ventricular_fraction\":" << beat.fusion_ventricular_fraction
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
        output << "],\"dynamic_traces\":[";
        for (unsigned int i = 0; detailed_annotations && i < render.record.dynamic_annotation_count(); ++i)
        {
            if (i)
                output << ',';
            const signal_synth::clinical_dynamic_annotation& annotation = render.record.dynamic_annotations()[i];
            output << "{\"annotation_index\":" << annotation.annotation_index
                   << ",\"beat_index\":" << annotation.beat_index
                   << ",\"kind\":" << json_string(dynamic_annotation_kind_name(annotation.kind))
                   << ",\"time_seconds\":" << annotation.time_seconds
                   << ",\"sample_index\":" << annotation.sample_index
                   << ",\"value\":" << annotation.value
                   << ",\"present\":" << boolean(annotation.present) << '}';
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
                       << ",\"channel\":\"ppg_green\""
                       << ",\"kind\":" << json_string(kind)
                       << ",\"source\":" << json_string(annotation.source == signal_synth::ppg_fiducial_construction ? "construction" : "measurement")
                       << ",\"sample_index\":" << annotation.sample_index
                       << ",\"time_seconds\":" << annotation.time_seconds
                       << ",\"value_au\":" << annotation.value_au << '}';
            }
            output << "],\"ppg_channel_fiducials\":[";
            bool first_channel_fiducial = true;
            for (unsigned int channel = 1; channel < render.ppg.channel_count(); ++channel)
            {
                const signal_synth::ppg_annotation* channel_annotations = render.ppg.channel_annotations(channel);
                for (unsigned int i = 0; i < render.ppg.channel_annotation_count(channel); ++i)
                {
                    if (!first_channel_fiducial)
                        output << ',';
                    first_channel_fiducial = false;
                    const signal_synth::ppg_annotation& annotation = channel_annotations[i];
                    const char* kind = annotation.kind == signal_synth::ppg_pulse_onset ? "pulse_onset"
                        : annotation.kind == signal_synth::ppg_systolic_peak ? "systolic_peak"
                        : annotation.kind == signal_synth::ppg_dicrotic_feature ? "dicrotic_feature" : "pulse_offset";
                    output << "{\"ecg_beat_index\":" << annotation.ecg_beat_index
                           << ",\"ecg_r_time_seconds\":" << annotation.ecg_r_time_seconds
                           << ",\"channel\":" << json_string(render.ppg.channel_name(channel))
                           << ",\"kind\":" << json_string(kind)
                           << ",\"source\":" << json_string(annotation.source == signal_synth::ppg_fiducial_construction ? "construction" : "measurement")
                           << ",\"sample_index\":" << annotation.sample_index
                           << ",\"time_seconds\":" << annotation.time_seconds
                           << ",\"value_au\":" << annotation.value_au << '}';
                }
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
                       << ",\"arrhythmia_linked\":" << boolean(pulse.arrhythmia_linked)
                       << ",\"arrhythmia_amplitude_scale\":" << pulse.arrhythmia_amplitude_scale
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
            write_artifact_channels(output, artifact, render.ppg);
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
               << ",\"vlf_power_seconds2\":" << metrics.vlf_power_seconds2
               << ",\"lf_power_seconds2\":" << metrics.lf_power_seconds2
               << ",\"hf_power_seconds2\":" << metrics.hf_power_seconds2
               << ",\"lf_hf_ratio\":" << metrics.lf_hf_ratio
               << ",\"lf_normalized_units\":" << metrics.lf_normalized_units
               << ",\"hf_normalized_units\":" << metrics.hf_normalized_units
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
                   << ",\"channel_count\":" << render.ppg.channel_count()
                   << ",\"expected_pulse_count\":" << metrics.ppg_expected_pulse_count
                   << ",\"intentionally_missing_pulse_count\":" << metrics.ppg_missing_pulse_count
                   << ",\"weak_pulse_count\":" << metrics.ppg_weak_pulse_count
                   << ",\"low_perfusion_pulse_count\":" << metrics.ppg_low_perfusion_pulse_count
                   << ",\"arrhythmia_linked_pulse_count\":" << metrics.ppg_arrhythmia_linked_pulse_count
                   << ",\"arrhythmia_linked_missing_pulse_count\":" << metrics.ppg_arrhythmia_linked_missing_pulse_count
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
               << ",\"git_commit\":" << json_string(signal_synth::signal_synth_generator_git_commit())
               << ",\"build_identity\":" << json_string(signal_synth::signal_synth_build_identity())
               << "},\"scenario\":{\"id\":" << json_string(render.document.scenario_id)
               << ",\"document_fingerprint\":" << json_string(render.document_identity.document_fingerprint)
               << ",\"generation_fingerprint\":" << json_u64_string(render.document_identity.generation_fingerprint)
               << ",\"resolved_document_fingerprint\":" << json_string(render.resolved_document_identity.document_fingerprint)
               << ",\"resolved_generation_fingerprint\":" << json_u64_string(render.resolved_document_identity.generation_fingerprint)
               << ",\"render_identity\":" << json_string(render.render_identity)
               << ",\"ecg_run_fingerprint\":" << json_u64_string(render.scenario_report.run_fingerprint())
               << ",\"scenario_schema_version\":" << render.document.schema_version
               << ",\"engine_version\":" << render.scenario_report.engine_version()
               << ",\"seed\":" << render.resolved_document.ecg.seed()
               << "},\"render\":{\"sample_rate_hz\":" << render.record.sampling_rate_hz()
               << ",\"sample_count\":" << render.record.sample_count()
               << ",\"duration_seconds\":" << std::setprecision(std::numeric_limits<double>::max_digits10) << render.document.duration_seconds
               << ",\"channel_count\":" << render.record.lead_count() + render.ppg.channel_count() + (render.signal_quality.accelerometer.empty() ? 0u : 1u)
               << ",\"channels\":[";
        for (unsigned int lead = 0; lead < render.record.lead_count(); ++lead)
        {
            if (lead)
                output << ',';
            output << "{\"name\":" << json_string(render.record.lead_name(lead)) << ",\"unit\":\"mV\"}";
        }
        for (unsigned int channel = 0; channel < render.ppg.channel_count(); ++channel)
            output << ",{\"name\":" << json_string(render.ppg.channel_name(channel))
                   << ",\"unit\":\"a.u.\""
                   << ",\"role\":\"optical_ppg\""
                   << ",\"dc_au\":" << render.ppg.channel_dc_au(channel)
                   << ",\"sensor_gain\":" << render.ppg.channel_sensor_gain(channel)
                   << ",\"delay_ms\":" << render.ppg.channel_delay_ms(channel)
                   << ",\"noise_std_au\":" << render.ppg.channel_noise_std_au(channel)
                   << ",\"ambient_offset_au\":" << render.ppg.channel_ambient_offset_au(channel)
                   << ",\"motion_sensitivity\":" << render.ppg.channel_motion_sensitivity(channel)
                   << ",\"ambient_sensitivity\":" << render.ppg.channel_ambient_sensitivity(channel)
                   << ",\"crosstalk_ratio\":" << render.ppg.channel_crosstalk_ratio(channel)
                   << ",\"minimum_output_au\":" << render.ppg.channel_minimum_output_au(channel)
                   << ",\"maximum_output_au\":" << render.ppg.channel_maximum_output_au(channel)
                   << ",\"quantization_bits\":" << render.ppg.channel_quantization_bits(channel)
                   << ",\"clipping_count\":" << (channel < render.ppg_clipping_counts.size() ? render.ppg_clipping_counts[channel] : 0u)
                   << ",\"seed\":" << render.ppg.channel_seed(channel) << '}';
        if (!render.signal_quality.accelerometer.empty())
            output << ",{\"name\":\"accel_motion\",\"unit\":\"g\",\"role\":\"motion_reference\"}";
        output << "],\"timestamp_policy\":\"common_latent_reference\""
               << ",\"compact_output\":" << (render.resolved_document.output.compact ? "true" : "false")
               << ",\"source_channels_retained\":" << (render.resolved_document.output.retain_source_channels ? "true" : "false")
               << "},\"wearable_timebase\":{\"enabled\":" << boolean(!render.wearable.streams.empty())
               << ",\"stream_count\":" << render.wearable.streams.size()
               << ",\"fingerprint\":" << json_string(render.wearable.fingerprint) << "},\"external_noise\":{\"enabled\":" << boolean(!render.external_noise.intervals.empty())
               << ",\"interval_count\":" << render.external_noise.intervals.size()
               << ",\"release_allowed\":" << boolean(render.external_noise.release_allowed) << "},\"intended_use\":\"synthetic engineering algorithm testing and QA\","
               << "\"not_for\":\"diagnosis, patient monitoring, clinical validation certificate, or standalone conformity assessment\"}";
        return output.str();
    }

    std::string provenance_json(const signal_synth::ecg_render_bundle& render)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10)
               << "{\"schema_version\":1,\"metadata_type\":\"synsigra_export_provenance\""
               << ",\"generator\":{\"name\":\"signal_synth\",\"product\":\"Synsigra Testbench\",\"version\":"
               << json_string(signal_synth::signal_synth_generator_version())
               << ",\"git_commit\":" << json_string(signal_synth::signal_synth_generator_git_commit())
               << ",\"build_identity\":" << json_string(signal_synth::signal_synth_build_identity()) << '}'
               << ",\"verifier\":{\"name\":\"synsigra\",\"version\":"
               << json_string(signal_synth::signal_synth_verifier_version())
               << ",\"package_contract_version\":" << json_string(signal_synth::signal_synth_package_contract_version())
               << ",\"scoring_manifest_contract_version\":" << json_string(signal_synth::signal_synth_scoring_manifest_contract_version()) << '}'
               << ",\"scenario\":{\"id\":" << json_string(render.document.scenario_id)
               << ",\"document_fingerprint\":" << json_string(render.document_identity.document_fingerprint)
               << ",\"generation_fingerprint\":" << json_u64_string(render.document_identity.generation_fingerprint)
               << ",\"resolved_document_fingerprint\":" << json_string(render.resolved_document_identity.document_fingerprint)
               << ",\"resolved_generation_fingerprint\":" << json_u64_string(render.resolved_document_identity.generation_fingerprint)
               << ",\"render_identity\":" << json_string(render.render_identity)
               << ",\"ecg_run_fingerprint\":" << json_u64_string(render.scenario_report.run_fingerprint())
               << ",\"scenario_schema_version\":" << render.document.schema_version
               << ",\"engine_version\":" << render.scenario_report.engine_version()
               << ",\"seed\":" << render.resolved_document.ecg.seed() << '}'
               << ",\"render\":{\"sample_rate_hz\":" << render.record.sampling_rate_hz()
               << ",\"sample_count\":" << render.record.sample_count()
               << ",\"duration_seconds\":" << render.document.duration_seconds
               << ",\"lead_count\":" << render.record.lead_count()
               << ",\"ppg_channel_count\":" << render.ppg.channel_count()
               << ",\"has_ppg\":" << (render.ppg.sample_count() ? "true" : "false")
               << ",\"has_motion_reference\":" << (render.signal_quality.accelerometer.empty() ? "false" : "true")
               << ",\"wearable_stream_count\":" << render.wearable.streams.size()
               << ",\"wearable_timebase_fingerprint\":" << json_string(render.wearable.fingerprint) << '}'
               << ",\"external_noise\":{\"enabled\":" << boolean(!render.external_noise.intervals.empty())
               << ",\"interval_count\":" << render.external_noise.intervals.size()
               << ",\"release_allowed\":" << boolean(render.external_noise.release_allowed) << '}'
               << ",\"provenance_checklist\":["
               << "{\"item\":\"scenario_json\",\"artifact\":\"scenario.json\",\"required\":true},"
               << "{\"item\":\"metadata_json\",\"artifact\":\"metadata.json\",\"required\":true},"
               << "{\"item\":\"provenance_json\",\"artifact\":\"provenance.json\",\"required\":true},"
               << "{\"item\":\"waveform_samples\",\"artifact\":\"waveform.csv or native waveform exports\",\"required\":true},"
               << "{\"item\":\"annotations\",\"artifact\":\"annotations.json\",\"required\":true},"
               << "{\"item\":\"ground_truth_metrics\",\"artifact\":\"ground_truth_metrics.json\",\"required\":true},"
               << "{\"item\":\"hrv_metrics\",\"artifact\":\"hrv_metrics.json\",\"required\":true},"
               << "{\"item\":\"warnings\",\"artifact\":\"warnings.json\",\"required\":true},"
               << "{\"item\":\"claim_boundary\",\"artifact\":\"ENGINEERING_CLAIM_BOUNDARY.txt\",\"required\":true},"
               << "{\"item\":\"human_report\",\"artifact\":\"report.html\",\"required\":true}"
               << "]"
               << ",\"claim_boundary\":{\"intended_use\":\"deterministic synthetic biosignal engineering QA and algorithm verification\""
               << ",\"verifies\":\"Package files are internally consistent with the generated synthetic scenario, ground-truth annotations and scoring contract.\""
               << ",\"does_not_verify\":\"Patient physiology, diagnostic performance on real populations, clinical safety, clinical effectiveness, or regulatory conformity.\""
               << ",\"not_for\":\"diagnosis, patient monitoring, clinical validation certificate, or standalone conformity assessment\"}"
               << ",\"determinism\":{\"byte_stable_export\":true,\"timestamp_policy\":" << json_string(render.wearable.streams.empty() ? "common_latent_reference" : "common_latent_reference_with_explicit_wearable_device_time") << "}}";
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
               << "not a diagnostic result, and not standalone evidence of medical-device conformity. See provenance.json and "
               << "ENGINEERING_CLAIM_BOUNDARY.txt for generator identity and the auditable engineering QA claim boundary.</p>"
               << "<h2>Identity</h2><table><tr><th>Scenario</th><td>" << html_text(render.document.scenario_id)
               << "</td></tr><tr><th>Document fingerprint</th><td>" << html_text(render.document_identity.document_fingerprint)
               << "</td></tr><tr><th>Generation fingerprint</th><td>" << render.document_identity.generation_fingerprint
               << "</td></tr><tr><th>Render identity</th><td>" << html_text(render.render_identity)
               << "</td></tr><tr><th>ECG run fingerprint</th><td>" << render.scenario_report.run_fingerprint()
               << "</td></tr><tr><th>Generator</th><td>" << signal_synth::signal_synth_generator_version()
               << "</td></tr><tr><th>Generator git commit</th><td>" << html_text(signal_synth::signal_synth_generator_git_commit())
               << "</td></tr><tr><th>Build identity</th><td>" << html_text(signal_synth::signal_synth_build_identity())
               << "</td></tr><tr><th>Package contract</th><td>" << signal_synth::signal_synth_package_contract_version()
               << "</td></tr><tr><th>Verifier</th><td>" << signal_synth::signal_synth_verifier_version()
               << "</td></tr></table><h2>Lead II Preview</h2>" << svg_preview(render)
               << (render.ppg.sample_count() ? "<h2>PPG Preview</h2>" + ppg_svg_preview(render) : "")
               << "<h2>Ground Truth Summary</h2><table><tr><th>Beats</th><td>" << render.metrics.beat_count
               << "</td></tr><tr><th>Mean HR</th><td>" << render.metrics.mean_heart_rate_bpm
               << " bpm</td></tr><tr><th>Mean RR</th><td>" << render.metrics.mean_rr_seconds
               << " s</td></tr><tr><th>SDNN</th><td>" << render.metrics.sdnn_seconds
               << " s</td></tr><tr><th>RMSSD</th><td>" << render.metrics.rmssd_seconds
               << " s</td></tr><tr><th>pNN50</th><td>" << render.metrics.pnn50_percent
               << " %</td></tr><tr><th>SD1 / SD2</th><td>" << render.metrics.sd1_seconds << " s / " << render.metrics.sd2_seconds
               << " s</td></tr><tr><th>VLF power</th><td>" << render.metrics.vlf_power_seconds2
               << " s2</td></tr><tr><th>LF/HF</th><td>" << render.metrics.lf_hf_ratio
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
            write_artifact_channels(channels, artifact, render.ppg);
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
               << "<h2>Artifacts</h2><p>scenario.json, metadata.json, provenance.json, waveform.csv, annotations.json, "
               << "rr_tachogram.csv, hrv_metrics.json, ground_truth_metrics.json, warnings.json, ENGINEERING_CLAIM_BOUNDARY.txt, report.html, README.txt, synsigra.hea, synsigra.dat, synsigra.atr, wfdb_metadata.json, synsigra.edf, synsigra.bdf, edf_bdf_metadata.json</p></body></html>";
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
        return "0.10.0-dev";
    }

    const char* signal_synth_generator_git_commit()
    {
        return SIGNAL_SYNTH_GIT_COMMIT;
    }

    const char* signal_synth_build_identity()
    {
        return "signal_synth/" SIGNAL_SYNTH_GIT_COMMIT;
    }

    const char* signal_synth_package_contract_version()
    {
        return challenge_package_contract_version();
    }

    const char* signal_synth_scoring_manifest_contract_version()
    {
        return "synsigra_scoring_manifest_v3";
    }

    const char* signal_synth_verification_protocol_contract_version()
    {
        return "synsigra_verification_protocol_v2";
    }

    const char* signal_synth_verifier_version()
    {
        return "0.10.0";
    }

    const char* signal_synth_engineering_claim_boundary_text()
    {
        return
            "Synsigra engineering QA claim boundary\n"
            "\n"
            "Intended use: deterministic synthetic biosignal engineering QA and algorithm verification.\n"
            "Verifies: package files are internally consistent with the generated synthetic scenario, ground-truth annotations, metrics, native waveform exports, and scoring contract.\n"
            "Does not verify: patient physiology, diagnostic performance on real populations, clinical safety, clinical effectiveness, MDR compliance, FDA compliance, or regulatory conformity.\n"
            "Not for: diagnosis, patient monitoring, clinical validation certification, or standalone conformity assessment.\n";
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
        add_artifact(fresh, "provenance.json", "application/json", provenance_json(render));
        if (render.resolved_document.output.include_waveform_csv)
            add_artifact(fresh, "waveform.csv", "text/csv", waveform_csv(render));
        if (!render.external_noise.intervals.empty())
        {
            add_artifact(fresh, "external_noise_truth.json", "application/json", external_noise_truth_json(render.external_noise));
            add_artifact(fresh, "external_noise_clean_ecg.csv", "text/csv", external_noise_clean_ecg_csv(render));
        }
        if (!render.wearable.streams.empty())
        {
            for (std::size_t i = 0; i < render.wearable.streams.size(); ++i)
                add_artifact(fresh, wearable_sample_artifact_name(render.wearable.streams[i].kind), "text/csv", wearable_samples_csv(render.wearable.streams[i]));
            add_artifact(fresh, "wearable_timestamp_truth.csv", "text/csv", wearable_timestamp_truth_csv(render.wearable));
            add_artifact(fresh, "wearable_timebase_truth.json", "application/json", wearable_timebase_truth_json(render.wearable));
            if (render.wearable.stream(wearable_stream_ecg) && render.wearable.stream(wearable_stream_ppg))
                add_artifact(fresh, "wearable_alignment_truth.json", "application/json", wearable_alignment_truth_json(render.wearable));
        }
        realism_analysis_result realism;
        if (!analyze_signal_realism(render, realism))
        {
            fresh_result.messages.push_back("signal characterization failed");
            result = fresh_result;
            return false;
        }
        add_artifact(fresh, "realism_metrics.json", "application/json", realism_analysis_json(realism));
        add_artifact(fresh, "realism_metrics.csv", "text/csv", realism_analysis_csv(realism));
        add_artifact(fresh, "realism_report.html", "text/html", realism_analysis_html(realism));
        if (render.ppg.optical_enabled())
        {
            add_artifact(fresh, "ppg_optical_latent.csv", "text/csv", ppg_optical_latent_csv(render));
            add_artifact(fresh, "ppg_optical_truth.json", "application/json", ppg_optical_truth_json(render));
        }
        add_artifact(fresh, "annotations.json", "application/json", annotations_json(render));
        add_artifact(fresh, "rr_tachogram.csv", "text/csv", rr_tachogram_csv(render));
        add_artifact(fresh, "hrv_metrics.json", "application/json", hrv_metrics_json(render));
        if (render.cardiorespiratory.prv_available || render.cardiorespiratory.respiration_available)
        {
            add_artifact(fresh, "cardiorespiratory_truth.json", "application/json", cardiorespiratory_truth_json(render));
            if (render.cardiorespiratory.prv_available) add_artifact(fresh, "prv_tachogram.csv", "text/csv", prv_tachogram_csv(render.cardiorespiratory));
            if (render.cardiorespiratory.respiration_available) add_artifact(fresh, "respiration_reference.csv", "text/csv", respiration_reference_csv(render.cardiorespiratory));
        }
        add_artifact(fresh, "ground_truth_metrics.json", "application/json", metrics_json(render));
        add_artifact(fresh, "warnings.json", "application/json", warnings_json(render));
        add_artifact(fresh, "ENGINEERING_CLAIM_BOUNDARY.txt", "text/plain", signal_synth_engineering_claim_boundary_text());
        add_artifact(fresh, "report.html", "text/html", report_html(render));
        add_artifact(fresh, "README.txt", "text/plain",
            "Synsigra deterministic synthetic ECG engineering evidence package.\n"
            "Intended for research, development, software testing, and algorithm QA.\n"
            "See provenance.json for generator, build, scenario and package-contract identity.\n"
            "Schema-v5 packages include explicit multi-rate wearable sample, timestamp, packet and alignment truth artifacts.\n"
            "realism_metrics.json contains separate engineering characterization domains and intentionally no single realism score.\n"
            "See ENGINEERING_CLAIM_BOUNDARY.txt for the exact engineering QA claim boundary.\n"
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
