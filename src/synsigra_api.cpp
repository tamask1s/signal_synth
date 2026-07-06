#include "synsigra_api.h"

#include "ecg_compare.h"
#include "ecg_export.h"

#include <sstream>

namespace
{
    signal_synth::synsigra_message make_message(const std::string& code, const std::string& path, const std::string& text)
    {
        signal_synth::synsigra_message message;
        message.code = code;
        message.path = path;
        message.message = text;
        return message;
    }

    const char* json_code_name(signal_synth::ecg_scenario_json_message_code code)
    {
        return signal_synth::ecg_scenario_json_message_code_name(code);
    }

    void copy_json_messages(const signal_synth::ecg_scenario_json_result& source, std::vector<signal_synth::synsigra_message>& output)
    {
        for (std::size_t i = 0; i < source.messages.size(); ++i)
        {
            const signal_synth::ecg_scenario_json_message& source_message = source.messages[i];
            output.push_back(make_message(json_code_name(source_message.code), source_message.path, source_message.message));
        }
    }

    void copy_export_messages(const signal_synth::ecg_export_result& source, std::vector<signal_synth::synsigra_message>& output)
    {
        for (std::size_t i = 0; i < source.messages.size(); ++i)
            output.push_back(make_message("EXPORT_ERROR", "$", source.messages[i]));
    }

    void fill_identity(const signal_synth::ecg_scenario_document& document, const signal_synth::ecg_scenario_json_result& identity, signal_synth::synsigra_identity& output)
    {
        output.scenario_id = document.scenario_id;
        output.schema_version = document.schema_version;
        output.sample_count = document.sample_count();
        output.document_fingerprint = identity.document_fingerprint;
        output.generation_fingerprint = identity.generation_fingerprint;
        output.generator_version = signal_synth::signal_synth_generator_version();
    }

    void fill_render_identity(const signal_synth::ecg_render_bundle& render, signal_synth::synsigra_identity& output)
    {
        fill_identity(render.document, render.document_identity, output);
        output.render_identity = render.render_identity;
    }

    void copy_artifacts(const signal_synth::ecg_export_bundle& source, std::vector<signal_synth::synsigra_artifact>& output)
    {
        output.clear();
        output.reserve(source.artifacts.size());
        for (std::size_t i = 0; i < source.artifacts.size(); ++i)
        {
            signal_synth::synsigra_artifact artifact;
            artifact.name = source.artifacts[i].name;
            artifact.media_type = source.artifacts[i].media_type;
            artifact.content = source.artifacts[i].content;
            output.push_back(artifact);
        }
    }

    signal_synth::ecg_compare_target to_internal_target(signal_synth::synsigra_compare_target target)
    {
        if (target == signal_synth::synsigra_compare_ppg_systolic_peak)
            return signal_synth::ecg_compare_ppg_systolic_peak;
        if (target == signal_synth::synsigra_compare_ppg_pulse_onset)
            return signal_synth::ecg_compare_ppg_pulse_onset;
        return signal_synth::ecg_compare_r_peak;
    }

    void copy_metrics(const signal_synth::ecg_compare_bin_metrics& source, signal_synth::synsigra_compare_metrics& output)
    {
        output.ground_truth_count = source.ground_truth_count;
        output.detection_count = source.detection_count;
        output.true_positive_count = source.true_positive_count;
        output.false_positive_count = source.false_positive_count;
        output.false_negative_count = source.false_negative_count;
        output.sensitivity = source.sensitivity;
        output.positive_predictive_value = source.positive_predictive_value;
        output.f1_score = source.f1_score;
        output.mean_absolute_error_seconds = source.mean_absolute_error_seconds;
        output.median_absolute_error_seconds = source.median_absolute_error_seconds;
        output.rms_error_seconds = source.rms_error_seconds;
        output.max_absolute_error_seconds = source.max_absolute_error_seconds;
    }

    void add_compare_artifact(std::vector<signal_synth::synsigra_artifact>& artifacts, const char* name, const char* media_type, const std::string& content)
    {
        signal_synth::synsigra_artifact artifact;
        artifact.name = name;
        artifact.media_type = media_type;
        artifact.content = content;
        artifacts.push_back(artifact);
    }
}

namespace signal_synth
{
    synsigra_message::synsigra_message() : code(), path(), message()
    {
    }

    synsigra_identity::synsigra_identity()
        : scenario_id(), schema_version(0), sample_count(0), document_fingerprint(), generation_fingerprint(0), render_identity(), generator_version()
    {
    }

    synsigra_validation_result::synsigra_validation_result() : success(false), identity(), canonical_scenario_json(), messages()
    {
    }

    synsigra_render_result::synsigra_render_result() : success(false), identity(), artifacts(), messages()
    {
    }

    const synsigra_artifact* synsigra_render_result::find_artifact(const std::string& name) const
    {
        for (std::size_t i = 0; i < artifacts.size(); ++i)
            if (artifacts[i].name == name)
                return &artifacts[i];
        return 0;
    }

    synsigra_detection_event::synsigra_detection_event() : time_seconds(0.0), label()
    {
    }

    synsigra_compare_options::synsigra_compare_options() : target(synsigra_compare_r_peak), tolerance_seconds(0.0)
    {
    }

    synsigra_compare_metrics::synsigra_compare_metrics()
        : ground_truth_count(0), detection_count(0), true_positive_count(0), false_positive_count(0), false_negative_count(0), sensitivity(0.0), positive_predictive_value(0.0), f1_score(0.0), mean_absolute_error_seconds(0.0), median_absolute_error_seconds(0.0), rms_error_seconds(0.0), max_absolute_error_seconds(0.0)
    {
    }

    synsigra_ppg_timing_metrics::synsigra_ppg_timing_metrics()
        : ground_truth_interval_count(0), detection_interval_count(0), matched_interval_count(0), mean_absolute_interval_error_seconds(0.0), rms_interval_error_seconds(0.0), max_absolute_interval_error_seconds(0.0), ground_truth_mean_pulse_rate_bpm(0.0), detection_mean_pulse_rate_bpm(0.0), absolute_pulse_rate_error_bpm(0.0)
    {
    }

    synsigra_compare_result::synsigra_compare_result()
        : success(false), identity(), target_name(), tolerance_seconds(0.0), total(), clean(), artifact(), motion(), dropout(), low_perfusion(), weak(), pulse_timing(), artifacts(), messages()
    {
    }

    const synsigra_artifact* synsigra_compare_result::find_artifact(const std::string& name) const
    {
        for (std::size_t i = 0; i < artifacts.size(); ++i)
            if (artifacts[i].name == name)
                return &artifacts[i];
        return 0;
    }

    const char* synsigra_api_version()
    {
        return "0.2.0";
    }

    double synsigra_default_compare_tolerance_seconds(synsigra_compare_target target)
    {
        return ecg_compare_default_tolerance_seconds(to_internal_target(target));
    }

    const char* synsigra_compare_target_name(synsigra_compare_target target)
    {
        return ecg_compare_target_name(to_internal_target(target));
    }

    bool synsigra_validate_scenario_json(const std::string& scenario_json, synsigra_validation_result& result)
    {
        synsigra_validation_result fresh;
        ecg_scenario_document document;
        ecg_scenario_json_result json_result;
        if (!parse_ecg_scenario_json(scenario_json, document, json_result))
        {
            copy_json_messages(json_result, fresh.messages);
            result = fresh;
            return false;
        }
        fill_identity(document, json_result, fresh.identity);
        fresh.canonical_scenario_json = json_result.canonical_json;
        fresh.success = true;
        result = fresh;
        return true;
    }

    bool synsigra_render_scenario_json(const std::string& scenario_json, synsigra_render_result& result)
    {
        synsigra_render_result fresh;
        ecg_scenario_document document;
        ecg_scenario_json_result json_result;
        if (!parse_ecg_scenario_json(scenario_json, document, json_result))
        {
            copy_json_messages(json_result, fresh.messages);
            result = fresh;
            return false;
        }
        ecg_render_bundle render;
        ecg_export_result export_result;
        if (!render_ecg_document(document, render, export_result))
        {
            copy_export_messages(export_result, fresh.messages);
            result = fresh;
            return false;
        }
        ecg_export_bundle export_bundle;
        if (!build_ecg_export_bundle(render, export_bundle, export_result))
        {
            copy_export_messages(export_result, fresh.messages);
            result = fresh;
            return false;
        }
        fill_render_identity(render, fresh.identity);
        copy_artifacts(export_bundle, fresh.artifacts);
        fresh.success = true;
        result = fresh;
        return true;
    }

    bool synsigra_compare_scenario_detections(const std::string& scenario_json, const std::vector<synsigra_detection_event>& detections, const synsigra_compare_options& options, synsigra_compare_result& result)
    {
        synsigra_compare_result fresh;
        ecg_scenario_document document;
        ecg_scenario_json_result json_result;
        if (!parse_ecg_scenario_json(scenario_json, document, json_result))
        {
            copy_json_messages(json_result, fresh.messages);
            result = fresh;
            return false;
        }
        ecg_render_bundle render;
        ecg_export_result export_result;
        if (!render_ecg_document(document, render, export_result))
        {
            copy_export_messages(export_result, fresh.messages);
            result = fresh;
            return false;
        }

        std::vector<ecg_detected_event> internal_detections;
        internal_detections.reserve(detections.size());
        for (std::size_t i = 0; i < detections.size(); ++i)
        {
            ecg_detected_event event;
            event.time_seconds = detections[i].time_seconds;
            event.label = detections[i].label;
            internal_detections.push_back(event);
        }
        ecg_compare_options internal_options;
        internal_options.target = to_internal_target(options.target);
        internal_options.tolerance_seconds = options.tolerance_seconds;
        ecg_compare_result compare_result;
        if (!compare_detections_to_render(render, internal_detections, internal_options, compare_result))
        {
            for (std::size_t i = 0; i < compare_result.messages.size(); ++i)
                fresh.messages.push_back(make_message("COMPARE_ERROR", "$", compare_result.messages[i]));
            result = fresh;
            return false;
        }

        fill_render_identity(render, fresh.identity);
        fresh.target_name = compare_result.target_name;
        fresh.tolerance_seconds = compare_result.tolerance_seconds;
        copy_metrics(compare_result.total, fresh.total);
        copy_metrics(compare_result.clean, fresh.clean);
        copy_metrics(compare_result.artifact, fresh.artifact);
        copy_metrics(compare_result.motion, fresh.motion);
        copy_metrics(compare_result.dropout, fresh.dropout);
        copy_metrics(compare_result.low_perfusion, fresh.low_perfusion);
        copy_metrics(compare_result.weak, fresh.weak);
        fresh.pulse_timing.ground_truth_interval_count = compare_result.pulse_timing.ground_truth_interval_count;
        fresh.pulse_timing.detection_interval_count = compare_result.pulse_timing.detection_interval_count;
        fresh.pulse_timing.matched_interval_count = compare_result.pulse_timing.matched_interval_count;
        fresh.pulse_timing.mean_absolute_interval_error_seconds = compare_result.pulse_timing.mean_absolute_interval_error_seconds;
        fresh.pulse_timing.rms_interval_error_seconds = compare_result.pulse_timing.rms_interval_error_seconds;
        fresh.pulse_timing.max_absolute_interval_error_seconds = compare_result.pulse_timing.max_absolute_interval_error_seconds;
        fresh.pulse_timing.ground_truth_mean_pulse_rate_bpm = compare_result.pulse_timing.ground_truth_mean_pulse_rate_bpm;
        fresh.pulse_timing.detection_mean_pulse_rate_bpm = compare_result.pulse_timing.detection_mean_pulse_rate_bpm;
        fresh.pulse_timing.absolute_pulse_rate_error_bpm = compare_result.pulse_timing.absolute_pulse_rate_error_bpm;
        add_compare_artifact(fresh.artifacts, "comparison.json", "application/json", ecg_compare_result_json(render, compare_result));
        add_compare_artifact(fresh.artifacts, "comparison.csv", "text/csv", ecg_compare_result_csv(compare_result));
        add_compare_artifact(fresh.artifacts, "comparison_report.html", "text/html", ecg_compare_report_html(render, compare_result));
        fresh.success = true;
        result = fresh;
        return true;
    }
}
