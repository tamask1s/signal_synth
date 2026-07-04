#include "../src/hrv_scoring.h"

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

    std::string user_json(const signal_synth::ecg_render_bundle& render, double sdnn_offset)
    {
        const signal_synth::hrv_metric_summary& metrics = render.hrv.metrics;
        std::ostringstream output;
        output.precision(17);
        output << "{\"schema_version\":1,\"algorithm\":{\"name\":\"unit_hrv\",\"version\":\"1.2\"},\"metrics\":{"
               << "\"mean_rr_seconds\":" << metrics.mean_rr_seconds
               << ",\"sdnn_seconds\":" << metrics.sdnn_seconds + sdnn_offset
               << ",\"rmssd_seconds\":" << metrics.rmssd_seconds
               << ",\"sd1_seconds\":" << metrics.sd1_seconds
               << ",\"sd2_seconds\":" << metrics.sd2_seconds
               << ",\"lf_hf_ratio\":" << metrics.lf_hf_ratio << "},\"rr_intervals\":[";
        bool first = true;
        for (std::size_t i = 0; i < render.hrv.intervals.size(); ++i)
        {
            if (render.hrv.intervals[i].excluded)
                continue;
            output << (first ? "" : ",") << "{\"beat_time_seconds\":" << render.hrv.intervals[i].beat_time_seconds
                   << ",\"rr_seconds\":" << render.hrv.intervals[i].rr_seconds << '}';
            first = false;
        }
        output << "]}";
        return output.str();
    }
}

int main()
{
    bool ok = true;
    signal_synth::ecg_scenario_document document;
    document.schema_version = 2;
    document.scenario_id = "hrv_score_unit";
    document.duration_seconds = 90.0;
    document.ecg.set_rr_variability_seconds(0.04);

    signal_synth::ecg_render_bundle render;
    signal_synth::ecg_export_result export_result;
    ok &= check(signal_synth::render_ecg_document(document, render, export_result), "render_hrv_scenario");
    ok &= check(render.hrv.metrics.accepted_interval_count > 20 && render.hrv.metrics.sdnn_seconds > 0.0, "render_has_hrv_truth");

    signal_synth::hrv_user_output user;
    std::vector<std::string> messages;
    ok &= check(signal_synth::parse_hrv_user_output_json(user_json(render, 0.0), user, messages), "parse_valid_user_output");
    ok &= check(user.algorithm_name == "unit_hrv" && user.metrics.size() == 6 && user.rr_intervals.size() == render.hrv.metrics.accepted_interval_count, "user_output_contract");

    signal_synth::hrv_score_result score;
    ok &= check(signal_synth::score_hrv_user_output(render, user, score) && score.success, "score_valid_output");
    ok &= check(score.passed_metric_count == score.metrics.size() && score.metric_pass_fraction == 1.0, "perfect_metric_score");
    ok &= check(score.rr.evaluated && score.rr.matched_count == score.rr.ground_truth_count && score.rr.missing_count == 0 && score.rr.extra_count == 0 && score.rr.passed_count == score.rr.matched_count, "perfect_rr_score");

    signal_synth::hrv_user_output changed;
    ok &= check(signal_synth::parse_hrv_user_output_json(user_json(render, 0.2), changed, messages), "parse_changed_output");
    signal_synth::hrv_score_result changed_score;
    ok &= check(signal_synth::score_hrv_user_output(render, changed, changed_score) && changed_score.passed_metric_count + 1 == changed_score.metrics.size(), "failed_metric_is_reported");

    const std::string json = signal_synth::hrv_score_result_json(score);
    const std::string csv = signal_synth::hrv_score_result_csv(score);
    const std::string html = signal_synth::hrv_score_report_html(score);
    ok &= check(json.find("\"score_type\":\"hrv_algorithm_qa\"") != std::string::npos && json.find("\"metric_definition_version\":\"synsigra_hrv_metrics_v1\"") != std::string::npos && json.find("\"matched_count\":") != std::string::npos, "json_report_contract");
    ok &= check(csv.find("row_type,name,unit") == 0 && csv.find("metric,mean_rr_seconds,s") != std::string::npos, "csv_report_contract");
    ok &= check(html.find("HRV Algorithm QA Score") != std::string::npos && html.find("not diagnosis or clinical validation certification") != std::string::npos, "html_report_contract");

    signal_synth::hrv_user_output preserved = user;
    ok &= check(!signal_synth::parse_hrv_user_output_json("{\"schema_version\":1,\"algorithm\":{\"name\":\"x\",\"version\":\"1\"},\"metrics\":{\"unknown\":1}}", preserved, messages) && preserved.algorithm_name == "unit_hrv", "unknown_metric_rejected_transactionally");
    ok &= check(!signal_synth::parse_hrv_user_output_json("{\"schema_version\":2,\"algorithm\":{\"name\":\"x\",\"version\":\"1\"},\"metrics\":{\"sdnn_seconds\":1}}", preserved, messages), "schema_version_rejected");
    ok &= check(!signal_synth::parse_hrv_user_output_json("{\"schema_version\":1,\"algorithm\":{\"name\":\"x\",\"version\":\"1\"},\"metrics\":{}}", preserved, messages), "empty_output_rejected");

    signal_synth::ecg_render_bundle incomplete;
    signal_synth::hrv_score_result preserved_score = score;
    ok &= check(!signal_synth::score_hrv_user_output(incomplete, user, preserved_score) && preserved_score.success == false, "incomplete_render_rejected");

    return ok ? 0 : 1;
}
