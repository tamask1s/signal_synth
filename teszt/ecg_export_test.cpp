#include "../src/ecg_export.h"

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

    unsigned int line_count(const std::string& text)
    {
        unsigned int count = 0;
        for (std::size_t i = 0; i < text.size(); ++i)
            count += text[i] == '\n' ? 1u : 0u;
        return count;
    }

    bool same_artifacts(const signal_synth::ecg_export_bundle& left, const signal_synth::ecg_export_bundle& right)
    {
        if (left.artifacts.size() != right.artifacts.size())
            return false;
        for (std::size_t i = 0; i < left.artifacts.size(); ++i)
            if (left.artifacts[i].name != right.artifacts[i].name || left.artifacts[i].media_type != right.artifacts[i].media_type || left.artifacts[i].content != right.artifacts[i].content)
                return false;
        return true;
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
    document.scenario_id = "export_test";
    document.name = "<Export & Report>";
    document.description = "Deterministic export";
    document.tags.push_back("export");

    signal_synth::ecg_render_bundle render;
    signal_synth::ecg_export_result result;
    ok &= check(signal_synth::render_ecg_document(document, render, result) && result.success, "render_valid_document");
    ok &= check(render.render_identity.find(render.document_identity.document_fingerprint + ":ecg-run-") == 0, "render_identity_contract");
    ok &= check(render.record.sample_count() == document.sample_count() && render.record.lead_count() == 12 && render.morphology.entry_count() == render.record.beat_count() * 12, "render_bundle_shape");
    ok &= check(render.metrics.beat_count == render.record.beat_count() && render.metrics.mean_rr_seconds > 0.0 && render.metrics.mean_heart_rate_bpm > 0.0, "ground_truth_metrics");
    ok &= check(render.metrics.sdnn_seconds == 0.0 && render.metrics.rmssd_seconds == 0.0, "constant_rr_metrics_normalize_roundoff");

    double mean_rr = 0.0;
    for (unsigned int i = 0; i < render.record.beat_count(); ++i)
        mean_rr += render.record.beats()[i].rr_interval_seconds;
    mean_rr /= render.record.beat_count();
    ok &= check(std::fabs(mean_rr - render.metrics.mean_rr_seconds) < 1e-12, "mean_rr_independent_check");

    signal_synth::ecg_export_bundle bundle;
    ok &= check(signal_synth::build_ecg_export_bundle(render, bundle, result) && result.success, "build_export_bundle");
    const char* expected[] = {"scenario.json","metadata.json","waveform.csv","annotations.json","ground_truth_metrics.json","warnings.json","report.html","README.txt","synsigra.hea","synsigra.dat","synsigra.atr","wfdb_metadata.json","synsigra.edf","synsigra.bdf","edf_bdf_metadata.json"};
    ok &= check(bundle.artifacts.size() == 15, "artifact_count");
    for (unsigned int i = 0; i < 15; ++i)
        ok &= check(bundle.artifacts[i].name == expected[i] && !bundle.artifacts[i].content.empty(), "artifact_order_and_content");

    const signal_synth::ecg_text_artifact* csv = bundle.find("waveform.csv");
    const signal_synth::ecg_text_artifact* annotations = bundle.find("annotations.json");
    const signal_synth::ecg_text_artifact* metrics = bundle.find("ground_truth_metrics.json");
    const signal_synth::ecg_text_artifact* report = bundle.find("report.html");
    const signal_synth::ecg_text_artifact* metadata = bundle.find("metadata.json");
    const signal_synth::ecg_text_artifact* wfdb_header = bundle.find("synsigra.hea");
    const signal_synth::ecg_text_artifact* wfdb_signal = bundle.find("synsigra.dat");
    const signal_synth::ecg_text_artifact* wfdb_annotations = bundle.find("synsigra.atr");
    const signal_synth::ecg_text_artifact* wfdb_metadata = bundle.find("wfdb_metadata.json");
    const signal_synth::ecg_text_artifact* edf = bundle.find("synsigra.edf");
    const signal_synth::ecg_text_artifact* bdf = bundle.find("synsigra.bdf");
    const signal_synth::ecg_text_artifact* edf_bdf_metadata = bundle.find("edf_bdf_metadata.json");
    ok &= check(csv && csv->content.find("sample_index,time_seconds,I_mv,II_mv,III_mv,aVR_mv,aVL_mv,aVF_mv,V1_mv,V2_mv,V3_mv,V4_mv,V5_mv,V6_mv\n") == 0 && line_count(csv->content) == render.record.sample_count() + 1, "csv_contract");
    ok &= check(annotations && annotations->content.find("\"artifact_intervals\":[]") != std::string::npos && annotations->content.find("\"r_peak\"") != std::string::npos, "annotation_contract");
    ok &= check(metrics && metrics->content.find("\"sdnn_seconds\":") != std::string::npos && metrics->content.find("\"assertions\":[") != std::string::npos, "metrics_contract");
    ok &= check(metadata && metadata->content.find("\"channels\":[{\"name\":\"I\",\"unit\":\"mV\"}") != std::string::npos && metadata->content.find("{\"name\":\"V6\",\"unit\":\"mV\"}]") != std::string::npos, "channel_metadata_contract");
    ok &= check(report && report->content.find("&lt;Export &amp; Report&gt;") != std::string::npos && report->content.find("<polyline") != std::string::npos, "html_escape_and_actual_plot");
    ok &= check(report && report->content.find("not a clinical validation certificate") != std::string::npos && report->content.find("clinically validated") == std::string::npos, "controlled_report_claims");
    ok &= check(csv && csv->content.find("ppg_green_au") == std::string::npos && annotations && annotations->content.find("\"ppg_fiducials\"") == std::string::npos, "schema_v1_export_remains_ecg_only");
    ok &= check(wfdb_header && wfdb_header->content.find(wfdb_record_line("synsigra", 12, render.record)) == 0 && wfdb_header->content.find("synsigra.dat 16 1000(0)/mV 16 0") != std::string::npos, "wfdb_header_contract");
    ok &= check(wfdb_signal && wfdb_signal->media_type == "application/octet-stream" && wfdb_signal->content.size() == render.record.sample_count() * render.record.lead_count() * 2u, "wfdb_signal_size");
    ok &= check(wfdb_annotations && wfdb_annotations->content.size() >= render.record.beat_count() * 2u + 2u, "wfdb_annotation_size");
    ok &= check(wfdb_metadata && wfdb_metadata->content.find("\"format\":\"wfdb\"") != std::string::npos && wfdb_metadata->content.find("\"full_ground_truth\":\"annotations.json\"") != std::string::npos, "wfdb_metadata_contract");
    ok &= check(edf && edf->content.size() > 256u + 13u * 256u && edf->content.find("EDF Annotations") != std::string::npos, "edf_contract");
    ok &= check(bdf && static_cast<unsigned char>(bdf->content[0]) == 0xffu && bdf->content.size() > 256u + 13u * 256u && bdf->content.find("BDF Annotations") != std::string::npos, "bdf_contract");
    ok &= check(edf_bdf_metadata && edf_bdf_metadata->content.find("\"formats\":[\"edf_plus\",\"bdf_plus\"]") != std::string::npos, "edf_bdf_metadata_contract");

    signal_synth::ecg_render_bundle repeated_render;
    signal_synth::ecg_export_bundle repeated_bundle;
    ok &= check(signal_synth::render_ecg_document(document, repeated_render, result) && signal_synth::build_ecg_export_bundle(repeated_render, repeated_bundle, result) && same_artifacts(bundle, repeated_bundle), "byte_deterministic_artifacts");

    signal_synth::ecg_scenario_document invalid = document;
    invalid.duration_seconds = 0.0;
    signal_synth::ecg_render_bundle preserved = render;
    ok &= check(!signal_synth::render_ecg_document(invalid, preserved, result) && preserved.document.scenario_id == "export_test" && preserved.record.sample_count() == render.record.sample_count(), "failed_render_is_transactional");

    signal_synth::ecg_render_bundle incomplete;
    signal_synth::ecg_export_bundle preserved_export = bundle;
    ok &= check(!signal_synth::build_ecg_export_bundle(incomplete, preserved_export, result) && preserved_export.artifacts.size() == 15, "failed_export_is_transactional");
    ok &= check(std::string(signal_synth::signal_synth_generator_version()) == "0.1.0-dev", "runtime_generator_version");

    signal_synth::ecg_scenario_document multimodal = document;
    multimodal.schema_version = 2;
    multimodal.scenario_id = "ecg_ppg_export";
    multimodal.ppg.enabled = true;
    signal_synth::ecg_render_bundle multimodal_render;
    signal_synth::ecg_export_bundle multimodal_bundle;
    ok &= check(signal_synth::render_ecg_document(multimodal, multimodal_render, result) && multimodal_render.ppg.sample_count() == multimodal_render.record.sample_count(), "multimodal_render");
    ok &= check(multimodal_render.render_identity != render.render_identity, "ppg_document_changes_render_identity");
    ok &= check(multimodal_render.metrics.ppg_pulse_count > 0 && std::fabs(multimodal_render.metrics.mean_ppg_onset_delay_seconds - 0.18) < 1e-12 && multimodal_render.metrics.mean_ppg_peak_delay_seconds > 0.18, "ppg_delay_metrics");
    ok &= check(signal_synth::build_ecg_export_bundle(multimodal_render, multimodal_bundle, result), "multimodal_export");
    const signal_synth::ecg_text_artifact* multimodal_csv = multimodal_bundle.find("waveform.csv");
    const signal_synth::ecg_text_artifact* multimodal_annotations = multimodal_bundle.find("annotations.json");
    const signal_synth::ecg_text_artifact* multimodal_metrics = multimodal_bundle.find("ground_truth_metrics.json");
    const signal_synth::ecg_text_artifact* multimodal_report = multimodal_bundle.find("report.html");
    const signal_synth::ecg_text_artifact* multimodal_wfdb_header = multimodal_bundle.find("synsigra.hea");
    const signal_synth::ecg_text_artifact* multimodal_wfdb_signal = multimodal_bundle.find("synsigra.dat");
    const signal_synth::ecg_text_artifact* multimodal_edf = multimodal_bundle.find("synsigra.edf");
    const signal_synth::ecg_text_artifact* multimodal_bdf = multimodal_bundle.find("synsigra.bdf");
    ok &= check(multimodal_csv && multimodal_csv->content.find(",ppg_green_au\n") != std::string::npos, "ppg_csv_channel");
    ok &= check(multimodal_annotations && multimodal_annotations->content.find("\"ppg_fiducials\":[{") != std::string::npos, "ppg_annotation_export");
    ok &= check(multimodal_metrics && multimodal_metrics->content.find("\"ppg\":{\"pulse_count\":") != std::string::npos, "ppg_metric_export");
    ok &= check(multimodal_report && multimodal_report->content.find("<h2>PPG Preview</h2><svg") != std::string::npos, "ppg_report_preview");
    ok &= check(multimodal_wfdb_header && multimodal_wfdb_header->content.find(wfdb_record_line("synsigra", 13, multimodal_render.record)) == 0 && multimodal_wfdb_header->content.find("10000(0)/NU 16 0") != std::string::npos, "ppg_wfdb_header");
    ok &= check(multimodal_wfdb_signal && multimodal_wfdb_signal->content.size() == multimodal_render.record.sample_count() * 13u * 2u, "ppg_wfdb_signal_size");
    ok &= check(multimodal_edf && multimodal_edf->content.find("ppg_systolic_peak") != std::string::npos, "ppg_edf_annotation");
    ok &= check(multimodal_bdf && multimodal_bdf->content.find("ppg_systolic_peak") != std::string::npos, "ppg_bdf_annotation");

    return ok ? 0 : 1;
}
