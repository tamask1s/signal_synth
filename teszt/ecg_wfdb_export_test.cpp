#include "../src/ecg_wfdb_export.h"

#include <cmath>
#include <iostream>
#include <sstream>
#include <string>

namespace
{
    bool check(bool condition, const char* name)
    {
        if (!condition)
            std::cerr << "FAIL: " << name << '\n';
        return condition;
    }

    const signal_synth::wfdb_export_artifact* find_artifact(const signal_synth::wfdb_export_bundle& bundle, const std::string& name)
    {
        for (std::size_t i = 0; i < bundle.artifacts.size(); ++i)
            if (bundle.artifacts[i].name == name)
                return &bundle.artifacts[i];
        return 0;
    }

    short read_i16_le(const std::string& data, std::size_t offset)
    {
        const unsigned int lo = static_cast<unsigned char>(data[offset]);
        const unsigned int hi = static_cast<unsigned char>(data[offset + 1]);
        return static_cast<short>(static_cast<unsigned short>(lo | (hi << 8)));
    }

    short expected_adc(double value, int gain)
    {
        const double scaled = std::floor(value * gain + (value >= 0.0 ? 0.5 : -0.5));
        if (scaled > 32767.0)
            return 32767;
        if (scaled < -32768.0)
            return -32768;
        return static_cast<short>(scaled);
    }

    std::string wfdb_record_line(const std::string& record_name, unsigned int channel_count, const signal_synth::clinical_ecg_record& record)
    {
        std::ostringstream output;
        output << record_name << ' ' << channel_count << ' ' << record.sampling_rate_hz() << ' ' << record.sample_count() << '\n';
        return output.str();
    }
}

int main()
{
    bool ok = true;
    signal_synth::ecg_scenario_document document;
    document.schema_version = 2;
    document.scenario_id = "wfdb_export_test";
    document.ppg.enabled = true;
    document.ecg.clear_conditions();
    document.ecg.add_condition(signal_synth::ecg_condition_pvc, 0.7);
    document.ecg.set_ectopic_every_n_beats(3);

    signal_synth::ecg_render_bundle render;
    signal_synth::ecg_export_result result;
    ok &= check(signal_synth::render_ecg_document(document, render, result), "render");

    signal_synth::wfdb_export_bundle bundle;
    ok &= check(signal_synth::build_wfdb_export_bundle(render, "wfdb test/01", bundle, result) && result.success, "build_wfdb");
    ok &= check(bundle.artifacts.size() == 4, "artifact_count");

    const signal_synth::wfdb_export_artifact* header = find_artifact(bundle, "wfdbtest01.hea");
    const signal_synth::wfdb_export_artifact* signal = find_artifact(bundle, "wfdbtest01.dat");
    const signal_synth::wfdb_export_artifact* annotations = find_artifact(bundle, "wfdbtest01.atr");
    const signal_synth::wfdb_export_artifact* metadata = find_artifact(bundle, "wfdb_metadata.json");
    ok &= check(header && signal && annotations && metadata, "artifact_names");
    ok &= check(header && header->content.find(wfdb_record_line("wfdbtest01", 13, render.record)) == 0, "header_record_line");
    ok &= check(header && header->content.find("# render_identity=" + render.render_identity) != std::string::npos, "header_identity");
    ok &= check(header && header->content.find("wfdbtest01.dat 16 1000(0)/mV 16 0") != std::string::npos && header->content.find("wfdbtest01.dat 16 10000(0)/NU 16 0") != std::string::npos, "header_gain_units");
    ok &= check(signal && signal->content.size() == render.record.sample_count() * 13u * 2u, "signal_size");
    if (signal && signal->content.size() >= 26u)
    {
        ok &= check(read_i16_le(signal->content, 0) == expected_adc(render.record.lead_data(0)[0], 1000), "first_i_adc");
        ok &= check(read_i16_le(signal->content, 2) == expected_adc(render.record.lead_data(1)[0], 1000), "first_ii_adc");
        ok &= check(read_i16_le(signal->content, 24) == expected_adc(render.ppg.samples()[0], 10000), "first_ppg_adc");
    }
    ok &= check(annotations && annotations->content.size() >= 2u && annotations->content[annotations->content.size() - 2] == 0 && annotations->content[annotations->content.size() - 1] == 0, "annotation_eof");
    ok &= check(metadata && metadata->content.find("\"record_name\":\"wfdbtest01\"") != std::string::npos && metadata->content.find("\"native_wfdb_annotation\":\"r_peak_with_beat_class\"") != std::string::npos, "metadata");
    bool has_normal = false;
    bool has_pvc = false;
    if (annotations)
        for (std::size_t offset = 0; offset + 1u < annotations->content.size(); offset += 2u)
        {
            const unsigned int type = static_cast<unsigned char>(annotations->content[offset + 1u]) >> 2;
            has_normal = has_normal || type == 1u;
            has_pvc = has_pvc || type == 5u;
        }
    ok &= check(has_normal && has_pvc, "wfdb_standard_beat_classes");

    signal_synth::ecg_render_bundle incomplete;
    signal_synth::wfdb_export_bundle preserved = bundle;
    ok &= check(!signal_synth::build_wfdb_export_bundle(incomplete, "bad", preserved, result) && preserved.artifacts.size() == 4, "transactional_failure");
    return ok ? 0 : 1;
}
