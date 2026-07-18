#include "synsigra_api.h"

#include "ecg_compare.h"
#include "ecg_export.h"
#include "scenario_authoring.h"

#include <iomanip>
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

    void copy_render_messages(const signal_synth::ecg_document_render_result& source, std::vector<signal_synth::synsigra_message>& output)
    {
        for (std::size_t i = 0; i < source.messages.size(); ++i)
            output.push_back(make_message("RENDER_ERROR", "$", source.messages[i]));
    }

    std::string json_text(const std::string& value)
    {
        std::ostringstream output;
        output << '"';
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            const unsigned char c = static_cast<unsigned char>(value[i]);
            if (c == '"' || c == '\\')
                output << '\\' << static_cast<char>(c);
            else if (c == '\b') output << "\\b";
            else if (c == '\f') output << "\\f";
            else if (c == '\n') output << "\\n";
            else if (c == '\r') output << "\\r";
            else if (c == '\t') output << "\\t";
            else if (c < 0x20)
                output << "\\u00" << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(c) << std::dec;
            else
                output << static_cast<char>(c);
        }
        output << '"';
        return output.str();
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

    bool to_internal_target(signal_synth::synsigra_compare_target target, signal_synth::ecg_compare_target& output)
    {
        switch (target)
        {
        case signal_synth::synsigra_compare_r_peak: output = signal_synth::ecg_compare_r_peak; return true;
        case signal_synth::synsigra_compare_ppg_systolic_peak: output = signal_synth::ecg_compare_ppg_systolic_peak; return true;
        case signal_synth::synsigra_compare_ppg_pulse_onset: output = signal_synth::ecg_compare_ppg_pulse_onset; return true;
        }
        return false;
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

    std::vector<signal_synth::external_noise_asset_input> external_assets(const std::vector<signal_synth::synsigra_external_noise_asset>& source)
    {
        std::vector<signal_synth::external_noise_asset_input> output(source.size());
        for (std::size_t i = 0; i < source.size(); ++i) { output[i].id = source[i].id; output[i].csv_content = source[i].csv_content; }
        return output;
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
        return "1.4.0";
    }

    const char* synsigra_integration_contract_version()
    {
        return "synsigra_core_integration_v6";
    }

    std::string synsigra_integration_contract_json()
    {
        std::ostringstream output;
        output << "{\"schema_version\":1,\"contract\":" << json_text(synsigra_integration_contract_version())
               << ",\"generator\":{\"name\":\"signal_synth\",\"version\":" << json_text(signal_synth_generator_version())
               << ",\"git_commit\":" << json_text(signal_synth_generator_git_commit())
               << ",\"build_identity\":" << json_text(signal_synth_build_identity()) << "}"
               << ",\"contracts\":{\"cpp_facade\":" << json_text(synsigra_api_version())
               << ",\"pack_schema_version\":2"
               << ",\"challenge_package\":" << json_text(signal_synth_package_contract_version())
               << ",\"scoring_manifest\":" << json_text(signal_synth_scoring_manifest_contract_version())
               << ",\"verification_protocol\":" << json_text(signal_synth_verification_protocol_contract_version())
               << ",\"submission\":\"synsigra_submission_v1\""
               << ",\"submission_formats\":\"synsigra_submission_formats_v1\""
               << ",\"scenario_authoring\":" << json_text(scenario_authoring_metadata_version())
               << ",\"scenario_templates\":" << json_text(scenario_template_catalog_version())
               << ",\"python_verifier\":" << json_text(signal_synth_verifier_version())
               << ",\"external_noise_truth\":\"synsigra_external_noise_truth_v1\"}"
               << ",\"external_noise\":{\"scenario_schema_version\":8,\"asset_transport\":\"in_memory_csv_registry\",\"asset_bytes_in_challenge\":false,\"release_gate\":\"external_noise_truth.release_allowed\",\"redistribution_modes\":[\"local_only\",\"rendered_output\",\"source_and_output\"]}"
               << ",\"scenario\":{\"latest_schema_version\":9,\"supported_schema_versions\":[2,3,4,5,6,7,8,9]}"
               << ",\"hrv\":{\"scenario_schema_version\":9,\"metric_definition\":\"synsigra_hrv_metrics_v2\",\"scoring_version\":\"synsigra_hrv_score_v2\",\"metrics\":[\"mean_rr_seconds\",\"mean_heart_rate_bpm\",\"sdnn_seconds\",\"rmssd_seconds\",\"pnn50_percent\",\"sd1_seconds\",\"sd2_seconds\",\"sd1_sd2_ratio\",\"vlf_power_seconds2\",\"lf_power_seconds2\",\"hf_power_seconds2\",\"lf_hf_ratio\",\"lf_normalized_units\",\"hf_normalized_units\",\"total_power_seconds2\"]}"
               << ",\"cli\":{\"challenge_command\":\"signal-synth pack challenge <pack.json> --out <new-directory>\""
               << ",\"challenge_success_media_type\":\"application/json\""
               << ",\"comparison_targets\":[\"r_peak\",\"ppg_systolic_peak\",\"ppg_pulse_onset\",\"ecg_beat_classification\"]"
               << ",\"interval_targets\":[\"rhythm_episode\",\"signal_quality\"]"
               << ",\"interval_output_schemas\":[\"interval_json_v1\",\"interval_csv_v1\"]"
               << ",\"delineation_targets\":[\"ecg_delineation\"]"
               << ",\"delineation_output_schemas\":[\"point_events_json_v1\",\"point_events_csv_v1\"]"
               << ",\"hrv_targets\":[\"hrv\"]"
               << ",\"measurement_targets\":[\"rr_interval\",\"qtc\",\"morphology_assertions\",\"ecg_ppg_alignment\",\"ppg_optical\",\"prv\",\"respiratory_rate\",\"rhythm_burden\"]"
               << ",\"customer_verification_command\":\"synsigra-verify <challenge> <submission-directory> <result-directory>\""
               << ",\"customer_output_schemas\":[\"point_events_json_v1\",\"point_events_csv_v1\",\"interval_events_json_v1\",\"interval_events_csv_v1\",\"hrv_metrics_json_v1\",\"measurement_values_json_v1\",\"measurement_values_csv_v1\"]}}";
        return output.str();
    }

    double synsigra_default_compare_tolerance_seconds(synsigra_compare_target target)
    {
        ecg_compare_target internal;
        return to_internal_target(target, internal) ? ecg_compare_default_tolerance_seconds(internal) : 0.0;
    }

    const char* synsigra_compare_target_name(synsigra_compare_target target)
    {
        ecg_compare_target internal;
        return to_internal_target(target, internal) ? ecg_compare_target_name(internal) : "";
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

    bool synsigra_render_scenario_json(const std::string& scenario_json, const std::vector<synsigra_external_noise_asset>& external_noise_assets, synsigra_render_result& result)
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
        ecg_document_render_result render_result;
        if (!render_ecg_document(document, external_assets(external_noise_assets), render, render_result))
        {
            copy_render_messages(render_result, fresh.messages);
            result = fresh;
            return false;
        }
        ecg_export_result export_result;
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

    bool synsigra_render_scenario_json(const std::string& scenario_json, synsigra_render_result& result)
    {
        const std::vector<synsigra_external_noise_asset> no_assets;
        return synsigra_render_scenario_json(scenario_json, no_assets, result);
    }

    bool synsigra_compare_scenario_detections(const std::string& scenario_json, const std::vector<synsigra_external_noise_asset>& external_noise_assets, const std::vector<synsigra_detection_event>& detections, const synsigra_compare_options& options, synsigra_compare_result& result)
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
        ecg_document_render_result render_result;
        if (!render_ecg_document(document, external_assets(external_noise_assets), render, render_result))
        {
            copy_render_messages(render_result, fresh.messages);
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
        if (!to_internal_target(options.target, internal_options.target))
        {
            fresh.messages.push_back(make_message("COMPARE_OPTIONS_ERROR", "$.target", "unsupported comparison target"));
            result = fresh;
            return false;
        }
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

    bool synsigra_compare_scenario_detections(const std::string& scenario_json, const std::vector<synsigra_detection_event>& detections, const synsigra_compare_options& options, synsigra_compare_result& result)
    {
        const std::vector<synsigra_external_noise_asset> no_assets;
        return synsigra_compare_scenario_detections(scenario_json, no_assets, detections, options, result);
    }
}
