#include "ecg_beat_classification.h"

#include "ecg_export.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace
{
    struct match_candidate
    {
        unsigned int ground_truth_index;
        unsigned int prediction_index;
        double error_seconds;
    };

    bool candidate_order(const match_candidate& left, const match_candidate& right)
    {
        if (left.error_seconds != right.error_seconds)
            return left.error_seconds < right.error_seconds;
        if (left.ground_truth_index != right.ground_truth_index)
            return left.ground_truth_index < right.ground_truth_index;
        return left.prediction_index < right.prediction_index;
    }

    bool finite_non_negative(double value)
    {
        return std::isfinite(value) && value >= 0.0;
    }

    double ratio(unsigned int numerator, unsigned int denominator)
    {
        return denominator ? static_cast<double>(numerator) / denominator : 0.0;
    }

    std::string json_text(const std::string& value)
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

    std::string csv_text(const std::string& value)
    {
        if (value.find_first_of(",\"\r\n") == std::string::npos)
            return value;
        std::string output("\"");
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            if (value[i] == '"')
                output += "\"\"";
            else
                output.push_back(value[i]);
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
            default: output.push_back(value[i]);
            }
        }
        return output;
    }

    bool valid_class(signal_synth::ecg_beat_class value)
    {
        return value >= signal_synth::ecg_beat_normal && value < signal_synth::ecg_beat_class_count;
    }

    bool scored_class(signal_synth::ecg_beat_class value)
    {
        return valid_class(value) && value != signal_synth::ecg_beat_unscored;
    }
}

namespace signal_synth
{
    const char* ecg_beat_class_name(ecg_beat_class value)
    {
        switch (value)
        {
        case ecg_beat_normal: return "normal";
        case ecg_beat_supraventricular_ectopic: return "supraventricular_ectopic";
        case ecg_beat_ventricular_ectopic: return "ventricular_ectopic";
        case ecg_beat_paced: return "paced";
        case ecg_beat_escape: return "escape";
        case ecg_beat_unscored: return "unscored";
        default: return "unscored";
        }
    }

    bool ecg_beat_class_from_name(const std::string& name, ecg_beat_class& value)
    {
        for (int candidate = ecg_beat_normal; candidate < ecg_beat_class_count; ++candidate)
        {
            const ecg_beat_class beat_class = static_cast<ecg_beat_class>(candidate);
            if (name == ecg_beat_class_name(beat_class))
            {
                value = beat_class;
                return true;
            }
        }
        return false;
    }

    ecg_beat_class ecg_beat_class_from_origin(clinical_ventricular_origin origin)
    {
        switch (origin)
        {
        case clinical_origin_conducted: return ecg_beat_normal;
        case clinical_origin_pac: return ecg_beat_supraventricular_ectopic;
        case clinical_origin_pvc: return ecg_beat_ventricular_ectopic;
        case clinical_origin_paced:
        case clinical_origin_atrial_paced: return ecg_beat_paced;
        case clinical_origin_junctional_escape:
        case clinical_origin_ventricular_escape: return ecg_beat_escape;
        case clinical_origin_vt:
        default: return ecg_beat_unscored;
        }
    }

    ecg_classified_beat_event::ecg_classified_beat_event() : time_seconds(0.0), beat_class(ecg_beat_unscored), original_index(0) {}
    ecg_beat_classification_options::ecg_beat_classification_options() : tolerance_seconds(0.075) {}
    ecg_beat_class_metrics::ecg_beat_class_metrics() : scored(false), ground_truth_count(0), prediction_count(0), true_positive_count(0), false_positive_count(0), false_negative_count(0), precision(0.0), recall(0.0), f1_score(0.0) {}
    ecg_beat_classification_match::ecg_beat_classification_match() : ground_truth_index(0), prediction_index(0), ground_truth_time_seconds(0.0), prediction_time_seconds(0.0), error_seconds(0.0), actual_class(ecg_beat_unscored), predicted_class(ecg_beat_unscored), scored(false), correct(false) {}
    ecg_beat_classification_unmatched::ecg_beat_classification_unmatched() : index(0), time_seconds(0.0), beat_class(ecg_beat_unscored) {}

    ecg_beat_classification_result::ecg_beat_classification_result()
        : success(false), tolerance_seconds(0.0), algorithm_name(), algorithm_version(), scored_ground_truth_count(0), scored_prediction_count(0), matched_count(0), correct_count(0), unscored_match_count(0), accuracy(0.0), micro_precision(0.0), micro_recall(0.0), micro_f1_score(0.0), mean_absolute_error_seconds(0.0), max_absolute_error_seconds(0.0), matches(), unmatched_ground_truth(), unmatched_predictions(), messages()
    {
        for (int actual = 0; actual < ecg_beat_class_count; ++actual)
            for (int predicted = 0; predicted < ecg_beat_class_count; ++predicted)
                confusion[actual][predicted] = 0;
        for (int beat_class = 0; beat_class < ecg_beat_class_count; ++beat_class)
            classes[beat_class].scored = beat_class != ecg_beat_unscored;
    }

    bool beat_classification_events_from_detection(const detection_io_document& document, std::vector<ecg_classified_beat_event>& output, std::vector<std::string>& messages)
    {
        std::vector<ecg_classified_beat_event> fresh;
        std::vector<std::string> fresh_messages;
        ecg_compare_target target;
        if (!detection_compare_target_from_name(document.target_name, target) || target != ecg_compare_beat_classification)
            fresh_messages.push_back("detection target must be ecg_beat_classification");
        try
        {
            fresh.reserve(document.events.size());
            for (std::size_t i = 0; i < document.events.size(); ++i)
            {
                const detection_io_event& source = document.events[i];
                ecg_classified_beat_event event;
                if (!finite_non_negative(source.time_seconds))
                    fresh_messages.push_back("classification event time must be finite and non-negative");
                if (!ecg_beat_class_from_name(source.label, event.beat_class))
                    fresh_messages.push_back("classification event label is not a canonical ECG beat class");
                event.time_seconds = source.time_seconds;
                event.original_index = source.original_index;
                fresh.push_back(event);
            }
        }
        catch (...)
        {
            fresh_messages.push_back("unable to allocate classification events");
        }
        if (!fresh_messages.empty())
        {
            messages = fresh_messages;
            return false;
        }
        output = fresh;
        messages.clear();
        return true;
    }

    bool score_ecg_beat_classification(const ecg_render_bundle& render, const detection_io_document& prediction_document, const ecg_beat_classification_options& options, ecg_beat_classification_result& result)
    {
        ecg_beat_classification_result fresh;
        fresh.tolerance_seconds = options.tolerance_seconds;
        fresh.algorithm_name = prediction_document.algorithm.name;
        fresh.algorithm_version = prediction_document.algorithm.version;
        if (!std::isfinite(options.tolerance_seconds) || options.tolerance_seconds <= 0.0)
            fresh.messages.push_back("classification tolerance must be finite and positive");
        std::vector<ecg_classified_beat_event> predictions;
        std::vector<std::string> conversion_messages;
        if (!beat_classification_events_from_detection(prediction_document, predictions, conversion_messages))
            fresh.messages.insert(fresh.messages.end(), conversion_messages.begin(), conversion_messages.end());
        if (!render.record.beats() && render.record.beat_count())
            fresh.messages.push_back("render has no accessible beat ground truth");
        if (!fresh.messages.empty())
        {
            result = fresh;
            return false;
        }

        try
        {
            std::vector<ecg_classified_beat_event> ground_truth;
            ground_truth.reserve(render.record.beat_count());
            for (unsigned int i = 0; i < render.record.beat_count(); ++i)
            {
                const clinical_beat_annotation& beat = render.record.beats()[i];
                if (!beat.qrs_present)
                    continue;
                ecg_classified_beat_event event;
                event.time_seconds = beat.r_peak_time_seconds;
                event.beat_class = ecg_beat_class_from_origin(beat.origin);
                event.original_index = i;
                ground_truth.push_back(event);
                ++fresh.classes[event.beat_class].ground_truth_count;
                if (scored_class(event.beat_class))
                    ++fresh.scored_ground_truth_count;
            }

            std::vector<match_candidate> candidates;
            for (unsigned int truth = 0; truth < ground_truth.size(); ++truth)
                for (unsigned int prediction = 0; prediction < predictions.size(); ++prediction)
                {
                    const double error = std::fabs(predictions[prediction].time_seconds - ground_truth[truth].time_seconds);
                    if (error <= options.tolerance_seconds)
                        candidates.push_back(match_candidate{truth, prediction, error});
                }
            std::sort(candidates.begin(), candidates.end(), candidate_order);
            std::vector<bool> truth_used(ground_truth.size(), false);
            std::vector<bool> prediction_used(predictions.size(), false);
            double timing_error_sum = 0.0;
            for (std::size_t i = 0; i < candidates.size(); ++i)
            {
                const match_candidate& candidate = candidates[i];
                if (truth_used[candidate.ground_truth_index] || prediction_used[candidate.prediction_index])
                    continue;
                truth_used[candidate.ground_truth_index] = true;
                prediction_used[candidate.prediction_index] = true;
                const ecg_classified_beat_event& truth = ground_truth[candidate.ground_truth_index];
                const ecg_classified_beat_event& prediction = predictions[candidate.prediction_index];
                ecg_beat_classification_match match;
                match.ground_truth_index = truth.original_index;
                match.prediction_index = prediction.original_index;
                match.ground_truth_time_seconds = truth.time_seconds;
                match.prediction_time_seconds = prediction.time_seconds;
                match.error_seconds = prediction.time_seconds - truth.time_seconds;
                match.actual_class = truth.beat_class;
                match.predicted_class = prediction.beat_class;
                match.scored = scored_class(truth.beat_class);
                match.correct = match.scored && truth.beat_class == prediction.beat_class;
                ++fresh.confusion[truth.beat_class][prediction.beat_class];
                ++fresh.classes[prediction.beat_class].prediction_count;
                ++fresh.matched_count;
                if (!match.scored)
                    ++fresh.unscored_match_count;
                else
                {
                    timing_error_sum += candidate.error_seconds;
                    fresh.max_absolute_error_seconds = std::max(fresh.max_absolute_error_seconds, candidate.error_seconds);
                    if (scored_class(prediction.beat_class))
                        ++fresh.scored_prediction_count;
                    if (match.correct)
                    {
                        ++fresh.correct_count;
                        ++fresh.classes[truth.beat_class].true_positive_count;
                    }
                    else
                    {
                        ++fresh.classes[truth.beat_class].false_negative_count;
                        if (scored_class(prediction.beat_class))
                            ++fresh.classes[prediction.beat_class].false_positive_count;
                    }
                }
                fresh.matches.push_back(match);
            }

            for (unsigned int truth = 0; truth < ground_truth.size(); ++truth)
            {
                if (truth_used[truth])
                    continue;
                ecg_beat_classification_unmatched unmatched;
                unmatched.index = ground_truth[truth].original_index;
                unmatched.time_seconds = ground_truth[truth].time_seconds;
                unmatched.beat_class = ground_truth[truth].beat_class;
                fresh.unmatched_ground_truth.push_back(unmatched);
                if (scored_class(unmatched.beat_class))
                    ++fresh.classes[unmatched.beat_class].false_negative_count;
            }
            for (unsigned int prediction = 0; prediction < predictions.size(); ++prediction)
            {
                if (prediction_used[prediction])
                    continue;
                ecg_beat_classification_unmatched unmatched;
                unmatched.index = predictions[prediction].original_index;
                unmatched.time_seconds = predictions[prediction].time_seconds;
                unmatched.beat_class = predictions[prediction].beat_class;
                fresh.unmatched_predictions.push_back(unmatched);
                ++fresh.classes[unmatched.beat_class].prediction_count;
                if (scored_class(unmatched.beat_class))
                {
                    ++fresh.scored_prediction_count;
                    ++fresh.classes[unmatched.beat_class].false_positive_count;
                }
            }

            const unsigned int scored_matches = fresh.matched_count - fresh.unscored_match_count;
            fresh.mean_absolute_error_seconds = scored_matches ? timing_error_sum / scored_matches : 0.0;
            for (int beat_class = 0; beat_class < ecg_beat_class_count; ++beat_class)
            {
                ecg_beat_class_metrics& metrics = fresh.classes[beat_class];
                if (!metrics.scored)
                    continue;
                metrics.precision = ratio(metrics.true_positive_count, metrics.true_positive_count + metrics.false_positive_count);
                metrics.recall = ratio(metrics.true_positive_count, metrics.true_positive_count + metrics.false_negative_count);
                metrics.f1_score = metrics.precision + metrics.recall > 0.0 ? 2.0 * metrics.precision * metrics.recall / (metrics.precision + metrics.recall) : 0.0;
            }
            fresh.accuracy = ratio(fresh.correct_count, fresh.scored_ground_truth_count);
            fresh.micro_precision = ratio(fresh.correct_count, fresh.scored_prediction_count);
            fresh.micro_recall = ratio(fresh.correct_count, fresh.scored_ground_truth_count);
            fresh.micro_f1_score = fresh.micro_precision + fresh.micro_recall > 0.0 ? 2.0 * fresh.micro_precision * fresh.micro_recall / (fresh.micro_precision + fresh.micro_recall) : 0.0;
            fresh.success = true;
        }
        catch (...)
        {
            fresh = ecg_beat_classification_result();
            fresh.messages.push_back("classification scoring failed");
        }
        result = fresh;
        return fresh.success;
    }

    std::string ecg_beat_classification_result_json(const ecg_render_bundle& render, const ecg_beat_classification_result& result)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10);
        output << "{\"schema_version\":1,\"score_type\":\"ecg_beat_classification_qa\",\"scenario_id\":" << json_text(render.document.scenario_id)
               << ",\"document_fingerprint\":" << json_text(render.document_identity.document_fingerprint)
               << ",\"render_identity\":" << json_text(render.render_identity)
               << ",\"algorithm\":{\"name\":" << json_text(result.algorithm_name) << ",\"version\":" << json_text(result.algorithm_version)
               << "},\"tolerance_seconds\":" << result.tolerance_seconds
               << ",\"summary\":{\"scored_ground_truth_count\":" << result.scored_ground_truth_count
               << ",\"scored_prediction_count\":" << result.scored_prediction_count
               << ",\"matched_count\":" << result.matched_count
               << ",\"correct_count\":" << result.correct_count
               << ",\"unscored_match_count\":" << result.unscored_match_count
               << ",\"accuracy\":" << result.accuracy
               << ",\"micro_precision\":" << result.micro_precision
               << ",\"micro_recall\":" << result.micro_recall
               << ",\"micro_f1_score\":" << result.micro_f1_score
               << ",\"mean_absolute_error_seconds\":" << result.mean_absolute_error_seconds
               << ",\"max_absolute_error_seconds\":" << result.max_absolute_error_seconds << "},\"classes\":[";
        for (int beat_class = 0; beat_class < ecg_beat_class_count; ++beat_class)
        {
            const ecg_beat_class_metrics& metrics = result.classes[beat_class];
            output << (beat_class ? "," : "") << "{\"class\":" << json_text(ecg_beat_class_name(static_cast<ecg_beat_class>(beat_class)))
                   << ",\"scored\":" << (metrics.scored ? "true" : "false")
                   << ",\"ground_truth_count\":" << metrics.ground_truth_count
                   << ",\"prediction_count\":" << metrics.prediction_count
                   << ",\"true_positive_count\":" << metrics.true_positive_count
                   << ",\"false_positive_count\":" << metrics.false_positive_count
                   << ",\"false_negative_count\":" << metrics.false_negative_count
                   << ",\"precision\":" << metrics.precision << ",\"recall\":" << metrics.recall << ",\"f1_score\":" << metrics.f1_score << '}';
        }
        output << "],\"confusion_matrix\":{\"labels\":[";
        for (int beat_class = 0; beat_class < ecg_beat_class_count; ++beat_class)
            output << (beat_class ? "," : "") << json_text(ecg_beat_class_name(static_cast<ecg_beat_class>(beat_class)));
        output << "],\"rows\":[";
        for (int actual = 0; actual < ecg_beat_class_count; ++actual)
        {
            output << (actual ? "," : "") << '[';
            for (int predicted = 0; predicted < ecg_beat_class_count; ++predicted)
                output << (predicted ? "," : "") << result.confusion[actual][predicted];
            output << ']';
        }
        output << "]},\"matches\":[";
        for (std::size_t i = 0; i < result.matches.size(); ++i)
        {
            const ecg_beat_classification_match& match = result.matches[i];
            output << (i ? "," : "") << "{\"ground_truth_index\":" << match.ground_truth_index
                   << ",\"prediction_index\":" << match.prediction_index
                   << ",\"ground_truth_time_seconds\":" << match.ground_truth_time_seconds
                   << ",\"prediction_time_seconds\":" << match.prediction_time_seconds
                   << ",\"error_seconds\":" << match.error_seconds
                   << ",\"actual_class\":" << json_text(ecg_beat_class_name(match.actual_class))
                   << ",\"predicted_class\":" << json_text(ecg_beat_class_name(match.predicted_class))
                   << ",\"scored\":" << (match.scored ? "true" : "false")
                   << ",\"correct\":" << (match.correct ? "true" : "false") << '}';
        }
        output << "],\"unmatched_ground_truth\":[";
        for (std::size_t i = 0; i < result.unmatched_ground_truth.size(); ++i)
        {
            const ecg_beat_classification_unmatched& unmatched = result.unmatched_ground_truth[i];
            output << (i ? "," : "") << "{\"ground_truth_index\":" << unmatched.index
                   << ",\"time_seconds\":" << unmatched.time_seconds
                   << ",\"actual_class\":" << json_text(ecg_beat_class_name(unmatched.beat_class)) << '}';
        }
        output << "],\"unmatched_predictions\":[";
        for (std::size_t i = 0; i < result.unmatched_predictions.size(); ++i)
        {
            const ecg_beat_classification_unmatched& unmatched = result.unmatched_predictions[i];
            output << (i ? "," : "") << "{\"prediction_index\":" << unmatched.index
                   << ",\"time_seconds\":" << unmatched.time_seconds
                   << ",\"predicted_class\":" << json_text(ecg_beat_class_name(unmatched.beat_class)) << '}';
        }
        output << "],\"intended_use\":\"synthetic engineering algorithm testing and QA\",\"not_for\":\"diagnosis or clinical classifier validation\"}";
        return output.str();
    }

    std::string ecg_beat_classification_result_csv(const ecg_beat_classification_result& result)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10);
        output << "row_type,class,scored,ground_truth_count,prediction_count,true_positive_count,false_positive_count,false_negative_count,precision,recall,f1_score\n";
        for (int beat_class = 0; beat_class < ecg_beat_class_count; ++beat_class)
        {
            const ecg_beat_class_metrics& metrics = result.classes[beat_class];
            output << "class," << csv_text(ecg_beat_class_name(static_cast<ecg_beat_class>(beat_class))) << ',' << (metrics.scored ? 1 : 0) << ','
                   << metrics.ground_truth_count << ',' << metrics.prediction_count << ',' << metrics.true_positive_count << ','
                   << metrics.false_positive_count << ',' << metrics.false_negative_count << ',' << metrics.precision << ',' << metrics.recall << ',' << metrics.f1_score << '\n';
        }
        output << "\nactual_class,predicted_class,count\n";
        for (int actual = 0; actual < ecg_beat_class_count; ++actual)
            for (int predicted = 0; predicted < ecg_beat_class_count; ++predicted)
                output << csv_text(ecg_beat_class_name(static_cast<ecg_beat_class>(actual))) << ',' << csv_text(ecg_beat_class_name(static_cast<ecg_beat_class>(predicted))) << ',' << result.confusion[actual][predicted] << '\n';
        output << "\nrow_type,ground_truth_index,prediction_index,ground_truth_time_seconds,prediction_time_seconds,error_seconds,actual_class,predicted_class,scored,correct\n";
        for (std::size_t i = 0; i < result.matches.size(); ++i)
        {
            const ecg_beat_classification_match& match = result.matches[i];
            output << "match," << match.ground_truth_index << ',' << match.prediction_index << ',' << match.ground_truth_time_seconds << ',' << match.prediction_time_seconds << ',' << match.error_seconds << ','
                   << csv_text(ecg_beat_class_name(match.actual_class)) << ',' << csv_text(ecg_beat_class_name(match.predicted_class)) << ',' << (match.scored ? 1 : 0) << ',' << (match.correct ? 1 : 0) << '\n';
        }
        for (std::size_t i = 0; i < result.unmatched_ground_truth.size(); ++i)
        {
            const ecg_beat_classification_unmatched& unmatched = result.unmatched_ground_truth[i];
            output << "unmatched_ground_truth," << unmatched.index << ",," << unmatched.time_seconds << ",,," << csv_text(ecg_beat_class_name(unmatched.beat_class)) << ",," << (scored_class(unmatched.beat_class) ? 1 : 0) << ",0\n";
        }
        for (std::size_t i = 0; i < result.unmatched_predictions.size(); ++i)
        {
            const ecg_beat_classification_unmatched& unmatched = result.unmatched_predictions[i];
            output << "unmatched_prediction,," << unmatched.index << ",," << unmatched.time_seconds << ",,," << csv_text(ecg_beat_class_name(unmatched.beat_class)) << ',' << (scored_class(unmatched.beat_class) ? 1 : 0) << ",0\n";
        }
        return output.str();
    }

    std::string ecg_beat_classification_report_html(const ecg_render_bundle& render, const ecg_beat_classification_result& result)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(6);
        output << "<!doctype html><html><head><meta charset=\"utf-8\"><title>ECG Beat Classification QA Report</title>"
               << "<style>body{font-family:Arial,sans-serif;margin:24px;line-height:1.45}table{border-collapse:collapse;margin:12px 0;width:100%}th,td{border:1px solid #d1d5db;padding:6px 8px;text-align:right}th:first-child,td:first-child{text-align:left}th{background:#f3f4f6}</style></head><body>"
               << "<h1>ECG Beat Classification QA Report</h1><p>Scenario <strong>" << html_text(render.document.scenario_id)
               << "</strong>; algorithm <strong>" << html_text(result.algorithm_name) << "</strong> " << html_text(result.algorithm_version)
               << ".</p><table><tr><th>Accuracy</th><td>" << result.accuracy
               << "</td></tr><tr><th>Micro F1</th><td>" << result.micro_f1_score
               << "</td></tr><tr><th>Correct / scored ground truth</th><td>" << result.correct_count << " / " << result.scored_ground_truth_count
               << "</td></tr><tr><th>Timing tolerance</th><td>" << result.tolerance_seconds << " s</td></tr></table>"
               << "<h2>Per-class metrics</h2><table><tr><th>Class</th><th>GT</th><th>Pred</th><th>Precision</th><th>Recall</th><th>F1</th></tr>";
        for (int beat_class = 0; beat_class < ecg_beat_class_count; ++beat_class)
        {
            const ecg_beat_class_metrics& metrics = result.classes[beat_class];
            output << "<tr><td>" << html_text(ecg_beat_class_name(static_cast<ecg_beat_class>(beat_class))) << (metrics.scored ? "" : " (unscored)")
                   << "</td><td>" << metrics.ground_truth_count << "</td><td>" << metrics.prediction_count
                   << "</td><td>" << metrics.precision << "</td><td>" << metrics.recall << "</td><td>" << metrics.f1_score << "</td></tr>";
        }
        output << "</table><h2>Confusion matrix</h2><table><tr><th>Actual / predicted</th>";
        for (int predicted = 0; predicted < ecg_beat_class_count; ++predicted)
            output << "<th>" << html_text(ecg_beat_class_name(static_cast<ecg_beat_class>(predicted))) << "</th>";
        output << "</tr>";
        for (int actual = 0; actual < ecg_beat_class_count; ++actual)
        {
            output << "<tr><th>" << html_text(ecg_beat_class_name(static_cast<ecg_beat_class>(actual))) << "</th>";
            for (int predicted = 0; predicted < ecg_beat_class_count; ++predicted)
                output << "<td>" << result.confusion[actual][predicted] << "</td>";
            output << "</tr>";
        }
        output << "</table><p>This report is for synthetic engineering algorithm QA, not diagnosis or clinical classifier validation.</p></body></html>";
        return output.str();
    }
}
