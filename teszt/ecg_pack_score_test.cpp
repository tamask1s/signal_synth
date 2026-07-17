#include "../src/ecg_export.h"
#include "../src/ecg_pack.h"
#include "../src/ecg_pack_score.h"

#include <iostream>
#include <vector>

namespace
{
    bool check(bool condition, const char* name)
    {
        if (!condition)
            std::cerr << "FAIL: " << name << '\n';
        return condition;
    }

    bool render_document(const char* scenario_id, signal_synth::ecg_render_bundle& render)
    {
        signal_synth::ecg_scenario_document document;
        document.scenario_id = scenario_id;
        document.duration_seconds = 8.0;
        signal_synth::ecg_document_render_result result;
        return signal_synth::render_ecg_document(document, render, result) && result.success;
    }

    std::vector<signal_synth::ecg_detected_event> r_peak_detections(const signal_synth::ecg_render_bundle& render)
    {
        std::vector<signal_synth::ecg_detected_event> detections;
        for (unsigned int i = 0; i < render.record.beat_count(); ++i)
        {
            signal_synth::ecg_detected_event event;
            event.time_seconds = render.record.beats()[i].r_peak_time_seconds;
            event.original_index = i;
            event.has_original_index = true;
            detections.push_back(event);
        }
        return detections;
    }

    signal_synth::ecg_pack_score_case score_case(const char* case_id, const char* scenario_id, const signal_synth::ecg_render_bundle& render, std::vector<signal_synth::ecg_detected_event> detections)
    {
        signal_synth::ecg_compare_options options;
        options.target = signal_synth::ecg_compare_r_peak;
        signal_synth::ecg_pack_score_case item;
        item.case_id = case_id;
        item.scenario_id = scenario_id;
        item.scenario_path = std::string("cases/") + case_id + ".json";
        item.document_fingerprint = std::string("sha256:") + case_id;
        item.render_identity = render.render_identity;
        item.detection_input_id = std::string("detections/") + case_id + ".json";
        item.detection_algorithm_name = "unit_detector";
        item.detection_algorithm_version = "1";
        signal_synth::compare_detections_to_render(render, detections, options, item.comparison);
        return item;
    }
}

int main()
{
    bool ok = true;
    signal_synth::ecg_render_bundle clean_render;
    signal_synth::ecg_render_bundle noisy_render;
    ok &= check(render_document("pack_score_clean", clean_render), "render_clean");
    ok &= check(render_document("pack_score_noisy", noisy_render), "render_noisy");

    std::vector<signal_synth::ecg_detected_event> clean_detections = r_peak_detections(clean_render);
    std::vector<signal_synth::ecg_detected_event> noisy_detections = r_peak_detections(noisy_render);
    noisy_detections.pop_back();
    signal_synth::ecg_detected_event false_positive;
    false_positive.time_seconds = 0.050;
    false_positive.original_index = static_cast<unsigned int>(noisy_detections.size());
    false_positive.has_original_index = true;
    noisy_detections.push_back(false_positive);

    signal_synth::ecg_pack_manifest manifest;
    manifest.pack_id = "pack_score_unit";
    manifest.name = "Pack Score Unit";
    manifest.version = "1";
    manifest.description = "Pack scoring unit test";
    manifest.targets.push_back("r_peak");
    signal_synth::ecg_pack_score_case clean = score_case("clean", "pack_score_clean", clean_render, clean_detections);
    signal_synth::ecg_pack_score_case noisy = score_case("noisy", "pack_score_noisy", noisy_render, noisy_detections);
    std::vector<signal_synth::ecg_pack_score_case> cases;
    cases.push_back(clean);
    cases.push_back(noisy);

    signal_synth::ecg_pack_score_summary summary;
    ok &= check(signal_synth::build_ecg_pack_score_summary(manifest, "sha256:pack", cases, summary) && summary.success, "build_summary");
    ok &= check(summary.targets.size() == 1 && summary.targets[0].target_name == "r_peak" && summary.targets[0].case_count == 2, "target_shape");
    ok &= check(summary.targets[0].total.ground_truth_count == clean.comparison.total.ground_truth_count + noisy.comparison.total.ground_truth_count, "aggregate_gt_count");
    ok &= check(summary.targets[0].total.false_negative_count == 1 && summary.targets[0].total.false_positive_count == 1, "aggregate_error_counts");
    ok &= check(summary.targets[0].total.sensitivity < 1.0 && summary.targets[0].total.positive_predictive_value < 1.0, "aggregate_ratios");

    const std::string json = signal_synth::ecg_pack_score_summary_json(summary);
    const std::string csv = signal_synth::ecg_pack_score_summary_csv(summary);
    const std::string html = signal_synth::ecg_pack_score_report_html(summary);
    ok &= check(json.find("\"summary_type\":\"algorithm_qa_pack_score\"") != std::string::npos
        && json.find("\"detection_input_id\":\"detections/noisy.json\"") != std::string::npos
        && json.find("\"motion\":") != std::string::npos && json.find("\"dropout\":") != std::string::npos, "json_contract");
    ok &= check(csv.find("row_type,target,bin") == 0 && csv.find("target_summary,r_peak,total") != std::string::npos, "csv_contract");
    ok &= check(html.find("Algorithm QA Pack Score") != std::string::npos && html.find("not diagnosis") != std::string::npos, "html_contract");

    std::vector<signal_synth::ecg_pack_score_case> empty;
    ok &= check(!signal_synth::build_ecg_pack_score_summary(manifest, "sha256:pack", empty, summary) && !summary.messages.empty(), "reject_empty_summary");

    return ok ? 0 : 1;
}
