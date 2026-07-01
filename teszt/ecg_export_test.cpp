#include "../src/ecg_export.h"

#include <cmath>
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
    const char* expected[] = {"scenario.json","metadata.json","waveform.csv","annotations.json","ground_truth_metrics.json","warnings.json","report.html","README.txt"};
    ok &= check(bundle.artifacts.size() == 8, "artifact_count");
    for (unsigned int i = 0; i < 8; ++i)
        ok &= check(bundle.artifacts[i].name == expected[i] && !bundle.artifacts[i].content.empty(), "artifact_order_and_content");

    const signal_synth::ecg_text_artifact* csv = bundle.find("waveform.csv");
    const signal_synth::ecg_text_artifact* annotations = bundle.find("annotations.json");
    const signal_synth::ecg_text_artifact* metrics = bundle.find("ground_truth_metrics.json");
    const signal_synth::ecg_text_artifact* report = bundle.find("report.html");
    const signal_synth::ecg_text_artifact* metadata = bundle.find("metadata.json");
    ok &= check(csv && csv->content.find("sample_index,time_seconds,I_mv,II_mv,III_mv,aVR_mv,aVL_mv,aVF_mv,V1_mv,V2_mv,V3_mv,V4_mv,V5_mv,V6_mv\n") == 0 && line_count(csv->content) == render.record.sample_count() + 1, "csv_contract");
    ok &= check(annotations && annotations->content.find("\"artifact_intervals\":[]") != std::string::npos && annotations->content.find("\"r_peak\"") != std::string::npos, "annotation_contract");
    ok &= check(metrics && metrics->content.find("\"sdnn_seconds\":") != std::string::npos && metrics->content.find("\"assertions\":[") != std::string::npos, "metrics_contract");
    ok &= check(metadata && metadata->content.find("\"channels\":[{\"name\":\"I\",\"unit\":\"mV\"}") != std::string::npos && metadata->content.find("{\"name\":\"V6\",\"unit\":\"mV\"}]") != std::string::npos, "channel_metadata_contract");
    ok &= check(report && report->content.find("&lt;Export &amp; Report&gt;") != std::string::npos && report->content.find("<polyline") != std::string::npos, "html_escape_and_actual_plot");
    ok &= check(report && report->content.find("not a clinical validation certificate") != std::string::npos && report->content.find("clinically validated") == std::string::npos, "controlled_report_claims");

    signal_synth::ecg_render_bundle repeated_render;
    signal_synth::ecg_export_bundle repeated_bundle;
    ok &= check(signal_synth::render_ecg_document(document, repeated_render, result) && signal_synth::build_ecg_export_bundle(repeated_render, repeated_bundle, result) && same_artifacts(bundle, repeated_bundle), "byte_deterministic_artifacts");

    signal_synth::ecg_scenario_document invalid = document;
    invalid.duration_seconds = 0.0;
    signal_synth::ecg_render_bundle preserved = render;
    ok &= check(!signal_synth::render_ecg_document(invalid, preserved, result) && preserved.document.scenario_id == "export_test" && preserved.record.sample_count() == render.record.sample_count(), "failed_render_is_transactional");

    signal_synth::ecg_render_bundle incomplete;
    signal_synth::ecg_export_bundle preserved_export = bundle;
    ok &= check(!signal_synth::build_ecg_export_bundle(incomplete, preserved_export, result) && preserved_export.artifacts.size() == 8, "failed_export_is_transactional");
    ok &= check(std::string(signal_synth::signal_synth_generator_version()) == "0.1.0-dev", "runtime_generator_version");

    return ok ? 0 : 1;
}
