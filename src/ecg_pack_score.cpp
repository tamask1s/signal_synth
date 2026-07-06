#include "ecg_pack_score.h"

#include "ecg_pack.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>

namespace
{
    const char* scoring_version = "pack_score_v2";

    std::string json_string(const std::string& value)
    {
        std::ostringstream output;
        output << '"';
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            const unsigned char ch = static_cast<unsigned char>(value[i]);
            switch (ch)
            {
            case '"': output << "\\\""; break;
            case '\\': output << "\\\\"; break;
            case '\b': output << "\\b"; break;
            case '\f': output << "\\f"; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default:
                if (ch < 0x20)
                    output << "\\u00" << "0123456789abcdef"[ch >> 4] << "0123456789abcdef"[ch & 15u];
                else
                    output << static_cast<char>(ch);
                break;
            }
        }
        output << '"';
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
            default: output.push_back(value[i]); break;
            }
        }
        return output;
    }

    std::string csv_cell(const std::string& value)
    {
        bool quote = false;
        for (std::size_t i = 0; i < value.size(); ++i)
            if (value[i] == ',' || value[i] == '"' || value[i] == '\n' || value[i] == '\r')
                quote = true;
        if (!quote)
            return value;
        std::string output = "\"";
        for (std::size_t i = 0; i < value.size(); ++i)
            output += value[i] == '"' ? "\"\"" : std::string(1, value[i]);
        output += '"';
        return output;
    }

    void add_counts(signal_synth::ecg_compare_bin_metrics& output, const signal_synth::ecg_compare_bin_metrics& input)
    {
        output.ground_truth_count += input.ground_truth_count;
        output.detection_count += input.detection_count;
        output.true_positive_count += input.true_positive_count;
        output.false_positive_count += input.false_positive_count;
        output.false_negative_count += input.false_negative_count;
    }

    void add_error(std::vector<double>& total_errors, std::vector<double>& clean_errors, std::vector<double>& artifact_errors, std::vector<double>& motion_errors, std::vector<double>& dropout_errors, std::vector<double>& low_perfusion_errors, const signal_synth::ecg_compare_match& match)
    {
        const double absolute_error = std::fabs(match.error_seconds);
        total_errors.push_back(absolute_error);
        if (match.in_artifact_interval)
            artifact_errors.push_back(absolute_error);
        else
            clean_errors.push_back(absolute_error);
        if (match.in_motion_artifact_interval)
            motion_errors.push_back(absolute_error);
        if (match.in_dropout_artifact_interval)
            dropout_errors.push_back(absolute_error);
        if (match.low_perfusion)
            low_perfusion_errors.push_back(absolute_error);
    }

    void finalize_metrics(signal_synth::ecg_compare_bin_metrics& metrics, const std::vector<double>& absolute_errors)
    {
        if (metrics.ground_truth_count)
            metrics.sensitivity = static_cast<double>(metrics.true_positive_count) / metrics.ground_truth_count;
        if (metrics.detection_count)
            metrics.positive_predictive_value = static_cast<double>(metrics.true_positive_count) / metrics.detection_count;
        if (metrics.sensitivity + metrics.positive_predictive_value > 0.0)
            metrics.f1_score = 2.0 * metrics.sensitivity * metrics.positive_predictive_value / (metrics.sensitivity + metrics.positive_predictive_value);
        if (!absolute_errors.empty())
        {
            std::vector<double> sorted = absolute_errors;
            std::sort(sorted.begin(), sorted.end());
            double sum = 0.0;
            double squared_sum = 0.0;
            for (std::size_t i = 0; i < sorted.size(); ++i)
            {
                sum += sorted[i];
                squared_sum += sorted[i] * sorted[i];
            }
            metrics.mean_absolute_error_seconds = sum / sorted.size();
            metrics.rms_error_seconds = std::sqrt(squared_sum / sorted.size());
            metrics.max_absolute_error_seconds = sorted.back();
            if (sorted.size() & 1u)
                metrics.median_absolute_error_seconds = sorted[sorted.size() / 2u];
            else
                metrics.median_absolute_error_seconds = 0.5 * (sorted[sorted.size() / 2u - 1u] + sorted[sorted.size() / 2u]);
        }
    }

    void write_metrics_json(std::ostringstream& output, const signal_synth::ecg_compare_bin_metrics& metrics)
    {
        output << "{\"ground_truth_count\":" << metrics.ground_truth_count
               << ",\"detection_count\":" << metrics.detection_count
               << ",\"true_positive_count\":" << metrics.true_positive_count
               << ",\"false_positive_count\":" << metrics.false_positive_count
               << ",\"false_negative_count\":" << metrics.false_negative_count
               << ",\"sensitivity\":" << metrics.sensitivity
               << ",\"positive_predictive_value\":" << metrics.positive_predictive_value
               << ",\"f1_score\":" << metrics.f1_score
               << ",\"mean_absolute_error_seconds\":" << metrics.mean_absolute_error_seconds
               << ",\"median_absolute_error_seconds\":" << metrics.median_absolute_error_seconds
               << ",\"rms_error_seconds\":" << metrics.rms_error_seconds
               << ",\"max_absolute_error_seconds\":" << metrics.max_absolute_error_seconds << '}';
    }

    signal_synth::ecg_pack_score_target* find_target(std::vector<signal_synth::ecg_pack_score_target>& targets, const std::string& target_name)
    {
        for (std::size_t i = 0; i < targets.size(); ++i)
            if (targets[i].target_name == target_name)
                return &targets[i];
        signal_synth::ecg_pack_score_target target;
        target.target_name = target_name;
        targets.push_back(target);
        return &targets.back();
    }

    void write_metric_csv_row(std::ostringstream& output, const std::string& row_type, const std::string& target, const std::string& bin, const signal_synth::ecg_compare_bin_metrics& metrics)
    {
        output << row_type << ',' << csv_cell(target) << ',' << csv_cell(bin) << ",,,,,,,,,"
               << metrics.ground_truth_count << ',' << metrics.detection_count << ',' << metrics.true_positive_count << ','
               << metrics.false_positive_count << ',' << metrics.false_negative_count << ',' << metrics.sensitivity << ','
               << metrics.positive_predictive_value << ',' << metrics.f1_score << ',' << metrics.mean_absolute_error_seconds << ','
               << metrics.median_absolute_error_seconds << ',' << metrics.rms_error_seconds << ',' << metrics.max_absolute_error_seconds << '\n';
    }
}

namespace signal_synth
{
    ecg_pack_score_case::ecg_pack_score_case() : case_id(), scenario_id(), scenario_path(), document_fingerprint(), render_identity(), detection_input_id(), detection_algorithm_name(), detection_algorithm_version(), comparison() {}
    ecg_pack_score_target::ecg_pack_score_target() : target_name(), total(), clean(), artifact(), motion(), dropout(), low_perfusion(), case_count(0) {}
    ecg_pack_score_summary::ecg_pack_score_summary() : success(false), pack_id(), pack_name(), pack_version(), pack_fingerprint(), scoring_version(), cases(), targets(), messages() {}

    bool build_ecg_pack_score_summary(const ecg_pack_manifest& manifest, const std::string& pack_fingerprint, const std::vector<ecg_pack_score_case>& cases, ecg_pack_score_summary& summary)
    {
        ecg_pack_score_summary fresh;
        fresh.pack_id = manifest.pack_id;
        fresh.pack_name = manifest.name;
        fresh.pack_version = manifest.version;
        fresh.pack_fingerprint = pack_fingerprint;
        fresh.scoring_version = scoring_version;
        if (cases.empty())
            fresh.messages.push_back("at least one scored case is required");
        for (std::size_t i = 0; i < cases.size(); ++i)
        {
            if (!cases[i].comparison.success)
                fresh.messages.push_back("all case comparisons must be successful");
            fresh.cases.push_back(cases[i]);
        }
        if (!fresh.messages.empty())
        {
            summary = fresh;
            return false;
        }

        std::vector<std::vector<double> > total_errors;
        std::vector<std::vector<double> > clean_errors;
        std::vector<std::vector<double> > artifact_errors;
        std::vector<std::vector<double> > motion_errors;
        std::vector<std::vector<double> > dropout_errors;
        std::vector<std::vector<double> > low_perfusion_errors;
        for (std::size_t i = 0; i < fresh.cases.size(); ++i)
        {
            const ecg_pack_score_case& score_case = fresh.cases[i];
            ecg_pack_score_target* target = find_target(fresh.targets, score_case.comparison.target_name);
            const std::size_t target_index = static_cast<std::size_t>(target - &fresh.targets[0]);
            while (total_errors.size() <= target_index)
            {
                total_errors.push_back(std::vector<double>());
                clean_errors.push_back(std::vector<double>());
                artifact_errors.push_back(std::vector<double>());
                motion_errors.push_back(std::vector<double>());
                dropout_errors.push_back(std::vector<double>());
                low_perfusion_errors.push_back(std::vector<double>());
            }
            add_counts(target->total, score_case.comparison.total);
            add_counts(target->clean, score_case.comparison.clean);
            add_counts(target->artifact, score_case.comparison.artifact);
            add_counts(target->motion, score_case.comparison.motion);
            add_counts(target->dropout, score_case.comparison.dropout);
            add_counts(target->low_perfusion, score_case.comparison.low_perfusion);
            ++target->case_count;
            for (std::size_t match = 0; match < score_case.comparison.matches.size(); ++match)
                add_error(total_errors[target_index], clean_errors[target_index], artifact_errors[target_index], motion_errors[target_index], dropout_errors[target_index], low_perfusion_errors[target_index], score_case.comparison.matches[match]);
        }
        for (std::size_t i = 0; i < fresh.targets.size(); ++i)
        {
            finalize_metrics(fresh.targets[i].total, total_errors[i]);
            finalize_metrics(fresh.targets[i].clean, clean_errors[i]);
            finalize_metrics(fresh.targets[i].artifact, artifact_errors[i]);
            finalize_metrics(fresh.targets[i].motion, motion_errors[i]);
            finalize_metrics(fresh.targets[i].dropout, dropout_errors[i]);
            finalize_metrics(fresh.targets[i].low_perfusion, low_perfusion_errors[i]);
        }
        fresh.success = true;
        summary = fresh;
        return true;
    }

    std::string ecg_pack_score_summary_json(const ecg_pack_score_summary& summary)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << "{\"schema_version\":1,\"summary_type\":\"algorithm_qa_pack_score\",\"pack_id\":" << json_string(summary.pack_id)
               << ",\"pack_name\":" << json_string(summary.pack_name)
               << ",\"pack_version\":" << json_string(summary.pack_version)
               << ",\"pack_fingerprint\":" << json_string(summary.pack_fingerprint)
               << ",\"scoring_version\":" << json_string(summary.scoring_version)
               << ",\"limitation\":\"synthetic engineering QA evidence, not diagnosis or clinical validation certification\""
               << ",\"targets\":[";
        for (std::size_t i = 0; i < summary.targets.size(); ++i)
        {
            const ecg_pack_score_target& target = summary.targets[i];
            output << (i ? "," : "") << "{\"target\":" << json_string(target.target_name)
                   << ",\"case_count\":" << target.case_count << ",\"total\":";
            write_metrics_json(output, target.total);
            output << ",\"clean\":";
            write_metrics_json(output, target.clean);
            output << ",\"artifact\":";
            write_metrics_json(output, target.artifact);
            output << ",\"motion\":";
            write_metrics_json(output, target.motion);
            output << ",\"dropout\":";
            write_metrics_json(output, target.dropout);
            output << ",\"low_perfusion\":";
            write_metrics_json(output, target.low_perfusion);
            output << "}";
        }
        output << "],\"cases\":[";
        for (std::size_t i = 0; i < summary.cases.size(); ++i)
        {
            const ecg_pack_score_case& item = summary.cases[i];
            output << (i ? "," : "") << "{\"case_id\":" << json_string(item.case_id)
                   << ",\"scenario_id\":" << json_string(item.scenario_id)
                   << ",\"scenario_path\":" << json_string(item.scenario_path)
                   << ",\"document_fingerprint\":" << json_string(item.document_fingerprint)
                   << ",\"render_identity\":" << json_string(item.render_identity)
                   << ",\"detection_input_id\":" << json_string(item.detection_input_id)
                   << ",\"detection_algorithm\":{\"name\":" << json_string(item.detection_algorithm_name)
                   << ",\"version\":" << json_string(item.detection_algorithm_version)
                   << "},\"target\":" << json_string(item.comparison.target_name)
                   << ",\"total\":";
            write_metrics_json(output, item.comparison.total);
            output << ",\"clean\":";
            write_metrics_json(output, item.comparison.clean);
            output << ",\"artifact\":";
            write_metrics_json(output, item.comparison.artifact);
            output << ",\"motion\":";
            write_metrics_json(output, item.comparison.motion);
            output << ",\"dropout\":";
            write_metrics_json(output, item.comparison.dropout);
            output << ",\"low_perfusion\":";
            write_metrics_json(output, item.comparison.low_perfusion);
            output << "}";
        }
        output << "]}";
        return output.str();
    }

    std::string ecg_pack_score_summary_csv(const ecg_pack_score_summary& summary)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << "row_type,target,bin,case_id,scenario_id,scenario_path,document_fingerprint,render_identity,detection_input_id,detection_algorithm_name,detection_algorithm_version,ground_truth_count,detection_count,true_positive_count,false_positive_count,false_negative_count,sensitivity,positive_predictive_value,f1_score,mean_absolute_error_seconds,median_absolute_error_seconds,rms_error_seconds,max_absolute_error_seconds\n";
        for (std::size_t i = 0; i < summary.targets.size(); ++i)
        {
            write_metric_csv_row(output, "target_summary", summary.targets[i].target_name, "total", summary.targets[i].total);
            write_metric_csv_row(output, "target_summary", summary.targets[i].target_name, "clean", summary.targets[i].clean);
            write_metric_csv_row(output, "target_summary", summary.targets[i].target_name, "artifact", summary.targets[i].artifact);
            write_metric_csv_row(output, "target_summary", summary.targets[i].target_name, "motion", summary.targets[i].motion);
            write_metric_csv_row(output, "target_summary", summary.targets[i].target_name, "dropout", summary.targets[i].dropout);
            write_metric_csv_row(output, "target_summary", summary.targets[i].target_name, "low_perfusion", summary.targets[i].low_perfusion);
        }
        for (std::size_t i = 0; i < summary.cases.size(); ++i)
        {
            const ecg_pack_score_case& item = summary.cases[i];
            output << "case," << csv_cell(item.comparison.target_name) << ",total," << csv_cell(item.case_id) << ',' << csv_cell(item.scenario_id)
                   << ',' << csv_cell(item.scenario_path) << ',' << csv_cell(item.document_fingerprint) << ',' << csv_cell(item.render_identity)
                   << ',' << csv_cell(item.detection_input_id) << ',' << csv_cell(item.detection_algorithm_name) << ',' << csv_cell(item.detection_algorithm_version)
                   << ',' << item.comparison.total.ground_truth_count << ',' << item.comparison.total.detection_count << ',' << item.comparison.total.true_positive_count
                   << ',' << item.comparison.total.false_positive_count << ',' << item.comparison.total.false_negative_count << ',' << item.comparison.total.sensitivity
                   << ',' << item.comparison.total.positive_predictive_value << ',' << item.comparison.total.f1_score << ',' << item.comparison.total.mean_absolute_error_seconds
                   << ',' << item.comparison.total.median_absolute_error_seconds << ',' << item.comparison.total.rms_error_seconds << ',' << item.comparison.total.max_absolute_error_seconds << '\n';
        }
        return output.str();
    }

    std::string ecg_pack_score_report_html(const ecg_pack_score_summary& summary)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << "<!doctype html><html><head><meta charset=\"utf-8\"><title>Algorithm QA Pack Score</title>"
               << "<style>body{font-family:Arial,sans-serif;margin:24px;line-height:1.45}table{border-collapse:collapse;width:100%;margin:12px 0}th,td{border:1px solid #d1d5db;padding:6px 8px;text-align:left}th{background:#f3f4f6}code{font-family:monospace}</style></head><body>"
               << "<h1>Algorithm QA Pack Score</h1><p>This report compares external algorithm event detections against synthetic ground truth. It is engineering QA evidence, not diagnosis or clinical validation certification.</p>"
               << "<table><tr><th>Pack</th><td>" << html_text(summary.pack_id) << "</td></tr><tr><th>Version</th><td>" << html_text(summary.pack_version)
               << "</td></tr><tr><th>Fingerprint</th><td><code>" << html_text(summary.pack_fingerprint) << "</code></td></tr><tr><th>Cases</th><td>" << summary.cases.size() << "</td></tr></table>"
               << "<h2>Target Summary</h2><table><tr><th>Target</th><th>Bin</th><th>GT</th><th>Detections</th><th>TP</th><th>FP</th><th>FN</th><th>Sensitivity</th><th>PPV</th><th>F1</th><th>MAE s</th></tr>";
        for (std::size_t i = 0; i < summary.targets.size(); ++i)
        {
            const ecg_pack_score_target& target = summary.targets[i];
            const ecg_compare_bin_metrics* metrics[] = {&target.total, &target.clean, &target.artifact, &target.motion, &target.dropout, &target.low_perfusion};
            const char* bins[] = {"total","clean","artifact","motion","dropout","low perfusion"};
            for (unsigned int bin = 0; bin < 6; ++bin)
                output << "<tr><td>" << html_text(target.target_name) << "</td><td>" << bins[bin] << "</td><td>" << metrics[bin]->ground_truth_count
                       << "</td><td>" << metrics[bin]->detection_count << "</td><td>" << metrics[bin]->true_positive_count
                       << "</td><td>" << metrics[bin]->false_positive_count << "</td><td>" << metrics[bin]->false_negative_count
                       << "</td><td>" << metrics[bin]->sensitivity << "</td><td>" << metrics[bin]->positive_predictive_value
                       << "</td><td>" << metrics[bin]->f1_score << "</td><td>" << metrics[bin]->mean_absolute_error_seconds << "</td></tr>";
        }
        output << "</table><h2>Cases</h2><table><tr><th>Case</th><th>Scenario</th><th>Target</th><th>Detection input</th><th>Algorithm</th><th>F1</th><th>Render identity</th></tr>";
        for (std::size_t i = 0; i < summary.cases.size(); ++i)
        {
            const ecg_pack_score_case& item = summary.cases[i];
            output << "<tr><td>" << html_text(item.case_id) << "</td><td>" << html_text(item.scenario_id)
                   << "</td><td>" << html_text(item.comparison.target_name) << "</td><td>" << html_text(item.detection_input_id)
                   << "</td><td>" << html_text(item.detection_algorithm_name) << " " << html_text(item.detection_algorithm_version)
                   << "</td><td>" << item.comparison.total.f1_score << "</td><td><code>" << html_text(item.render_identity) << "</code></td></tr>";
        }
        output << "</table><h2>Artifacts</h2><p>pack_score_summary.json, pack_score_summary.csv, pack_score_report.html</p></body></html>";
        return output.str();
    }
}
