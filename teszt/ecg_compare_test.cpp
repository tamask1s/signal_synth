#include "../src/ecg_compare.h"
#include "../src/ecg_export.h"

#include <cmath>
#include <iostream>
#include <sstream>
#include <vector>

namespace
{
    bool check(bool condition, const char* name)
    {
        if (!condition)
            std::cerr << "FAIL: " << name << '\n';
        return condition;
    }

    std::string generation_fingerprint_field(unsigned long long value)
    {
        std::ostringstream output;
        output << "\"generation_fingerprint\":\"" << value << '"';
        return output.str();
    }

    bool render_document(const signal_synth::ecg_scenario_document& document, signal_synth::ecg_render_bundle& render)
    {
        signal_synth::ecg_export_result result;
        return signal_synth::render_ecg_document(document, render, result) && result.success;
    }

    std::vector<signal_synth::ecg_detected_event> r_peak_detections(const signal_synth::ecg_render_bundle& render)
    {
        std::vector<signal_synth::ecg_detected_event> detections;
        for (unsigned int i = 0; i < render.record.beat_count(); ++i)
        {
            signal_synth::ecg_detected_event event;
            event.time_seconds = render.record.beats()[i].r_peak_time_seconds;
            event.label = "r";
            detections.push_back(event);
        }
        return detections;
    }

    std::vector<signal_synth::ecg_detected_event> ppg_peak_detections(const signal_synth::ecg_render_bundle& render)
    {
        std::vector<signal_synth::ecg_detected_event> detections;
        for (unsigned int i = 0; i < render.ppg.annotation_count(); ++i)
        {
            const signal_synth::ppg_annotation& annotation = render.ppg.annotations()[i];
            if (annotation.kind == signal_synth::ppg_systolic_peak && annotation.source == signal_synth::ppg_fiducial_measurement)
            {
                signal_synth::ecg_detected_event event;
                event.time_seconds = annotation.time_seconds;
                event.label = "ppg_peak";
                detections.push_back(event);
            }
        }
        return detections;
    }

    bool compare_r_peaks(const signal_synth::ecg_render_bundle& render, const std::vector<signal_synth::ecg_detected_event>& detections, signal_synth::ecg_compare_result& result)
    {
        signal_synth::ecg_compare_options options;
        options.target = signal_synth::ecg_compare_r_peak;
        return signal_synth::compare_detections_to_render(render, detections, options, result);
    }
}

int main()
{
    bool ok = true;

    signal_synth::ecg_scenario_document document;
    document.scenario_id = "compare_clean";
    document.duration_seconds = 8.0;
    document.ecg.set_seed(1);
    signal_synth::ecg_render_bundle render;
    ok &= check(render_document(document, render), "render_clean_document");

    const std::vector<signal_synth::ecg_detected_event> perfect = r_peak_detections(render);
    signal_synth::ecg_compare_result result;
    ok &= check(compare_r_peaks(render, perfect, result) && result.success, "perfect_compare_success");
    ok &= check(result.total.ground_truth_count == render.record.beat_count() && result.total.detection_count == perfect.size(), "perfect_counts");
    ok &= check(result.total.true_positive_count == result.total.ground_truth_count && result.total.false_positive_count == 0 && result.total.false_negative_count == 0, "perfect_classification");
    ok &= check(result.total.sensitivity == 1.0 && result.total.positive_predictive_value == 1.0 && result.total.f1_score == 1.0, "perfect_metrics");
    ok &= check(result.total.max_absolute_error_seconds == 0.0, "perfect_timing_error");

    std::vector<signal_synth::ecg_detected_event> jittered = perfect;
    for (std::size_t i = 0; i < jittered.size(); ++i)
        jittered[i].time_seconds += (i & 1u) ? -0.010 : 0.010;
    ok &= check(compare_r_peaks(render, jittered, result) && result.total.true_positive_count == result.total.ground_truth_count, "jittered_within_tolerance");
    ok &= check(std::fabs(result.total.mean_absolute_error_seconds - 0.010) < 1e-12 && result.total.rms_error_seconds > 0.009, "jittered_error_metrics");

    std::vector<signal_synth::ecg_detected_event> missed = perfect;
    missed.pop_back();
    ok &= check(compare_r_peaks(render, missed, result) && result.total.false_negative_count == 1 && result.total.false_positive_count == 0, "missed_detection");

    std::vector<signal_synth::ecg_detected_event> false_positive = perfect;
    signal_synth::ecg_detected_event extra;
    extra.time_seconds = document.duration_seconds - 0.050;
    false_positive.push_back(extra);
    ok &= check(compare_r_peaks(render, false_positive, result) && result.total.false_positive_count == 1 && result.total.false_negative_count == 0, "false_positive_detection");

    std::vector<signal_synth::ecg_detected_event> duplicate = perfect;
    signal_synth::ecg_detected_event duplicate_event;
    duplicate_event.time_seconds = perfect[0].time_seconds + 0.001;
    duplicate.insert(duplicate.begin(), duplicate_event);
    ok &= check(compare_r_peaks(render, duplicate, result) && result.total.true_positive_count == result.total.ground_truth_count && result.total.false_positive_count == 1, "duplicate_near_one_truth_is_fp");

    signal_synth::ecg_scenario_document artifact_document;
    artifact_document.schema_version = 2;
    artifact_document.scenario_id = "compare_artifact";
    artifact_document.duration_seconds = 8.0;
    signal_synth::signal_quality_artifact_config artifact;
    artifact.type = signal_synth::signal_quality_ecg_baseline_wander;
    artifact.start_seconds = 0.0;
    artifact.duration_seconds = 2.0;
    artifact.severity = 0.4;
    for (unsigned int lead = 0; lead < signal_synth::clinical_lead_count; ++lead)
        artifact.ecg_leads[lead] = true;
    artifact_document.signal_quality.artifacts.push_back(artifact);
    signal_synth::ecg_render_bundle artifact_render;
    ok &= check(render_document(artifact_document, artifact_render), "render_artifact_document");
    ok &= check(compare_r_peaks(artifact_render, r_peak_detections(artifact_render), result), "artifact_compare_success");
    ok &= check(result.artifact.ground_truth_count > 0 && result.clean.ground_truth_count > 0, "artifact_split_has_both_bins");
    ok &= check(result.artifact.true_positive_count == result.artifact.ground_truth_count && result.clean.true_positive_count == result.clean.ground_truth_count, "artifact_split_tp_counts");

    signal_synth::ecg_scenario_document ppg_document;
    ppg_document.schema_version = 2;
    ppg_document.scenario_id = "compare_ppg";
    ppg_document.duration_seconds = 8.0;
    ppg_document.ppg.enabled = true;
    signal_synth::ecg_render_bundle ppg_render;
    ok &= check(render_document(ppg_document, ppg_render), "render_ppg_document");
    signal_synth::ecg_compare_options ppg_options;
    ppg_options.target = signal_synth::ecg_compare_ppg_systolic_peak;
    ok &= check(signal_synth::compare_detections_to_render(ppg_render, ppg_peak_detections(ppg_render), ppg_options, result), "ppg_compare_success");
    ok &= check(result.target_name == "ppg_systolic_peak" && result.total.ground_truth_count == result.total.true_positive_count && result.total.f1_score == 1.0, "ppg_perfect_metrics");

    signal_synth::ecg_scenario_document no_ppg_document;
    signal_synth::ecg_render_bundle no_ppg_render;
    ok &= check(render_document(no_ppg_document, no_ppg_render), "render_no_ppg_document");
    ok &= check(!signal_synth::compare_detections_to_render(no_ppg_render, std::vector<signal_synth::ecg_detected_event>(), ppg_options, result) && !result.messages.empty(), "ppg_compare_rejects_ecg_only");

    const std::string json = signal_synth::ecg_compare_result_json(render, result);
    const std::string csv = signal_synth::ecg_compare_result_csv(result);
    const std::string html = signal_synth::ecg_compare_report_html(render, result);
    ok &= check(json.find("\"comparison\"") != std::string::npos && json.find("not a clinical validation certificate") != std::string::npos, "json_report_contract");
    ok &= check(render.document_identity.generation_fingerprint > 9223372036854775807ULL && json.find(generation_fingerprint_field(render.document_identity.generation_fingerprint)) != std::string::npos, "json_fingerprint_is_decimal_string");
    ok &= check(csv.find("row_type,bin,ground_truth_index") == 0, "csv_report_contract");
    ok &= check(html.find("Algorithm Comparison Report") != std::string::npos && html.find("not diagnosis") != std::string::npos, "html_report_contract");

    return ok ? 0 : 1;
}
