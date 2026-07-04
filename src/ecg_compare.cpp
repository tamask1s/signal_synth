#include "ecg_compare.h"

#include "ecg_export.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace
{
    struct truth_event
    {
        unsigned int index;
        double time_seconds;
        bool in_artifact_interval;
    };

    struct detection_event
    {
        unsigned int original_index;
        double time_seconds;
        bool in_artifact_interval;
    };

    struct candidate_match
    {
        unsigned int truth_index;
        unsigned int detection_sorted_index;
        double absolute_error;
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
            default: output.push_back(value[i]);
            }
        }
        return output;
    }

    const char* boolean(bool value)
    {
        return value ? "true" : "false";
    }

    bool finite_non_negative(double value)
    {
        return value >= 0.0 && value <= std::numeric_limits<double>::max();
    }

    bool artifact_affects_target(const signal_synth::signal_quality_artifact_interval& artifact, signal_synth::ecg_compare_target target)
    {
        if (target == signal_synth::ecg_compare_ppg_systolic_peak)
            return artifact.ppg;
        for (unsigned int lead = 0; lead < signal_synth::clinical_lead_count; ++lead)
            if (artifact.ecg_leads[lead])
                return true;
        return false;
    }

    bool in_artifact_interval(const signal_synth::ecg_render_bundle& render, signal_synth::ecg_compare_target target, double time_seconds)
    {
        for (std::size_t i = 0; i < render.signal_quality.artifacts.size(); ++i)
        {
            const signal_synth::signal_quality_artifact_interval& artifact = render.signal_quality.artifacts[i];
            if (artifact_affects_target(artifact, target) && time_seconds >= artifact.start_seconds && time_seconds < artifact.end_seconds)
                return true;
        }
        return false;
    }

    bool collect_truth_events(const signal_synth::ecg_render_bundle& render, signal_synth::ecg_compare_target target, std::vector<truth_event>& truth, std::string& message)
    {
        truth.clear();
        if (target == signal_synth::ecg_compare_r_peak)
        {
            for (unsigned int i = 0; i < render.record.beat_count(); ++i)
            {
                const signal_synth::clinical_beat_annotation& beat = render.record.beats()[i];
                if (beat.qrs_present && finite_non_negative(beat.r_peak_time_seconds))
                {
                    truth_event event;
                    event.index = i;
                    event.time_seconds = beat.r_peak_time_seconds;
                    event.in_artifact_interval = in_artifact_interval(render, target, event.time_seconds);
                    truth.push_back(event);
                }
            }
            return true;
        }
        if (target == signal_synth::ecg_compare_ppg_systolic_peak)
        {
            if (!render.ppg.sample_count())
            {
                message = "scenario has no PPG channel";
                return false;
            }
            for (unsigned int i = 0; i < render.ppg.annotation_count(); ++i)
            {
                const signal_synth::ppg_annotation& annotation = render.ppg.annotations()[i];
                if (annotation.kind == signal_synth::ppg_systolic_peak && annotation.source == signal_synth::ppg_fiducial_measurement && finite_non_negative(annotation.time_seconds))
                {
                    truth_event event;
                    event.index = static_cast<unsigned int>(truth.size());
                    event.time_seconds = annotation.time_seconds;
                    event.in_artifact_interval = in_artifact_interval(render, target, event.time_seconds);
                    truth.push_back(event);
                }
            }
            return true;
        }
        message = "unsupported comparison target";
        return false;
    }

    bool detection_less(const detection_event& left, const detection_event& right)
    {
        if (left.time_seconds != right.time_seconds)
            return left.time_seconds < right.time_seconds;
        return left.original_index < right.original_index;
    }

    bool candidate_less(const candidate_match& left, const candidate_match& right)
    {
        if (left.absolute_error != right.absolute_error)
            return left.absolute_error < right.absolute_error;
        if (left.truth_index != right.truth_index)
            return left.truth_index < right.truth_index;
        return left.detection_sorted_index < right.detection_sorted_index;
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

    void write_metrics_row(std::ostringstream& output, const char* bin, const signal_synth::ecg_compare_bin_metrics& metrics)
    {
        output << "metrics," << bin << ",,,,,," << metrics.ground_truth_count << ',' << metrics.detection_count << ',' << metrics.true_positive_count << ','
               << metrics.false_positive_count << ',' << metrics.false_negative_count << ',' << metrics.sensitivity << ','
               << metrics.positive_predictive_value << ',' << metrics.f1_score << ',' << metrics.mean_absolute_error_seconds << ','
               << metrics.median_absolute_error_seconds << ',' << metrics.rms_error_seconds << ',' << metrics.max_absolute_error_seconds << '\n';
    }
}

namespace signal_synth
{
    ecg_detected_event::ecg_detected_event() : time_seconds(0.0), label(), original_index(0), has_original_index(false)
    {
    }

    ecg_compare_options::ecg_compare_options() : target(ecg_compare_r_peak), tolerance_seconds(0.0)
    {
    }

    ecg_compare_bin_metrics::ecg_compare_bin_metrics()
        : ground_truth_count(0), detection_count(0), true_positive_count(0), false_positive_count(0), false_negative_count(0), sensitivity(0.0), positive_predictive_value(0.0), f1_score(0.0), mean_absolute_error_seconds(0.0), median_absolute_error_seconds(0.0), rms_error_seconds(0.0), max_absolute_error_seconds(0.0)
    {
    }

    ecg_compare_match::ecg_compare_match()
        : ground_truth_index(0), detection_index(0), ground_truth_time_seconds(0.0), detection_time_seconds(0.0), error_seconds(0.0), in_artifact_interval(false)
    {
    }

    ecg_compare_unmatched_event::ecg_compare_unmatched_event() : index(0), time_seconds(0.0), in_artifact_interval(false)
    {
    }

    ecg_compare_result::ecg_compare_result() : success(false), target_name(), tolerance_seconds(0.0), total(), clean(), artifact(), matches(), false_positives(), false_negatives(), messages()
    {
    }

    double ecg_compare_default_tolerance_seconds(ecg_compare_target target)
    {
        return target == ecg_compare_ppg_systolic_peak ? 0.080 : 0.050;
    }

    const char* ecg_compare_target_name(ecg_compare_target target)
    {
        switch (target)
        {
        case ecg_compare_r_peak: return "r_peak";
        case ecg_compare_ppg_systolic_peak: return "ppg_systolic_peak";
        }
        return "unknown";
    }

    bool compare_detections_to_render(const ecg_render_bundle& render, const std::vector<ecg_detected_event>& detections, const ecg_compare_options& options, ecg_compare_result& result)
    {
        ecg_compare_result fresh;
        fresh.target_name = ecg_compare_target_name(options.target);
        fresh.tolerance_seconds = options.tolerance_seconds > 0.0 ? options.tolerance_seconds : ecg_compare_default_tolerance_seconds(options.target);
        if (!render.record.sample_count() || !render.scenario_report.success())
        {
            fresh.messages.push_back("render bundle is incomplete");
            result = fresh;
            return false;
        }
        if (!(fresh.tolerance_seconds > 0.0))
        {
            fresh.messages.push_back("comparison tolerance must be positive");
            result = fresh;
            return false;
        }
        for (std::size_t i = 0; i < detections.size(); ++i)
        {
            if (!finite_non_negative(detections[i].time_seconds))
            {
                fresh.messages.push_back("detection time must be finite and non-negative");
                result = fresh;
                return false;
            }
        }

        std::vector<truth_event> truth;
        std::string message;
        if (!collect_truth_events(render, options.target, truth, message))
        {
            fresh.messages.push_back(message);
            result = fresh;
            return false;
        }

        std::vector<detection_event> sorted_detections;
        sorted_detections.reserve(detections.size());
        for (std::size_t i = 0; i < detections.size(); ++i)
        {
            detection_event event;
            event.original_index = detections[i].has_original_index ? detections[i].original_index : static_cast<unsigned int>(i);
            event.time_seconds = detections[i].time_seconds;
            event.in_artifact_interval = in_artifact_interval(render, options.target, event.time_seconds);
            sorted_detections.push_back(event);
        }
        std::sort(sorted_detections.begin(), sorted_detections.end(), detection_less);

        std::vector<candidate_match> candidates;
        for (std::size_t ti = 0; ti < truth.size(); ++ti)
        {
            for (std::size_t di = 0; di < sorted_detections.size(); ++di)
            {
                const double absolute_error = std::fabs(sorted_detections[di].time_seconds - truth[ti].time_seconds);
                if (absolute_error <= fresh.tolerance_seconds)
                {
                    candidate_match candidate;
                    candidate.truth_index = static_cast<unsigned int>(ti);
                    candidate.detection_sorted_index = static_cast<unsigned int>(di);
                    candidate.absolute_error = absolute_error;
                    candidates.push_back(candidate);
                }
            }
        }
        std::sort(candidates.begin(), candidates.end(), candidate_less);

        std::vector<bool> truth_matched(truth.size(), false);
        std::vector<bool> detection_matched(sorted_detections.size(), false);
        std::vector<double> total_errors, clean_errors, artifact_errors;
        for (std::size_t i = 0; i < candidates.size(); ++i)
        {
            const candidate_match& candidate = candidates[i];
            if (truth_matched[candidate.truth_index] || detection_matched[candidate.detection_sorted_index])
                continue;
            truth_matched[candidate.truth_index] = true;
            detection_matched[candidate.detection_sorted_index] = true;

            const truth_event& truth_event_ref = truth[candidate.truth_index];
            const detection_event& detection_event_ref = sorted_detections[candidate.detection_sorted_index];
            ecg_compare_match match;
            match.ground_truth_index = truth_event_ref.index;
            match.detection_index = detection_event_ref.original_index;
            match.ground_truth_time_seconds = truth_event_ref.time_seconds;
            match.detection_time_seconds = detection_event_ref.time_seconds;
            match.error_seconds = detection_event_ref.time_seconds - truth_event_ref.time_seconds;
            match.in_artifact_interval = truth_event_ref.in_artifact_interval;
            fresh.matches.push_back(match);

            ++fresh.total.true_positive_count;
            total_errors.push_back(std::fabs(match.error_seconds));
            if (match.in_artifact_interval)
            {
                ++fresh.artifact.true_positive_count;
                artifact_errors.push_back(std::fabs(match.error_seconds));
            }
            else
            {
                ++fresh.clean.true_positive_count;
                clean_errors.push_back(std::fabs(match.error_seconds));
            }
        }

        fresh.total.ground_truth_count = static_cast<unsigned int>(truth.size());
        fresh.total.detection_count = static_cast<unsigned int>(detections.size());
        for (std::size_t i = 0; i < truth.size(); ++i)
        {
            if (truth[i].in_artifact_interval)
                ++fresh.artifact.ground_truth_count;
            else
                ++fresh.clean.ground_truth_count;
            if (!truth_matched[i])
            {
                ecg_compare_unmatched_event event;
                event.index = truth[i].index;
                event.time_seconds = truth[i].time_seconds;
                event.in_artifact_interval = truth[i].in_artifact_interval;
                fresh.false_negatives.push_back(event);
                ++fresh.total.false_negative_count;
                if (event.in_artifact_interval)
                    ++fresh.artifact.false_negative_count;
                else
                    ++fresh.clean.false_negative_count;
            }
        }
        for (std::size_t i = 0; i < sorted_detections.size(); ++i)
        {
            if (sorted_detections[i].in_artifact_interval)
                ++fresh.artifact.detection_count;
            else
                ++fresh.clean.detection_count;
            if (!detection_matched[i])
            {
                ecg_compare_unmatched_event event;
                event.index = sorted_detections[i].original_index;
                event.time_seconds = sorted_detections[i].time_seconds;
                event.in_artifact_interval = sorted_detections[i].in_artifact_interval;
                fresh.false_positives.push_back(event);
                ++fresh.total.false_positive_count;
                if (event.in_artifact_interval)
                    ++fresh.artifact.false_positive_count;
                else
                    ++fresh.clean.false_positive_count;
            }
        }
        finalize_metrics(fresh.total, total_errors);
        finalize_metrics(fresh.clean, clean_errors);
        finalize_metrics(fresh.artifact, artifact_errors);
        fresh.success = true;
        result = fresh;
        return true;
    }

    std::string ecg_compare_result_json(const ecg_render_bundle& render, const ecg_compare_result& result)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10);
        output << "{\"schema_version\":1,\"generator\":{\"name\":\"signal_synth\",\"product\":\"Synsigra Testbench\",\"version\":"
               << json_string(signal_synth_generator_version())
               << "},\"scenario\":{\"id\":" << json_string(render.document.scenario_id)
               << ",\"document_fingerprint\":" << json_string(render.document_identity.document_fingerprint)
               << ",\"generation_fingerprint\":" << render.document_identity.generation_fingerprint
               << ",\"render_identity\":" << json_string(render.render_identity)
               << "},\"comparison\":{\"target\":" << json_string(result.target_name)
               << ",\"tolerance_seconds\":" << result.tolerance_seconds
               << ",\"success\":" << boolean(result.success)
               << ",\"metrics\":{\"total\":";
        write_metrics_json(output, result.total);
        output << ",\"clean\":";
        write_metrics_json(output, result.clean);
        output << ",\"artifact\":";
        write_metrics_json(output, result.artifact);
        output << "},\"matches\":[";
        for (std::size_t i = 0; i < result.matches.size(); ++i)
        {
            const ecg_compare_match& match = result.matches[i];
            output << (i ? "," : "") << "{\"ground_truth_index\":" << match.ground_truth_index
                   << ",\"detection_index\":" << match.detection_index
                   << ",\"ground_truth_time_seconds\":" << match.ground_truth_time_seconds
                   << ",\"detection_time_seconds\":" << match.detection_time_seconds
                   << ",\"error_seconds\":" << match.error_seconds
                   << ",\"in_artifact_interval\":" << boolean(match.in_artifact_interval) << '}';
        }
        output << "],\"false_positives\":[";
        for (std::size_t i = 0; i < result.false_positives.size(); ++i)
        {
            const ecg_compare_unmatched_event& event = result.false_positives[i];
            output << (i ? "," : "") << "{\"detection_index\":" << event.index
                   << ",\"time_seconds\":" << event.time_seconds
                   << ",\"in_artifact_interval\":" << boolean(event.in_artifact_interval) << '}';
        }
        output << "],\"false_negatives\":[";
        for (std::size_t i = 0; i < result.false_negatives.size(); ++i)
        {
            const ecg_compare_unmatched_event& event = result.false_negatives[i];
            output << (i ? "," : "") << "{\"ground_truth_index\":" << event.index
                   << ",\"time_seconds\":" << event.time_seconds
                   << ",\"in_artifact_interval\":" << boolean(event.in_artifact_interval) << '}';
        }
        output << "],\"notes\":[\"Synthetic engineering QA comparison; not a clinical validation certificate or diagnostic result.\"]}}";
        return output.str();
    }

    std::string ecg_compare_result_csv(const ecg_compare_result& result)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10);
        output << "row_type,bin,ground_truth_index,detection_index,ground_truth_time_seconds,detection_time_seconds,error_seconds,ground_truth_count,detection_count,true_positive_count,false_positive_count,false_negative_count,sensitivity,positive_predictive_value,f1_score,mean_absolute_error_seconds,median_absolute_error_seconds,rms_error_seconds,max_absolute_error_seconds\n";
        write_metrics_row(output, "total", result.total);
        write_metrics_row(output, "clean", result.clean);
        write_metrics_row(output, "artifact", result.artifact);
        for (std::size_t i = 0; i < result.matches.size(); ++i)
        {
            const ecg_compare_match& match = result.matches[i];
            output << "match," << (match.in_artifact_interval ? "artifact" : "clean") << ',' << match.ground_truth_index << ',' << match.detection_index << ','
                   << match.ground_truth_time_seconds << ',' << match.detection_time_seconds << ',' << match.error_seconds << ",,,,,,,,,,,,\n";
        }
        for (std::size_t i = 0; i < result.false_positives.size(); ++i)
        {
            const ecg_compare_unmatched_event& event = result.false_positives[i];
            output << "false_positive," << (event.in_artifact_interval ? "artifact" : "clean") << ",," << event.index << ",," << event.time_seconds << ",,,,,,,,,,,,,\n";
        }
        for (std::size_t i = 0; i < result.false_negatives.size(); ++i)
        {
            const ecg_compare_unmatched_event& event = result.false_negatives[i];
            output << "false_negative," << (event.in_artifact_interval ? "artifact" : "clean") << ',' << event.index << ",," << event.time_seconds << ",,,,,,,,,,,,,,\n";
        }
        return output.str();
    }

    std::string ecg_compare_report_html(const ecg_render_bundle& render, const ecg_compare_result& result)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(6);
        output << "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\"><title>"
               << html_text(render.document.scenario_id)
               << " - Algorithm Comparison Report</title><style>"
               << "body{font:14px Arial,sans-serif;color:#202124;max-width:1100px;margin:32px auto;padding:0 20px}"
               << "h1,h2{color:#111827}table{border-collapse:collapse;width:100%;margin:12px 0 24px}"
               << "th,td{border:1px solid #d1d5db;padding:7px;text-align:left}th{background:#f3f4f6}"
               << ".notice{border-left:4px solid #b42318;padding:10px 14px;background:#fef3f2}</style></head><body>"
               << "<h1>Algorithm Comparison Report</h1><p class=\"notice\">This report compares external algorithm event detections against synthetic ground truth. "
               << "It is intended for software testing and algorithm QA, not diagnosis, patient monitoring, clinical validation certification, or standalone conformity assessment.</p>"
               << "<h2>Identity</h2><table><tr><th>Scenario</th><td>" << html_text(render.document.scenario_id)
               << "</td></tr><tr><th>Document fingerprint</th><td>" << html_text(render.document_identity.document_fingerprint)
               << "</td></tr><tr><th>Render identity</th><td>" << html_text(render.render_identity)
               << "</td></tr><tr><th>Target</th><td>" << html_text(result.target_name)
               << "</td></tr><tr><th>Tolerance</th><td>" << result.tolerance_seconds << " s</td></tr></table>"
               << "<h2>Metrics</h2><table><tr><th>Bin</th><th>GT</th><th>Detections</th><th>TP</th><th>FP</th><th>FN</th><th>Sensitivity</th><th>PPV</th><th>F1</th><th>Mean abs error</th><th>RMS error</th></tr>";
        const ecg_compare_bin_metrics* metrics[] = {&result.total, &result.clean, &result.artifact};
        const char* names[] = {"total", "clean", "artifact"};
        for (unsigned int i = 0; i < 3; ++i)
        {
            output << "<tr><td>" << names[i] << "</td><td>" << metrics[i]->ground_truth_count
                   << "</td><td>" << metrics[i]->detection_count
                   << "</td><td>" << metrics[i]->true_positive_count
                   << "</td><td>" << metrics[i]->false_positive_count
                   << "</td><td>" << metrics[i]->false_negative_count
                   << "</td><td>" << metrics[i]->sensitivity
                   << "</td><td>" << metrics[i]->positive_predictive_value
                   << "</td><td>" << metrics[i]->f1_score
                   << "</td><td>" << metrics[i]->mean_absolute_error_seconds
                   << " s</td><td>" << metrics[i]->rms_error_seconds << " s</td></tr>";
        }
        output << "</table><h2>Unmatched Events</h2><table><tr><th>Type</th><th>Index</th><th>Time</th><th>Bin</th></tr>";
        for (std::size_t i = 0; i < result.false_positives.size(); ++i)
            output << "<tr><td>false positive</td><td>" << result.false_positives[i].index << "</td><td>" << result.false_positives[i].time_seconds << "</td><td>" << (result.false_positives[i].in_artifact_interval ? "artifact" : "clean") << "</td></tr>";
        for (std::size_t i = 0; i < result.false_negatives.size(); ++i)
            output << "<tr><td>false negative</td><td>" << result.false_negatives[i].index << "</td><td>" << result.false_negatives[i].time_seconds << "</td><td>" << (result.false_negatives[i].in_artifact_interval ? "artifact" : "clean") << "</td></tr>";
        if (result.false_positives.empty() && result.false_negatives.empty())
            output << "<tr><td colspan=\"4\">No unmatched events.</td></tr>";
        output << "</table><h2>Artifacts</h2><p>comparison.json, comparison.csv, comparison_report.html</p></body></html>";
        return output.str();
    }
}
