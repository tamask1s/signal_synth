#include "ecg_wfdb_export.h"
#include "ecg_beat_classification.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace
{
    const int ecg_gain_adc_per_mv = 1000;
    const int ppg_gain_adc_per_au = 10000;
    const int wfdb_ann_normal = 1;
    const int wfdb_ann_pvc = 5;
    const int wfdb_ann_pac = 8;
    const int wfdb_ann_ventricular_escape = 10;
    const int wfdb_ann_junctional_escape = 11;
    const int wfdb_ann_paced = 12;
    const int wfdb_ann_unknown = 13;

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

    const char* clinical_lead_name(unsigned int lead)
    {
        static const char* names[signal_synth::clinical_lead_count] = {"I","II","III","aVR","aVL","aVF","V1","V2","V3","V4","V5","V6"};
        return lead < signal_synth::clinical_lead_count ? names[lead] : "";
    }

    std::string safe_record_name(const std::string& requested)
    {
        std::string output;
        for (std::size_t i = 0; i < requested.size(); ++i)
        {
            const char c = requested[i];
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')
                output.push_back(c);
        }
        return output.empty() ? "synsigra" : output;
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

    short clamp_adc(double value, int gain)
    {
        const double scaled = std::floor(value * gain + (value >= 0.0 ? 0.5 : -0.5));
        if (scaled > 32767.0)
            return 32767;
        if (scaled < -32768.0)
            return -32768;
        return static_cast<short>(scaled);
    }

    void append_i16_le(std::string& output, short value)
    {
        const unsigned int raw = static_cast<unsigned short>(value);
        output.push_back(static_cast<char>(raw & 0xffu));
        output.push_back(static_cast<char>((raw >> 8) & 0xffu));
    }

    void append_annotation_word(std::string& output, unsigned int interval, int type)
    {
        output.push_back(static_cast<char>(interval & 0xffu));
        output.push_back(static_cast<char>(((type & 0x3f) << 2) | ((interval >> 8) & 0x03u)));
    }

    void append_time_gap(std::string& output, unsigned int interval)
    {
        while (interval > 1023u)
        {
            append_annotation_word(output, 1023u, 0);
            interval -= 1023u;
        }
        if (interval)
            append_annotation_word(output, interval, 0);
    }

    unsigned int time_to_sample(double time_seconds, double sampling_rate_hz)
    {
        const double sample = std::floor(time_seconds * sampling_rate_hz + 0.5);
        return sample < 0.0 ? 0u : static_cast<unsigned int>(sample);
    }

    int wfdb_beat_annotation(signal_synth::clinical_ventricular_origin origin)
    {
        switch (origin)
        {
        case signal_synth::clinical_origin_conducted: return wfdb_ann_normal;
        case signal_synth::clinical_origin_pac: return wfdb_ann_pac;
        case signal_synth::clinical_origin_pvc: return wfdb_ann_pvc;
        case signal_synth::clinical_origin_junctional_escape: return wfdb_ann_junctional_escape;
        case signal_synth::clinical_origin_ventricular_escape: return wfdb_ann_ventricular_escape;
        case signal_synth::clinical_origin_paced: return wfdb_ann_paced;
        case signal_synth::clinical_origin_vt:
        default: return wfdb_ann_unknown;
        }
    }

    void add_artifact(signal_synth::wfdb_export_bundle& bundle, const char* name, const char* media_type, const std::string& content)
    {
        signal_synth::wfdb_export_artifact artifact;
        artifact.name = name;
        artifact.media_type = media_type;
        artifact.content = content;
        bundle.artifacts.push_back(artifact);
    }

    std::string wfdb_dat(const signal_synth::ecg_render_bundle& render, std::vector<long>& checksums, std::vector<short>& initial_values)
    {
        const unsigned int ecg_count = render.record.lead_count();
        const unsigned int channel_count = ecg_count + (render.ppg.sample_count() ? 1u : 0u);
        checksums.assign(channel_count, 0);
        initial_values.assign(channel_count, 0);

        std::string output;
        output.reserve(static_cast<std::size_t>(render.record.sample_count()) * channel_count * 2u);
        const double* ppg = rendered_ppg(render);
        for (unsigned int sample = 0; sample < render.record.sample_count(); ++sample)
        {
            for (unsigned int lead = 0; lead < ecg_count; ++lead)
            {
                const short value = clamp_adc(rendered_ecg_lead(render, lead)[sample], ecg_gain_adc_per_mv);
                if (!sample)
                    initial_values[lead] = value;
                checksums[lead] += value;
                append_i16_le(output, value);
            }
            if (ppg)
            {
                const short value = clamp_adc(ppg[sample], ppg_gain_adc_per_au);
                if (!sample)
                    initial_values[ecg_count] = value;
                checksums[ecg_count] += value;
                append_i16_le(output, value);
            }
        }
        return output;
    }

    int wfdb_checksum(long checksum)
    {
        int value = static_cast<int>(checksum % 65536L);
        if (value > 32767)
            value -= 65536;
        if (value < -32768)
            value += 65536;
        return value;
    }

    std::string wfdb_header(const signal_synth::ecg_render_bundle& render, const std::string& record_name, const std::vector<long>& checksums, const std::vector<short>& initial_values)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        const unsigned int ecg_count = render.record.lead_count();
        const unsigned int channel_count = ecg_count + (render.ppg.sample_count() ? 1u : 0u);
        output << record_name << ' ' << channel_count << ' ' << std::setprecision(std::numeric_limits<double>::max_digits10)
               << render.record.sampling_rate_hz() << ' ' << render.record.sample_count() << '\n';
        output << "# generator=signal_synth " << signal_synth::signal_synth_generator_version() << '\n';
        output << "# scenario_id=" << render.document.scenario_id << '\n';
        output << "# document_fingerprint=" << render.document_identity.document_fingerprint << '\n';
        output << "# render_identity=" << render.render_identity << '\n';
        for (unsigned int lead = 0; lead < ecg_count; ++lead)
            output << record_name << ".dat 16 " << ecg_gain_adc_per_mv << "(0)/mV 16 0 " << initial_values[lead] << ' ' << wfdb_checksum(checksums[lead]) << " 0 " << clinical_lead_name(lead) << '\n';
        if (render.ppg.sample_count())
            output << record_name << ".dat 16 " << ppg_gain_adc_per_au << "(0)/NU 16 0 " << initial_values[ecg_count] << ' ' << wfdb_checksum(checksums[ecg_count]) << " 0 ppg_green\n";
        return output.str();
    }

    std::string wfdb_atr(const signal_synth::ecg_render_bundle& render)
    {
        std::string output;
        unsigned int previous_sample = 0;
        for (unsigned int i = 0; i < render.record.beat_count(); ++i)
        {
            const signal_synth::clinical_beat_annotation& beat = render.record.beats()[i];
            if (!beat.qrs_present)
                continue;
            const unsigned int r_peak_sample = time_to_sample(beat.r_peak_time_seconds, render.record.sampling_rate_hz());
            if (r_peak_sample < previous_sample)
                continue;
            unsigned int interval = r_peak_sample - previous_sample;
            append_time_gap(output, interval / 1023u * 1023u);
            interval %= 1023u;
            append_annotation_word(output, interval, wfdb_beat_annotation(beat.origin));
            previous_sample = r_peak_sample;
        }
        append_annotation_word(output, 0u, 0);
        return output;
    }

    std::string wfdb_metadata_json(const signal_synth::ecg_render_bundle& render, const std::string& record_name)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10);
        output << "{\"schema_version\":1,\"format\":\"wfdb\",\"record_name\":" << json_string(record_name)
               << ",\"files\":{\"header\":" << json_string(record_name + ".hea")
               << ",\"signal\":" << json_string(record_name + ".dat")
               << ",\"rpeak_annotations\":" << json_string(record_name + ".atr")
               << "},\"scenario\":{\"id\":" << json_string(render.document.scenario_id)
               << ",\"document_fingerprint\":" << json_string(render.document_identity.document_fingerprint)
               << ",\"render_identity\":" << json_string(render.render_identity)
               << "},\"signals\":[";
        for (unsigned int lead = 0; lead < render.record.lead_count(); ++lead)
        {
            if (lead)
                output << ',';
            output << "{\"name\":" << json_string(clinical_lead_name(lead))
                   << ",\"unit\":\"mV\",\"gain_adc_per_unit\":" << ecg_gain_adc_per_mv
                   << ",\"adc_zero\":0}";
        }
        if (render.ppg.sample_count())
            output << ",{\"name\":\"ppg_green\",\"unit\":\"normalized_unit\",\"gain_adc_per_unit\":" << ppg_gain_adc_per_au << ",\"adc_zero\":0}";
        output << "],\"annotation_strategy\":{\"native_wfdb_annotation\":\"r_peak_with_beat_class\",\"native_file\":" << json_string(record_name + ".atr")
               << ",\"full_ground_truth\":\"annotations.json\"},"
               << "\"intended_use\":\"synthetic engineering algorithm testing and QA\","
               << "\"not_for\":\"diagnosis, patient monitoring, clinical validation certificate, or standalone conformity assessment\"}";
        return output.str();
    }
}

namespace signal_synth
{
    bool build_wfdb_export_bundle(const ecg_render_bundle& render, const std::string& requested_record_name, wfdb_export_bundle& output, ecg_export_result& result)
    {
        ecg_export_result fresh_result;
        wfdb_export_bundle fresh;
        if (!render.document_identity.success || !render.scenario_report.success() || !render.record.sample_count() || render.record.lead_count() != clinical_lead_count)
        {
            fresh_result.messages.push_back("render bundle is incomplete");
            result = fresh_result;
            return false;
        }
        const std::string record_name = safe_record_name(requested_record_name);
        std::vector<long> checksums;
        std::vector<short> initial_values;
        const std::string dat = wfdb_dat(render, checksums, initial_values);
        add_artifact(fresh, (record_name + ".hea").c_str(), "text/plain", wfdb_header(render, record_name, checksums, initial_values));
        add_artifact(fresh, (record_name + ".dat").c_str(), "application/octet-stream", dat);
        add_artifact(fresh, (record_name + ".atr").c_str(), "application/octet-stream", wfdb_atr(render));
        add_artifact(fresh, "wfdb_metadata.json", "application/json", wfdb_metadata_json(render, record_name));
        fresh_result.success = true;
        output = fresh;
        result = fresh_result;
        return true;
    }
}
