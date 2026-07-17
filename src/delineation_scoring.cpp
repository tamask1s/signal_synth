#include "delineation_scoring.h"

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

    std::string uint64_text(unsigned long long value)
    {
        std::ostringstream output;
        output << value;
        return output.str();
    }

    std::string identity(const signal_synth::delineation_event& event)
    {
        return uint64_text(event.beat_index) + "|" + event.lead + "|" + signal_synth::delineation_kind_name(event.kind);
    }

    int lead_rank(const std::string& lead)
    {
        const char* names[] = {"I","II","III","aVR","aVL","aVF","V1","V2","V3","V4","V5","V6"};
        for (int i = 0; i < 12; ++i)
            if (lead == names[i]) return i;
        return -1;
    }

    bool lead_less(const std::string& left, const std::string& right)
    {
        return lead_rank(left) < lead_rank(right);
    }

    bool event_less(const signal_synth::delineation_event& left, const signal_synth::delineation_event& right)
    {
        if (left.beat_index != right.beat_index) return left.beat_index < right.beat_index;
        if (left.lead != right.lead) return lead_less(left.lead, right.lead);
        if (left.kind != right.kind) return left.kind < right.kind;
        return left.original_index < right.original_index;
    }

    bool valid_event(const signal_synth::delineation_event& event, double duration_seconds)
    {
        signal_synth::delineation_kind kind;
        return lead_rank(event.lead) >= 0
            && signal_synth::delineation_kind_from_name(signal_synth::delineation_kind_name(event.kind), kind)
            && std::isfinite(event.time_seconds) && event.time_seconds >= 0.0 && event.time_seconds <= duration_seconds + 1e-9
            && (!event.has_confidence || (std::isfinite(event.confidence) && event.confidence >= 0.0 && event.confidence <= 1.0));
    }

    bool validate_events(const std::vector<signal_synth::delineation_event>& events, double duration_seconds, const char* role, std::vector<std::string>& messages)
    {
        std::set<std::string> identities;
        for (std::size_t i = 0; i < events.size(); ++i)
        {
            if (!valid_event(events[i], duration_seconds))
            {
                messages.push_back(std::string(role) + " event lies outside the record or is malformed");
                return false;
            }
            if (!identities.insert(identity(events[i])).second)
            {
                messages.push_back(std::string(role) + " contains a duplicate beat, lead, and kind identity");
                return false;
            }
        }
        return true;
    }

    const signal_synth::clinical_fiducial_annotation* find_fiducial(const signal_synth::clinical_ecg_record& record, unsigned long long beat_index, int lead_index, signal_synth::clinical_fiducial_kind kind, signal_synth::clinical_fiducial_source source, bool require_present)
    {
        const signal_synth::clinical_fiducial_annotation* fiducials = record.fiducials();
        for (unsigned int i = 0; i < record.fiducial_count(); ++i)
        {
            const signal_synth::clinical_fiducial_annotation& item = fiducials[i];
            if (item.beat_index == beat_index && item.lead_index == lead_index && item.kind == kind && item.source == source && (!require_present || item.present))
                return &item;
        }
        return 0;
    }

    int find_lead(const signal_synth::clinical_ecg_record& record, const std::string& name)
    {
        for (unsigned int lead = 0; lead < record.lead_count(); ++lead)
            if (name == record.lead_name(lead))
                return static_cast<int>(lead);
        return -1;
    }

    bool contains_beat(const signal_synth::clinical_ecg_record& record, unsigned long long beat_index)
    {
        const signal_synth::clinical_beat_annotation* beats = record.beats();
        for (unsigned int i = 0; i < record.beat_count(); ++i)
            if (beats[i].beat_index == beat_index)
                return true;
        return false;
    }

    void add_truth_event(std::vector<signal_synth::delineation_event>& output, unsigned long long beat_index, const std::string& lead, signal_synth::delineation_kind kind, const signal_synth::clinical_fiducial_annotation* fiducial)
    {
        if (!fiducial || !fiducial->present)
            return;
        signal_synth::delineation_event event;
        event.beat_index = beat_index;
        event.lead = lead;
        event.kind = kind;
        event.time_seconds = fiducial->time_seconds;
        event.original_index = static_cast<unsigned int>(output.size());
        output.push_back(event);
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
        const std::size_t middle = values.size() / 2u;
        return values.size() % 2u ? values[middle] : 0.5 * (values[middle - 1u] + values[middle]);
    }

    double percentile95(std::vector<double> values)
    {
        if (values.empty()) return 0.0;
        std::sort(values.begin(), values.end());
        const std::size_t rank = static_cast<std::size_t>(std::ceil(0.95 * static_cast<double>(values.size())));
        return values[rank > 0u ? rank - 1u : 0u];
    }

    void finalize_metrics(signal_synth::delineation_score_metrics& metrics, const std::vector<double>& errors)
    {
        metrics.false_negative_count = metrics.missing_prediction_count + metrics.out_of_tolerance_count;
        metrics.false_positive_count = metrics.unexpected_prediction_count + metrics.out_of_tolerance_count;
        metrics.sensitivity = metrics.ground_truth_count ? static_cast<double>(metrics.within_tolerance_count) / metrics.ground_truth_count : 0.0;
        metrics.positive_predictive_value = metrics.prediction_count ? static_cast<double>(metrics.within_tolerance_count) / metrics.prediction_count : 0.0;
        const unsigned int denominator = metrics.ground_truth_count + metrics.prediction_count;
        metrics.f1_score = denominator ? 2.0 * metrics.within_tolerance_count / denominator : 0.0;
        metrics.within_tolerance_fraction = metrics.paired_count ? static_cast<double>(metrics.within_tolerance_count) / metrics.paired_count : 0.0;
        std::vector<double> absolute;
        double square_sum = 0.0;
        for (std::size_t i = 0; i < errors.size(); ++i)
        {
            absolute.push_back(std::fabs(errors[i]));
            square_sum += errors[i] * errors[i];
        }
        metrics.mean_error_seconds = mean(errors);
        metrics.mean_absolute_error_seconds = mean(absolute);
        metrics.median_absolute_error_seconds = median(absolute);
        metrics.rms_error_seconds = errors.empty() ? 0.0 : std::sqrt(square_sum / errors.size());
        metrics.p95_absolute_error_seconds = percentile95(absolute);
        metrics.max_absolute_error_seconds = absolute.empty() ? 0.0 : *std::max_element(absolute.begin(), absolute.end());
    }

    bool matches_group(const signal_synth::delineation_event& event, const std::string& kind, const std::string& lead)
    {
        return (kind.empty() || kind == signal_synth::delineation_kind_name(event.kind)) && (lead.empty() || lead == event.lead);
    }

    signal_synth::delineation_score_metrics group_metrics(const std::vector<signal_synth::delineation_event>& ground_truth, const std::vector<signal_synth::delineation_event>& predictions, const std::vector<signal_synth::delineation_score_match>& matches, const std::vector<signal_synth::delineation_event>& missing, const std::vector<signal_synth::delineation_event>& unexpected, const std::string& kind, const std::string& lead)
    {
        signal_synth::delineation_score_metrics metrics;
        std::vector<double> errors;
        for (std::size_t i = 0; i < ground_truth.size(); ++i) if (matches_group(ground_truth[i], kind, lead)) ++metrics.ground_truth_count;
        for (std::size_t i = 0; i < predictions.size(); ++i) if (matches_group(predictions[i], kind, lead)) ++metrics.prediction_count;
        for (std::size_t i = 0; i < matches.size(); ++i)
        {
            signal_synth::delineation_event event;
            event.kind = matches[i].kind;
            event.lead = matches[i].lead;
            if (!matches_group(event, kind, lead)) continue;
            ++metrics.paired_count;
            if (matches[i].within_tolerance) ++metrics.within_tolerance_count;
            else ++metrics.out_of_tolerance_count;
            errors.push_back(matches[i].error_seconds);
        }
        for (std::size_t i = 0; i < missing.size(); ++i) if (matches_group(missing[i], kind, lead)) ++metrics.missing_prediction_count;
        for (std::size_t i = 0; i < unexpected.size(); ++i) if (matches_group(unexpected[i], kind, lead)) ++metrics.unexpected_prediction_count;
        finalize_metrics(metrics, errors);
        return metrics;
    }

    bool score_core(double record_duration_seconds, const std::vector<signal_synth::delineation_event>& ground_truth, const std::vector<signal_synth::delineation_event>& predictions, const std::vector<std::string>& scoped_leads, const signal_synth::delineation_score_options& options, signal_synth::delineation_score_result& result)
    {
        signal_synth::delineation_score_result fresh;
        fresh.record_duration_seconds = record_duration_seconds;
        fresh.tolerance_seconds = options.tolerance_seconds;
        if (!std::isfinite(record_duration_seconds) || record_duration_seconds <= 0.0)
            fresh.messages.push_back("record duration must be finite and positive");
        if (!std::isfinite(options.tolerance_seconds) || options.tolerance_seconds <= 0.0)
            fresh.messages.push_back("delineation tolerance must be finite and positive");
        if (fresh.messages.empty())
        {
            validate_events(ground_truth, record_duration_seconds, "ground truth", fresh.messages);
            validate_events(predictions, record_duration_seconds, "prediction", fresh.messages);
        }
        if (!fresh.messages.empty())
        {
            result = fresh;
            return false;
        }

        std::map<std::string, std::size_t> prediction_by_identity;
        for (std::size_t i = 0; i < predictions.size(); ++i)
            prediction_by_identity[identity(predictions[i])] = i;
        std::set<std::size_t> used_predictions;
        for (std::size_t i = 0; i < ground_truth.size(); ++i)
        {
            const std::map<std::string, std::size_t>::const_iterator found = prediction_by_identity.find(identity(ground_truth[i]));
            if (found == prediction_by_identity.end())
            {
                fresh.missing_events.push_back(ground_truth[i]);
                continue;
            }
            const std::size_t prediction_index = found->second;
            used_predictions.insert(prediction_index);
            signal_synth::delineation_score_match match;
            match.ground_truth_index = ground_truth[i].original_index;
            match.prediction_index = predictions[prediction_index].original_index;
            match.beat_index = ground_truth[i].beat_index;
            match.lead = ground_truth[i].lead;
            match.kind = ground_truth[i].kind;
            match.ground_truth_time_seconds = ground_truth[i].time_seconds;
            match.prediction_time_seconds = predictions[prediction_index].time_seconds;
            match.error_seconds = match.prediction_time_seconds - match.ground_truth_time_seconds;
            match.within_tolerance = std::fabs(match.error_seconds) <= options.tolerance_seconds + 1e-15;
            fresh.matches.push_back(match);
        }
        for (std::size_t i = 0; i < predictions.size(); ++i)
            if (used_predictions.find(i) == used_predictions.end()) fresh.unexpected_events.push_back(predictions[i]);
        fresh.total = group_metrics(ground_truth, predictions, fresh.matches, fresh.missing_events, fresh.unexpected_events, "", "");

        for (int kind = 0; kind < static_cast<int>(signal_synth::delineation_kind_count); ++kind)
        {
            signal_synth::delineation_score_group group;
            group.kind = signal_synth::delineation_kind_name(static_cast<signal_synth::delineation_kind>(kind));
            group.metrics = group_metrics(ground_truth, predictions, fresh.matches, fresh.missing_events, fresh.unexpected_events, group.kind, "");
            fresh.kinds.push_back(group);
        }
        std::vector<std::string> leads = scoped_leads;
        for (std::size_t i = 0; i < ground_truth.size(); ++i)
            if (std::find(leads.begin(), leads.end(), ground_truth[i].lead) == leads.end()) leads.push_back(ground_truth[i].lead);
        for (std::size_t i = 0; i < predictions.size(); ++i)
            if (std::find(leads.begin(), leads.end(), predictions[i].lead) == leads.end()) leads.push_back(predictions[i].lead);
        std::sort(leads.begin(), leads.end(), lead_less);
        for (std::size_t lead = 0; lead < leads.size(); ++lead)
        {
            signal_synth::delineation_score_group lead_group;
            lead_group.lead = leads[lead];
            lead_group.metrics = group_metrics(ground_truth, predictions, fresh.matches, fresh.missing_events, fresh.unexpected_events, "", lead_group.lead);
            fresh.leads.push_back(lead_group);
            for (int kind = 0; kind < static_cast<int>(signal_synth::delineation_kind_count); ++kind)
            {
                signal_synth::delineation_score_group group;
                group.kind = signal_synth::delineation_kind_name(static_cast<signal_synth::delineation_kind>(kind));
                group.lead = leads[lead];
                group.metrics = group_metrics(ground_truth, predictions, fresh.matches, fresh.missing_events, fresh.unexpected_events, group.kind, group.lead);
                fresh.kind_leads.push_back(group);
            }
        }
        fresh.success = true;
        result = fresh;
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

    void write_metrics_json(std::ostringstream& output, const signal_synth::delineation_score_metrics& metrics)
    {
        output << "{\"ground_truth_count\":" << metrics.ground_truth_count << ",\"prediction_count\":" << metrics.prediction_count
               << ",\"paired_count\":" << metrics.paired_count << ",\"within_tolerance_count\":" << metrics.within_tolerance_count
               << ",\"missing_prediction_count\":" << metrics.missing_prediction_count << ",\"unexpected_prediction_count\":" << metrics.unexpected_prediction_count
               << ",\"out_of_tolerance_count\":" << metrics.out_of_tolerance_count << ",\"false_negative_count\":" << metrics.false_negative_count
               << ",\"false_positive_count\":" << metrics.false_positive_count << ",\"sensitivity\":";
        write_nullable(output, metrics.sensitivity, metrics.ground_truth_count > 0u);
        output << ",\"positive_predictive_value\":";
        write_nullable(output, metrics.positive_predictive_value, metrics.prediction_count > 0u);
        output << ",\"f1_score\":";
        write_nullable(output, metrics.f1_score, metrics.ground_truth_count + metrics.prediction_count > 0u);
        output << ",\"within_tolerance_fraction\":";
        write_nullable(output, metrics.within_tolerance_fraction, metrics.paired_count > 0u);
        output << ",\"timing_error_seconds\":{\"mean\":";
        write_nullable(output, metrics.mean_error_seconds, metrics.paired_count > 0u);
        output << ",\"mean_absolute\":";
        write_nullable(output, metrics.mean_absolute_error_seconds, metrics.paired_count > 0u);
        output << ",\"median_absolute\":";
        write_nullable(output, metrics.median_absolute_error_seconds, metrics.paired_count > 0u);
        output << ",\"rms\":";
        write_nullable(output, metrics.rms_error_seconds, metrics.paired_count > 0u);
        output << ",\"p95_absolute\":";
        write_nullable(output, metrics.p95_absolute_error_seconds, metrics.paired_count > 0u);
        output << ",\"max_absolute\":";
        write_nullable(output, metrics.max_absolute_error_seconds, metrics.paired_count > 0u);
        output << "}}";
    }

    void write_group_json(std::ostringstream& output, const signal_synth::delineation_score_group& group)
    {
        output << '{';
        bool comma = false;
        if (!group.kind.empty())
        {
            output << "\"kind\":" << json_string(group.kind);
            comma = true;
        }
        if (!group.lead.empty())
            output << (comma ? "," : "") << "\"lead\":" << json_string(group.lead);
        output << ",\"metrics\":";
        write_metrics_json(output, group.metrics);
        output << '}';
    }

    void write_event_json(std::ostringstream& output, const signal_synth::delineation_event& event)
    {
        output << "{\"beat_index\":" << json_string(uint64_text(event.beat_index)) << ",\"lead\":" << json_string(event.lead)
               << ",\"kind\":" << json_string(signal_synth::delineation_kind_name(event.kind)) << ",\"time_seconds\":" << event.time_seconds << '}';
    }

    void write_metric_csv_row(std::ostringstream& output, const std::string& group_type, const std::string& kind, const std::string& lead, const signal_synth::delineation_score_metrics& metrics)
    {
        output << "metrics," << group_type << ',' << csv_cell(kind) << ',' << csv_cell(lead) << ",,,," << metrics.ground_truth_count << ',' << metrics.prediction_count << ',' << metrics.paired_count
               << ',' << metrics.within_tolerance_count << ',' << metrics.missing_prediction_count << ',' << metrics.unexpected_prediction_count << ',' << metrics.out_of_tolerance_count
               << ',' << metrics.false_negative_count << ',' << metrics.false_positive_count << ',' << csv_number(metrics.sensitivity, metrics.ground_truth_count > 0u)
               << ',' << csv_number(metrics.positive_predictive_value, metrics.prediction_count > 0u) << ',' << csv_number(metrics.f1_score, metrics.ground_truth_count + metrics.prediction_count > 0u)
               << ',' << csv_number(metrics.mean_error_seconds, metrics.paired_count > 0u) << ',' << csv_number(metrics.mean_absolute_error_seconds, metrics.paired_count > 0u)
               << ',' << csv_number(metrics.p95_absolute_error_seconds, metrics.paired_count > 0u) << '\n';
    }
}

namespace signal_synth
{
    delineation_score_options::delineation_score_options() : tolerance_seconds(0.040) {}
    delineation_score_metrics::delineation_score_metrics()
        : ground_truth_count(0), prediction_count(0), paired_count(0), within_tolerance_count(0), missing_prediction_count(0), unexpected_prediction_count(0), out_of_tolerance_count(0), false_negative_count(0), false_positive_count(0), sensitivity(0.0), positive_predictive_value(0.0), f1_score(0.0), within_tolerance_fraction(0.0), mean_error_seconds(0.0), mean_absolute_error_seconds(0.0), median_absolute_error_seconds(0.0), rms_error_seconds(0.0), p95_absolute_error_seconds(0.0), max_absolute_error_seconds(0.0) {}
    delineation_score_result::delineation_score_result() : success(false), record_duration_seconds(0.0), tolerance_seconds(0.0), total(), kinds(), leads(), kind_leads(), matches(), missing_events(), unexpected_events(), messages() {}

    bool delineation_ground_truth_from_render(const ecg_render_bundle& render, const delineation_output_document& scope, std::vector<delineation_event>& output, std::vector<std::string>& messages)
    {
        output.clear();
        messages.clear();
        delineation_io_result validation;
        if (!write_delineation_output(scope, validation))
        {
            messages.push_back(validation.messages.empty() ? "invalid delineation scope" : validation.messages[0].message);
            return false;
        }
        std::vector<unsigned long long> beats = scope.beat_indices;
        if (scope.scope_mode == delineation_scope_all_beats)
        {
            const clinical_beat_annotation* record_beats = render.record.beats();
            for (unsigned int i = 0; i < render.record.beat_count(); ++i)
                beats.push_back(record_beats[i].beat_index);
        }
        else
        {
            for (std::size_t i = 0; i < beats.size(); ++i)
            {
                if (!contains_beat(render.record, beats[i]))
                {
                    messages.push_back("selected delineation beat does not exist in the rendered record: " + uint64_text(beats[i]));
                    return false;
                }
            }
        }
        for (std::size_t lead = 0; lead < scope.leads.size(); ++lead)
        {
            const int lead_index = find_lead(render.record, scope.leads[lead]);
            if (lead_index < 0)
            {
                messages.push_back("selected delineation lead does not exist in the rendered record: " + scope.leads[lead]);
                return false;
            }
            for (std::size_t beat = 0; beat < beats.size(); ++beat)
            {
                const unsigned long long beat_index = beats[beat];
                const clinical_fiducial_annotation* p_peak = find_fiducial(render.record, beat_index, lead_index, clinical_p_peak, clinical_fiducial_lead_measurement, true);
                if (p_peak)
                {
                    add_truth_event(output, beat_index, scope.leads[lead], delineation_p_onset, find_fiducial(render.record, beat_index, -1, clinical_p_onset, clinical_fiducial_construction, true));
                    add_truth_event(output, beat_index, scope.leads[lead], delineation_p_peak, p_peak);
                    add_truth_event(output, beat_index, scope.leads[lead], delineation_p_offset, find_fiducial(render.record, beat_index, -1, clinical_p_offset, clinical_fiducial_construction, true));
                }
                const bool qrs_visible = find_fiducial(render.record, beat_index, lead_index, clinical_q_peak, clinical_fiducial_lead_measurement, true)
                    || find_fiducial(render.record, beat_index, lead_index, clinical_r_peak, clinical_fiducial_lead_measurement, true)
                    || find_fiducial(render.record, beat_index, lead_index, clinical_s_peak, clinical_fiducial_lead_measurement, true);
                if (qrs_visible)
                {
                    add_truth_event(output, beat_index, scope.leads[lead], delineation_qrs_onset, find_fiducial(render.record, beat_index, -1, clinical_qrs_onset, clinical_fiducial_construction, true));
                    add_truth_event(output, beat_index, scope.leads[lead], delineation_j_point, find_fiducial(render.record, beat_index, -1, clinical_j_point, clinical_fiducial_construction, true));
                    add_truth_event(output, beat_index, scope.leads[lead], delineation_qrs_offset, find_fiducial(render.record, beat_index, -1, clinical_qrs_offset, clinical_fiducial_construction, true));
                }
                const clinical_fiducial_annotation* t_peak = find_fiducial(render.record, beat_index, lead_index, clinical_t_peak, clinical_fiducial_lead_measurement, true);
                if (t_peak)
                {
                    add_truth_event(output, beat_index, scope.leads[lead], delineation_t_onset, find_fiducial(render.record, beat_index, -1, clinical_t_onset, clinical_fiducial_construction, true));
                    add_truth_event(output, beat_index, scope.leads[lead], delineation_t_peak, t_peak);
                    add_truth_event(output, beat_index, scope.leads[lead], delineation_t_offset, find_fiducial(render.record, beat_index, -1, clinical_t_offset, clinical_fiducial_construction, true));
                }
            }
        }
        std::stable_sort(output.begin(), output.end(), event_less);
        for (std::size_t i = 0; i < output.size(); ++i)
            output[i].original_index = static_cast<unsigned int>(i);
        return true;
    }

    bool score_delineation_events(double record_duration_seconds, const std::vector<delineation_event>& ground_truth, const std::vector<delineation_event>& predictions, const delineation_score_options& options, delineation_score_result& result)
    {
        std::vector<std::string> leads;
        return score_core(record_duration_seconds, ground_truth, predictions, leads, options, result);
    }

    bool score_delineation_output_to_render(const ecg_render_bundle& render, const delineation_output_document& predictions, const delineation_score_options& options, delineation_score_result& result)
    {
        delineation_score_result fresh;
        delineation_io_result validation;
        if (!write_delineation_output(predictions, validation))
        {
            fresh.messages.push_back(validation.messages.empty() ? "invalid delineation output document" : validation.messages[0].message);
            result = fresh;
            return false;
        }
        std::vector<delineation_event> ground_truth;
        if (!delineation_ground_truth_from_render(render, predictions, ground_truth, fresh.messages))
        {
            result = fresh;
            return false;
        }
        return score_core(render.document.duration_seconds, ground_truth, predictions.events, predictions.leads, options, result);
    }

    std::string delineation_score_result_json(const ecg_render_bundle& render, const delineation_output_document& predictions, const delineation_score_result& result)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10)
               << "{\"schema_version\":1,\"score_type\":\"ecg_delineation_qa\",\"target\":\"ecg_delineation\",\"scenario\":{\"scenario_id\":" << json_string(render.document.scenario_id)
               << ",\"duration_seconds\":" << result.record_duration_seconds << ",\"render_identity\":" << json_string(render.render_identity)
               << "},\"algorithm\":{\"name\":" << json_string(predictions.algorithm.name) << ",\"version\":" << json_string(predictions.algorithm.version)
               << "},\"scope\":{\"mode\":" << json_string(delineation_scope_mode_name(predictions.scope_mode)) << ",\"leads\":[";
        for (std::size_t i = 0; i < predictions.leads.size(); ++i) output << (i ? "," : "") << json_string(predictions.leads[i]);
        output << ']';
        if (predictions.scope_mode == delineation_scope_selected_beats)
        {
            output << ",\"beat_indices\":[";
            for (std::size_t i = 0; i < predictions.beat_indices.size(); ++i) output << (i ? "," : "") << json_string(uint64_text(predictions.beat_indices[i]));
            output << ']';
        }
        output << "},\"options\":{\"tolerance_seconds\":" << result.tolerance_seconds << "},\"overall\":";
        write_metrics_json(output, result.total);
        output << ",\"by_kind\":[";
        for (std::size_t i = 0; i < result.kinds.size(); ++i) { output << (i ? "," : ""); write_group_json(output, result.kinds[i]); }
        output << "],\"by_lead\":[";
        for (std::size_t i = 0; i < result.leads.size(); ++i) { output << (i ? "," : ""); write_group_json(output, result.leads[i]); }
        output << "],\"by_kind_lead\":[";
        for (std::size_t i = 0; i < result.kind_leads.size(); ++i) { output << (i ? "," : ""); write_group_json(output, result.kind_leads[i]); }
        output << "],\"matches\":[";
        for (std::size_t i = 0; i < result.matches.size(); ++i)
        {
            const delineation_score_match& match = result.matches[i];
            output << (i ? "," : "") << "{\"beat_index\":" << json_string(uint64_text(match.beat_index)) << ",\"lead\":" << json_string(match.lead)
                   << ",\"kind\":" << json_string(delineation_kind_name(match.kind)) << ",\"ground_truth_time_seconds\":" << match.ground_truth_time_seconds
                   << ",\"prediction_time_seconds\":" << match.prediction_time_seconds << ",\"error_seconds\":" << match.error_seconds
                   << ",\"within_tolerance\":" << (match.within_tolerance ? "true" : "false") << '}';
        }
        output << "],\"missing_events\":[";
        for (std::size_t i = 0; i < result.missing_events.size(); ++i) { output << (i ? "," : ""); write_event_json(output, result.missing_events[i]); }
        output << "],\"unexpected_events\":[";
        for (std::size_t i = 0; i < result.unexpected_events.size(); ++i) { output << (i ? "," : ""); write_event_json(output, result.unexpected_events[i]); }
        output << "],\"notes\":[\"Events are paired by beat, lead, and kind identity; undefined zero-denominator metrics are null.\",\"Synthetic engineering QA evidence; not a clinical validation claim.\"]}";
        return output.str();
    }

    std::string delineation_score_result_csv(const delineation_score_result& result)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10)
               << "row_type,group_type,kind,lead,beat_index,ground_truth_time_seconds,prediction_time_seconds,ground_truth_count,prediction_count,paired_count,within_tolerance_count,missing_prediction_count,unexpected_prediction_count,out_of_tolerance_count,false_negative_count,false_positive_count,sensitivity,positive_predictive_value,f1_score,mean_error_seconds,mean_absolute_error_seconds,p95_absolute_error_seconds\n";
        write_metric_csv_row(output, "overall", "", "", result.total);
        for (std::size_t i = 0; i < result.kinds.size(); ++i) write_metric_csv_row(output, "kind", result.kinds[i].kind, "", result.kinds[i].metrics);
        for (std::size_t i = 0; i < result.leads.size(); ++i) write_metric_csv_row(output, "lead", "", result.leads[i].lead, result.leads[i].metrics);
        for (std::size_t i = 0; i < result.kind_leads.size(); ++i) write_metric_csv_row(output, "kind_lead", result.kind_leads[i].kind, result.kind_leads[i].lead, result.kind_leads[i].metrics);
        for (std::size_t i = 0; i < result.matches.size(); ++i)
        {
            const delineation_score_match& match = result.matches[i];
            output << (match.within_tolerance ? "match" : "out_of_tolerance") << ",," << delineation_kind_name(match.kind) << ',' << csv_cell(match.lead) << ',' << match.beat_index
                   << ',' << match.ground_truth_time_seconds << ',' << match.prediction_time_seconds << ",,,,,,,,,,,,,,,\n";
        }
        for (std::size_t i = 0; i < result.missing_events.size(); ++i)
            output << "missing,," << delineation_kind_name(result.missing_events[i].kind) << ',' << csv_cell(result.missing_events[i].lead) << ',' << result.missing_events[i].beat_index << ',' << result.missing_events[i].time_seconds << ",,,,,,,,,,,,,,,,\n";
        for (std::size_t i = 0; i < result.unexpected_events.size(); ++i)
            output << "unexpected,," << delineation_kind_name(result.unexpected_events[i].kind) << ',' << csv_cell(result.unexpected_events[i].lead) << ',' << result.unexpected_events[i].beat_index << ",," << result.unexpected_events[i].time_seconds << ",,,,,,,,,,,,,,,\n";
        return output.str();
    }

    std::string delineation_score_report_html(const ecg_render_bundle& render, const delineation_score_result& result)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(6)
               << "<!doctype html><html><head><meta charset=\"utf-8\"><title>ECG delineation scoring</title><style>body{font-family:Arial,sans-serif;margin:24px;color:#20252b}table{border-collapse:collapse;margin:16px 0}th,td{border:1px solid #c9ced4;padding:6px 9px;text-align:right}th:first-child,td:first-child{text-align:left}.note{color:#555;max-width:900px}</style></head><body>"
               << "<h1>ECG delineation scoring</h1><p>Scenario: " << html_text(render.document.scenario_id) << " | Tolerance: " << 1000.0 * result.tolerance_seconds << " ms</p>"
               << "<table><tr><th>Group</th><th>Truth</th><th>Predicted</th><th>Within tolerance</th><th>Missing</th><th>Unexpected</th><th>Out of tolerance</th><th>Sensitivity</th><th>PPV</th><th>F1</th><th>MAE (ms)</th><th>P95 (ms)</th></tr>";
        for (std::size_t row = 0; row <= result.kinds.size(); ++row)
        {
            const delineation_score_metrics& metrics = row == 0 ? result.total : result.kinds[row - 1].metrics;
            const std::string label = row == 0 ? "Overall" : result.kinds[row - 1].kind;
            output << "<tr><td>" << html_text(label) << "</td><td>" << metrics.ground_truth_count << "</td><td>" << metrics.prediction_count << "</td><td>" << metrics.within_tolerance_count
                   << "</td><td>" << metrics.missing_prediction_count << "</td><td>" << metrics.unexpected_prediction_count << "</td><td>" << metrics.out_of_tolerance_count
                   << "</td><td>" << csv_number(metrics.sensitivity, metrics.ground_truth_count > 0u) << "</td><td>" << csv_number(metrics.positive_predictive_value, metrics.prediction_count > 0u)
                   << "</td><td>" << csv_number(metrics.f1_score, metrics.ground_truth_count + metrics.prediction_count > 0u) << "</td><td>"
                   << (metrics.paired_count ? csv_number(1000.0 * metrics.mean_absolute_error_seconds, true) : "NA") << "</td><td>"
                   << (metrics.paired_count ? csv_number(1000.0 * metrics.p95_absolute_error_seconds, true) : "NA") << "</td></tr>";
        }
        output << "</table><p class=\"note\">Events are paired by exact beat, lead, and fiducial identity. Undefined metrics are NA. This is synthetic engineering QA evidence, not a clinical validation claim.</p></body></html>";
        return output.str();
    }
}
