#include "measurement_scoring.h"

#include "ecg_scenario.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>

namespace
{
    struct match_candidate
    {
        unsigned int truth_index;
        unsigned int prediction_index;
        bool exact_beat;
        double distance;
    };

    bool finite_value(double value)
    {
        return std::isfinite(value) != 0;
    }

    std::string json_string(const std::string& value)
    {
        static const char hex[] = "0123456789abcdef";
        std::string output("\"");
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            const unsigned char ch = static_cast<unsigned char>(value[i]);
            switch (ch)
            {
            case '"': output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if (ch < 0x20u)
                {
                    output += "\\u00";
                    output.push_back(hex[ch >> 4]);
                    output.push_back(hex[ch & 0x0fu]);
                }
                else output.push_back(static_cast<char>(ch));
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

    std::string lower_ascii(const std::string& value)
    {
        std::string output;
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            const unsigned char ch = static_cast<unsigned char>(value[i]);
            if (ch >= 'A' && ch <= 'Z') output.push_back(static_cast<char>(ch - 'A' + 'a'));
            else if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) output.push_back(static_cast<char>(ch));
            else output.push_back('_');
        }
        return output;
    }

    const char* assertion_code_name(signal_synth::ecg_phenotype_assertion_code code)
    {
        static const char* names[] = {
            "rhythm", "heart_rate", "rr_variability", "p_wave_presence", "atrial_ventricular_ratio", "ectopic_origin", "ectopic_cadence", "premature_coupling",
            "pr_interval", "dropped_atrial_events", "av_pattern", "ventricular_escape", "qrs_duration", "terminal_v1_polarity", "qtc_interval", "pacing",
            "q_wave_amplitude", "q_wave_duration", "q_wave_lead_count", "low_qrs_voltage", "high_qrs_voltage", "left_ventricular_voltage", "right_precordial_rs_ratio", "septal_qrs_voltage",
            "p_wave_duration", "p_wave_amplitude", "posterior_reciprocal_r_amplitude", "posterior_reciprocal_lead_count", "injury_st_deviation", "injury_st_lead_count",
            "st_deviation", "st_lead_count", "st_slope", "t_amplitude", "t_lead_count", "t_polarity_dispersion", "t_duration", "frontal_axis",
            "lateral_qrs_polarity", "inferior_qrs_polarity", "delta_wave", "complete_bbb_exclusion", "episode_coverage"
        };
        const unsigned int index = static_cast<unsigned int>(code);
        return index < sizeof(names) / sizeof(names[0]) ? names[index] : "unknown";
    }

    const char* qt_formula_name(signal_synth::ecg_qt_adaptation_model model)
    {
        switch (model)
        {
        case signal_synth::ecg_qt_adaptation_bazett: return "bazett";
        case signal_synth::ecg_qt_adaptation_fridericia: return "fridericia";
        case signal_synth::ecg_qt_adaptation_framingham: return "framingham";
        case signal_synth::ecg_qt_adaptation_hodges: return "hodges";
        case signal_synth::ecg_qt_adaptation_fixed: return "fixed";
        }
        return "fixed";
    }

    signal_synth::measurement_truth make_truth(const char* name, const char* unit, signal_synth::measurement_status status, signal_synth::measurement_scope scope, double value, double absolute_tolerance, double relative_tolerance, const char* reason)
    {
        signal_synth::measurement_truth output;
        output.measurement.name = name;
        output.measurement.unit = unit;
        output.measurement.status = status;
        output.measurement.scope = scope;
        output.measurement.has_value = status == signal_synth::measurement_valid;
        output.measurement.value = output.measurement.has_value ? value : 0.0;
        output.absolute_tolerance = absolute_tolerance;
        output.relative_tolerance_percent = relative_tolerance;
        output.reason = reason ? reason : "";
        return output;
    }

    void set_beat_anchor(signal_synth::measurement_truth& truth, const signal_synth::clinical_beat_annotation& beat)
    {
        truth.measurement.has_beat_index = true;
        truth.measurement.beat_index = beat.beat_index;
        truth.measurement.has_time_seconds = true;
        truth.measurement.time_seconds = beat.r_peak_time_seconds;
    }

    void add_timing_truth(const signal_synth::ecg_render_bundle& render, std::vector<signal_synth::measurement_truth>& output)
    {
        const signal_synth::clinical_beat_annotation* beats = render.record.beats();
        const char* formula = qt_formula_name(render.resolved_document.ecg.qt_adaptation_model());
        for (unsigned int i = 0; beats && i < render.record.beat_count(); ++i)
        {
            const signal_synth::clinical_beat_annotation& beat = beats[i];
            signal_synth::measurement_truth pr = make_truth("pr_interval", "s", beat.linked_atrial_index >= 0 && beat.p_present ? signal_synth::measurement_valid : signal_synth::measurement_absent, signal_synth::measurement_beat, beat.pr_interval_seconds, 0.020, 5.0, beat.p_present ? "" : "atrial_wave_absent");
            set_beat_anchor(pr, beat);
            output.push_back(pr);
            signal_synth::measurement_truth qrs = make_truth("qrs_duration", "s", beat.qrs_present ? signal_synth::measurement_valid : signal_synth::measurement_absent, signal_synth::measurement_beat, beat.qrs_duration_seconds, 0.020, 5.0, beat.qrs_present ? "" : "qrs_absent");
            set_beat_anchor(qrs, beat);
            output.push_back(qrs);
            const signal_synth::measurement_status repolarization_status = !beat.qrs_present ? signal_synth::measurement_absent : beat.t_present ? signal_synth::measurement_valid : signal_synth::measurement_absent;
            signal_synth::measurement_truth qt = make_truth("qt_interval", "s", repolarization_status, signal_synth::measurement_beat, beat.qt_interval_seconds, 0.025, 5.0, beat.t_present ? "" : "t_wave_absent");
            set_beat_anchor(qt, beat);
            output.push_back(qt);
            signal_synth::measurement_truth qtc = make_truth("qtc_interval", "s", repolarization_status, signal_synth::measurement_beat, beat.qtc_interval_seconds, 0.025, 5.0, beat.t_present ? "" : "t_wave_absent");
            set_beat_anchor(qtc, beat);
            qtc.measurement.formula = formula;
            output.push_back(qtc);
        }
    }

    void add_lead_morphology_truth(const signal_synth::ecg_render_bundle& render, std::vector<signal_synth::measurement_truth>& output)
    {
        const signal_synth::ecg_lead_morphology* entries = render.morphology.entries();
        for (unsigned int i = 0; entries && i < render.morphology.entry_count(); ++i)
        {
            const signal_synth::ecg_lead_morphology& entry = entries[i];
            const signal_synth::clinical_beat_annotation* beat = 0;
            for (unsigned int b = 0; b < render.record.beat_count(); ++b)
                if (render.record.beats()[b].beat_index == entry.beat_index) { beat = &render.record.beats()[b]; break; }
            if (!beat || entry.lead_index >= render.record.lead_count())
                continue;
            const std::string lead = render.record.lead_name(entry.lead_index);
            signal_synth::measurement_truth stj = make_truth("st_j_level", "mV", beat->qrs_present ? signal_synth::measurement_valid : signal_synth::measurement_absent, signal_synth::measurement_beat_lead, entry.st_j_mv, 0.050, 10.0, beat->qrs_present ? "" : "qrs_absent");
            set_beat_anchor(stj, *beat); stj.measurement.channel = lead; output.push_back(stj);
            signal_synth::measurement_truth stj60 = make_truth("st_j60_level", "mV", beat->qrs_present ? signal_synth::measurement_valid : signal_synth::measurement_absent, signal_synth::measurement_beat_lead, entry.st_j60_mv, 0.050, 10.0, beat->qrs_present ? "" : "qrs_absent");
            set_beat_anchor(stj60, *beat); stj60.measurement.channel = lead; output.push_back(stj60);
            signal_synth::measurement_truth slope = make_truth("st_slope", "mV/s", beat->qrs_present ? signal_synth::measurement_valid : signal_synth::measurement_absent, signal_synth::measurement_beat_lead, (entry.st_j60_mv - entry.st_j_mv) / 0.060, 0.25, 10.0, beat->qrs_present ? "" : "qrs_absent");
            set_beat_anchor(slope, *beat); slope.measurement.channel = lead; output.push_back(slope);
            const signal_synth::measurement_status t_status = !beat->t_present ? signal_synth::measurement_absent : entry.t_present ? signal_synth::measurement_valid : signal_synth::measurement_not_evaluable;
            signal_synth::measurement_truth t = make_truth("t_amplitude", "mV", t_status, signal_synth::measurement_beat_lead, entry.t_amplitude_mv, 0.050, 10.0, !beat->t_present ? "t_wave_absent" : entry.t_present ? "" : "t_wave_not_measurable_in_lead");
            set_beat_anchor(t, *beat); t.measurement.channel = lead; output.push_back(t);
        }
    }

    bool mean_fiducial_amplitude(const signal_synth::clinical_ecg_record& record, unsigned int lead, signal_synth::clinical_fiducial_kind kind, double& output)
    {
        double sum = 0.0;
        unsigned int count = 0;
        const signal_synth::clinical_fiducial_annotation* fiducials = record.fiducials();
        for (unsigned int i = 0; fiducials && i < record.fiducial_count(); ++i)
        {
            const signal_synth::clinical_fiducial_annotation& item = fiducials[i];
            if (item.lead_index == static_cast<int>(lead) && item.kind == kind && item.source == signal_synth::clinical_fiducial_lead_measurement && item.present)
            {
                sum += item.amplitude_mv;
                ++count;
            }
        }
        output = count ? sum / count : 0.0;
        return count > 0;
    }

    bool frontal_axis(const signal_synth::clinical_ecg_record& record, signal_synth::clinical_fiducial_kind kind, double& output)
    {
        double lead_i = 0.0;
        double lead_ii = 0.0;
        if (!mean_fiducial_amplitude(record, signal_synth::clinical_lead_i, kind, lead_i) || !mean_fiducial_amplitude(record, signal_synth::clinical_lead_ii, kind, lead_ii))
            return false;
        const double y = (lead_ii - 0.5 * lead_i) / 0.8660254037844386;
        if (std::fabs(lead_i) + std::fabs(y) < 1e-12)
            return false;
        output = std::atan2(y, lead_i) * 180.0 / 3.14159265358979323846;
        return true;
    }

    void add_axis_truth(const signal_synth::ecg_render_bundle& render, std::vector<signal_synth::measurement_truth>& output)
    {
        struct axis_definition { const char* name; signal_synth::clinical_fiducial_kind kind; };
        static const axis_definition axes[] = {{"p_axis", signal_synth::clinical_p_peak}, {"qrs_axis", signal_synth::clinical_r_peak}, {"t_axis", signal_synth::clinical_t_peak}};
        for (std::size_t i = 0; i < sizeof(axes) / sizeof(axes[0]); ++i)
        {
            double value = 0.0;
            const bool available = frontal_axis(render.record, axes[i].kind, value);
            signal_synth::measurement_truth truth = make_truth(axes[i].name, "deg", available ? signal_synth::measurement_valid : signal_synth::measurement_not_evaluable, signal_synth::measurement_record, value, 10.0, 0.0, available ? "" : "axis_not_measurable");
            truth.error_model = signal_synth::measurement_error_circular_degrees;
            output.push_back(truth);
        }
    }

    void tolerance_for_unit(const std::string& unit, double& absolute, double& relative)
    {
        relative = 10.0;
        if (unit == "s") absolute = 0.020;
        else if (unit == "mV") absolute = 0.050;
        else if (unit == "mV/s") absolute = 0.25;
        else if (unit == "deg") { absolute = 10.0; relative = 0.0; }
        else if (unit == "bpm") absolute = 2.0;
        else if (unit == "%") absolute = 2.0;
        else if (unit == "count" || unit == "bool") { absolute = 0.5; relative = 0.0; }
        else { absolute = 0.10; relative = 10.0; }
    }

    void add_assertion_truth(const signal_synth::ecg_render_bundle& render, std::vector<signal_synth::measurement_truth>& output)
    {
        for (unsigned int i = 0; i < render.scenario_report.assertion_count(); ++i)
        {
            const signal_synth::ecg_condition_info* condition = signal_synth::find_ecg_condition(render.scenario_report.assertion_condition(i));
            const signal_synth::ecg_phenotype_assertion_code code = render.scenario_report.assertion_code(i);
            std::ostringstream name;
            name << "assertion." << lower_ascii(condition ? condition->scp_code : "unknown") << '.' << assertion_code_name(code) << '.' << i;
            double absolute = 0.0;
            double relative = 0.0;
            tolerance_for_unit(render.scenario_report.assertion_unit(i), absolute, relative);
            const signal_synth::ecg_phenotype_assertion_status status = render.scenario_report.assertion_status(i);
            signal_synth::measurement_truth truth = make_truth(name.str().c_str(), render.scenario_report.assertion_unit(i), status == signal_synth::ecg_assertion_not_evaluated ? signal_synth::measurement_not_evaluable : signal_synth::measurement_valid, signal_synth::measurement_record, render.scenario_report.assertion_measured_value(i), absolute, relative, status == signal_synth::ecg_assertion_not_evaluated ? "phenotype_assertion_not_evaluable" : "");
            truth.has_expected_range = true;
            truth.expected_minimum = render.scenario_report.assertion_minimum(i);
            truth.expected_maximum = render.scenario_report.assertion_maximum(i);
            if (truth.measurement.unit == "deg") truth.error_model = signal_synth::measurement_error_circular_degrees;
            output.push_back(truth);
        }
    }

    const signal_synth::ppg_annotation* measured_ppg_peak(const signal_synth::ppg_record& ppg, unsigned long long beat_index)
    {
        const signal_synth::ppg_annotation* annotations = ppg.annotations();
        for (unsigned int i = 0; annotations && i < ppg.annotation_count(); ++i)
            if (annotations[i].ecg_beat_index == beat_index && annotations[i].kind == signal_synth::ppg_systolic_peak && annotations[i].source == signal_synth::ppg_fiducial_measurement)
                return &annotations[i];
        return 0;
    }

    const signal_synth::wearable_alignment_annotation* wearable_alignment(const signal_synth::ecg_render_bundle& render, unsigned long long beat_index)
    {
        for (std::size_t i = 0; i < render.wearable.alignments.size(); ++i)
            if (render.wearable.alignments[i].ecg_beat_index == beat_index)
                return &render.wearable.alignments[i];
        return 0;
    }

    void add_device_alignment_truth(const signal_synth::ppg_pulse_annotation& pulse, const signal_synth::wearable_alignment_annotation& alignment, std::vector<signal_synth::measurement_truth>& output)
    {
        const bool onset_received = alignment.has_observed_onset_device_delta && alignment.ecg_r.received && alignment.ppg_onset.received;
        const bool onset_absent = pulse.intentionally_missing || pulse.state == signal_synth::ppg_pulse_missing;
        const signal_synth::measurement_status onset_status = onset_received ? signal_synth::measurement_valid : onset_absent ? signal_synth::measurement_absent : signal_synth::measurement_not_evaluable;
        const char* onset_reason = onset_received ? "" : onset_absent ? "pulse_intentionally_missing" : alignment.has_observed_onset_device_delta ? "device_sample_dropped" : "pulse_out_of_record";
        signal_synth::measurement_truth onset_delta = make_truth("device_timestamp_onset_delta", "s", onset_status, signal_synth::measurement_paired_signal, alignment.observed_onset_device_delta_seconds, 0.020, 5.0, onset_reason);
        onset_delta.measurement.has_beat_index = true; onset_delta.measurement.beat_index = pulse.ecg_beat_index;
        onset_delta.measurement.has_time_seconds = true; onset_delta.measurement.time_seconds = pulse.ecg_r_time_seconds;
        onset_delta.measurement.channel = "wearable_ecg_r_to_ppg_onset";
        output.push_back(onset_delta);
        signal_synth::measurement_truth onset_error = make_truth("clock_and_sampling_onset_error", "s", onset_status, signal_synth::measurement_paired_signal, alignment.onset_observed_minus_physiological_seconds, 0.010, 0.0, onset_reason);
        onset_error.measurement = onset_delta.measurement; onset_error.measurement.name = "clock_and_sampling_onset_error";
        output.push_back(onset_error);

        const bool peak_received = alignment.has_observed_peak_device_delta && alignment.ecg_r.received && alignment.ppg_peak.received;
        const signal_synth::measurement_status peak_status = peak_received ? signal_synth::measurement_valid : onset_absent ? signal_synth::measurement_absent : signal_synth::measurement_not_evaluable;
        const char* peak_reason = peak_received ? "" : onset_absent ? "pulse_intentionally_missing" : alignment.has_observed_peak_device_delta ? "device_sample_dropped" : "ppg_peak_not_measurable";
        signal_synth::measurement_truth peak_delta = make_truth("device_timestamp_peak_delta", "s", peak_status, signal_synth::measurement_paired_signal, alignment.observed_peak_device_delta_seconds, 0.025, 5.0, peak_reason);
        peak_delta.measurement.has_beat_index = true; peak_delta.measurement.beat_index = pulse.ecg_beat_index;
        peak_delta.measurement.has_time_seconds = true; peak_delta.measurement.time_seconds = pulse.ecg_r_time_seconds;
        peak_delta.measurement.channel = "wearable_ecg_r_to_ppg_peak";
        output.push_back(peak_delta);
        signal_synth::measurement_truth peak_error = make_truth("clock_and_sampling_peak_error", "s", peak_status, signal_synth::measurement_paired_signal, alignment.peak_observed_minus_physiological_seconds, 0.010, 0.0, peak_reason);
        peak_error.measurement = peak_delta.measurement; peak_error.measurement.name = "clock_and_sampling_peak_error";
        output.push_back(peak_error);
    }

    void add_alignment_truth(const signal_synth::ecg_render_bundle& render, std::vector<signal_synth::measurement_truth>& output)
    {
        const signal_synth::ppg_pulse_annotation* pulses = render.ppg.pulses();
        for (unsigned int i = 0; pulses && i < render.ppg.pulse_count(); ++i)
        {
            const signal_synth::ppg_pulse_annotation& pulse = pulses[i];
            signal_synth::measurement_status onset_status = signal_synth::measurement_valid;
            const char* onset_reason = "";
            if (pulse.intentionally_missing || pulse.state == signal_synth::ppg_pulse_missing) { onset_status = signal_synth::measurement_absent; onset_reason = "pulse_intentionally_missing"; }
            else if (!pulse.generated || pulse.state == signal_synth::ppg_pulse_out_of_record) { onset_status = signal_synth::measurement_not_evaluable; onset_reason = "pulse_out_of_record"; }
            signal_synth::measurement_truth onset = make_truth("pulse_transit_time", "s", onset_status, signal_synth::measurement_paired_signal, pulse.pulse_delay_seconds, 0.020, 5.0, onset_reason);
            onset.measurement.has_beat_index = true; onset.measurement.beat_index = pulse.ecg_beat_index;
            onset.measurement.has_time_seconds = true; onset.measurement.time_seconds = pulse.ecg_r_time_seconds;
            onset.measurement.channel = "ecg_r_to_ppg_green_onset";
            output.push_back(onset);
            const signal_synth::ppg_annotation* peak_annotation = measured_ppg_peak(render.ppg, pulse.ecg_beat_index);
            signal_synth::measurement_status peak_status = peak_annotation ? signal_synth::measurement_valid : onset_status == signal_synth::measurement_absent ? signal_synth::measurement_absent : signal_synth::measurement_not_evaluable;
            signal_synth::measurement_truth peak = make_truth("ecg_ppg_peak_delay", "s", peak_status, signal_synth::measurement_paired_signal, peak_annotation ? peak_annotation->time_seconds - pulse.ecg_r_time_seconds : 0.0, 0.025, 5.0, peak_annotation ? "" : peak_status == signal_synth::measurement_absent ? "pulse_intentionally_missing" : "ppg_peak_not_measurable");
            peak.measurement.has_beat_index = true; peak.measurement.beat_index = pulse.ecg_beat_index;
            peak.measurement.has_time_seconds = true; peak.measurement.time_seconds = pulse.ecg_r_time_seconds;
            peak.measurement.channel = "ecg_r_to_ppg_green_peak";
            output.push_back(peak);
            const signal_synth::wearable_alignment_annotation* device = wearable_alignment(render, pulse.ecg_beat_index);
            if (device)
                add_device_alignment_truth(pulse, *device, output);
        }
    }

    void add_optical_truth(const signal_synth::ecg_render_bundle& render, std::vector<signal_synth::measurement_truth>& output)
    {
        const signal_synth::ppg_optical_pulse_state* states = render.ppg.optical_states();
        const std::string calibration = render.ppg.optical_config().calibration_id;
        for (unsigned int i = 0; states && i < render.ppg.optical_state_count(); ++i)
        {
            const signal_synth::ppg_optical_pulse_state& state = states[i];
            const signal_synth::measurement_status status = state.valid_for_measurement ? signal_synth::measurement_valid : state.generated ? signal_synth::measurement_not_evaluable : signal_synth::measurement_absent;
            const char* reason = state.valid_for_measurement ? "" : state.generated ? "optical_calibration_or_perfusion_not_evaluable" : "pulse_not_generated";
            struct value_definition { const char* name; const char* unit; double value; double tolerance; };
            const value_definition values[] = {
                {"spo2_target", "%", state.spo2_percent, 1.0},
                {"ratio_of_ratios", "ratio", state.ratio_of_ratios, 0.02},
                {"red_perfusion_index", "%", state.red_perfusion_index_percent, 0.10},
                {"infrared_perfusion_index", "%", state.infrared_perfusion_index_percent, 0.10},
                {"red_ac_dc_ratio", "ratio", state.red_perfusion_index_percent / 100.0, 0.001},
                {"infrared_ac_dc_ratio", "ratio", state.infrared_perfusion_index_percent / 100.0, 0.001}
            };
            for (unsigned int value = 0; value < sizeof(values) / sizeof(values[0]); ++value)
            {
                signal_synth::measurement_truth truth = make_truth(values[value].name, values[value].unit, status, signal_synth::measurement_paired_signal, values[value].value, values[value].tolerance, 2.0, reason);
                truth.measurement.has_beat_index = true; truth.measurement.beat_index = state.ecg_beat_index;
                truth.measurement.has_time_seconds = true; truth.measurement.time_seconds = state.time_seconds;
                truth.measurement.channel = "ppg_red_infrared";
                if (value < 2u) truth.measurement.formula = calibration;
                output.push_back(truth);
            }
        }
    }

    void add_prv_metric(std::vector<signal_synth::measurement_truth>& output, const char* name, const char* unit, double value, bool valid, double absolute_tolerance)
    {
        output.push_back(make_truth(name, unit, valid ? signal_synth::measurement_valid : signal_synth::measurement_undefined, signal_synth::measurement_record, value, absolute_tolerance, 10.0, valid ? "" : "insufficient_valid_pulse_intervals"));
    }

    void add_prv_truth(const signal_synth::ecg_render_bundle& render, std::vector<signal_synth::measurement_truth>& output)
    {
        const signal_synth::hrv_analysis_result& prv = render.cardiorespiratory.prv;
        const signal_synth::hrv_metric_summary& metrics = prv.metrics;
        const bool any = metrics.accepted_interval_count > 0u;
        const bool successive = metrics.accepted_interval_count > 1u;
        bool spectral = false;
        double first = 0.0, last = 0.0;
        for (std::size_t i = 0; i < prv.intervals.size(); ++i) if (!prv.intervals[i].excluded) { if (!spectral) first = prv.intervals[i].beat_time_seconds; last = prv.intervals[i].beat_time_seconds; spectral = true; }
        spectral = spectral && last - first >= 60.0 && metrics.accepted_interval_count >= 8u;
        add_prv_metric(output, "mean_pulse_interval_seconds", "s", metrics.mean_rr_seconds, any, 0.010);
        add_prv_metric(output, "mean_pulse_rate_bpm", "bpm", metrics.mean_heart_rate_bpm, any, 1.0);
        add_prv_metric(output, "prv_sdnn_seconds", "s", metrics.sdnn_seconds, any, 0.010);
        add_prv_metric(output, "prv_rmssd_seconds", "s", metrics.rmssd_seconds, successive, 0.010);
        add_prv_metric(output, "prv_pnn50_percent", "%", metrics.pnn50_percent, successive, 2.0);
        add_prv_metric(output, "prv_sd1_seconds", "s", metrics.sd1_seconds, successive, 0.010);
        add_prv_metric(output, "prv_sd2_seconds", "s", metrics.sd2_seconds, successive, 0.010);
        add_prv_metric(output, "prv_sd1_sd2_ratio", "ratio", metrics.sd1_sd2_ratio, successive, 0.10);
        add_prv_metric(output, "prv_lf_power_seconds2", "s2", metrics.lf_power_seconds2, spectral, 0.0005);
        add_prv_metric(output, "prv_hf_power_seconds2", "s2", metrics.hf_power_seconds2, spectral, 0.0005);
        add_prv_metric(output, "prv_lf_hf_ratio", "ratio", metrics.lf_hf_ratio, spectral && metrics.hf_power_seconds2 > 0.0, 0.20);
        add_prv_metric(output, "prv_total_power_seconds2", "s2", metrics.total_power_seconds2, spectral, 0.001);
        for (std::size_t i = 0; i < prv.intervals.size(); ++i)
        {
            const signal_synth::hrv_rr_interval& interval = prv.intervals[i];
            signal_synth::measurement_truth truth = make_truth("pulse_interval", "s", interval.excluded ? signal_synth::measurement_not_evaluable : signal_synth::measurement_valid, signal_synth::measurement_paired_signal, interval.rr_seconds, 0.020, 5.0, interval.excluded ? interval.artifact_overlap ? "ppg_artifact_overlap" : interval.ectopic ? "arrhythmia_linked_pulse" : "missing_or_low_perfusion_pulse" : "");
            truth.measurement.has_time_seconds = true; truth.measurement.time_seconds = interval.beat_time_seconds;
            truth.measurement.has_beat_index = true; truth.measurement.beat_index = interval.beat_index;
            truth.measurement.channel = "ppg_green";
            output.push_back(truth);
        }
    }

    void add_respiratory_rate_truth(const signal_synth::ecg_render_bundle& render, std::vector<signal_synth::measurement_truth>& output)
    {
        const signal_synth::cardiorespiratory_analysis_result& analysis = render.cardiorespiratory;
        output.push_back(make_truth("respiratory_rate_mean", "bpm", signal_synth::measurement_valid, signal_synth::measurement_record, analysis.respiratory_rate_bpm, 1.0, 5.0, ""));
        for (std::size_t i = 0; i < analysis.respiration.size(); i += analysis.respiration_sample_rate_hz)
        {
            signal_synth::measurement_truth truth = make_truth("respiratory_rate", "bpm", signal_synth::measurement_valid, signal_synth::measurement_paired_signal, analysis.respiration[i].respiratory_rate_bpm, 1.0, 5.0, "");
            truth.measurement.has_time_seconds = true; truth.measurement.time_seconds = analysis.respiration[i].time_seconds;
            truth.measurement.channel = "respiration_reference";
            output.push_back(truth);
        }
    }

    bool same_descriptor(const signal_synth::measurement_value& truth, const signal_synth::measurement_value& prediction)
    {
        return truth.name == prediction.name && truth.unit == prediction.unit && truth.scope == prediction.scope && truth.channel == prediction.channel && truth.formula == prediction.formula;
    }

    bool candidate_less(const match_candidate& left, const match_candidate& right)
    {
        if (left.exact_beat != right.exact_beat) return left.exact_beat;
        if (left.distance != right.distance) return left.distance < right.distance;
        if (left.truth_index != right.truth_index) return left.truth_index < right.truth_index;
        return left.prediction_index < right.prediction_index;
    }

    double signed_error(const signal_synth::measurement_truth& truth, const signal_synth::measurement_value& prediction)
    {
        double error = prediction.value - truth.measurement.value;
        if (truth.error_model == signal_synth::measurement_error_circular_degrees)
        {
            error = std::fmod(error + 180.0, 360.0);
            if (error < 0.0) error += 360.0;
            error -= 180.0;
        }
        return error;
    }

    void finalize_metrics(signal_synth::measurement_score_metrics& metrics, const std::vector<double>& signed_errors, const std::vector<double>& absolute_errors)
    {
        if (metrics.numeric_pair_count) metrics.tolerance_pass_fraction = static_cast<double>(metrics.tolerance_pass_count) / metrics.numeric_pair_count;
        if (metrics.matched_count) metrics.status_match_fraction = static_cast<double>(metrics.status_match_count) / metrics.matched_count;
        if (metrics.assertion_comparable_count) metrics.assertion_agreement_fraction = static_cast<double>(metrics.assertion_agreement_count) / metrics.assertion_comparable_count;
        if (metrics.ground_truth_count) metrics.truth_match_fraction = static_cast<double>(metrics.matched_count) / metrics.ground_truth_count;
        if (metrics.prediction_count) metrics.prediction_match_fraction = static_cast<double>(metrics.matched_count) / metrics.prediction_count;
        if (absolute_errors.empty()) return;
        std::vector<double> sorted = absolute_errors;
        std::sort(sorted.begin(), sorted.end());
        double signed_sum = 0.0;
        double absolute_sum = 0.0;
        double squared_sum = 0.0;
        for (std::size_t i = 0; i < sorted.size(); ++i)
        {
            signed_sum += signed_errors[i];
            absolute_sum += absolute_errors[i];
            squared_sum += absolute_errors[i] * absolute_errors[i];
        }
        metrics.bias = signed_sum / signed_errors.size();
        metrics.mean_absolute_error = absolute_sum / absolute_errors.size();
        metrics.root_mean_square_error = std::sqrt(squared_sum / absolute_errors.size());
        metrics.maximum_absolute_error = sorted.back();
        metrics.median_absolute_error = sorted.size() & 1u ? sorted[sorted.size() / 2u] : 0.5 * (sorted[sorted.size() / 2u - 1u] + sorted[sorted.size() / 2u]);
        const std::size_t p95 = static_cast<std::size_t>(std::ceil(0.95 * sorted.size())) - 1u;
        metrics.p95_absolute_error = sorted[p95];
    }

    signal_synth::measurement_score_metrics metrics_for(const signal_synth::measurement_score_result& result, const std::string* name, const std::string* channel)
    {
        signal_synth::measurement_score_metrics metrics;
        std::vector<bool> truth_selected(result.ground_truth.size(), false);
        std::vector<bool> prediction_selected(result.predictions.size(), false);
        for (std::size_t i = 0; i < result.ground_truth.size(); ++i)
        {
            const signal_synth::measurement_value& value = result.ground_truth[i].measurement;
            if (name && value.name != *name) continue;
            if (channel && value.channel != *channel) continue;
            truth_selected[i] = true;
            ++metrics.ground_truth_count;
            if (value.status == signal_synth::measurement_valid) ++metrics.valid_truth_count;
            else if (value.status == signal_synth::measurement_undefined) ++metrics.undefined_truth_count;
            else if (value.status == signal_synth::measurement_absent) ++metrics.absent_truth_count;
            else ++metrics.not_evaluable_truth_count;
        }
        for (std::size_t i = 0; i < result.predictions.size(); ++i)
        {
            const signal_synth::measurement_value& value = result.predictions[i];
            if (name && value.name != *name) continue;
            if (channel && value.channel != *channel) continue;
            prediction_selected[i] = true;
            ++metrics.prediction_count;
        }
        std::vector<double> signed_errors;
        std::vector<double> absolute_errors;
        for (std::size_t i = 0; i < result.matches.size(); ++i)
        {
            const signal_synth::measurement_score_match& match = result.matches[i];
            if (!truth_selected[match.ground_truth_index] || !prediction_selected[match.prediction_index]) continue;
            ++metrics.matched_count;
            if (match.status_matches) ++metrics.status_match_count; else ++metrics.status_mismatch_count;
            if (match.numeric_pair)
            {
                ++metrics.numeric_pair_count;
                if (match.within_tolerance) ++metrics.tolerance_pass_count;
                signed_errors.push_back(match.signed_error);
                absolute_errors.push_back(match.absolute_error);
            }
            if (match.has_assertion_result)
            {
                ++metrics.assertion_comparable_count;
                if (match.ground_truth_assertion_passed == match.prediction_assertion_passed) ++metrics.assertion_agreement_count;
            }
        }
        for (std::size_t i = 0; i < result.missing_ground_truth_indices.size(); ++i)
            if (truth_selected[result.missing_ground_truth_indices[i]]) ++metrics.missing_count;
        for (std::size_t i = 0; i < result.extra_prediction_indices.size(); ++i)
            if (prediction_selected[result.extra_prediction_indices[i]]) ++metrics.extra_count;
        finalize_metrics(metrics, signed_errors, absolute_errors);
        return metrics;
    }

    void build_groups(signal_synth::measurement_score_result& result)
    {
        std::set<std::string> names;
        std::set<std::string> channels;
        std::set<std::pair<std::string, std::string> > pairs;
        for (std::size_t i = 0; i < result.ground_truth.size(); ++i)
        {
            names.insert(result.ground_truth[i].measurement.name);
            channels.insert(result.ground_truth[i].measurement.channel);
            pairs.insert(std::make_pair(result.ground_truth[i].measurement.name, result.ground_truth[i].measurement.channel));
        }
        for (std::size_t i = 0; i < result.predictions.size(); ++i)
        {
            names.insert(result.predictions[i].name);
            channels.insert(result.predictions[i].channel);
            pairs.insert(std::make_pair(result.predictions[i].name, result.predictions[i].channel));
        }
        result.total = metrics_for(result, 0, 0);
        for (std::set<std::string>::const_iterator it = names.begin(); it != names.end(); ++it)
        {
            signal_synth::measurement_score_group group;
            group.name = *it;
            group.metrics = metrics_for(result, &group.name, 0);
            result.measurements.push_back(group);
        }
        for (std::set<std::string>::const_iterator it = channels.begin(); it != channels.end(); ++it)
        {
            signal_synth::measurement_score_group group;
            group.channel = *it;
            group.metrics = metrics_for(result, 0, &group.channel);
            result.channels.push_back(group);
        }
        for (std::set<std::pair<std::string, std::string> >::const_iterator it = pairs.begin(); it != pairs.end(); ++it)
        {
            signal_synth::measurement_score_group group;
            group.name = it->first;
            group.channel = it->second;
            group.metrics = metrics_for(result, &group.name, &group.channel);
            result.measurement_channels.push_back(group);
        }
    }

    void write_optional(std::ostringstream& output, bool available, double value)
    {
        if (available) output << value; else output << "null";
    }

    void write_metrics_json(std::ostringstream& output, const signal_synth::measurement_score_metrics& metrics)
    {
        output << "{\"ground_truth_count\":" << metrics.ground_truth_count
               << ",\"valid_truth_count\":" << metrics.valid_truth_count
               << ",\"undefined_truth_count\":" << metrics.undefined_truth_count
               << ",\"absent_truth_count\":" << metrics.absent_truth_count
               << ",\"not_evaluable_truth_count\":" << metrics.not_evaluable_truth_count
               << ",\"prediction_count\":" << metrics.prediction_count
               << ",\"matched_count\":" << metrics.matched_count
               << ",\"numeric_pair_count\":" << metrics.numeric_pair_count
               << ",\"tolerance_pass_count\":" << metrics.tolerance_pass_count
               << ",\"status_match_count\":" << metrics.status_match_count
               << ",\"status_mismatch_count\":" << metrics.status_mismatch_count
               << ",\"missing_count\":" << metrics.missing_count
               << ",\"extra_count\":" << metrics.extra_count
               << ",\"assertion_comparable_count\":" << metrics.assertion_comparable_count
               << ",\"assertion_agreement_count\":" << metrics.assertion_agreement_count
               << ",\"tolerance_pass_fraction\":";
        write_optional(output, metrics.numeric_pair_count > 0u, metrics.tolerance_pass_fraction);
        output << ",\"status_match_fraction\":";
        write_optional(output, metrics.matched_count > 0u, metrics.status_match_fraction);
        output << ",\"assertion_agreement_fraction\":";
        write_optional(output, metrics.assertion_comparable_count > 0u, metrics.assertion_agreement_fraction);
        output << ",\"truth_match_fraction\":";
        write_optional(output, metrics.ground_truth_count > 0u, metrics.truth_match_fraction);
        output << ",\"prediction_match_fraction\":";
        write_optional(output, metrics.prediction_count > 0u, metrics.prediction_match_fraction);
        output << ",\"error\":{\"bias\":";
        write_optional(output, metrics.numeric_pair_count > 0u, metrics.bias);
        output << ",\"mean_absolute\":"; write_optional(output, metrics.numeric_pair_count > 0u, metrics.mean_absolute_error);
        output << ",\"root_mean_square\":"; write_optional(output, metrics.numeric_pair_count > 0u, metrics.root_mean_square_error);
        output << ",\"median_absolute\":"; write_optional(output, metrics.numeric_pair_count > 0u, metrics.median_absolute_error);
        output << ",\"p95_absolute\":"; write_optional(output, metrics.numeric_pair_count > 0u, metrics.p95_absolute_error);
        output << ",\"maximum_absolute\":"; write_optional(output, metrics.numeric_pair_count > 0u, metrics.maximum_absolute_error);
        output << "}}";
    }

    void write_measurement_json(std::ostringstream& output, const signal_synth::measurement_value& item)
    {
        output << "{\"name\":" << json_string(item.name);
        if (item.has_value) output << ",\"value\":" << item.value;
        output << ",\"unit\":" << json_string(item.unit)
               << ",\"status\":" << json_string(signal_synth::measurement_status_name(item.status))
               << ",\"scope\":" << json_string(signal_synth::measurement_scope_name(item.scope));
        if (item.has_time_seconds) output << ",\"time_seconds\":" << item.time_seconds;
        if (item.has_beat_index) output << ",\"beat_index\":\"" << item.beat_index << '"';
        if (!item.channel.empty()) output << ",\"channel\":" << json_string(item.channel);
        if (!item.formula.empty()) output << ",\"formula\":" << json_string(item.formula);
        if (item.has_confidence) output << ",\"confidence\":" << item.confidence;
        output << '}';
    }

    void write_truth_json(std::ostringstream& output, const signal_synth::measurement_truth& truth)
    {
        output << "{\"measurement\":";
        write_measurement_json(output, truth.measurement);
        output << ",\"absolute_tolerance\":" << truth.absolute_tolerance
               << ",\"relative_tolerance_percent\":" << truth.relative_tolerance_percent
               << ",\"error_model\":" << json_string(signal_synth::measurement_error_model_name(truth.error_model));
        if (truth.has_expected_range)
            output << ",\"expected_range\":{\"minimum\":" << truth.expected_minimum << ",\"maximum\":" << truth.expected_maximum << '}';
        output << ",\"reason\":" << json_string(truth.reason) << '}';
    }

    std::string csv_optional(bool available, double value)
    {
        if (!available) return "NA";
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
        return output.str();
    }
}

namespace signal_synth
{
    measurement_truth::measurement_truth()
        : measurement(), absolute_tolerance(0.0), relative_tolerance_percent(0.0), error_model(measurement_error_linear), has_expected_range(false), expected_minimum(0.0), expected_maximum(0.0), reason() {}
    measurement_score_options::measurement_score_options() : pairing_window_seconds(0.20) {}
    measurement_score_metrics::measurement_score_metrics()
        : ground_truth_count(0), valid_truth_count(0), undefined_truth_count(0), absent_truth_count(0), not_evaluable_truth_count(0), prediction_count(0), matched_count(0), numeric_pair_count(0), tolerance_pass_count(0), status_match_count(0), status_mismatch_count(0), missing_count(0), extra_count(0), assertion_comparable_count(0), assertion_agreement_count(0), tolerance_pass_fraction(0.0), status_match_fraction(0.0), assertion_agreement_fraction(0.0), truth_match_fraction(0.0), prediction_match_fraction(0.0), bias(0.0), mean_absolute_error(0.0), root_mean_square_error(0.0), median_absolute_error(0.0), p95_absolute_error(0.0), maximum_absolute_error(0.0) {}
    measurement_score_match::measurement_score_match()
        : ground_truth_index(0), prediction_index(0), ground_truth_status(measurement_undefined), prediction_status(measurement_undefined), status_matches(false), numeric_pair(false), signed_error(0.0), absolute_error(0.0), relative_error_percent(0.0), has_relative_error(false), within_tolerance(false), has_assertion_result(false), ground_truth_assertion_passed(false), prediction_assertion_passed(false) {}
    measurement_score_result::measurement_score_result()
        : success(false), target(), pairing_window_seconds(0.0), total(), measurements(), channels(), measurement_channels(), ground_truth(), predictions(), matches(), missing_ground_truth_indices(), extra_prediction_indices(), messages() {}

    const char* measurement_error_model_name(measurement_error_model model)
    {
        return model == measurement_error_circular_degrees ? "circular_degrees" : "linear";
    }

    bool measurement_target_supported(const std::string& target)
    {
        return target == "morphology_assertions" || target == "ecg_ppg_alignment" || target == "ppg_optical" || target == "prv" || target == "respiratory_rate";
    }

    bool measurement_ground_truth_from_render(const ecg_render_bundle& render, const std::string& target, std::vector<measurement_truth>& output, std::vector<std::string>& messages)
    {
        output.clear();
        messages.clear();
        if (target == "morphology_assertions")
        {
            add_timing_truth(render, output);
            add_lead_morphology_truth(render, output);
            add_axis_truth(render, output);
            add_assertion_truth(render, output);
        }
        else if (target == "ecg_ppg_alignment")
        {
            if (!render.ppg.sample_count())
            {
                messages.push_back("ECG/PPG alignment measurement truth requires PPG");
                return false;
            }
            add_alignment_truth(render, output);
        }
        else if (target == "ppg_optical")
        {
            if (!render.ppg.optical_enabled())
            {
                messages.push_back("PPG optical measurement truth requires optical PPG");
                return false;
            }
            add_optical_truth(render, output);
        }
        else if (target == "prv")
        {
            if (!render.cardiorespiratory.prv_available) { messages.push_back("PRV measurement truth requires PPG"); return false; }
            add_prv_truth(render, output);
        }
        else if (target == "respiratory_rate")
        {
            if (!render.cardiorespiratory.respiration_available) { messages.push_back("respiratory-rate truth requires at least one respiratory coupling"); return false; }
            add_respiratory_rate_truth(render, output);
        }
        else
        {
            messages.push_back("unsupported measurement target");
            return false;
        }
        for (std::size_t i = 0; i < output.size(); ++i) output[i].measurement.original_index = static_cast<unsigned int>(i);
        return true;
    }

    bool score_measurements(const std::string& target, const std::vector<measurement_truth>& ground_truth, const std::vector<measurement_value>& predictions, const measurement_score_options& options, measurement_score_result& result)
    {
        measurement_score_result fresh;
        fresh.target = target;
        fresh.pairing_window_seconds = options.pairing_window_seconds;
        fresh.ground_truth = ground_truth;
        fresh.predictions = predictions;
        if (!measurement_target_supported(target)) fresh.messages.push_back("unsupported measurement target");
        if (!finite_value(options.pairing_window_seconds) || options.pairing_window_seconds <= 0.0) fresh.messages.push_back("pairing window must be finite and positive");
        if (!fresh.messages.empty()) { result = fresh; return false; }
        std::vector<match_candidate> candidates;
        for (std::size_t truth_index = 0; truth_index < ground_truth.size(); ++truth_index)
        {
            const measurement_value& truth = ground_truth[truth_index].measurement;
            for (std::size_t prediction_index = 0; prediction_index < predictions.size(); ++prediction_index)
            {
                const measurement_value& prediction = predictions[prediction_index];
                if (!same_descriptor(truth, prediction)) continue;
                match_candidate candidate;
                candidate.truth_index = static_cast<unsigned int>(truth_index);
                candidate.prediction_index = static_cast<unsigned int>(prediction_index);
                candidate.exact_beat = truth.has_beat_index && prediction.has_beat_index && truth.beat_index == prediction.beat_index;
                if (truth.has_beat_index && prediction.has_beat_index && !candidate.exact_beat) continue;
                if (truth.scope == measurement_record || truth.scope == measurement_lead) candidate.distance = 0.0;
                else if (candidate.exact_beat) candidate.distance = truth.has_time_seconds && prediction.has_time_seconds ? std::fabs(truth.time_seconds - prediction.time_seconds) : 0.0;
                else if (truth.has_time_seconds && prediction.has_time_seconds) candidate.distance = std::fabs(truth.time_seconds - prediction.time_seconds);
                else continue;
                if (!candidate.exact_beat && candidate.distance > options.pairing_window_seconds) continue;
                candidates.push_back(candidate);
            }
        }
        std::sort(candidates.begin(), candidates.end(), candidate_less);
        std::vector<bool> truth_used(ground_truth.size(), false);
        std::vector<bool> prediction_used(predictions.size(), false);
        for (std::size_t i = 0; i < candidates.size(); ++i)
        {
            const match_candidate& candidate = candidates[i];
            if (truth_used[candidate.truth_index] || prediction_used[candidate.prediction_index]) continue;
            truth_used[candidate.truth_index] = true;
            prediction_used[candidate.prediction_index] = true;
            const measurement_truth& truth = ground_truth[candidate.truth_index];
            const measurement_value& prediction = predictions[candidate.prediction_index];
            measurement_score_match match;
            match.ground_truth_index = candidate.truth_index;
            match.prediction_index = candidate.prediction_index;
            match.ground_truth_status = truth.measurement.status;
            match.prediction_status = prediction.status;
            match.status_matches = match.ground_truth_status == match.prediction_status;
            match.numeric_pair = truth.measurement.status == measurement_valid && prediction.status == measurement_valid;
            if (match.numeric_pair)
            {
                match.signed_error = ::signed_error(truth, prediction);
                match.absolute_error = std::fabs(match.signed_error);
                match.has_relative_error = std::fabs(truth.measurement.value) > 1e-15;
                if (match.has_relative_error) match.relative_error_percent = 100.0 * match.absolute_error / std::fabs(truth.measurement.value);
                double tolerance = truth.absolute_tolerance;
                if (match.has_relative_error) tolerance = std::max(tolerance, std::fabs(truth.measurement.value) * truth.relative_tolerance_percent / 100.0);
                match.within_tolerance = match.absolute_error <= tolerance;
                if (truth.has_expected_range)
                {
                    match.has_assertion_result = true;
                    match.ground_truth_assertion_passed = truth.measurement.value >= truth.expected_minimum && truth.measurement.value <= truth.expected_maximum;
                    match.prediction_assertion_passed = prediction.value >= truth.expected_minimum && prediction.value <= truth.expected_maximum;
                }
            }
            fresh.matches.push_back(match);
        }
        for (std::size_t i = 0; i < truth_used.size(); ++i) if (!truth_used[i]) fresh.missing_ground_truth_indices.push_back(static_cast<unsigned int>(i));
        for (std::size_t i = 0; i < prediction_used.size(); ++i) if (!prediction_used[i]) fresh.extra_prediction_indices.push_back(static_cast<unsigned int>(i));
        build_groups(fresh);
        fresh.success = true;
        result = fresh;
        return true;
    }

    bool score_measurement_output_to_render(const ecg_render_bundle& render, const std::string& target, const measurement_output_document& predictions, const measurement_score_options& options, measurement_score_result& result)
    {
        std::vector<measurement_truth> truth;
        std::vector<std::string> messages;
        if (!measurement_ground_truth_from_render(render, target, truth, messages))
        {
            measurement_score_result fresh;
            fresh.target = target;
            fresh.messages = messages;
            result = fresh;
            return false;
        }
        return score_measurements(target, truth, predictions.measurements, options, result);
    }

    std::string measurement_truth_bundle_json(const ecg_render_bundle& render, const std::vector<std::string>& targets)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10)
               << "{\"schema_version\":1,\"contract\":\"synsigra_measurement_truth_v1\",\"targets\":[";
        bool first_target = true;
        std::set<std::string> emitted;
        for (std::size_t i = 0; i < targets.size(); ++i)
        {
            if (!measurement_target_supported(targets[i]) || !emitted.insert(targets[i]).second) continue;
            std::vector<measurement_truth> truth;
            std::vector<std::string> messages;
            if (!measurement_ground_truth_from_render(render, targets[i], truth, messages)) continue;
            output << (first_target ? "" : ",") << "{\"target\":" << json_string(targets[i]) << ",\"measurements\":[";
            for (std::size_t item = 0; item < truth.size(); ++item)
            {
                output << (item ? "," : "");
                write_truth_json(output, truth[item]);
            }
            output << "]}";
            first_target = false;
        }
        output << "]}";
        return output.str();
    }

    std::string measurement_score_result_json(const ecg_render_bundle& render, const measurement_score_result& result)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10)
               << "{\"schema_version\":1,\"score_type\":\"measurement_qa\",\"scoring_version\":\"synsigra_measurement_score_v1\",\"target\":" << json_string(result.target)
               << ",\"scenario\":{\"scenario_id\":" << json_string(render.document.scenario_id) << ",\"document_fingerprint\":" << json_string(render.document_identity.document_fingerprint) << ",\"render_identity\":" << json_string(render.render_identity) << "}"
               << ",\"options\":{\"pairing_window_seconds\":" << result.pairing_window_seconds << "},\"overall\":";
        write_metrics_json(output, result.total);
        output << ",\"by_measurement\":[";
        for (std::size_t i = 0; i < result.measurements.size(); ++i) { output << (i ? "," : "") << "{\"name\":" << json_string(result.measurements[i].name) << ",\"metrics\":"; write_metrics_json(output, result.measurements[i].metrics); output << '}'; }
        output << "],\"by_channel\":[";
        for (std::size_t i = 0; i < result.channels.size(); ++i) { output << (i ? "," : "") << "{\"channel\":" << json_string(result.channels[i].channel.empty() ? "global" : result.channels[i].channel) << ",\"metrics\":"; write_metrics_json(output, result.channels[i].metrics); output << '}'; }
        output << "],\"by_measurement_channel\":[";
        for (std::size_t i = 0; i < result.measurement_channels.size(); ++i) { output << (i ? "," : "") << "{\"name\":" << json_string(result.measurement_channels[i].name) << ",\"channel\":" << json_string(result.measurement_channels[i].channel.empty() ? "global" : result.measurement_channels[i].channel) << ",\"metrics\":"; write_metrics_json(output, result.measurement_channels[i].metrics); output << '}'; }
        output << "],\"matches\":[";
        for (std::size_t i = 0; i < result.matches.size(); ++i)
        {
            const measurement_score_match& match = result.matches[i];
            const measurement_value& truth = result.ground_truth[match.ground_truth_index].measurement;
            output << (i ? "," : "") << "{\"ground_truth_index\":" << match.ground_truth_index << ",\"prediction_index\":" << match.prediction_index
                   << ",\"name\":" << json_string(truth.name) << ",\"unit\":" << json_string(truth.unit) << ",\"scope\":" << json_string(measurement_scope_name(truth.scope)) << ",\"channel\":" << json_string(truth.channel.empty() ? "global" : truth.channel)
                   << ",\"ground_truth_status\":" << json_string(measurement_status_name(match.ground_truth_status)) << ",\"prediction_status\":" << json_string(measurement_status_name(match.prediction_status))
                   << ",\"status_matches\":" << boolean(match.status_matches) << ",\"numeric_pair\":" << boolean(match.numeric_pair);
            if (match.numeric_pair)
            {
                output << ",\"signed_error\":" << match.signed_error << ",\"absolute_error\":" << match.absolute_error << ",\"relative_error_percent\":";
                write_optional(output, match.has_relative_error, match.relative_error_percent);
                output << ",\"within_tolerance\":" << boolean(match.within_tolerance);
            }
            if (match.has_assertion_result)
                output << ",\"ground_truth_assertion_passed\":" << boolean(match.ground_truth_assertion_passed) << ",\"prediction_assertion_passed\":" << boolean(match.prediction_assertion_passed);
            output << '}';
        }
        output << "],\"missing_ground_truth_indices\":[";
        for (std::size_t i = 0; i < result.missing_ground_truth_indices.size(); ++i) output << (i ? "," : "") << result.missing_ground_truth_indices[i];
        output << "],\"extra_prediction_indices\":[";
        for (std::size_t i = 0; i < result.extra_prediction_indices.size(); ++i) output << (i ? "," : "") << result.extra_prediction_indices[i];
        output << "],\"messages\":[";
        for (std::size_t i = 0; i < result.messages.size(); ++i) output << (i ? "," : "") << json_string(result.messages[i]);
        output << "]}";
        return output.str();
    }

    std::string measurement_score_result_csv(const measurement_score_result& result)
    {
        std::ostringstream output;
        output << "row_type,name,channel,ground_truth_count,prediction_count,matched_count,truth_match_fraction,prediction_match_fraction,numeric_pair_count,tolerance_pass_count,tolerance_pass_fraction,status_mismatch_count,missing_count,extra_count,bias,mean_absolute_error,root_mean_square_error,p95_absolute_error\n";
        const measurement_score_metrics* total = &result.total;
        output << "overall,,," << total->ground_truth_count << ',' << total->prediction_count << ',' << total->matched_count << ',' << csv_optional(total->ground_truth_count > 0u, total->truth_match_fraction) << ',' << csv_optional(total->prediction_count > 0u, total->prediction_match_fraction) << ',' << total->numeric_pair_count << ',' << total->tolerance_pass_count << ',' << csv_optional(total->numeric_pair_count > 0u, total->tolerance_pass_fraction) << ',' << total->status_mismatch_count << ',' << total->missing_count << ',' << total->extra_count << ',' << csv_optional(total->numeric_pair_count > 0u, total->bias) << ',' << csv_optional(total->numeric_pair_count > 0u, total->mean_absolute_error) << ',' << csv_optional(total->numeric_pair_count > 0u, total->root_mean_square_error) << ',' << csv_optional(total->numeric_pair_count > 0u, total->p95_absolute_error) << '\n';
        for (std::size_t i = 0; i < result.measurement_channels.size(); ++i)
        {
            const measurement_score_group& group = result.measurement_channels[i];
            const measurement_score_metrics& metrics = group.metrics;
            output << "measurement_channel," << group.name << ',' << (group.channel.empty() ? "global" : group.channel) << ',' << metrics.ground_truth_count << ',' << metrics.prediction_count << ',' << metrics.matched_count << ',' << csv_optional(metrics.ground_truth_count > 0u, metrics.truth_match_fraction) << ',' << csv_optional(metrics.prediction_count > 0u, metrics.prediction_match_fraction) << ',' << metrics.numeric_pair_count << ',' << metrics.tolerance_pass_count << ',' << csv_optional(metrics.numeric_pair_count > 0u, metrics.tolerance_pass_fraction) << ',' << metrics.status_mismatch_count << ',' << metrics.missing_count << ',' << metrics.extra_count << ',' << csv_optional(metrics.numeric_pair_count > 0u, metrics.bias) << ',' << csv_optional(metrics.numeric_pair_count > 0u, metrics.mean_absolute_error) << ',' << csv_optional(metrics.numeric_pair_count > 0u, metrics.root_mean_square_error) << ',' << csv_optional(metrics.numeric_pair_count > 0u, metrics.p95_absolute_error) << '\n';
        }
        return output.str();
    }

    std::string measurement_score_report_html(const ecg_render_bundle& render, const measurement_score_result& result)
    {
        std::ostringstream rows;
        for (std::size_t i = 0; i < result.measurements.size(); ++i)
        {
            const measurement_score_group& group = result.measurements[i];
            rows << "<tr><td>" << html_text(group.name) << "</td><td>" << group.metrics.ground_truth_count << "</td><td>" << group.metrics.prediction_count << "</td><td>" << group.metrics.numeric_pair_count << "</td><td>" << csv_optional(group.metrics.numeric_pair_count > 0u, group.metrics.tolerance_pass_fraction) << "</td><td>" << group.metrics.status_mismatch_count << "</td><td>" << group.metrics.missing_count << "</td><td>" << group.metrics.extra_count << "</td><td>" << csv_optional(group.metrics.numeric_pair_count > 0u, group.metrics.mean_absolute_error) << "</td></tr>";
        }
        std::ostringstream output;
        output << "<!doctype html><html><head><meta charset=\"utf-8\"><title>Measurement QA</title><style>body{font-family:Arial,sans-serif;margin:24px;color:#20252b}table{border-collapse:collapse}th,td{border:1px solid #c9ced4;padding:6px 9px;text-align:right}th:first-child,td:first-child{text-align:left}.notice{color:#555}</style></head><body><h1>Measurement QA Report</h1><p class=\"notice\">Synthetic engineering QA only; not a clinical validation certificate.</p><p>Scenario: " << html_text(render.document.scenario_id) << " | Target: " << html_text(result.target) << " | Pairing window: " << result.pairing_window_seconds << " s</p><table><tr><th>Measurement</th><th>Truth</th><th>Predictions</th><th>Numeric pairs</th><th>Pass fraction</th><th>Status mismatch</th><th>Missing</th><th>Extra</th><th>MAE</th></tr>" << rows.str() << "</table></body></html>";
        return output.str();
    }
}
