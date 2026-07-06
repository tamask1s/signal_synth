#include "ecg_edf_bdf_export.h"
#include "ecg_beat_classification.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    const int edf_ecg_gain_adc_per_mv = 1000;
    const int edf_ppg_gain_adc_per_au = 10000;
    const int bdf_ecg_gain_adc_per_mv = 100000;
    const int bdf_ppg_gain_adc_per_au = 1000000;
    const int edf_accelerometer_gain_adc_per_g = 10000;
    const int bdf_accelerometer_gain_adc_per_g = 1000000;

    enum standard_kind
    {
        standard_edf,
        standard_bdf
    };

    struct edf_record_layout
    {
        unsigned int data_record_count;
        unsigned int waveform_samples_per_record;
        unsigned int annotation_bytes_per_record;
        unsigned int annotation_samples_per_record;
        double data_record_duration_seconds;
        std::vector<std::string> annotations;
    };

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

    const char* clinical_lead_name(unsigned int lead)
    {
        static const char* names[signal_synth::clinical_lead_count] = {"I","II","III","aVR","aVL","aVF","V1","V2","V3","V4","V5","V6"};
        return lead < signal_synth::clinical_lead_count ? names[lead] : "";
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

    const double* rendered_accelerometer(const signal_synth::ecg_render_bundle& render)
    {
        return render.signal_quality.accelerometer.size() == render.record.sample_count() && !render.signal_quality.accelerometer.empty()
            ? &render.signal_quality.accelerometer[0] : 0;
    }

    std::string number(double value)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(8) << value;
        return output.str();
    }

    std::string integer(unsigned int value)
    {
        std::ostringstream output;
        output << value;
        return output.str();
    }

    std::string fixed_field(const std::string& value, std::size_t width)
    {
        std::string output = value.substr(0, width);
        while (output.size() < width)
            output.push_back(' ');
        return output;
    }

    int clamp_i16(double value, int gain)
    {
        const double scaled = std::floor(value * gain + (value >= 0.0 ? 0.5 : -0.5));
        if (scaled > 32767.0)
            return 32767;
        if (scaled < -32768.0)
            return -32768;
        return static_cast<int>(scaled);
    }

    int clamp_i24(double value, int gain)
    {
        const double scaled = std::floor(value * gain + (value >= 0.0 ? 0.5 : -0.5));
        if (scaled > 8388607.0)
            return 8388607;
        if (scaled < -8388608.0)
            return -8388608;
        return static_cast<int>(scaled);
    }

    void append_i16_le(std::string& output, int value)
    {
        const unsigned int raw = static_cast<unsigned short>(static_cast<short>(value));
        output.push_back(static_cast<char>(raw & 0xffu));
        output.push_back(static_cast<char>((raw >> 8) & 0xffu));
    }

    void append_i24_le(std::string& output, int value)
    {
        unsigned int raw = static_cast<unsigned int>(value);
        output.push_back(static_cast<char>(raw & 0xffu));
        output.push_back(static_cast<char>((raw >> 8) & 0xffu));
        output.push_back(static_cast<char>((raw >> 16) & 0xffu));
    }

    std::string tal_time(double seconds)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << (seconds >= 0.0 ? "+" : "") << std::fixed << std::setprecision(6) << seconds;
        std::string text = output.str();
        while (text.size() > 2 && text[text.size() - 1] == '0')
            text.erase(text.size() - 1);
        if (!text.empty() && text[text.size() - 1] == '.')
            text.erase(text.size() - 1);
        return text;
    }

    void append_tal(std::string& output, double onset_seconds, const char* label)
    {
        output += tal_time(onset_seconds);
        output.push_back(0x14);
        output += label;
        output.push_back(0x14);
        output.push_back(0x00);
    }

    std::string annotation_payload(const signal_synth::ecg_render_bundle& render, double record_start_seconds, double record_end_seconds)
    {
        std::string output;
        output += tal_time(record_start_seconds);
        output.push_back(0x14);
        output.push_back(0x14);
        output.push_back(0x00);
        if (record_start_seconds == 0.0)
            append_tal(output, 0.0, "record_start");
        for (unsigned int i = 0; i < render.record.beat_count(); ++i)
        {
            const signal_synth::clinical_beat_annotation& beat = render.record.beats()[i];
            if (beat.qrs_present && beat.r_peak_time_seconds >= record_start_seconds && beat.r_peak_time_seconds < record_end_seconds)
            {
                const std::string label = std::string("beat:") + signal_synth::ecg_beat_class_name(signal_synth::ecg_beat_class_from_origin(beat.origin));
                append_tal(output, beat.r_peak_time_seconds, label.c_str());
            }
        }
        for (unsigned int i = 0; i < render.ppg.annotation_count(); ++i)
        {
            const signal_synth::ppg_annotation& annotation = render.ppg.annotations()[i];
            if (annotation.kind == signal_synth::ppg_systolic_peak && annotation.source == signal_synth::ppg_fiducial_measurement && annotation.time_seconds >= record_start_seconds && annotation.time_seconds < record_end_seconds)
                append_tal(output, annotation.time_seconds, "ppg_systolic_peak");
        }
        if (output.empty() || output[output.size() - 1] != 0)
            output.push_back(0);
        return output;
    }

    unsigned int padded_annotation_bytes(const std::string& annotations, unsigned int sample_bytes)
    {
        unsigned int bytes = static_cast<unsigned int>(annotations.size());
        if (!bytes)
            bytes = sample_bytes;
        const unsigned int remainder = bytes % sample_bytes;
        if (remainder)
            bytes += sample_bytes - remainder;
        return bytes;
    }

    std::string format_version(standard_kind kind)
    {
        if (kind == standard_edf)
            return fixed_field("0", 8);
        std::string output;
        output.push_back(static_cast<char>(0xff));
        output += "BIOSEMI";
        return fixed_field(output, 8);
    }

    edf_record_layout make_layout(const signal_synth::ecg_render_bundle& render, unsigned int sample_bytes)
    {
        edf_record_layout layout = {};
        const unsigned int rate = render.record.sampling_rate_hz();
        layout.waveform_samples_per_record = rate > 0 && render.record.sample_count() % rate == 0 ? rate : render.record.sample_count();
        if (!layout.waveform_samples_per_record)
            layout.waveform_samples_per_record = render.record.sample_count();
        layout.data_record_count = layout.waveform_samples_per_record ? render.record.sample_count() / layout.waveform_samples_per_record : 0;
        if (!layout.data_record_count)
            layout.data_record_count = 1;
        layout.data_record_duration_seconds = static_cast<double>(layout.waveform_samples_per_record) / render.record.sampling_rate_hz();
        layout.annotation_bytes_per_record = 57u * sample_bytes;
        layout.annotations.reserve(layout.data_record_count);
        for (unsigned int record = 0; record < layout.data_record_count; ++record)
        {
            const double start = record * layout.data_record_duration_seconds;
            const double end = start + layout.data_record_duration_seconds;
            layout.annotations.push_back(annotation_payload(render, start, end));
            layout.annotation_bytes_per_record = std::max(layout.annotation_bytes_per_record, padded_annotation_bytes(layout.annotations.back(), sample_bytes));
        }
        layout.annotation_samples_per_record = layout.annotation_bytes_per_record / sample_bytes;
        return layout;
    }

    std::string header(const signal_synth::ecg_render_bundle& render, standard_kind kind, const std::string& record_name, const edf_record_layout& layout)
    {
        (void)record_name;
        const bool has_ppg = render.ppg.sample_count() != 0;
        const bool has_accelerometer = !render.signal_quality.accelerometer.empty();
        const unsigned int waveform_signal_count = render.record.lead_count() + (has_ppg ? 1u : 0u) + (has_accelerometer ? 1u : 0u);
        const unsigned int signal_count = waveform_signal_count + 1u;
        const unsigned int header_bytes = 256u + signal_count * 256u;
        const char* annotation_label = kind == standard_edf ? "EDF Annotations" : "BDF Annotations";

        std::vector<std::string> labels;
        std::vector<std::string> transducers;
        std::vector<std::string> dimensions;
        std::vector<std::string> physical_minimum;
        std::vector<std::string> physical_maximum;
        std::vector<std::string> digital_minimum;
        std::vector<std::string> digital_maximum;
        std::vector<std::string> prefilters;
        std::vector<std::string> samples_per_record;
        std::vector<std::string> signal_reserved;

        for (unsigned int lead = 0; lead < render.record.lead_count(); ++lead)
        {
            labels.push_back(clinical_lead_name(lead));
            transducers.push_back("synthetic");
            dimensions.push_back("mV");
            physical_minimum.push_back(kind == standard_edf ? "-32.768" : "-83.886");
            physical_maximum.push_back(kind == standard_edf ? "32.767" : "83.886");
            digital_minimum.push_back(kind == standard_edf ? "-32768" : "-8388608");
            digital_maximum.push_back(kind == standard_edf ? "32767" : "8388607");
            prefilters.push_back("none");
            samples_per_record.push_back(integer(layout.waveform_samples_per_record));
            signal_reserved.push_back("");
        }
        if (has_ppg)
        {
            labels.push_back("ppg_green");
            transducers.push_back("synthetic");
            dimensions.push_back("NU");
            physical_minimum.push_back(kind == standard_edf ? "-3.2768" : "-8.3886");
            physical_maximum.push_back(kind == standard_edf ? "3.2767" : "8.3886");
            digital_minimum.push_back(kind == standard_edf ? "-32768" : "-8388608");
            digital_maximum.push_back(kind == standard_edf ? "32767" : "8388607");
            prefilters.push_back("none");
            samples_per_record.push_back(integer(layout.waveform_samples_per_record));
            signal_reserved.push_back("");
        }
        if (has_accelerometer)
        {
            labels.push_back("accel_motion");
            transducers.push_back("synthetic");
            dimensions.push_back("g");
            physical_minimum.push_back(kind == standard_edf ? "-3.2768" : "-8.3886");
            physical_maximum.push_back(kind == standard_edf ? "3.2767" : "8.3886");
            digital_minimum.push_back(kind == standard_edf ? "-32768" : "-8388608");
            digital_maximum.push_back(kind == standard_edf ? "32767" : "8388607");
            prefilters.push_back("none");
            samples_per_record.push_back(integer(layout.waveform_samples_per_record));
            signal_reserved.push_back("motion_reference");
        }
        labels.push_back(annotation_label);
        transducers.push_back("");
        dimensions.push_back("");
        physical_minimum.push_back("-1");
        physical_maximum.push_back("1");
        digital_minimum.push_back(kind == standard_edf ? "-32768" : "-8388608");
        digital_maximum.push_back(kind == standard_edf ? "32767" : "8388607");
        prefilters.push_back("");
        samples_per_record.push_back(integer(layout.annotation_samples_per_record));
        signal_reserved.push_back("");

        std::string output;
        output.reserve(header_bytes);
        output += format_version(kind);
        output += fixed_field("X X X X", 80);
        output += fixed_field("Startdate 01-JAN-2025 X X X", 80);
        output += fixed_field("01.01.25", 8);
        output += fixed_field("00.00.00", 8);
        output += fixed_field(integer(header_bytes), 8);
        output += fixed_field(kind == standard_edf ? "EDF+C" : "BDF+C", 44);
        output += fixed_field(integer(layout.data_record_count), 8);
        output += fixed_field(number(layout.data_record_duration_seconds), 8);
        output += fixed_field(integer(signal_count), 4);
        for (std::size_t i = 0; i < labels.size(); ++i) output += fixed_field(labels[i], 16);
        for (std::size_t i = 0; i < transducers.size(); ++i) output += fixed_field(transducers[i], 80);
        for (std::size_t i = 0; i < dimensions.size(); ++i) output += fixed_field(dimensions[i], 8);
        for (std::size_t i = 0; i < physical_minimum.size(); ++i) output += fixed_field(physical_minimum[i], 8);
        for (std::size_t i = 0; i < physical_maximum.size(); ++i) output += fixed_field(physical_maximum[i], 8);
        for (std::size_t i = 0; i < digital_minimum.size(); ++i) output += fixed_field(digital_minimum[i], 8);
        for (std::size_t i = 0; i < digital_maximum.size(); ++i) output += fixed_field(digital_maximum[i], 8);
        for (std::size_t i = 0; i < prefilters.size(); ++i) output += fixed_field(prefilters[i], 80);
        for (std::size_t i = 0; i < samples_per_record.size(); ++i) output += fixed_field(samples_per_record[i], 8);
        for (std::size_t i = 0; i < signal_reserved.size(); ++i) output += fixed_field(signal_reserved[i], 32);
        return output;
    }

    std::string data_record(const signal_synth::ecg_render_bundle& render, standard_kind kind, const edf_record_layout& layout)
    {
        std::string output;
        const bool has_ppg = render.ppg.sample_count() != 0;
        const bool has_accelerometer = !render.signal_quality.accelerometer.empty();
        const unsigned int waveform_signal_count = render.record.lead_count() + (has_ppg ? 1u : 0u) + (has_accelerometer ? 1u : 0u);
        const unsigned int sample_bytes = kind == standard_edf ? 2u : 3u;
        const std::size_t bytes_per_record = static_cast<std::size_t>(waveform_signal_count) * layout.waveform_samples_per_record * sample_bytes + layout.annotation_bytes_per_record;
        output.reserve(layout.data_record_count * bytes_per_record);
        const double* ppg = rendered_ppg(render);
        const double* accelerometer = rendered_accelerometer(render);
        for (unsigned int record = 0; record < layout.data_record_count; ++record)
        {
            const unsigned int first_sample = record * layout.waveform_samples_per_record;
            const unsigned int past_sample = std::min(render.record.sample_count(), first_sample + layout.waveform_samples_per_record);
            for (unsigned int lead = 0; lead < render.record.lead_count(); ++lead)
            {
                const double* samples = rendered_ecg_lead(render, lead);
                for (unsigned int sample = first_sample; sample < past_sample; ++sample)
                {
                    if (kind == standard_edf)
                        append_i16_le(output, clamp_i16(samples[sample], edf_ecg_gain_adc_per_mv));
                    else
                        append_i24_le(output, clamp_i24(samples[sample], bdf_ecg_gain_adc_per_mv));
                }
            }
            if (ppg)
            {
                for (unsigned int sample = first_sample; sample < past_sample; ++sample)
                {
                    if (kind == standard_edf)
                        append_i16_le(output, clamp_i16(ppg[sample], edf_ppg_gain_adc_per_au));
                    else
                        append_i24_le(output, clamp_i24(ppg[sample], bdf_ppg_gain_adc_per_au));
                }
            }
            if (accelerometer)
            {
                for (unsigned int sample = first_sample; sample < past_sample; ++sample)
                {
                    if (kind == standard_edf)
                        append_i16_le(output, clamp_i16(accelerometer[sample], edf_accelerometer_gain_adc_per_g));
                    else
                        append_i24_le(output, clamp_i24(accelerometer[sample], bdf_accelerometer_gain_adc_per_g));
                }
            }
            output += layout.annotations[record];
            while (output.size() < (record + 1u) * bytes_per_record)
                output.push_back(0);
        }
        return output;
    }

    std::string standard_file(const signal_synth::ecg_render_bundle& render, standard_kind kind, const std::string& record_name)
    {
        const unsigned int sample_bytes = kind == standard_edf ? 2u : 3u;
        const edf_record_layout layout = make_layout(render, sample_bytes);
        return header(render, kind, record_name, layout) + data_record(render, kind, layout);
    }

    std::string metadata_json(const signal_synth::ecg_render_bundle& render, const std::string& record_name)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10);
        output << "{\"schema_version\":1,\"formats\":[\"edf_plus\",\"bdf_plus\"],\"record_name\":" << json_string(record_name)
               << ",\"files\":{\"edf\":" << json_string(record_name + ".edf")
               << ",\"bdf\":" << json_string(record_name + ".bdf")
               << "},\"scenario\":{\"id\":" << json_string(render.document.scenario_id)
               << ",\"document_fingerprint\":" << json_string(render.document_identity.document_fingerprint)
               << ",\"render_identity\":" << json_string(render.render_identity)
               << "},\"signals\":[";
        for (unsigned int lead = 0; lead < render.record.lead_count(); ++lead)
        {
            if (lead)
                output << ',';
            output << "{\"name\":" << json_string(clinical_lead_name(lead))
                   << ",\"unit\":\"mV\",\"edf_gain_adc_per_unit\":" << edf_ecg_gain_adc_per_mv
                   << ",\"bdf_gain_adc_per_unit\":" << bdf_ecg_gain_adc_per_mv << "}";
        }
        if (render.ppg.sample_count())
            output << ",{\"name\":\"ppg_green\",\"unit\":\"normalized_unit\",\"edf_gain_adc_per_unit\":" << edf_ppg_gain_adc_per_au << ",\"bdf_gain_adc_per_unit\":" << bdf_ppg_gain_adc_per_au << "}";
        if (!render.signal_quality.accelerometer.empty())
            output << ",{\"name\":\"accel_motion\",\"unit\":\"g\",\"edf_gain_adc_per_unit\":" << edf_accelerometer_gain_adc_per_g << ",\"bdf_gain_adc_per_unit\":" << bdf_accelerometer_gain_adc_per_g << ",\"role\":\"motion_reference\"}";
        output << "],\"annotation_strategy\":{\"native_annotation_signal\":[\"r_peak\",\"ppg_systolic_peak\"],"
               << "\"full_ground_truth\":\"annotations.json\"},"
               << "\"intended_use\":\"synthetic engineering algorithm testing and QA\","
               << "\"not_for\":\"diagnosis, patient monitoring, clinical validation certificate, or standalone conformity assessment\"}";
        return output.str();
    }

    void add_artifact(signal_synth::edf_bdf_export_bundle& bundle, const char* name, const char* media_type, const std::string& content)
    {
        signal_synth::edf_bdf_export_artifact artifact;
        artifact.name = name;
        artifact.media_type = media_type;
        artifact.content = content;
        bundle.artifacts.push_back(artifact);
    }
}

namespace signal_synth
{
    bool build_edf_bdf_export_bundle(const ecg_render_bundle& render, const std::string& requested_record_name, edf_bdf_export_bundle& output, ecg_export_result& result)
    {
        ecg_export_result fresh_result;
        edf_bdf_export_bundle fresh;
        if (!render.document_identity.success || !render.scenario_report.success() || !render.record.sample_count() || render.record.lead_count() != clinical_lead_count)
        {
            fresh_result.messages.push_back("render bundle is incomplete");
            result = fresh_result;
            return false;
        }
        const std::string record_name = safe_record_name(requested_record_name);
        add_artifact(fresh, (record_name + ".edf").c_str(), "application/edf", standard_file(render, standard_edf, record_name));
        add_artifact(fresh, (record_name + ".bdf").c_str(), "application/octet-stream", standard_file(render, standard_bdf, record_name));
        add_artifact(fresh, "edf_bdf_metadata.json", "application/json", metadata_json(render, record_name));
        fresh_result.success = true;
        output = fresh;
        result = fresh_result;
        return true;
    }
}
