#include "../src/ecg_edf_bdf_export.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
    bool check(bool condition, const char* name)
    {
        if (!condition)
            std::cerr << "FAIL: " << name << '\n';
        return condition;
    }

    const signal_synth::edf_bdf_export_artifact* find_artifact(const signal_synth::edf_bdf_export_bundle& bundle, const std::string& name)
    {
        for (std::size_t i = 0; i < bundle.artifacts.size(); ++i)
            if (bundle.artifacts[i].name == name)
                return &bundle.artifacts[i];
        return 0;
    }

    std::string trim_field(const std::string& text, std::size_t offset, std::size_t width)
    {
        std::string value = text.substr(offset, width);
        while (!value.empty() && value[value.size() - 1] == ' ')
            value.erase(value.size() - 1);
        return value;
    }

    unsigned int uint_field(const std::string& text, std::size_t offset, std::size_t width)
    {
        return static_cast<unsigned int>(std::strtoul(trim_field(text, offset, width).c_str(), 0, 10));
    }

    int read_i16_le(const std::string& data, std::size_t offset)
    {
        const unsigned int lo = static_cast<unsigned char>(data[offset]);
        const unsigned int hi = static_cast<unsigned char>(data[offset + 1]);
        return static_cast<short>(static_cast<unsigned short>(lo | (hi << 8)));
    }

    int read_i24_le(const std::string& data, std::size_t offset)
    {
        unsigned int value = static_cast<unsigned char>(data[offset])
            | (static_cast<unsigned int>(static_cast<unsigned char>(data[offset + 1])) << 8)
            | (static_cast<unsigned int>(static_cast<unsigned char>(data[offset + 2])) << 16);
        if (value & 0x800000u)
            value |= 0xff000000u;
        return static_cast<int>(value);
    }

    int expected_adc(double value, int gain, int minimum, int maximum)
    {
        const double scaled = std::floor(value * gain + (value >= 0.0 ? 0.5 : -0.5));
        if (scaled > maximum)
            return maximum;
        if (scaled < minimum)
            return minimum;
        return static_cast<int>(scaled);
    }
}

int main()
{
    bool ok = true;
    signal_synth::ecg_scenario_document document;
    document.schema_version = 2;
    document.scenario_id = "edf_bdf_export_test";
    document.ppg.enabled = true;
    document.ecg.clear_conditions();
    document.ecg.add_condition(signal_synth::ecg_condition_pvc, 0.7);
    document.ecg.set_ectopic_every_n_beats(3);

    signal_synth::ecg_render_bundle render;
    signal_synth::ecg_export_result result;
    ok &= check(signal_synth::render_ecg_document(document, render, result), "render");

    signal_synth::edf_bdf_export_bundle bundle;
    ok &= check(signal_synth::build_edf_bdf_export_bundle(render, "edf bdf/01", bundle, result) && result.success, "build_edf_bdf");
    ok &= check(bundle.artifacts.size() == 3, "artifact_count");

    const signal_synth::edf_bdf_export_artifact* edf = find_artifact(bundle, "edfbdf01.edf");
    const signal_synth::edf_bdf_export_artifact* bdf = find_artifact(bundle, "edfbdf01.bdf");
    const signal_synth::edf_bdf_export_artifact* metadata = find_artifact(bundle, "edf_bdf_metadata.json");
    ok &= check(edf && bdf && metadata, "artifact_names");

    const unsigned int edf_header_bytes = edf ? uint_field(edf->content, 184, 8) : 0;
    const unsigned int bdf_header_bytes = bdf ? uint_field(bdf->content, 184, 8) : 0;
    ok &= check(edf && trim_field(edf->content, 0, 8) == "0" && trim_field(edf->content, 192, 44) == "EDF+C" && uint_field(edf->content, 252, 4) == 14, "edf_header");
    ok &= check(bdf && static_cast<unsigned char>(bdf->content[0]) == 0xffu && trim_field(bdf->content, 1, 7) == "BIOSEMI" && trim_field(bdf->content, 192, 44) == "BDF+C" && uint_field(bdf->content, 252, 4) == 14, "bdf_header");
    ok &= check(edf_header_bytes == 256u + 14u * 256u && bdf_header_bytes == 256u + 14u * 256u, "header_bytes");
    ok &= check(edf && edf->content.find("EDF Annotations") != std::string::npos && bdf && bdf->content.find("BDF Annotations") != std::string::npos, "annotation_signal_label");

    if (edf && edf->content.size() > edf_header_bytes + render.record.sample_count() * 12u * 2u)
    {
        ok &= check(read_i16_le(edf->content, edf_header_bytes) == expected_adc(render.record.lead_data(0)[0], 1000, -32768, 32767), "edf_first_i");
        ok &= check(read_i16_le(edf->content, edf_header_bytes + render.record.sample_count() * 2u) == expected_adc(render.record.lead_data(1)[0], 1000, -32768, 32767), "edf_first_ii");
        ok &= check(read_i16_le(edf->content, edf_header_bytes + render.record.sample_count() * 12u * 2u) == expected_adc(render.ppg.samples()[0], 10000, -32768, 32767), "edf_first_ppg");
    }
    if (bdf && bdf->content.size() > bdf_header_bytes + render.record.sample_count() * 12u * 3u)
    {
        ok &= check(read_i24_le(bdf->content, bdf_header_bytes) == expected_adc(render.record.lead_data(0)[0], 100000, -8388608, 8388607), "bdf_first_i");
        ok &= check(read_i24_le(bdf->content, bdf_header_bytes + render.record.sample_count() * 3u) == expected_adc(render.record.lead_data(1)[0], 100000, -8388608, 8388607), "bdf_first_ii");
        ok &= check(read_i24_le(bdf->content, bdf_header_bytes + render.record.sample_count() * 12u * 3u) == expected_adc(render.ppg.samples()[0], 1000000, -8388608, 8388607), "bdf_first_ppg");
    }
    ok &= check(edf && edf->content.find("beat:normal") != std::string::npos && edf->content.find("beat:ventricular_ectopic") != std::string::npos && edf->content.find("ppg_systolic_peak") != std::string::npos, "edf_annotations");
    ok &= check(bdf && bdf->content.find("beat:normal") != std::string::npos && bdf->content.find("beat:ventricular_ectopic") != std::string::npos && bdf->content.find("ppg_systolic_peak") != std::string::npos, "bdf_annotations");
    ok &= check(metadata && metadata->content.find("\"formats\":[\"edf_plus\",\"bdf_plus\"]") != std::string::npos && metadata->content.find("\"full_ground_truth\":\"annotations.json\"") != std::string::npos, "metadata");

    signal_synth::ecg_render_bundle incomplete;
    signal_synth::edf_bdf_export_bundle preserved = bundle;
    ok &= check(!signal_synth::build_edf_bdf_export_bundle(incomplete, "bad", preserved, result) && preserved.artifacts.size() == 3, "transactional_failure");
    return ok ? 0 : 1;
}
