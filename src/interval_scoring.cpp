#include "interval_scoring.h"

#include "ecg_render.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <set>
#include <sstream>

namespace
{
    struct segment
    {
        double start;
        double end;
    };

    struct match_candidate
    {
        std::size_t ground_truth_index;
        std::size_t prediction_index;
        double intersection_over_union;
        double overlap_seconds;
        double boundary_error_seconds;
    };

    struct match_selection
    {
        std::vector<match_candidate> matches;
        std::vector<bool> ground_truth_matched;
        std::vector<bool> prediction_matched;
    };

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
            default: output += value[i]; break;
            }
        }
        return output;
    }

    std::string csv_cell(const std::string& value)
    {
        if (value.find_first_of(",\"\r\n") == std::string::npos)
            return value;
        std::string output = "\"";
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            if (value[i] == '"') output += '"';
            output += value[i];
        }
        return output + '"';
    }

    const char* episode_kind_name(signal_synth::clinical_episode_kind kind)
    {
        switch (kind)
        {
        case signal_synth::clinical_episode_psvt: return "psvt";
        case signal_synth::clinical_episode_svarr: return "svarr";
        case signal_synth::clinical_episode_repolarization: return "dynamic_repolarization";
        case signal_synth::clinical_episode_none: return "none";
        }
        return "unknown";
    }

    bool segment_less(const segment& left, const segment& right)
    {
        return left.start != right.start ? left.start < right.start : left.end < right.end;
    }

    std::vector<segment> merged_segments(const std::vector<signal_synth::interval_output_event>& intervals, const std::string& label, const std::string& channel)
    {
        std::vector<segment> segments;
        for (std::size_t i = 0; i < intervals.size(); ++i)
        {
            if (intervals[i].label == label && intervals[i].channel == channel)
            {
                segment item = {intervals[i].start_seconds, intervals[i].end_seconds};
                segments.push_back(item);
            }
        }
        std::sort(segments.begin(), segments.end(), segment_less);
        std::vector<segment> merged;
        for (std::size_t i = 0; i < segments.size(); ++i)
        {
            if (merged.empty() || segments[i].start > merged.back().end)
                merged.push_back(segments[i]);
            else if (segments[i].end > merged.back().end)
                merged.back().end = segments[i].end;
        }
        return merged;
    }

    double duration(const std::vector<segment>& segments)
    {
        double output = 0.0;
        for (std::size_t i = 0; i < segments.size(); ++i)
            output += segments[i].end - segments[i].start;
        return output;
    }

    double overlap_duration(const std::vector<segment>& left, const std::vector<segment>& right)
    {
        std::size_t i = 0, j = 0;
        double output = 0.0;
        while (i < left.size() && j < right.size())
        {
            const double start = std::max(left[i].start, right[j].start);
            const double end = std::min(left[i].end, right[j].end);
            if (end > start)
                output += end - start;
            if (left[i].end < right[j].end)
                ++i;
            else
                ++j;
        }
        return output;
    }

    double interval_overlap(const signal_synth::interval_output_event& left, const signal_synth::interval_output_event& right)
    {
        return std::max(0.0, std::min(left.end_seconds, right.end_seconds) - std::max(left.start_seconds, right.start_seconds));
    }

    bool candidate_less(const match_candidate& left, const match_candidate& right)
    {
        if (left.intersection_over_union != right.intersection_over_union) return left.intersection_over_union > right.intersection_over_union;
        if (left.boundary_error_seconds != right.boundary_error_seconds) return left.boundary_error_seconds < right.boundary_error_seconds;
        if (left.ground_truth_index != right.ground_truth_index) return left.ground_truth_index < right.ground_truth_index;
        return left.prediction_index < right.prediction_index;
    }

    match_selection select_matches(const std::vector<signal_synth::interval_output_event>& ground_truth, const std::vector<signal_synth::interval_output_event>& predictions, double minimum_iou, bool require_same_label)
    {
        std::vector<match_candidate> candidates;
        for (std::size_t truth_index = 0; truth_index < ground_truth.size(); ++truth_index)
        {
            for (std::size_t prediction_index = 0; prediction_index < predictions.size(); ++prediction_index)
            {
                const signal_synth::interval_output_event& truth = ground_truth[truth_index];
                const signal_synth::interval_output_event& prediction = predictions[prediction_index];
                if (truth.channel != prediction.channel || (require_same_label && truth.label != prediction.label))
                    continue;
                const double overlap = interval_overlap(truth, prediction);
                if (overlap <= 0.0)
                    continue;
                const double union_seconds = truth.end_seconds - truth.start_seconds + prediction.end_seconds - prediction.start_seconds - overlap;
                const double iou = union_seconds > 0.0 ? overlap / union_seconds : 0.0;
                if (iou + 1e-15 < minimum_iou)
                    continue;
                match_candidate candidate;
                candidate.ground_truth_index = truth_index;
                candidate.prediction_index = prediction_index;
                candidate.intersection_over_union = iou;
                candidate.overlap_seconds = overlap;
                candidate.boundary_error_seconds = std::fabs(prediction.start_seconds - truth.start_seconds) + std::fabs(prediction.end_seconds - truth.end_seconds);
                candidates.push_back(candidate);
            }
        }
        std::sort(candidates.begin(), candidates.end(), candidate_less);
        match_selection output;
        output.ground_truth_matched.assign(ground_truth.size(), false);
        output.prediction_matched.assign(predictions.size(), false);
        for (std::size_t i = 0; i < candidates.size(); ++i)
        {
            const match_candidate& candidate = candidates[i];
            if (output.ground_truth_matched[candidate.ground_truth_index] || output.prediction_matched[candidate.prediction_index])
                continue;
            output.ground_truth_matched[candidate.ground_truth_index] = true;
            output.prediction_matched[candidate.prediction_index] = true;
            output.matches.push_back(candidate);
        }
        return output;
    }

    double mean(const std::vector<double>& values)
    {
        double sum = 0.0;
        for (std::size_t i = 0; i < values.size(); ++i) sum += values[i];
        return values.empty() ? 0.0 : sum / values.size();
    }

    double median(std::vector<double> values)
    {
        if (values.empty()) return 0.0;
        std::sort(values.begin(), values.end());
        const std::size_t middle = values.size() / 2;
        return values.size() % 2 ? values[middle] : 0.5 * (values[middle - 1] + values[middle]);
    }

    double maximum(const std::vector<double>& values)
    {
        return values.empty() ? 0.0 : *std::max_element(values.begin(), values.end());
    }

    void add_duration_metrics(const std::string& label, const std::vector<signal_synth::interval_output_event>& ground_truth, const std::vector<signal_synth::interval_output_event>& predictions, signal_synth::interval_score_metrics& metrics)
    {
        std::set<std::string> channels;
        for (std::size_t i = 0; i < ground_truth.size(); ++i) if (ground_truth[i].label == label) channels.insert(ground_truth[i].channel);
        for (std::size_t i = 0; i < predictions.size(); ++i) if (predictions[i].label == label) channels.insert(predictions[i].channel);
        for (std::set<std::string>::const_iterator channel = channels.begin(); channel != channels.end(); ++channel)
        {
            const std::vector<segment> truth_segments = merged_segments(ground_truth, label, *channel);
            const std::vector<segment> prediction_segments = merged_segments(predictions, label, *channel);
            metrics.ground_truth_duration_seconds += duration(truth_segments);
            metrics.prediction_duration_seconds += duration(prediction_segments);
            metrics.overlap_duration_seconds += overlap_duration(truth_segments, prediction_segments);
        }
    }

    void finalize_metrics(signal_synth::interval_score_metrics& metrics, double record_duration_seconds, const std::vector<double>& onset_errors, const std::vector<double>& offset_errors)
    {
        metrics.false_alarm_count = metrics.prediction_count - metrics.matched_count;
        metrics.missed_count = metrics.ground_truth_count - metrics.matched_count;
        metrics.time_sensitivity = metrics.ground_truth_duration_seconds > 0.0 ? metrics.overlap_duration_seconds / metrics.ground_truth_duration_seconds : 0.0;
        metrics.time_precision = metrics.prediction_duration_seconds > 0.0 ? metrics.overlap_duration_seconds / metrics.prediction_duration_seconds : 0.0;
        const double duration_sum = metrics.ground_truth_duration_seconds + metrics.prediction_duration_seconds;
        metrics.time_f1_score = duration_sum > 0.0 ? 2.0 * metrics.overlap_duration_seconds / duration_sum : 0.0;
        const double duration_union = duration_sum - metrics.overlap_duration_seconds;
        metrics.temporal_iou = duration_union > 0.0 ? metrics.overlap_duration_seconds / duration_union : 0.0;
        metrics.event_sensitivity = metrics.ground_truth_count ? static_cast<double>(metrics.matched_count) / metrics.ground_truth_count : 0.0;
        metrics.event_precision = metrics.prediction_count ? static_cast<double>(metrics.matched_count) / metrics.prediction_count : 0.0;
        metrics.false_alarms_per_hour = record_duration_seconds > 0.0 ? metrics.false_alarm_count * 3600.0 / record_duration_seconds : 0.0;
        std::vector<double> absolute_onset, absolute_offset;
        for (std::size_t i = 0; i < onset_errors.size(); ++i) absolute_onset.push_back(std::fabs(onset_errors[i]));
        for (std::size_t i = 0; i < offset_errors.size(); ++i) absolute_offset.push_back(std::fabs(offset_errors[i]));
        metrics.mean_onset_error_seconds = mean(onset_errors);
        metrics.mean_absolute_onset_error_seconds = mean(absolute_onset);
        metrics.median_absolute_onset_error_seconds = median(absolute_onset);
        metrics.max_absolute_onset_error_seconds = maximum(absolute_onset);
        metrics.mean_offset_error_seconds = mean(offset_errors);
        metrics.mean_absolute_offset_error_seconds = mean(absolute_offset);
        metrics.median_absolute_offset_error_seconds = median(absolute_offset);
        metrics.max_absolute_offset_error_seconds = maximum(absolute_offset);
    }

    bool valid_interval(const signal_synth::interval_output_event& interval, double duration_seconds)
    {
        return std::isfinite(interval.start_seconds) && std::isfinite(interval.end_seconds) && interval.start_seconds >= 0.0 && interval.end_seconds > interval.start_seconds && interval.end_seconds <= duration_seconds + 1e-9
            && !interval.label.empty() && interval.label.size() <= 64 && !interval.channel.empty() && interval.channel.size() <= 64
            && (!interval.has_confidence || (std::isfinite(interval.confidence) && interval.confidence >= 0.0 && interval.confidence <= 1.0));
    }

    std::string interval_identity(const signal_synth::interval_output_event& interval)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(17) << interval.start_seconds << '|' << interval.end_seconds << '|' << interval.label << '|' << interval.channel;
        return output.str();
    }

    bool validate_intervals(const std::vector<signal_synth::interval_output_event>& intervals, double duration_seconds, signal_synth::interval_target target, const char* role, std::vector<std::string>& messages)
    {
        std::set<std::string> identities;
        bool has_global = false, has_physical = false;
        for (std::size_t i = 0; i < intervals.size(); ++i)
        {
            if (!valid_interval(intervals[i], duration_seconds))
            {
                messages.push_back(std::string(role) + " interval lies outside the record or is malformed");
                return false;
            }
            if (target == signal_synth::interval_target_rhythm_episode && intervals[i].channel != "global")
            {
                messages.push_back(std::string(role) + " rhythm_episode interval must use channel global");
                return false;
            }
            has_global = has_global || intervals[i].channel == "global";
            has_physical = has_physical || intervals[i].channel != "global";
            if (!identities.insert(interval_identity(intervals[i])).second)
            {
                messages.push_back(std::string(role) + " contains a duplicate interval");
                return false;
            }
        }
        if (target == signal_synth::interval_target_signal_quality && has_global && has_physical)
        {
            messages.push_back(std::string(role) + " signal_quality intervals cannot mix global and physical channels");
            return false;
        }
        return true;
    }

    void write_nullable(std::ostringstream& output, double value, bool defined)
    {
        if (defined) output << value;
        else output << "null";
    }

    std::string csv_number(double value, bool defined)
    {
        if (!defined) return "NA";
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
        return output.str();
    }

    void write_metrics_json(std::ostringstream& output, const signal_synth::interval_score_metrics& metrics)
    {
        const bool has_truth_time = metrics.ground_truth_duration_seconds > 0.0;
        const bool has_prediction_time = metrics.prediction_duration_seconds > 0.0;
        const bool has_any_time = metrics.ground_truth_duration_seconds + metrics.prediction_duration_seconds > 0.0;
        output << "{\"ground_truth_count\":" << metrics.ground_truth_count
               << ",\"prediction_count\":" << metrics.prediction_count
               << ",\"matched_count\":" << metrics.matched_count
               << ",\"false_alarm_count\":" << metrics.false_alarm_count
               << ",\"missed_count\":" << metrics.missed_count
               << ",\"ground_truth_duration_seconds\":" << metrics.ground_truth_duration_seconds
               << ",\"prediction_duration_seconds\":" << metrics.prediction_duration_seconds
               << ",\"overlap_duration_seconds\":" << metrics.overlap_duration_seconds
               << ",\"time_sensitivity\":";
        write_nullable(output, metrics.time_sensitivity, has_truth_time);
        output << ",\"time_precision\":";
        write_nullable(output, metrics.time_precision, has_prediction_time);
        output << ",\"time_f1_score\":";
        write_nullable(output, metrics.time_f1_score, has_any_time);
        output << ",\"temporal_iou\":";
        write_nullable(output, metrics.temporal_iou, has_any_time);
        output << ",\"event_sensitivity\":";
        write_nullable(output, metrics.event_sensitivity, metrics.ground_truth_count > 0);
        output << ",\"event_precision\":";
        write_nullable(output, metrics.event_precision, metrics.prediction_count > 0);
        output << ",\"false_alarms_per_hour\":" << metrics.false_alarms_per_hour
               << ",\"onset_error_seconds\":{\"mean\":";
        write_nullable(output, metrics.mean_onset_error_seconds, metrics.matched_count > 0);
        output << ",\"mean_absolute\":";
        write_nullable(output, metrics.mean_absolute_onset_error_seconds, metrics.matched_count > 0);
        output << ",\"median_absolute\":";
        write_nullable(output, metrics.median_absolute_onset_error_seconds, metrics.matched_count > 0);
        output << ",\"max_absolute\":";
        write_nullable(output, metrics.max_absolute_onset_error_seconds, metrics.matched_count > 0);
        output << "},\"offset_error_seconds\":{\"mean\":";
        write_nullable(output, metrics.mean_offset_error_seconds, metrics.matched_count > 0);
        output << ",\"mean_absolute\":";
        write_nullable(output, metrics.mean_absolute_offset_error_seconds, metrics.matched_count > 0);
        output << ",\"median_absolute\":";
        write_nullable(output, metrics.median_absolute_offset_error_seconds, metrics.matched_count > 0);
        output << ",\"max_absolute\":";
        write_nullable(output, metrics.max_absolute_offset_error_seconds, metrics.matched_count > 0);
        output << "}}";
    }

    void write_metric_csv_row(std::ostringstream& output, const std::string& label, const signal_synth::interval_score_metrics& metrics)
    {
        const bool has_truth_time = metrics.ground_truth_duration_seconds > 0.0;
        const bool has_prediction_time = metrics.prediction_duration_seconds > 0.0;
        const bool has_any_time = metrics.ground_truth_duration_seconds + metrics.prediction_duration_seconds > 0.0;
        output << "metrics," << csv_cell(label) << ",," << metrics.ground_truth_count << ',' << metrics.prediction_count << ',' << metrics.matched_count << ',' << metrics.false_alarm_count << ',' << metrics.missed_count
               << ',' << metrics.ground_truth_duration_seconds << ',' << metrics.prediction_duration_seconds << ',' << metrics.overlap_duration_seconds
               << ',' << csv_number(metrics.time_sensitivity, has_truth_time) << ',' << csv_number(metrics.time_precision, has_prediction_time)
               << ',' << csv_number(metrics.time_f1_score, has_any_time) << ',' << csv_number(metrics.temporal_iou, has_any_time)
               << ',' << csv_number(metrics.event_sensitivity, metrics.ground_truth_count > 0) << ',' << csv_number(metrics.event_precision, metrics.prediction_count > 0)
               << ',' << metrics.false_alarms_per_hour << ',' << csv_number(metrics.mean_absolute_onset_error_seconds, metrics.matched_count > 0)
               << ',' << csv_number(metrics.mean_absolute_offset_error_seconds, metrics.matched_count > 0) << '\n';
    }
}

namespace signal_synth
{
    interval_score_options::interval_score_options() : minimum_iou(0.1) {}
    interval_score_metrics::interval_score_metrics()
        : ground_truth_count(0), prediction_count(0), matched_count(0), false_alarm_count(0), missed_count(0), ground_truth_duration_seconds(0.0), prediction_duration_seconds(0.0), overlap_duration_seconds(0.0), time_sensitivity(0.0), time_precision(0.0), time_f1_score(0.0), temporal_iou(0.0), event_sensitivity(0.0), event_precision(0.0), false_alarms_per_hour(0.0), mean_onset_error_seconds(0.0), mean_absolute_onset_error_seconds(0.0), median_absolute_onset_error_seconds(0.0), max_absolute_onset_error_seconds(0.0), mean_offset_error_seconds(0.0), mean_absolute_offset_error_seconds(0.0), median_absolute_offset_error_seconds(0.0), max_absolute_offset_error_seconds(0.0) {}
    interval_score_result::interval_score_result() : success(false), target_name(), channel_mode(interval_channel_global), record_duration_seconds(0.0), minimum_iou(0.0), total(), classes(), matches(), false_positive_indices(), false_negative_indices(), confusion_matrix(), messages() {}

    const char* interval_channel_mode_name(interval_channel_mode mode)
    {
        return mode == interval_channel_per_channel ? "per_channel" : "global";
    }

    bool interval_ground_truth_from_render(const ecg_render_bundle& render, interval_target target, interval_channel_mode mode, std::vector<interval_output_event>& output, std::vector<std::string>& messages)
    {
        output.clear();
        messages.clear();
        if (target == interval_target_rhythm_episode)
        {
            if (mode != interval_channel_global)
            {
                messages.push_back("rhythm_episode ground truth is global only");
                return false;
            }
            const clinical_episode_annotation* episodes = render.record.episodes();
            for (unsigned int i = 0; i < render.record.episode_count(); ++i)
            {
                if (!episodes[i].present || (episodes[i].kind != clinical_episode_psvt && episodes[i].kind != clinical_episode_svarr))
                    continue;
                interval_output_event interval;
                interval.start_seconds = episodes[i].start_time_seconds;
                interval.end_seconds = episodes[i].end_time_seconds;
                interval.label = episode_kind_name(episodes[i].kind);
                interval.channel = "global";
                interval.original_index = static_cast<unsigned int>(output.size());
                output.push_back(interval);
            }
            return true;
        }
        for (std::size_t i = 0; i < render.signal_quality.artifacts.size(); ++i)
        {
            const signal_quality_artifact_interval& artifact = render.signal_quality.artifacts[i];
            std::vector<std::string> channels;
            if (mode == interval_channel_global)
                channels.push_back("global");
            else
            {
                for (unsigned int lead = 0; lead < render.record.lead_count(); ++lead)
                    if (artifact.ecg_leads[lead]) channels.push_back(render.record.lead_name(lead));
                if (artifact.ppg) channels.push_back("ppg_green");
                if (artifact.accelerometer_reference) channels.push_back("accel_motion");
            }
            for (std::size_t channel = 0; channel < channels.size(); ++channel)
            {
                interval_output_event interval;
                interval.start_seconds = artifact.start_seconds;
                interval.end_seconds = artifact.end_seconds;
                interval.label = signal_quality_artifact_type_name(artifact.type);
                interval.channel = channels[channel];
                interval.original_index = static_cast<unsigned int>(output.size());
                output.push_back(interval);
            }
        }
        return true;
    }

    bool score_intervals(const std::string& target_name, double record_duration_seconds, const std::vector<interval_output_event>& ground_truth, const std::vector<interval_output_event>& predictions, const interval_score_options& options, interval_score_result& result)
    {
        interval_score_result fresh;
        fresh.target_name = target_name;
        fresh.record_duration_seconds = record_duration_seconds;
        fresh.minimum_iou = options.minimum_iou;
        if (!std::isfinite(record_duration_seconds) || record_duration_seconds <= 0.0)
            fresh.messages.push_back("record duration must be finite and positive");
        if (!std::isfinite(options.minimum_iou) || options.minimum_iou <= 0.0 || options.minimum_iou > 1.0)
            fresh.messages.push_back("minimum IoU must be in the interval (0,1]");
        interval_target target;
        if (!interval_target_from_name(target_name, target))
            fresh.messages.push_back("unknown interval target");
        if (fresh.messages.empty())
        {
            validate_intervals(ground_truth, record_duration_seconds, target, "ground truth", fresh.messages);
            validate_intervals(predictions, record_duration_seconds, target, "prediction", fresh.messages);
        }
        if (!fresh.messages.empty())
        {
            result = fresh;
            return false;
        }

        std::set<std::string> labels;
        for (std::size_t i = 0; i < ground_truth.size(); ++i) labels.insert(ground_truth[i].label);
        for (std::size_t i = 0; i < predictions.size(); ++i) labels.insert(predictions[i].label);
        const match_selection same_label_matches = select_matches(ground_truth, predictions, options.minimum_iou, true);

        fresh.total.ground_truth_count = static_cast<unsigned int>(ground_truth.size());
        fresh.total.prediction_count = static_cast<unsigned int>(predictions.size());
        fresh.total.matched_count = static_cast<unsigned int>(same_label_matches.matches.size());
        std::vector<double> total_onset_errors, total_offset_errors;
        for (std::size_t i = 0; i < same_label_matches.matches.size(); ++i)
        {
            const match_candidate& selected = same_label_matches.matches[i];
            const interval_output_event& truth = ground_truth[selected.ground_truth_index];
            const interval_output_event& prediction = predictions[selected.prediction_index];
            interval_score_match match;
            match.ground_truth_index = truth.original_index;
            match.prediction_index = prediction.original_index;
            match.ground_truth_label = truth.label;
            match.prediction_label = prediction.label;
            match.channel = truth.channel;
            match.intersection_over_union = selected.intersection_over_union;
            match.overlap_seconds = selected.overlap_seconds;
            match.onset_error_seconds = prediction.start_seconds - truth.start_seconds;
            match.offset_error_seconds = prediction.end_seconds - truth.end_seconds;
            fresh.matches.push_back(match);
            total_onset_errors.push_back(match.onset_error_seconds);
            total_offset_errors.push_back(match.offset_error_seconds);
        }
        for (std::size_t i = 0; i < predictions.size(); ++i)
            if (!same_label_matches.prediction_matched[i]) fresh.false_positive_indices.push_back(predictions[i].original_index);
        for (std::size_t i = 0; i < ground_truth.size(); ++i)
            if (!same_label_matches.ground_truth_matched[i]) fresh.false_negative_indices.push_back(ground_truth[i].original_index);

        for (std::set<std::string>::const_iterator label = labels.begin(); label != labels.end(); ++label)
        {
            interval_score_class item;
            item.label = *label;
            std::vector<double> onset_errors, offset_errors;
            for (std::size_t i = 0; i < ground_truth.size(); ++i) if (ground_truth[i].label == *label) ++item.metrics.ground_truth_count;
            for (std::size_t i = 0; i < predictions.size(); ++i) if (predictions[i].label == *label) ++item.metrics.prediction_count;
            for (std::size_t i = 0; i < fresh.matches.size(); ++i)
            {
                if (fresh.matches[i].ground_truth_label != *label) continue;
                ++item.metrics.matched_count;
                onset_errors.push_back(fresh.matches[i].onset_error_seconds);
                offset_errors.push_back(fresh.matches[i].offset_error_seconds);
            }
            add_duration_metrics(*label, ground_truth, predictions, item.metrics);
            fresh.total.ground_truth_duration_seconds += item.metrics.ground_truth_duration_seconds;
            fresh.total.prediction_duration_seconds += item.metrics.prediction_duration_seconds;
            fresh.total.overlap_duration_seconds += item.metrics.overlap_duration_seconds;
            finalize_metrics(item.metrics, record_duration_seconds, onset_errors, offset_errors);
            fresh.classes.push_back(item);
        }
        finalize_metrics(fresh.total, record_duration_seconds, total_onset_errors, total_offset_errors);

        const match_selection confusion_matches = select_matches(ground_truth, predictions, options.minimum_iou, false);
        std::map<std::pair<std::string, std::string>, unsigned int> confusion;
        for (std::size_t i = 0; i < confusion_matches.matches.size(); ++i)
        {
            const match_candidate& selected = confusion_matches.matches[i];
            ++confusion[std::make_pair(ground_truth[selected.ground_truth_index].label, predictions[selected.prediction_index].label)];
        }
        for (std::size_t i = 0; i < ground_truth.size(); ++i)
            if (!confusion_matches.ground_truth_matched[i]) ++confusion[std::make_pair(ground_truth[i].label, "__missed__")];
        for (std::size_t i = 0; i < predictions.size(); ++i)
            if (!confusion_matches.prediction_matched[i]) ++confusion[std::make_pair("__false_alarm__", predictions[i].label)];
        for (std::map<std::pair<std::string, std::string>, unsigned int>::const_iterator cell = confusion.begin(); cell != confusion.end(); ++cell)
        {
            interval_confusion_cell item;
            item.ground_truth_label = cell->first.first;
            item.prediction_label = cell->first.second;
            item.count = cell->second;
            fresh.confusion_matrix.push_back(item);
        }
        fresh.success = true;
        result = fresh;
        return true;
    }

    bool score_interval_output_to_render(const ecg_render_bundle& render, const interval_output_document& predictions, const interval_score_options& options, interval_score_result& result)
    {
        interval_score_result fresh;
        if (predictions.schema_version != 1 || predictions.algorithm.name.size() > 128 || predictions.algorithm.version.size() > 128)
        {
            fresh.messages.push_back("invalid interval output document metadata");
            result = fresh;
            return false;
        }
        interval_target target;
        if (!interval_target_from_name(predictions.target_name, target))
        {
            fresh.messages.push_back("unknown interval target");
            result = fresh;
            return false;
        }
        interval_channel_mode mode = interval_channel_global;
        if (target == interval_target_signal_quality)
        {
            bool has_global = false, has_physical = false;
            for (std::size_t i = 0; i < predictions.intervals.size(); ++i)
            {
                has_global = has_global || predictions.intervals[i].channel == "global";
                has_physical = has_physical || predictions.intervals[i].channel != "global";
            }
            if (has_global && has_physical)
            {
                fresh.messages.push_back("signal_quality predictions cannot mix global and physical channels");
                result = fresh;
                return false;
            }
            mode = has_physical ? interval_channel_per_channel : interval_channel_global;
        }
        std::vector<interval_output_event> truth;
        if (!interval_ground_truth_from_render(render, target, mode, truth, fresh.messages))
        {
            result = fresh;
            return false;
        }
        if (!score_intervals(predictions.target_name, render.document.duration_seconds, truth, predictions.intervals, options, fresh))
        {
            result = fresh;
            return false;
        }
        fresh.channel_mode = mode;
        result = fresh;
        return true;
    }

    std::string interval_score_result_json(const ecg_render_bundle& render, const interval_output_document& predictions, const interval_score_result& result)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10)
               << "{\"schema_version\":1,\"score_type\":\"interval_detection_qa\",\"target\":" << json_string(result.target_name)
               << ",\"scenario\":{\"scenario_id\":" << json_string(render.document.scenario_id) << ",\"duration_seconds\":" << result.record_duration_seconds
               << ",\"render_identity\":" << json_string(render.render_identity) << "},\"algorithm\":{\"name\":" << json_string(predictions.algorithm.name)
               << ",\"version\":" << json_string(predictions.algorithm.version) << "},\"options\":{\"minimum_iou\":" << result.minimum_iou
               << ",\"channel_mode\":" << json_string(interval_channel_mode_name(result.channel_mode)) << "},\"overall\":";
        write_metrics_json(output, result.total);
        output << ",\"classes\":[";
        for (std::size_t i = 0; i < result.classes.size(); ++i)
        {
            output << (i ? "," : "") << "{\"label\":" << json_string(result.classes[i].label) << ",\"metrics\":";
            write_metrics_json(output, result.classes[i].metrics);
            output << '}';
        }
        output << "],\"confusion_matrix\":[";
        for (std::size_t i = 0; i < result.confusion_matrix.size(); ++i)
            output << (i ? "," : "") << "{\"ground_truth_label\":" << json_string(result.confusion_matrix[i].ground_truth_label) << ",\"prediction_label\":" << json_string(result.confusion_matrix[i].prediction_label) << ",\"count\":" << result.confusion_matrix[i].count << '}';
        output << "],\"matches\":[";
        for (std::size_t i = 0; i < result.matches.size(); ++i)
        {
            const interval_score_match& match = result.matches[i];
            output << (i ? "," : "") << "{\"ground_truth_index\":" << match.ground_truth_index << ",\"prediction_index\":" << match.prediction_index
                   << ",\"label\":" << json_string(match.ground_truth_label) << ",\"channel\":" << json_string(match.channel)
                   << ",\"intersection_over_union\":" << match.intersection_over_union << ",\"overlap_seconds\":" << match.overlap_seconds
                   << ",\"onset_error_seconds\":" << match.onset_error_seconds << ",\"offset_error_seconds\":" << match.offset_error_seconds << '}';
        }
        output << "],\"false_positive_indices\":[";
        for (std::size_t i = 0; i < result.false_positive_indices.size(); ++i) output << (i ? "," : "") << result.false_positive_indices[i];
        output << "],\"false_negative_indices\":[";
        for (std::size_t i = 0; i < result.false_negative_indices.size(); ++i) output << (i ? "," : "") << result.false_negative_indices[i];
        output << "],\"notes\":[\"Intervals are half-open [start,end); undefined zero-denominator metrics are null.\",\"Synthetic engineering QA evidence; not a clinical validation claim.\"]}";
        return output.str();
    }

    std::string interval_score_result_csv(const interval_score_result& result)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10)
               << "row_type,label,prediction_label,ground_truth_count,prediction_count,matched_count,false_alarm_count,missed_count,ground_truth_duration_seconds,prediction_duration_seconds,overlap_duration_seconds,time_sensitivity,time_precision,time_f1_score,temporal_iou,event_sensitivity,event_precision,false_alarms_per_hour,mean_absolute_onset_error_seconds,mean_absolute_offset_error_seconds\n";
        write_metric_csv_row(output, "__overall__", result.total);
        for (std::size_t i = 0; i < result.classes.size(); ++i) write_metric_csv_row(output, result.classes[i].label, result.classes[i].metrics);
        for (std::size_t i = 0; i < result.confusion_matrix.size(); ++i)
            output << "confusion," << csv_cell(result.confusion_matrix[i].ground_truth_label) << ',' << csv_cell(result.confusion_matrix[i].prediction_label) << ',' << result.confusion_matrix[i].count << ",,,,,,,,,,,,,,,,\n";
        return output.str();
    }

    std::string interval_score_report_html(const ecg_render_bundle& render, const interval_score_result& result)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(6)
               << "<!doctype html><html><head><meta charset=\"utf-8\"><title>Interval scoring</title><style>body{font-family:Arial,sans-serif;margin:24px;color:#20252b}table{border-collapse:collapse;margin:16px 0}th,td{border:1px solid #c9ced4;padding:6px 9px;text-align:right}th:first-child,td:first-child{text-align:left}.note{color:#555;max-width:900px}</style></head><body>"
               << "<h1>Interval scoring</h1><p>Scenario: " << html_text(render.document.scenario_id) << " | Target: " << html_text(result.target_name)
               << " | Channel mode: " << interval_channel_mode_name(result.channel_mode) << " | Minimum IoU: " << result.minimum_iou << "</p>"
               << "<table><tr><th>Class</th><th>Truth</th><th>Predicted</th><th>Matched</th><th>Time sensitivity</th><th>Time precision</th><th>Time F1</th><th>IoU</th><th>False alarms/hour</th><th>Onset MAE (s)</th><th>Offset MAE (s)</th></tr>";
        for (std::size_t row = 0; row <= result.classes.size(); ++row)
        {
            const interval_score_metrics& metrics = row == 0 ? result.total : result.classes[row - 1].metrics;
            const std::string label = row == 0 ? "Overall" : result.classes[row - 1].label;
            output << "<tr><td>" << html_text(label) << "</td><td>" << metrics.ground_truth_count << "</td><td>" << metrics.prediction_count << "</td><td>" << metrics.matched_count << "</td><td>"
                   << (metrics.ground_truth_duration_seconds > 0.0 ? csv_number(metrics.time_sensitivity, true) : "NA") << "</td><td>"
                   << (metrics.prediction_duration_seconds > 0.0 ? csv_number(metrics.time_precision, true) : "NA") << "</td><td>"
                   << (metrics.ground_truth_duration_seconds + metrics.prediction_duration_seconds > 0.0 ? csv_number(metrics.time_f1_score, true) : "NA") << "</td><td>"
                   << (metrics.ground_truth_duration_seconds + metrics.prediction_duration_seconds > 0.0 ? csv_number(metrics.temporal_iou, true) : "NA") << "</td><td>" << metrics.false_alarms_per_hour << "</td><td>"
                   << (metrics.matched_count ? csv_number(metrics.mean_absolute_onset_error_seconds, true) : "NA") << "</td><td>"
                   << (metrics.matched_count ? csv_number(metrics.mean_absolute_offset_error_seconds, true) : "NA") << "</td></tr>";
        }
        output << "</table><h2>Confusion matrix</h2><table><tr><th>Ground truth</th><th>Prediction</th><th>Count</th></tr>";
        for (std::size_t i = 0; i < result.confusion_matrix.size(); ++i)
            output << "<tr><td>" << html_text(result.confusion_matrix[i].ground_truth_label) << "</td><td>" << html_text(result.confusion_matrix[i].prediction_label) << "</td><td>" << result.confusion_matrix[i].count << "</td></tr>";
        output << "</table><p class=\"note\">Intervals are half-open [start,end). Undefined zero-denominator metrics are reported as NA. This is synthetic engineering QA evidence, not a clinical validation claim.</p></body></html>";
        return output.str();
    }
}
