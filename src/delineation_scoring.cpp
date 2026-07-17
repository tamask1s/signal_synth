#include "delineation_scoring.h"

#include "ecg_render.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <tuple>

namespace
{
    const char* lead_names[] = {"I","II","III","aVR","aVL","aVF","V1","V2","V3","V4","V5","V6"};

    int lead_rank(const std::string& lead)
    {
        for (int i = 0; i < 12; ++i) if (lead == lead_names[i]) return i;
        return -1;
    }

    bool lead_less(const std::string& left, const std::string& right)
    {
        return lead_rank(left) < lead_rank(right);
    }

    std::string uint64_text(unsigned long long value)
    {
        std::ostringstream output;
        output << value;
        return output.str();
    }

    std::string json_string(const std::string& value)
    {
        std::ostringstream output;
        output << '"';
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            const unsigned char ch = static_cast<unsigned char>(value[i]);
            if (ch == '"') output << "\\\"";
            else if (ch == '\\') output << "\\\\";
            else if (ch == '\n') output << "\\n";
            else if (ch == '\r') output << "\\r";
            else if (ch == '\t') output << "\\t";
            else if (ch < 0x20) output << "\\u00" << "0123456789abcdef"[ch >> 4] << "0123456789abcdef"[ch & 15u];
            else output << static_cast<char>(ch);
        }
        return output.str() + '"';
    }

    std::string html_text(const std::string& value)
    {
        std::string output;
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            if (value[i] == '&') output += "&amp;";
            else if (value[i] == '<') output += "&lt;";
            else if (value[i] == '>') output += "&gt;";
            else if (value[i] == '"') output += "&quot;";
            else output += value[i];
        }
        return output;
    }

    bool in_window(double time, const signal_synth::delineation_time_window& window)
    {
        return time >= window.start_seconds && time < window.end_seconds;
    }

    bool in_scope_time(double time, const signal_synth::delineation_evaluation_scope& scope)
    {
        if (scope.windows.empty()) return true;
        for (std::size_t i = 0; i < scope.windows.size(); ++i)
            if (in_window(time, scope.windows[i])) return true;
        return false;
    }

    bool in_scope_event(const signal_synth::delineation_event& event, const signal_synth::delineation_evaluation_scope& scope)
    {
        return std::find(scope.leads.begin(), scope.leads.end(), event.lead) != scope.leads.end() && in_scope_time(event.time_seconds, scope);
    }

    bool validate_scope(const signal_synth::clinical_ecg_record& record, const signal_synth::delineation_evaluation_scope& scope, double duration, std::vector<std::string>& messages)
    {
        if (scope.leads.empty()) messages.push_back("delineation evaluation scope requires at least one lead");
        std::set<std::string> leads;
        for (std::size_t i = 0; i < scope.leads.size(); ++i)
        {
            if (lead_rank(scope.leads[i]) < 0 || static_cast<unsigned int>(lead_rank(scope.leads[i])) >= record.lead_count()) messages.push_back("delineation evaluation scope contains an unavailable ECG lead: " + scope.leads[i]);
            else if (!leads.insert(scope.leads[i]).second) messages.push_back("delineation evaluation scope contains a duplicate lead: " + scope.leads[i]);
        }
        for (std::size_t i = 0; i < scope.windows.size(); ++i)
        {
            const signal_synth::delineation_time_window& window = scope.windows[i];
            if (!std::isfinite(window.start_seconds) || !std::isfinite(window.end_seconds) || window.start_seconds < 0.0 || window.end_seconds <= window.start_seconds || window.end_seconds > duration + 1e-9)
                messages.push_back("delineation evaluation window must be finite, increasing, and inside the record");
        }
        return messages.empty();
    }

    const signal_synth::clinical_fiducial_annotation* atrial_measurement(const signal_synth::clinical_ecg_record& record, unsigned long long atrial_index, int lead, signal_synth::clinical_fiducial_kind kind)
    {
        const signal_synth::clinical_fiducial_annotation* items = record.fiducials();
        for (unsigned int i = 0; i < record.fiducial_count(); ++i)
            if (items[i].atrial_index >= 0 && static_cast<unsigned long long>(items[i].atrial_index) == atrial_index && items[i].lead_index == lead && items[i].kind == kind && items[i].source == signal_synth::clinical_fiducial_lead_measurement) return &items[i];
        return 0;
    }

    const signal_synth::clinical_fiducial_annotation* beat_measurement(const signal_synth::clinical_ecg_record& record, unsigned long long beat_index, int lead, signal_synth::clinical_fiducial_kind kind)
    {
        const signal_synth::clinical_fiducial_annotation* items = record.fiducials();
        for (unsigned int i = 0; i < record.fiducial_count(); ++i)
            if (items[i].beat_index == beat_index && items[i].lead_index == lead && items[i].kind == kind && items[i].source == signal_synth::clinical_fiducial_lead_measurement) return &items[i];
        return 0;
    }

    double clamp_time(double value, double duration)
    {
        return std::max(0.0, std::min(duration, value));
    }

    void add_truth(std::vector<signal_synth::delineation_truth_point>& output, const signal_synth::delineation_evaluation_scope& scope, signal_synth::delineation_anchor_type anchor_type, unsigned long long anchor_index, const std::string& lead, signal_synth::delineation_kind kind, signal_synth::delineation_truth_status status, const std::string& reason, double reference_time, double window_start, double window_end, double duration)
    {
        const double scoped_time = clamp_time(reference_time, duration > 0.0 ? duration - 1e-12 : 0.0);
        if (!in_scope_time(scoped_time, scope)) return;
        signal_synth::delineation_truth_point point;
        point.anchor_type = anchor_type;
        point.anchor_index = anchor_index;
        point.lead = lead;
        point.kind = kind;
        point.status = status;
        point.reason = reason;
        point.time_seconds = reference_time;
        point.evaluation_start_seconds = clamp_time(window_start, duration);
        point.evaluation_end_seconds = clamp_time(window_end, duration);
        if (point.evaluation_end_seconds <= point.evaluation_start_seconds)
        {
            point.evaluation_start_seconds = clamp_time(scoped_time - 0.04, duration);
            point.evaluation_end_seconds = clamp_time(scoped_time + 0.04, duration);
        }
        point.original_index = static_cast<unsigned int>(output.size());
        output.push_back(point);
    }

    bool reference_inside(double time, double duration)
    {
        return std::isfinite(time) && time >= 0.0 && time < duration;
    }

    void add_atrial_truth(const signal_synth::ecg_render_bundle& render, const signal_synth::delineation_evaluation_scope& scope, const signal_synth::clinical_atrial_event& atrial, const std::string& lead, int lead_index, std::vector<signal_synth::delineation_truth_point>& output)
    {
        const double duration = render.document.duration_seconds;
        const signal_synth::clinical_fiducial_annotation* measured = atrial_measurement(render.record, atrial.atrial_index, lead_index, signal_synth::clinical_p_peak);
        const bool inside = reference_inside(atrial.onset_time_seconds, duration) && reference_inside(atrial.peak_time_seconds, duration) && reference_inside(atrial.offset_time_seconds, duration);
        signal_synth::delineation_truth_status status = signal_synth::delineation_truth_present;
        std::string reason;
        if (!atrial.visible) { status = signal_synth::delineation_truth_absent; reason = "wave_absent"; }
        else if (!inside) { status = signal_synth::delineation_truth_not_evaluable; reason = "record_boundary"; }
        else if (!measured || !measured->present) { status = signal_synth::delineation_truth_not_evaluable; reason = "below_lead_threshold"; }
        const double peak = status == signal_synth::delineation_truth_present && measured ? measured->time_seconds : atrial.peak_time_seconds;
        add_truth(output, scope, signal_synth::delineation_anchor_atrial_event, atrial.atrial_index, lead, signal_synth::delineation_p_onset, status, reason, atrial.onset_time_seconds, atrial.onset_time_seconds, atrial.offset_time_seconds, duration);
        add_truth(output, scope, signal_synth::delineation_anchor_atrial_event, atrial.atrial_index, lead, signal_synth::delineation_p_peak, status, reason, peak, atrial.onset_time_seconds, atrial.offset_time_seconds, duration);
        add_truth(output, scope, signal_synth::delineation_anchor_atrial_event, atrial.atrial_index, lead, signal_synth::delineation_p_offset, status, reason, atrial.offset_time_seconds, atrial.onset_time_seconds, atrial.offset_time_seconds, duration);
    }

    void add_absent_p_truth(const signal_synth::ecg_render_bundle& render, const signal_synth::delineation_evaluation_scope& scope, const signal_synth::clinical_beat_annotation& beat, const std::string& lead, std::vector<signal_synth::delineation_truth_point>& output)
    {
        const double reference = beat.qrs_onset_time_seconds - 0.12;
        const double start = beat.qrs_onset_time_seconds - 0.28;
        const double end = beat.qrs_onset_time_seconds - 0.02;
        add_truth(output, scope, signal_synth::delineation_anchor_ventricular_beat, beat.beat_index, lead, signal_synth::delineation_p_onset, signal_synth::delineation_truth_absent, "no_atrial_event", reference - 0.04, start, end, render.document.duration_seconds);
        add_truth(output, scope, signal_synth::delineation_anchor_ventricular_beat, beat.beat_index, lead, signal_synth::delineation_p_peak, signal_synth::delineation_truth_absent, "no_atrial_event", reference, start, end, render.document.duration_seconds);
        add_truth(output, scope, signal_synth::delineation_anchor_ventricular_beat, beat.beat_index, lead, signal_synth::delineation_p_offset, signal_synth::delineation_truth_absent, "no_atrial_event", reference + 0.04, start, end, render.document.duration_seconds);
    }

    void add_ventricular_truth(const signal_synth::ecg_render_bundle& render, const signal_synth::delineation_evaluation_scope& scope, const signal_synth::clinical_beat_annotation& beat, const std::string& lead, int lead_index, std::vector<signal_synth::delineation_truth_point>& output)
    {
        const double duration = render.document.duration_seconds;
        const bool qrs_inside = reference_inside(beat.qrs_onset_time_seconds, duration) && reference_inside(beat.j_point_time_seconds, duration) && reference_inside(beat.qrs_offset_time_seconds, duration);
        const signal_synth::clinical_fiducial_annotation* measured_q = beat_measurement(render.record, beat.beat_index, lead_index, signal_synth::clinical_q_peak);
        const signal_synth::clinical_fiducial_annotation* measured_r = beat_measurement(render.record, beat.beat_index, lead_index, signal_synth::clinical_r_peak);
        const signal_synth::clinical_fiducial_annotation* measured_s = beat_measurement(render.record, beat.beat_index, lead_index, signal_synth::clinical_s_peak);
        const bool qrs_visible = (measured_q && measured_q->present) || (measured_r && measured_r->present) || (measured_s && measured_s->present);
        signal_synth::delineation_truth_status qrs_status = signal_synth::delineation_truth_present;
        std::string qrs_reason;
        if (!beat.qrs_present) { qrs_status = signal_synth::delineation_truth_absent; qrs_reason = "wave_absent"; }
        else if (!qrs_inside) { qrs_status = signal_synth::delineation_truth_not_evaluable; qrs_reason = "record_boundary"; }
        else if (!qrs_visible) { qrs_status = signal_synth::delineation_truth_not_evaluable; qrs_reason = "below_lead_threshold"; }
        add_truth(output, scope, signal_synth::delineation_anchor_ventricular_beat, beat.beat_index, lead, signal_synth::delineation_qrs_onset, qrs_status, qrs_reason, beat.qrs_onset_time_seconds, beat.qrs_onset_time_seconds, beat.qrs_offset_time_seconds, duration);
        add_truth(output, scope, signal_synth::delineation_anchor_ventricular_beat, beat.beat_index, lead, signal_synth::delineation_j_point, qrs_status, qrs_reason, beat.j_point_time_seconds, beat.qrs_onset_time_seconds, beat.qrs_offset_time_seconds, duration);
        add_truth(output, scope, signal_synth::delineation_anchor_ventricular_beat, beat.beat_index, lead, signal_synth::delineation_qrs_offset, qrs_status, qrs_reason, beat.qrs_offset_time_seconds, beat.qrs_onset_time_seconds, beat.qrs_offset_time_seconds, duration);

        const signal_synth::clinical_fiducial_annotation* measured_t = beat_measurement(render.record, beat.beat_index, lead_index, signal_synth::clinical_t_peak);
        const bool t_inside = reference_inside(beat.t_onset_time_seconds, duration) && reference_inside(beat.t_peak_time_seconds, duration) && reference_inside(beat.t_offset_time_seconds, duration);
        signal_synth::delineation_truth_status t_status = signal_synth::delineation_truth_present;
        std::string t_reason;
        if (!beat.t_present) { t_status = signal_synth::delineation_truth_absent; t_reason = "wave_absent"; }
        else if (!t_inside) { t_status = signal_synth::delineation_truth_not_evaluable; t_reason = "record_boundary"; }
        else if (!measured_t || !measured_t->present) { t_status = signal_synth::delineation_truth_not_evaluable; t_reason = "below_lead_threshold"; }
        const double t_peak = t_status == signal_synth::delineation_truth_present && measured_t ? measured_t->time_seconds : beat.t_peak_time_seconds;
        add_truth(output, scope, signal_synth::delineation_anchor_ventricular_beat, beat.beat_index, lead, signal_synth::delineation_t_onset, t_status, t_reason, beat.t_onset_time_seconds, beat.t_onset_time_seconds, beat.t_offset_time_seconds, duration);
        add_truth(output, scope, signal_synth::delineation_anchor_ventricular_beat, beat.beat_index, lead, signal_synth::delineation_t_peak, t_status, t_reason, t_peak, beat.t_onset_time_seconds, beat.t_offset_time_seconds, duration);
        add_truth(output, scope, signal_synth::delineation_anchor_ventricular_beat, beat.beat_index, lead, signal_synth::delineation_t_offset, t_status, t_reason, beat.t_offset_time_seconds, beat.t_onset_time_seconds, beat.t_offset_time_seconds, duration);
    }

    bool truth_less(const signal_synth::delineation_truth_point& left, const signal_synth::delineation_truth_point& right)
    {
        if (left.time_seconds != right.time_seconds) return left.time_seconds < right.time_seconds;
        if (left.lead != right.lead) return lead_less(left.lead, right.lead);
        if (left.kind != right.kind) return left.kind < right.kind;
        if (left.anchor_type != right.anchor_type) return left.anchor_type < right.anchor_type;
        return left.anchor_index < right.anchor_index;
    }

    bool valid_prediction(const signal_synth::delineation_event& event, double duration)
    {
        return lead_rank(event.lead) >= 0 && event.kind >= signal_synth::delineation_p_onset && event.kind < signal_synth::delineation_kind_count
            && std::isfinite(event.time_seconds) && event.time_seconds >= 0.0 && event.time_seconds <= duration + 1e-9
            && (!event.has_confidence || (std::isfinite(event.confidence) && event.confidence >= 0.0 && event.confidence <= 1.0));
    }

    struct candidate
    {
        double absolute_error;
        std::size_t truth_index;
        std::size_t prediction_index;
    };

    bool candidate_less(const candidate& left, const candidate& right)
    {
        if (left.absolute_error != right.absolute_error) return left.absolute_error < right.absolute_error;
        if (left.truth_index != right.truth_index) return left.truth_index < right.truth_index;
        return left.prediction_index < right.prediction_index;
    }

    bool same_group(const signal_synth::delineation_truth_point& truth, const signal_synth::delineation_event& prediction)
    {
        return truth.lead == prediction.lead && truth.kind == prediction.kind;
    }

    bool point_matches_group(const signal_synth::delineation_truth_point& point, const std::string& kind, const std::string& lead)
    {
        return (kind.empty() || kind == signal_synth::delineation_kind_name(point.kind)) && (lead.empty() || lead == point.lead);
    }

    bool event_matches_group(const signal_synth::delineation_event& event, const std::string& kind, const std::string& lead)
    {
        return (kind.empty() || kind == signal_synth::delineation_kind_name(event.kind)) && (lead.empty() || lead == event.lead);
    }

    double mean(const std::vector<double>& values)
    {
        double sum = 0.0;
        for (std::size_t i = 0; i < values.size(); ++i) sum += values[i];
        return values.empty() ? 0.0 : sum / values.size();
    }

    double percentile(std::vector<double> values, double fraction)
    {
        if (values.empty()) return 0.0;
        std::sort(values.begin(), values.end());
        const std::size_t rank = static_cast<std::size_t>(std::ceil(fraction * values.size()));
        return values[rank ? rank - 1u : 0u];
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
        for (std::size_t i = 0; i < errors.size(); ++i) { absolute.push_back(std::fabs(errors[i])); square_sum += errors[i] * errors[i]; }
        metrics.mean_error_seconds = mean(errors);
        metrics.mean_absolute_error_seconds = mean(absolute);
        metrics.median_absolute_error_seconds = percentile(absolute, 0.5);
        metrics.rms_error_seconds = errors.empty() ? 0.0 : std::sqrt(square_sum / errors.size());
        metrics.p95_absolute_error_seconds = percentile(absolute, 0.95);
        metrics.max_absolute_error_seconds = absolute.empty() ? 0.0 : *std::max_element(absolute.begin(), absolute.end());
    }

    signal_synth::delineation_score_metrics group_metrics(const signal_synth::delineation_score_result& result, const std::vector<signal_synth::delineation_event>& scored_predictions, const std::string& kind, const std::string& lead)
    {
        signal_synth::delineation_score_metrics metrics;
        std::vector<double> errors;
        for (std::size_t i = 0; i < result.truth.size(); ++i)
        {
            if (!point_matches_group(result.truth[i], kind, lead)) continue;
            if (result.truth[i].status == signal_synth::delineation_truth_present) ++metrics.ground_truth_count;
            else if (result.truth[i].status == signal_synth::delineation_truth_absent) ++metrics.absent_truth_count;
            else ++metrics.not_evaluable_truth_count;
        }
        for (std::size_t i = 0; i < scored_predictions.size(); ++i) if (event_matches_group(scored_predictions[i], kind, lead)) ++metrics.prediction_count;
        for (std::size_t i = 0; i < result.excluded_predictions.size(); ++i) if (event_matches_group(result.excluded_predictions[i].event, kind, lead)) ++metrics.excluded_prediction_count;
        for (std::size_t i = 0; i < result.matches.size(); ++i)
        {
            signal_synth::delineation_event event;
            event.lead = result.matches[i].lead;
            event.kind = result.matches[i].kind;
            if (!event_matches_group(event, kind, lead)) continue;
            ++metrics.paired_count;
            if (result.matches[i].within_tolerance) ++metrics.within_tolerance_count;
            else ++metrics.out_of_tolerance_count;
            errors.push_back(result.matches[i].error_seconds);
        }
        for (std::size_t i = 0; i < result.missing_events.size(); ++i) if (point_matches_group(result.missing_events[i], kind, lead)) ++metrics.missing_prediction_count;
        for (std::size_t i = 0; i < result.unexpected_events.size(); ++i) if (event_matches_group(result.unexpected_events[i], kind, lead)) ++metrics.unexpected_prediction_count;
        finalize_metrics(metrics, errors);
        return metrics;
    }

    void write_nullable(std::ostringstream& output, double value, bool defined)
    {
        if (defined) output << value; else output << "null";
    }

    void write_metrics(std::ostringstream& output, const signal_synth::delineation_score_metrics& metrics)
    {
        output << "{\"ground_truth_count\":" << metrics.ground_truth_count << ",\"absent_truth_count\":" << metrics.absent_truth_count << ",\"not_evaluable_truth_count\":" << metrics.not_evaluable_truth_count
               << ",\"prediction_count\":" << metrics.prediction_count << ",\"excluded_prediction_count\":" << metrics.excluded_prediction_count << ",\"paired_count\":" << metrics.paired_count
               << ",\"within_tolerance_count\":" << metrics.within_tolerance_count << ",\"missing_prediction_count\":" << metrics.missing_prediction_count << ",\"unexpected_prediction_count\":" << metrics.unexpected_prediction_count
               << ",\"out_of_tolerance_count\":" << metrics.out_of_tolerance_count << ",\"false_negative_count\":" << metrics.false_negative_count << ",\"false_positive_count\":" << metrics.false_positive_count << ",\"sensitivity\":";
        write_nullable(output, metrics.sensitivity, metrics.ground_truth_count > 0u);
        output << ",\"positive_predictive_value\":"; write_nullable(output, metrics.positive_predictive_value, metrics.prediction_count > 0u);
        output << ",\"f1_score\":"; write_nullable(output, metrics.f1_score, metrics.ground_truth_count + metrics.prediction_count > 0u);
        output << ",\"within_tolerance_fraction\":"; write_nullable(output, metrics.within_tolerance_fraction, metrics.paired_count > 0u);
        output << ",\"timing_error_seconds\":{\"mean\":"; write_nullable(output, metrics.mean_error_seconds, metrics.paired_count > 0u);
        output << ",\"mean_absolute\":"; write_nullable(output, metrics.mean_absolute_error_seconds, metrics.paired_count > 0u);
        output << ",\"median_absolute\":"; write_nullable(output, metrics.median_absolute_error_seconds, metrics.paired_count > 0u);
        output << ",\"rms\":"; write_nullable(output, metrics.rms_error_seconds, metrics.paired_count > 0u);
        output << ",\"p95_absolute\":"; write_nullable(output, metrics.p95_absolute_error_seconds, metrics.paired_count > 0u);
        output << ",\"max_absolute\":"; write_nullable(output, metrics.max_absolute_error_seconds, metrics.paired_count > 0u);
        output << "}}";
    }

    void write_group(std::ostringstream& output, const signal_synth::delineation_score_group& group)
    {
        output << '{';
        if (!group.kind.empty()) output << "\"kind\":" << json_string(group.kind) << ',';
        if (!group.lead.empty()) output << "\"lead\":" << json_string(group.lead) << ',';
        output << "\"metrics\":";
        write_metrics(output, group.metrics);
        output << '}';
    }

    void write_truth(std::ostringstream& output, const signal_synth::delineation_truth_point& point)
    {
        output << "{\"anchor_type\":" << json_string(signal_synth::delineation_anchor_type_name(point.anchor_type)) << ",\"anchor_index\":" << json_string(uint64_text(point.anchor_index))
               << ",\"lead\":" << json_string(point.lead) << ",\"kind\":" << json_string(signal_synth::delineation_kind_name(point.kind)) << ",\"status\":" << json_string(signal_synth::delineation_truth_status_name(point.status))
               << ",\"reason\":" << json_string(point.reason) << ",\"time_seconds\":" << point.time_seconds << ",\"evaluation_start_seconds\":" << point.evaluation_start_seconds << ",\"evaluation_end_seconds\":" << point.evaluation_end_seconds << '}';
    }

    void write_event(std::ostringstream& output, const signal_synth::delineation_event& event)
    {
        output << "{\"lead\":" << json_string(event.lead) << ",\"kind\":" << json_string(signal_synth::delineation_kind_name(event.kind)) << ",\"time_seconds\":" << event.time_seconds << '}';
    }

    std::string csv_number(double value, bool defined)
    {
        if (!defined) return "NA";
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
        return output.str();
    }
}

namespace signal_synth
{
    delineation_time_window::delineation_time_window() : start_seconds(0.0), end_seconds(0.0) {}
    delineation_time_window::delineation_time_window(double start, double end) : start_seconds(start), end_seconds(end) {}
    delineation_truth_point::delineation_truth_point() : anchor_type(delineation_anchor_ventricular_beat), anchor_index(0), lead(), kind(delineation_p_onset), status(delineation_truth_present), reason(), time_seconds(0.0), evaluation_start_seconds(0.0), evaluation_end_seconds(0.0), original_index(0) {}
    delineation_score_options::delineation_score_options() : tolerance_seconds(0.040), pairing_window_seconds(0.200) {}
    delineation_score_metrics::delineation_score_metrics() : ground_truth_count(0), absent_truth_count(0), not_evaluable_truth_count(0), prediction_count(0), excluded_prediction_count(0), paired_count(0), within_tolerance_count(0), missing_prediction_count(0), unexpected_prediction_count(0), out_of_tolerance_count(0), false_negative_count(0), false_positive_count(0), sensitivity(0.0), positive_predictive_value(0.0), f1_score(0.0), within_tolerance_fraction(0.0), mean_error_seconds(0.0), mean_absolute_error_seconds(0.0), median_absolute_error_seconds(0.0), rms_error_seconds(0.0), p95_absolute_error_seconds(0.0), max_absolute_error_seconds(0.0) {}
    delineation_score_result::delineation_score_result() : success(false), record_duration_seconds(0.0), tolerance_seconds(0.0), pairing_window_seconds(0.0), total(), kinds(), leads(), kind_leads(), truth(), matches(), missing_events(), unexpected_events(), excluded_predictions(), messages() {}

    const char* delineation_anchor_type_name(delineation_anchor_type type)
    {
        return type == delineation_anchor_atrial_event ? "atrial_event" : type == delineation_anchor_ventricular_beat ? "ventricular_beat" : "";
    }

    const char* delineation_truth_status_name(delineation_truth_status status)
    {
        return status == delineation_truth_present ? "present" : status == delineation_truth_absent ? "absent" : status == delineation_truth_not_evaluable ? "not_evaluable" : "";
    }

    bool delineation_ground_truth_from_render(const ecg_render_bundle& render, const delineation_evaluation_scope& scope, std::vector<delineation_truth_point>& output, std::vector<std::string>& messages)
    {
        output.clear();
        messages.clear();
        if (!validate_scope(render.record, scope, render.document.duration_seconds, messages)) return false;
        for (std::size_t lead = 0; lead < scope.leads.size(); ++lead)
        {
            const int lead_index = lead_rank(scope.leads[lead]);
            const clinical_atrial_event* atrials = render.record.atrial_events();
            for (unsigned int i = 0; i < render.record.atrial_event_count(); ++i) add_atrial_truth(render, scope, atrials[i], scope.leads[lead], lead_index, output);
            const clinical_beat_annotation* beats = render.record.beats();
            for (unsigned int i = 0; i < render.record.beat_count(); ++i)
            {
                if (beats[i].linked_atrial_index < 0) add_absent_p_truth(render, scope, beats[i], scope.leads[lead], output);
                add_ventricular_truth(render, scope, beats[i], scope.leads[lead], lead_index, output);
            }
        }
        std::stable_sort(output.begin(), output.end(), truth_less);
        for (std::size_t i = 0; i < output.size(); ++i) output[i].original_index = static_cast<unsigned int>(i);
        return true;
    }

    bool score_delineation_events(double record_duration_seconds, const std::vector<delineation_truth_point>& ground_truth, const std::vector<delineation_event>& predictions, const delineation_evaluation_scope& scope, const delineation_score_options& options, delineation_score_result& result)
    {
        delineation_score_result fresh;
        fresh.record_duration_seconds = record_duration_seconds;
        fresh.tolerance_seconds = options.tolerance_seconds;
        fresh.pairing_window_seconds = options.pairing_window_seconds;
        fresh.truth = ground_truth;
        if (!std::isfinite(record_duration_seconds) || record_duration_seconds <= 0.0) fresh.messages.push_back("record duration must be finite and positive");
        if (!std::isfinite(options.tolerance_seconds) || options.tolerance_seconds <= 0.0) fresh.messages.push_back("delineation tolerance must be finite and positive");
        if (!std::isfinite(options.pairing_window_seconds) || options.pairing_window_seconds < options.tolerance_seconds) fresh.messages.push_back("delineation pairing window must be finite and at least the scoring tolerance");
        if (scope.leads.empty()) fresh.messages.push_back("delineation evaluation scope requires at least one lead");
        std::set<std::string> scope_leads;
        for (std::size_t i = 0; i < scope.leads.size(); ++i)
            if (lead_rank(scope.leads[i]) < 0 || !scope_leads.insert(scope.leads[i]).second) fresh.messages.push_back("delineation evaluation scope contains an invalid or duplicate lead");
        for (std::size_t i = 0; i < scope.windows.size(); ++i)
            if (!std::isfinite(scope.windows[i].start_seconds) || !std::isfinite(scope.windows[i].end_seconds) || scope.windows[i].start_seconds < 0.0 || scope.windows[i].end_seconds <= scope.windows[i].start_seconds || scope.windows[i].end_seconds > record_duration_seconds + 1e-9) fresh.messages.push_back("delineation evaluation window is invalid");
        std::set<std::tuple<int, unsigned long long, std::string, int> > truth_identities;
        for (std::size_t i = 0; i < ground_truth.size(); ++i)
        {
            const delineation_truth_point& point = ground_truth[i];
            const bool valid_enum = (point.anchor_type == delineation_anchor_atrial_event || point.anchor_type == delineation_anchor_ventricular_beat) && point.kind >= delineation_p_onset && point.kind < delineation_kind_count && point.status >= delineation_truth_present && point.status <= delineation_truth_not_evaluable;
            const bool valid_window = std::isfinite(point.evaluation_start_seconds) && std::isfinite(point.evaluation_end_seconds) && point.evaluation_start_seconds >= 0.0 && point.evaluation_end_seconds > point.evaluation_start_seconds && point.evaluation_end_seconds <= record_duration_seconds + 1e-9;
            if (!valid_enum || scope_leads.count(point.lead) == 0u || !std::isfinite(point.time_seconds) || !valid_window) fresh.messages.push_back("delineation ground-truth point is malformed or outside scope");
            if (!truth_identities.insert(std::make_tuple(static_cast<int>(point.anchor_type), point.anchor_index, point.lead, static_cast<int>(point.kind))).second) fresh.messages.push_back("delineation ground truth contains a duplicate anchor/lead/kind identity");
        }
        std::set<std::tuple<std::string, int, double> > prediction_identities;
        for (std::size_t i = 0; i < predictions.size(); ++i)
        {
            if (!valid_prediction(predictions[i], record_duration_seconds)) fresh.messages.push_back("prediction event lies outside the record or is malformed");
            if (!prediction_identities.insert(std::make_tuple(predictions[i].lead, static_cast<int>(predictions[i].kind), predictions[i].time_seconds)).second) fresh.messages.push_back("predictions contain a duplicate lead/kind/time event");
        }
        if (!fresh.messages.empty()) { result = fresh; return false; }

        std::vector<bool> truth_used(ground_truth.size(), false), prediction_used(predictions.size(), false), prediction_excluded(predictions.size(), false);
        std::vector<candidate> candidates;
        for (std::size_t t = 0; t < ground_truth.size(); ++t)
        {
            if (ground_truth[t].status != delineation_truth_present) continue;
            for (std::size_t p = 0; p < predictions.size(); ++p)
            {
                if (!in_scope_event(predictions[p], scope) || !same_group(ground_truth[t], predictions[p])) continue;
                const double error = std::fabs(predictions[p].time_seconds - ground_truth[t].time_seconds);
                if (error <= options.pairing_window_seconds + 1e-15)
                {
                    candidate item = {error, t, p};
                    candidates.push_back(item);
                }
            }
        }
        std::sort(candidates.begin(), candidates.end(), candidate_less);
        for (std::size_t i = 0; i < candidates.size(); ++i)
        {
            const candidate& item = candidates[i];
            if (truth_used[item.truth_index] || prediction_used[item.prediction_index]) continue;
            truth_used[item.truth_index] = true;
            prediction_used[item.prediction_index] = true;
            const delineation_truth_point& truth = ground_truth[item.truth_index];
            const delineation_event& prediction = predictions[item.prediction_index];
            delineation_score_match match;
            match.ground_truth_index = truth.original_index;
            match.prediction_index = prediction.original_index;
            match.anchor_type = truth.anchor_type;
            match.anchor_index = truth.anchor_index;
            match.lead = truth.lead;
            match.kind = truth.kind;
            match.ground_truth_time_seconds = truth.time_seconds;
            match.prediction_time_seconds = prediction.time_seconds;
            match.error_seconds = prediction.time_seconds - truth.time_seconds;
            match.within_tolerance = std::fabs(match.error_seconds) <= options.tolerance_seconds + 1e-15;
            fresh.matches.push_back(match);
        }
        for (std::size_t t = 0; t < ground_truth.size(); ++t)
            if (ground_truth[t].status == delineation_truth_present && !truth_used[t]) fresh.missing_events.push_back(ground_truth[t]);
        for (std::size_t p = 0; p < predictions.size(); ++p)
        {
            if (prediction_used[p]) continue;
            std::string reason;
            if (!in_scope_event(predictions[p], scope)) reason = "outside_scope";
            else
            {
                for (std::size_t t = 0; t < ground_truth.size(); ++t)
                    if (ground_truth[t].status == delineation_truth_not_evaluable && same_group(ground_truth[t], predictions[p]) && predictions[p].time_seconds >= ground_truth[t].evaluation_start_seconds && predictions[p].time_seconds <= ground_truth[t].evaluation_end_seconds) { reason = "truth_not_evaluable"; break; }
            }
            if (!reason.empty())
            {
                delineation_excluded_prediction excluded;
                excluded.event = predictions[p];
                excluded.reason = reason;
                fresh.excluded_predictions.push_back(excluded);
                prediction_excluded[p] = true;
            }
            else fresh.unexpected_events.push_back(predictions[p]);
        }
        std::vector<delineation_event> scored_predictions;
        for (std::size_t p = 0; p < predictions.size(); ++p) if (!prediction_excluded[p]) scored_predictions.push_back(predictions[p]);
        fresh.total = group_metrics(fresh, scored_predictions, "", "");
        for (int kind = 0; kind < static_cast<int>(delineation_kind_count); ++kind)
        {
            delineation_score_group group;
            group.kind = delineation_kind_name(static_cast<delineation_kind>(kind));
            group.metrics = group_metrics(fresh, scored_predictions, group.kind, "");
            fresh.kinds.push_back(group);
        }
        std::vector<std::string> leads = scope.leads;
        std::sort(leads.begin(), leads.end(), lead_less);
        for (std::size_t lead = 0; lead < leads.size(); ++lead)
        {
            delineation_score_group lead_group;
            lead_group.lead = leads[lead];
            lead_group.metrics = group_metrics(fresh, scored_predictions, "", lead_group.lead);
            fresh.leads.push_back(lead_group);
            for (int kind = 0; kind < static_cast<int>(delineation_kind_count); ++kind)
            {
                delineation_score_group group;
                group.kind = delineation_kind_name(static_cast<delineation_kind>(kind));
                group.lead = leads[lead];
                group.metrics = group_metrics(fresh, scored_predictions, group.kind, group.lead);
                fresh.kind_leads.push_back(group);
            }
        }
        fresh.success = true;
        result = fresh;
        return true;
    }

    bool score_delineation_output_to_render(const ecg_render_bundle& render, const delineation_output_document& predictions, const delineation_evaluation_scope& scope, const delineation_score_options& options, delineation_score_result& result)
    {
        delineation_io_result validation;
        if (!write_delineation_point_events(predictions, validation))
        {
            delineation_score_result fresh;
            fresh.messages.push_back(validation.messages.empty() ? "invalid delineation point-event document" : validation.messages[0].message);
            result = fresh;
            return false;
        }
        std::vector<delineation_truth_point> truth;
        std::vector<std::string> messages;
        if (!delineation_ground_truth_from_render(render, scope, truth, messages))
        {
            delineation_score_result fresh;
            fresh.messages = messages;
            result = fresh;
            return false;
        }
        return score_delineation_events(render.document.duration_seconds, truth, predictions.events, scope, options, result);
    }

    std::string delineation_score_result_json(const ecg_render_bundle& render, const delineation_evaluation_scope& scope, const delineation_score_result& result)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10) << "{\"schema_version\":2,\"score_type\":\"ecg_delineation_qa\",\"target\":\"ecg_delineation\",\"scenario\":{\"scenario_id\":" << json_string(render.document.scenario_id)
               << ",\"duration_seconds\":" << result.record_duration_seconds << ",\"render_identity\":" << json_string(render.render_identity) << "},\"scope\":{\"mode\":" << json_string(scope.windows.empty() ? "all_record" : "selected_windows") << ",\"leads\":[";
        for (std::size_t i = 0; i < scope.leads.size(); ++i) output << (i ? "," : "") << json_string(scope.leads[i]);
        output << "],\"windows\":[";
        for (std::size_t i = 0; i < scope.windows.size(); ++i) output << (i ? "," : "") << "{\"start_seconds\":" << scope.windows[i].start_seconds << ",\"end_seconds\":" << scope.windows[i].end_seconds << '}';
        output << "]},\"options\":{\"tolerance_seconds\":" << result.tolerance_seconds << ",\"pairing_window_seconds\":" << result.pairing_window_seconds << "},\"overall\":";
        write_metrics(output, result.total);
        output << ",\"by_kind\":[";
        for (std::size_t i = 0; i < result.kinds.size(); ++i) { if (i) output << ','; write_group(output, result.kinds[i]); }
        output << "],\"by_lead\":[";
        for (std::size_t i = 0; i < result.leads.size(); ++i) { if (i) output << ','; write_group(output, result.leads[i]); }
        output << "],\"by_kind_lead\":[";
        for (std::size_t i = 0; i < result.kind_leads.size(); ++i) { if (i) output << ','; write_group(output, result.kind_leads[i]); }
        output << "],\"truth\":[";
        for (std::size_t i = 0; i < result.truth.size(); ++i) { if (i) output << ','; write_truth(output, result.truth[i]); }
        output << "],\"matches\":[";
        for (std::size_t i = 0; i < result.matches.size(); ++i)
        {
            const delineation_score_match& match = result.matches[i];
            output << (i ? "," : "") << "{\"anchor_type\":" << json_string(delineation_anchor_type_name(match.anchor_type)) << ",\"anchor_index\":" << json_string(uint64_text(match.anchor_index)) << ",\"lead\":" << json_string(match.lead)
                   << ",\"kind\":" << json_string(delineation_kind_name(match.kind)) << ",\"ground_truth_time_seconds\":" << match.ground_truth_time_seconds << ",\"prediction_time_seconds\":" << match.prediction_time_seconds << ",\"error_seconds\":" << match.error_seconds << ",\"within_tolerance\":" << (match.within_tolerance ? "true" : "false") << '}';
        }
        output << "],\"missing_events\":[";
        for (std::size_t i = 0; i < result.missing_events.size(); ++i) { if (i) output << ','; write_truth(output, result.missing_events[i]); }
        output << "],\"unexpected_events\":[";
        for (std::size_t i = 0; i < result.unexpected_events.size(); ++i) { if (i) output << ','; write_event(output, result.unexpected_events[i]); }
        output << "],\"excluded_predictions\":[";
        for (std::size_t i = 0; i < result.excluded_predictions.size(); ++i) { if (i) output << ','; output << "{\"reason\":" << json_string(result.excluded_predictions[i].reason) << ",\"event\":"; write_event(output, result.excluded_predictions[i].event); output << '}'; }
        output << "],\"notes\":[\"Predictions are paired by lead, kind, and time; generator anchor identities are truth/report metadata only.\",\"Synthetic engineering QA evidence; not a clinical validation claim.\"]}";
        return output.str();
    }

    std::string delineation_score_result_csv(const delineation_score_result& result)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << "group_type,kind,lead,present_truth,absent_truth,not_evaluable_truth,predictions,excluded_predictions,paired,within_tolerance,missing,unexpected,out_of_tolerance,sensitivity,precision,f1,mae_seconds,p95_seconds\n";
        const delineation_score_metrics& m = result.total;
        output << "overall,,," << m.ground_truth_count << ',' << m.absent_truth_count << ',' << m.not_evaluable_truth_count << ',' << m.prediction_count << ',' << m.excluded_prediction_count << ',' << m.paired_count << ',' << m.within_tolerance_count << ',' << m.missing_prediction_count << ',' << m.unexpected_prediction_count << ',' << m.out_of_tolerance_count << ',' << csv_number(m.sensitivity, m.ground_truth_count > 0u) << ',' << csv_number(m.positive_predictive_value, m.prediction_count > 0u) << ',' << csv_number(m.f1_score, m.ground_truth_count + m.prediction_count > 0u) << ',' << csv_number(m.mean_absolute_error_seconds, m.paired_count > 0u) << ',' << csv_number(m.p95_absolute_error_seconds, m.paired_count > 0u) << '\n';
        for (std::size_t i = 0; i < result.kind_leads.size(); ++i)
        {
            const delineation_score_group& g = result.kind_leads[i];
            const delineation_score_metrics& x = g.metrics;
            output << "kind_lead," << g.kind << ',' << g.lead << ',' << x.ground_truth_count << ',' << x.absent_truth_count << ',' << x.not_evaluable_truth_count << ',' << x.prediction_count << ',' << x.excluded_prediction_count << ',' << x.paired_count << ',' << x.within_tolerance_count << ',' << x.missing_prediction_count << ',' << x.unexpected_prediction_count << ',' << x.out_of_tolerance_count << ',' << csv_number(x.sensitivity, x.ground_truth_count > 0u) << ',' << csv_number(x.positive_predictive_value, x.prediction_count > 0u) << ',' << csv_number(x.f1_score, x.ground_truth_count + x.prediction_count > 0u) << ',' << csv_number(x.mean_absolute_error_seconds, x.paired_count > 0u) << ',' << csv_number(x.p95_absolute_error_seconds, x.paired_count > 0u) << '\n';
        }
        return output.str();
    }

    std::string delineation_score_report_html(const ecg_render_bundle& render, const delineation_score_result& result)
    {
        std::ostringstream output;
        output << "<!doctype html><html><head><meta charset=\"utf-8\"><title>ECG delineation scoring</title><style>body{font-family:Arial,sans-serif;margin:24px;color:#20252b}table{border-collapse:collapse}th,td{border:1px solid #c9ced4;padding:6px 9px;text-align:right}th:first-child,td:first-child{text-align:left}</style></head><body><h1>ECG delineation scoring</h1><p>Scenario: " << html_text(render.document.scenario_id) << "</p><table><tr><th>Metric</th><th>Value</th></tr>"
               << "<tr><td>Present truth</td><td>" << result.total.ground_truth_count << "</td></tr><tr><td>Absent truth</td><td>" << result.total.absent_truth_count << "</td></tr><tr><td>Not evaluable truth</td><td>" << result.total.not_evaluable_truth_count << "</td></tr><tr><td>Predictions</td><td>" << result.total.prediction_count << "</td></tr><tr><td>Excluded predictions</td><td>" << result.total.excluded_prediction_count << "</td></tr><tr><td>F1</td><td>" << csv_number(result.total.f1_score, result.total.ground_truth_count + result.total.prediction_count > 0u) << "</td></tr><tr><td>MAE (ms)</td><td>" << csv_number(1000.0 * result.total.mean_absolute_error_seconds, result.total.paired_count > 0u) << "</td></tr></table><p>Predictions contain only lead, kind and time. Atrial and ventricular anchors remain ground-truth audit metadata. Synthetic engineering QA only; not a clinical validation claim.</p></body></html>";
        return output.str();
    }
}
