#include "scenario_authoring.h"

#include "clinical_ecg.h"
#include "signal_quality.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace
{
    const char* metadata_version = "synsigra_authoring_v11";
    const char* template_version = "synsigra_templates_v4";

    struct field_definition
    {
        const char* path;
        const char* label;
        const char* group;
        const char* value_type;
        const char* control;
        const char* default_json;
        const char* minimum_json;
        const char* maximum_json;
        const char* step_json;
        const char* unit;
        const char* options_json;
        const char* visible_when_json;
        bool required;
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

    std::string index_string(std::size_t value)
    {
        std::ostringstream output;
        output << value;
        return output.str();
    }

    const char* category_name(signal_synth::ecg_condition_category category)
    {
        switch (category)
        {
        case signal_synth::ecg_category_normal: return "normal";
        case signal_synth::ecg_category_rhythm: return "rhythm";
        case signal_synth::ecg_category_conduction: return "conduction";
        case signal_synth::ecg_category_morphology: return "morphology";
        case signal_synth::ecg_category_hypertrophy: return "hypertrophy";
        case signal_synth::ecg_category_infarction_injury: return "infarction_injury";
        case signal_synth::ecg_category_ischemia_repolarization: return "ischemia_repolarization";
        }
        return "unknown";
    }

    const char* condition_support_name(signal_synth::ecg_condition_support support)
    {
        switch (support)
        {
        case signal_synth::ecg_support_catalog_only: return "catalog_only";
        case signal_synth::ecg_support_parameterized: return "parameterized";
        case signal_synth::ecg_support_native: return "native";
        }
        return "unknown";
    }

    void write_field(std::ostringstream& output, const field_definition& field)
    {
        output << "{\"path\":" << json_string(field.path)
               << ",\"label\":" << json_string(field.label)
               << ",\"group\":" << json_string(field.group)
               << ",\"value_type\":" << json_string(field.value_type)
               << ",\"control\":" << json_string(field.control)
               << ",\"required\":" << (field.required ? "true" : "false");
        if (field.default_json) output << ",\"default\":" << field.default_json;
        if (field.minimum_json) output << ",\"minimum\":" << field.minimum_json;
        if (field.maximum_json) output << ",\"maximum\":" << field.maximum_json;
        if (field.step_json) output << ",\"step\":" << field.step_json;
        if (field.unit && field.unit[0]) output << ",\"unit\":" << json_string(field.unit);
        if (field.options_json) output << ",\"options\":" << field.options_json;
        if (field.visible_when_json) output << ",\"visible_when\":" << field.visible_when_json;
        output << '}';
    }

    signal_synth::scenario_pack_analysis_message make_message(bool error, const std::string& code, const std::string& path, const std::string& message)
    {
        signal_synth::scenario_pack_analysis_message output;
        output.error = error;
        output.code = code;
        output.path = path;
        output.message = message;
        return output;
    }

    signal_synth::scenario_pack_target_analysis* find_target(std::vector<signal_synth::scenario_pack_target_analysis>& targets, const std::string& name)
    {
        for (std::size_t i = 0; i < targets.size(); ++i)
            if (targets[i].target == name)
                return &targets[i];
        signal_synth::scenario_pack_target_analysis target;
        target.target = name;
        target.support = signal_synth::scenario_target_support_for_name(name);
        targets.push_back(target);
        return &targets.back();
    }

    bool target_compatible(const std::string& target, const signal_synth::ecg_scenario_document& document, std::string& message)
    {
        if ((target == "ppg_systolic_peak" || target == "ppg_pulse_onset") && !document.ppg.enabled)
        {
            message = "PPG event scoring requires ppg.enabled=true.";
            return false;
        }
        if (target == "ecg_ppg_alignment" && !document.ppg.enabled)
        {
            message = "ECG/PPG alignment reference requires ppg.enabled=true.";
            return false;
        }
        if (target == "ppg_optical" && !document.ppg.optical.enabled)
        {
            message = "PPG optical measurement scoring requires ppg.optical.enabled=true.";
            return false;
        }
        if (target == "hrv" && !document.hrv.enabled)
        {
            message = "HRV scoring requires hrv.enabled=true and a supported analysis window.";
            return false;
        }
        if (target == "signal_quality" && document.signal_quality.artifacts.empty())
        {
            message = "Signal-quality reference requires at least one artifact interval.";
            return false;
        }
        if (target == "rhythm_episode" && document.ecg.episode_type() == signal_synth::ecg_episode_none)
        {
            message = "Rhythm-episode scoring requires a configured PSVT or SVARR episode.";
            return false;
        }
        return true;
    }

    unsigned long long estimate_case_package_bytes(const signal_synth::ecg_scenario_document& document, unsigned int channel_count, unsigned long long& csv_bytes, unsigned long long& binary_bytes)
    {
        const unsigned long long samples = document.sample_count();
        const unsigned long long channels = channel_count;
        double maximum_heart_rate = document.hrv.enabled ? document.hrv.target_mean_hr_bpm : document.ecg.heart_rate_bpm();
        for (std::size_t i = 0; i < document.randomization.envelopes.size(); ++i)
            if (document.randomization.envelopes[i].parameter == "ecg.heart_rate_bpm")
                maximum_heart_rate = std::max(maximum_heart_rate, document.randomization.envelopes[i].maximum);
        maximum_heart_rate += document.physiology.activity_intensity * 40.0;
        const unsigned long long estimated_beats = static_cast<unsigned long long>(std::ceil(document.duration_seconds * maximum_heart_rate / 60.0)) + 4u;
        const unsigned long long annotation_bytes = estimated_beats * (document.output.compact ? 1800u : 17000u);
        csv_bytes = document.output.include_waveform_csv ? 256u + samples * (channels + 1u) * 18u : 0u;
        binary_bytes = samples * channels * (document.output.include_edf_bdf ? 7u : 2u);
        unsigned long long wearable_bytes = 0;
        const signal_synth::wearable_stream_config* streams[] = {&document.wearable.ecg, &document.wearable.ppg, &document.wearable.accelerometer};
        for (unsigned int i = 0; i < 3u; ++i)
        {
            if (!streams[i]->enabled)
                continue;
            const unsigned long long stream_samples = signal_synth::wearable_stream_sample_count(*streams[i], document.duration_seconds);
            const unsigned long long stream_channels = i == 0u ? static_cast<unsigned int>(signal_synth::clinical_lead_count) : i == 1u ? 1u + (document.ppg.optical.enabled ? 2u : 0u) : 1u;
            wearable_bytes += 256u + stream_samples * (stream_channels + 3u) * 20u;
            wearable_bytes += stream_samples * 7u * 20u;
            const unsigned long long packet_size = std::max(1u, streams[i]->packet_size_samples);
            wearable_bytes += ((stream_samples + packet_size - 1u) / packet_size) * 256u;
        }
        return csv_bytes + binary_bytes + annotation_bytes + wearable_bytes + 262144u;
    }

    unsigned long long estimate_case_peak_memory_bytes(const signal_synth::ecg_scenario_document& document)
    {
        const unsigned long long samples = document.sample_count();
        const unsigned long long generation_double_channels = document.output.retain_source_channels ? 60u : 36u;
        const unsigned long long ppg_double_channels = document.ppg.enabled ? 2u : 0u;
        unsigned long long wearable_bytes = 0;
        const signal_synth::wearable_stream_config* streams[] = {&document.wearable.ecg, &document.wearable.ppg, &document.wearable.accelerometer};
        for (unsigned int i = 0; i < 3u; ++i)
            if (streams[i]->enabled)
            {
                const unsigned long long stream_samples = signal_synth::wearable_stream_sample_count(*streams[i], document.duration_seconds);
                const unsigned long long stream_channels = i == 0u ? static_cast<unsigned int>(signal_synth::clinical_lead_count) : i == 1u ? 1u + (document.ppg.optical.enabled ? 2u : 0u) : 1u;
                wearable_bytes += stream_samples * (sizeof(signal_synth::wearable_sample_mapping) + stream_channels * sizeof(double));
            }
        return samples * (generation_double_channels + ppg_double_channels) * sizeof(double) + wearable_bytes + 1048576u;
    }

    void write_string_array(std::ostringstream& output, const std::vector<std::string>& values)
    {
        output << '[';
        for (std::size_t i = 0; i < values.size(); ++i)
            output << (i ? "," : "") << json_string(values[i]);
        output << ']';
    }

    bool contains_string(const std::vector<std::string>& values, const std::string& value)
    {
        return std::find(values.begin(), values.end(), value) != values.end();
    }

    bool target_is_scoreable(const signal_synth::scenario_pack_target_analysis& target)
    {
        return target.support == signal_synth::scenario_target_local_scoring;
    }

    std::vector<std::string> case_ids_for_target(const signal_synth::scenario_pack_analysis& analysis, const std::string& target)
    {
        std::vector<std::string> output;
        for (std::size_t i = 0; i < analysis.cases.size(); ++i)
            if (contains_string(analysis.cases[i].targets, target))
                output.push_back(analysis.cases[i].case_id);
        return output;
    }

    std::vector<std::string> targets_by_support(const std::vector<std::string>& targets, const signal_synth::scenario_pack_analysis& analysis, signal_synth::scenario_target_support support)
    {
        std::vector<std::string> output;
        for (std::size_t i = 0; i < targets.size(); ++i)
            for (std::size_t target_index = 0; target_index < analysis.targets.size(); ++target_index)
                if (analysis.targets[target_index].target == targets[i] && analysis.targets[target_index].support == support)
                    output.push_back(targets[i]);
        return output;
    }

    const char* target_score_type(const std::string& target)
    {
        if (target == "r_peak" || target == "ppg_systolic_peak" || target == "ppg_pulse_onset")
            return "event_detection";
        if (target == "ecg_beat_classification")
            return "classification";
        if (target == "hrv")
            return "hrv_metrics";
        if (target == "rhythm_episode" || target == "signal_quality")
            return "interval_detection";
        if (target == "ecg_delineation")
            return "ecg_delineation";
        if (target == "morphology_assertions" || target == "ecg_ppg_alignment" || target == "ppg_optical")
            return "measurement";
        return "generated_reference_only";
    }

    const char* target_primary_metric(const std::string& target)
    {
        if (target == "ecg_beat_classification")
            return "micro_f1_score";
        if (target == "hrv")
            return "metric_pass_fraction";
        if (target == "rhythm_episode" || target == "signal_quality")
            return "time_f1_score";
        if (target == "ecg_delineation")
            return "f1_score";
        if (target == "morphology_assertions" || target == "ecg_ppg_alignment" || target == "ppg_optical")
            return "tolerance_pass_fraction";
        if (target == "r_peak" || target == "ppg_systolic_peak" || target == "ppg_pulse_onset")
            return "f1_score";
        return "";
    }

    double target_default_tolerance_seconds(const std::string& target)
    {
        if (target == "ppg_systolic_peak")
            return 0.08;
        if (target == "ecg_beat_classification")
            return 0.075;
        if (target == "r_peak" || target == "ppg_pulse_onset")
            return 0.05;
        if (target == "ecg_delineation")
            return 0.04;
        return 0.0;
    }

    std::vector<std::string> target_submission_output_schemas(const std::string& target)
    {
        std::vector<std::string> output;
        if (target == "r_peak" || target == "ppg_systolic_peak" || target == "ppg_pulse_onset" || target == "ecg_beat_classification")
        {
            output.push_back("point_events_json_v1");
            output.push_back("point_events_csv_v1");
        }
        else if (target == "hrv")
            output.push_back("hrv_metrics_json_v1");
        else if (target == "rhythm_episode" || target == "signal_quality")
        {
            output.push_back("interval_events_json_v1");
            output.push_back("interval_events_csv_v1");
        }
        else if (target == "ecg_delineation")
        {
            output.push_back("point_events_json_v1");
            output.push_back("point_events_csv_v1");
        }
        else if (target == "morphology_assertions" || target == "ecg_ppg_alignment" || target == "ppg_optical")
        {
            output.push_back("measurement_values_json_v1");
            output.push_back("measurement_values_csv_v1");
        }
        return output;
    }

    std::vector<std::string> target_reference_artifacts(const std::string& target)
    {
        std::vector<std::string> output;
        if (target == "signal_quality")
        {
            output.push_back("artifact_intervals");
            output.push_back("waveform_channels");
            output.push_back("annotations_json");
            output.push_back("case_summary_json");
        }
        else if (target == "morphology_assertions")
        {
            output.push_back("conditions");
            output.push_back("fiducials");
            output.push_back("annotations_json");
            output.push_back("case_summary_json");
        }
        else if (target == "ecg_ppg_alignment")
        {
            output.push_back("ecg_ppg_timing");
            output.push_back("ppg_fiducials");
            output.push_back("annotations_json");
            output.push_back("case_summary_json");
        }
        else if (target == "ppg_optical")
        {
            output.push_back("ppg_optical_latent_csv");
            output.push_back("ppg_optical_truth_json");
            output.push_back("waveform_channels");
            output.push_back("case_summary_json");
        }
        else
        {
            output.push_back("annotations_json");
            output.push_back("case_summary_json");
        }
        return output;
    }

    std::string analysis_scoring_mode(const signal_synth::scenario_pack_analysis& analysis)
    {
        bool has_scoreable = false;
        bool has_reference = false;
        for (std::size_t i = 0; i < analysis.targets.size(); ++i)
        {
            has_scoreable = has_scoreable || analysis.targets[i].support == signal_synth::scenario_target_local_scoring;
            has_reference = has_reference || analysis.targets[i].support == signal_synth::scenario_target_reference_only;
        }
        if (has_scoreable && has_reference)
            return "mixed";
        if (has_scoreable)
            return "local";
        if (has_reference)
            return "reference_only";
        return "unsupported";
    }

    std::string recommended_profile_for_analysis(const signal_synth::scenario_pack_analysis& analysis)
    {
        for (std::size_t i = 0; i < analysis.targets.size(); ++i)
            if (analysis.targets[i].target == "hrv")
                return "benchmark";
        for (std::size_t i = 0; i < analysis.targets.size(); ++i)
            if (analysis.targets[i].support == signal_synth::scenario_target_reference_only)
                return "stress";
        for (std::size_t i = 0; i < analysis.targets.size(); ++i)
            if (analysis.targets[i].target == "ecg_beat_classification")
                return "regression";
        return analysis.case_count > 1 ? "stress" : "smoke";
    }

    void append_unique(std::vector<std::string>& values, const std::string& value)
    {
        if (!contains_string(values, value))
            values.push_back(value);
    }

    void write_target_contract(std::ostringstream& output, const signal_synth::scenario_pack_analysis& analysis, const signal_synth::scenario_pack_target_analysis& target)
    {
        const bool scoreable = target_is_scoreable(target);
        output << "{\"target\":" << json_string(target.target)
               << ",\"support\":" << json_string(signal_synth::scenario_target_support_name(target.support))
               << ",\"scoreable\":" << (scoreable ? "true" : "false")
               << ",\"score_type\":" << json_string(target_score_type(target.target))
               << ",\"case_count\":" << target.case_count
               << ",\"case_ids\":";
        write_string_array(output, case_ids_for_target(analysis, target.target));
        if (scoreable)
        {
            output << ",\"accepted_formats\":";
            write_string_array(output, target_submission_output_schemas(target.target));
            output << ",\"primary_metric\":" << json_string(target_primary_metric(target.target));
            const double tolerance = target_default_tolerance_seconds(target.target);
            if (tolerance > 0.0)
                output << ",\"default_tolerance_seconds\":" << tolerance;
        }
        else
        {
            output << ",\"reference_artifacts\":";
            write_string_array(output, target_reference_artifacts(target.target));
        }
        output << '}';
    }

    void write_target_contract_array(std::ostringstream& output, const signal_synth::scenario_pack_analysis& analysis, bool scoreable)
    {
        output << '[';
        bool first = true;
        for (std::size_t i = 0; i < analysis.targets.size(); ++i)
        {
            if (scoreable && !target_is_scoreable(analysis.targets[i]))
                continue;
            if (!scoreable && analysis.targets[i].support != signal_synth::scenario_target_reference_only)
                continue;
            output << (first ? "" : ",");
            write_target_contract(output, analysis, analysis.targets[i]);
            first = false;
        }
        output << ']';
    }

    void write_submission_output_schemas(std::ostringstream& output, const signal_synth::scenario_pack_analysis& analysis)
    {
        std::vector<std::string> schemas;
        for (std::size_t i = 0; i < analysis.targets.size(); ++i)
        {
            if (!target_is_scoreable(analysis.targets[i]))
                continue;
            const std::vector<std::string> target_schemas = target_submission_output_schemas(analysis.targets[i].target);
            for (std::size_t schema_index = 0; schema_index < target_schemas.size(); ++schema_index)
                append_unique(schemas, target_schemas[schema_index]);
        }
        write_string_array(output, schemas);
    }

    void write_output_artifacts(std::ostringstream& output, const signal_synth::scenario_pack_analysis& analysis)
    {
        output << "[{\"role\":\"manifest_json\",\"required\":true},"
               << "{\"role\":\"scoring_manifest_json\",\"required\":true},"
               << "{\"role\":\"provenance_json\",\"required\":true},"
               << "{\"role\":\"engineering_claim_boundary_txt\",\"required\":true},"
               << "{\"role\":\"case_summary_json\",\"required\":true},"
               << "{\"role\":\"annotations_json\",\"required\":true},"
               << "{\"role\":\"waveform_csv\",\"required\":true},"
               << "{\"role\":\"wfdb\",\"required\":true},"
               << "{\"role\":\"edf_bdf\",\"required\":true},"
               << "{\"role\":\"realism_metrics_json\",\"required\":true},"
               << "{\"role\":\"realism_metrics_csv\",\"required\":true},"
               << "{\"role\":\"realism_report_html\",\"required\":true}";
        bool has_hrv = false;
        bool has_wearable = false;
        bool has_optical = false;
        std::vector<std::string> reference_targets;
        for (std::size_t i = 0; i < analysis.targets.size(); ++i)
        {
            has_hrv = has_hrv || analysis.targets[i].target == "hrv";
            has_optical = has_optical || analysis.targets[i].target == "ppg_optical";
            if (analysis.targets[i].support == signal_synth::scenario_target_reference_only)
                reference_targets.push_back(analysis.targets[i].target);
        }
        for (std::size_t i = 0; i < analysis.cases.size(); ++i)
            has_wearable = has_wearable || analysis.cases[i].wearable_timebase;
        if (has_hrv)
            output << ",{\"role\":\"hrv_metrics_json\",\"required\":true},{\"role\":\"rr_tachogram_csv\",\"required\":true}";
        if (has_wearable)
            output << ",{\"role\":\"wearable_samples_csv\",\"required\":true},{\"role\":\"wearable_timestamp_truth_csv\",\"required\":true},{\"role\":\"wearable_timebase_truth_json\",\"required\":true},{\"role\":\"wearable_alignment_truth_json\",\"required\":true}";
        if (has_optical)
            output << ",{\"role\":\"ppg_optical_latent_csv\",\"required\":true},{\"role\":\"ppg_optical_truth_json\",\"required\":true}";
        if (!reference_targets.empty())
        {
            output << ",{\"role\":\"reference_ground_truth\",\"required\":true,\"targets\":";
            write_string_array(output, reference_targets);
            output << '}';
        }
        output << ']';
    }
}

namespace signal_synth
{
    scenario_pack_analysis_message::scenario_pack_analysis_message() : error(false), code(), path(), message() {}
    scenario_pack_case_analysis::scenario_pack_case_analysis()
        : case_id(), scenario_id(), duration_seconds(0.0), sampling_rate_hz(0), sample_count(0), channel_count(0), wearable_timebase(false), estimated_waveform_csv_bytes(0), estimated_binary_signal_bytes(0), estimated_package_bytes(0), estimated_peak_memory_bytes(0), targets() {}
    scenario_pack_target_analysis::scenario_pack_target_analysis() : target(), support(scenario_target_unsupported), case_count(0) {}
    scenario_pack_analysis::scenario_pack_analysis()
        : success(false), pack_id(), pack_version(), case_count(0), total_duration_seconds(0.0), total_sample_count(0), estimated_package_bytes(0), estimated_peak_memory_bytes(0), cases(), targets(), messages() {}

    const char* scenario_authoring_metadata_version()
    {
        return metadata_version;
    }

    const char* scenario_template_catalog_version()
    {
        return template_version;
    }

    const char* scenario_target_support_name(scenario_target_support support)
    {
        switch (support)
        {
        case scenario_target_local_scoring: return "local_scoring";
        case scenario_target_reference_only: return "reference_only";
        case scenario_target_unsupported: return "unsupported";
        }
        return "unsupported";
    }

    scenario_target_support scenario_target_support_for_name(const std::string& target)
    {
        if (target == "r_peak" || target == "ppg_systolic_peak" || target == "ppg_pulse_onset" || target == "ecg_beat_classification" || target == "hrv" || target == "rhythm_episode" || target == "signal_quality" || target == "ecg_delineation" || target == "morphology_assertions" || target == "ecg_ppg_alignment" || target == "ppg_optical")
            return scenario_target_local_scoring;
        return scenario_target_unsupported;
    }

    std::string scenario_authoring_metadata_json()
    {
        static const field_definition fields[] = {
            {"$.scenario_id","Scenario ID","identity","string","text","\"scenario_001\"",0,0,0,"",0,0,true},
            {"$.name","Name","identity","string","text","\"Scenario\"",0,0,0,"",0,0,true},
            {"$.description","Description","identity","string","textarea","\"\"",0,0,0,"",0,0,true},
            {"$.author","Author","identity","string","text","\"\"",0,0,0,"",0,0,true},
            {"$.tags","Tags","identity","string_array","tag_editor","[]","0","64",0,"items",0,0,true},
            {"$.duration_seconds","Duration","render","number","number","10","0.01","86400","1","s",0,0,true},
            {"$.sample_rate_hz","Sample rate","render","integer","number","500","100","1000000","1","Hz","[100,125,200,250,360,500,1000]",0,true},
            {"$.seed","Seed","render","uint64_string","text","\"5488122220847091249\"",0,0,0,"",0,0,true},
            {"$.ecg.conditions","ECG conditions","ecg","condition_array","condition_picker","[{\"code\":\"NORM\",\"severity\":1}]",0,0,0,"",0,0,true},
            {"$.ecg.heart_rate_bpm","Heart rate","ecg","number","number","70","10","400","1","bpm",0,0,true},
            {"$.ecg.rr_variability_seconds","RR variability","ecg","number","number","0","0","2","0.001","s",0,0,true},
            {"$.ecg.ectopic_every_n_beats","Ectopic cadence","ecg","integer","number","0","0","1000000","1","beats",0,0,true},
            {"$.ecg.second_degree_av_pattern","Second-degree AV pattern","ecg","string","select","\"unspecified\"",0,0,0,"","[\"unspecified\",\"mobitz_i\",\"mobitz_ii\"]",0,true},
            {"$.ecg.q_wave_territory","Q-wave territory","ecg","string","select","\"unspecified\"",0,0,0,"","[\"unspecified\",\"inferior\",\"anterior\",\"lateral\"]",0,true},
            {"$.ecg.episode_type","Episode type","episode","string","segmented","\"none\"",0,0,0,"","[\"none\",\"psvt\",\"svarr\"]",0,true},
            {"$.ecg.episode_start_seconds","Episode start","episode","number","number","2","0","86400","0.1","s",0,"{\"path\":\"$.ecg.episode_type\",\"not_equals\":\"none\"}",true},
            {"$.ecg.episode_duration_seconds","Episode duration","episode","number","number","4","0.01","86400","0.1","s",0,"{\"path\":\"$.ecg.episode_type\",\"not_equals\":\"none\"}",true},
            {"$.ecg.episode_rate_bpm","Episode rate","episode","number","number","170","100.000001","400","1","bpm",0,"{\"path\":\"$.ecg.episode_type\",\"not_equals\":\"none\"}",true},
            {"$.ecg.flutter_conduction_pattern","Flutter conduction","ecg","string","select","\"fixed\"",0,0,0,"","[\"fixed\",\"alternate_2_3\",\"cycle_2_3_4\"]","{\"condition_code\":\"AFLT\"}",true},
            {"$.ecg.pacing_mode","Pacing mode","ecg","string","segmented","\"ventricular\"",0,0,0,"","[\"ventricular\",\"atrial\",\"dual_chamber\"]","{\"condition_code\":\"PACE\"}",true},
            {"$.ecg.pacing_non_capture_every_n_beats","Non-capture cadence","ecg","integer","number","0","0","1000000","1","beats",0,"{\"condition_code\":\"PACE\"}",true},
            {"$.ecg.fidelity_policy","Fidelity policy","ecg","string","segmented","\"allow_parameterized\"",0,0,0,"","[\"native_only\",\"allow_parameterized\"]",0,true},
            {"$.hrv.enabled","HRV modulation","hrv","boolean","toggle","false",0,0,0,"",0,0,true},
            {"$.hrv.target_mean_hr_bpm","Target mean heart rate","hrv","number","number","60","30","220","1","bpm",0,"{\"path\":\"$.hrv.enabled\",\"equals\":true}",true},
            {"$.hrv.target_sdnn_seconds","Target SDNN","hrv","number","number","0.04","0","2","0.001","s",0,"{\"path\":\"$.hrv.enabled\",\"equals\":true}",true},
            {"$.hrv.lf_hf_ratio","LF/HF ratio","hrv","number","number","1","0","100","0.1","ratio",0,"{\"path\":\"$.hrv.enabled\",\"equals\":true}",true},
            {"$.hrv.lf_center_hz","LF center","hrv","number","number","0.1","0.000001","1","0.01","Hz",0,"{\"path\":\"$.hrv.enabled\",\"equals\":true}",true},
            {"$.hrv.lf_bandwidth_hz","LF bandwidth","hrv","number","number","0.04","0.000001","1","0.01","Hz",0,"{\"path\":\"$.hrv.enabled\",\"equals\":true}",true},
            {"$.hrv.hf_center_hz","HF center","hrv","number","number","0.25","0.000001","1","0.01","Hz",0,"{\"path\":\"$.hrv.enabled\",\"equals\":true}",true},
            {"$.hrv.hf_bandwidth_hz","HF bandwidth","hrv","number","number","0.12","0.000001","1","0.01","Hz",0,"{\"path\":\"$.hrv.enabled\",\"equals\":true}",true},
            {"$.hrv.respiratory_frequency_hz","Respiratory frequency","hrv","number","number","0.25","0.000001","1","0.01","Hz",0,"{\"path\":\"$.hrv.enabled\",\"equals\":true}",true},
            {"$.hrv.respiratory_amplitude_seconds","Respiratory RR amplitude","hrv","number","number","0","0","2","0.001","s",0,"{\"path\":\"$.hrv.enabled\",\"equals\":true}",true},
            {"$.hrv.minimum_rr_seconds","Minimum RR","hrv","number","number","0.25","0.000001","10","0.01","s",0,"{\"path\":\"$.hrv.enabled\",\"equals\":true}",true},
            {"$.hrv.maximum_rr_seconds","Maximum RR","hrv","number","number","3","0.000001","10","0.01","s",0,"{\"path\":\"$.hrv.enabled\",\"equals\":true}",true},
            {"$.hrv.seed","HRV seed","hrv","uint64_string","text","\"5202315400806710833\"",0,0,0,"",0,"{\"path\":\"$.hrv.enabled\",\"equals\":true}",true},
            {"$.ppg.enabled","PPG channel","ppg","boolean","toggle","false",0,0,0,"",0,0,true},
            {"$.ppg.pulse_delay_ms","Pulse delay","ppg","number","number","180","0","2000","1","ms",0,"{\"path\":\"$.ppg.enabled\",\"equals\":true}",true},
            {"$.ppg.rise_time_ms","Rise time","ppg","number","number","120","10","1000","1","ms",0,"{\"path\":\"$.ppg.enabled\",\"equals\":true}",true},
            {"$.ppg.decay_time_ms","Decay time","ppg","number","number","300","10","3000","1","ms",0,"{\"path\":\"$.ppg.enabled\",\"equals\":true}",true},
            {"$.ppg.amplitude_au","Pulse amplitude","ppg","number","number","1","0.000001","100","0.01","a.u.",0,"{\"path\":\"$.ppg.enabled\",\"equals\":true}",true},
            {"$.ppg.baseline_au","Baseline","ppg","number","number","0","-100","100","0.01","a.u.",0,"{\"path\":\"$.ppg.enabled\",\"equals\":true}",true},
            {"$.ppg.dicrotic_delay_ms","Dicrotic delay","ppg","number","number","180","0","1000","1","ms",0,"{\"path\":\"$.ppg.enabled\",\"equals\":true}",true},
            {"$.ppg.dicrotic_width_ms","Dicrotic width","ppg","number","number","80","1","500","1","ms",0,"{\"path\":\"$.ppg.enabled\",\"equals\":true}",true},
            {"$.ppg.dicrotic_amplitude_ratio","Dicrotic amplitude","ppg","number","number","0.15","0","1","0.01","ratio",0,"{\"path\":\"$.ppg.enabled\",\"equals\":true}",true},
            {"$.ppg.pulse_delay_variation_ms","Pulse-delay variation","ppg_stress","number","number","0","0","2000","1","ms",0,"{\"path\":\"$.ppg.enabled\",\"equals\":true}",true},
            {"$.ppg.pulse_delay_variation_hz","Pulse-delay variation frequency","ppg_stress","number","number","0.1","0.000001","1","0.01","Hz",0,"{\"path\":\"$.ppg.pulse_delay_variation_ms\",\"greater_than\":0}",true},
            {"$.ppg.missing_pulse_every_n_beats","Missing-pulse cadence","ppg_stress","integer","number","0","0","1000000","1","beats",0,"{\"path\":\"$.ppg.enabled\",\"equals\":true}",true},
            {"$.ppg.seed","PPG stress seed","ppg_stress","uint64_string","text","\"5787213827044626759\"",0,0,0,"",0,"{\"path\":\"$.ppg.enabled\",\"equals\":true}",true},
            {"$.ppg.pulse_delay_jitter_ms","Beat-to-beat PTT jitter","ppg_physiology","number","number","0","0","1000","1","ms",0,"{\"path\":\"$.ppg.enabled\",\"equals\":true}",true},
            {"$.ppg.low_frequency_amplitude_modulation_ratio","Low-frequency amplitude modulation","ppg_physiology","number","number","0","0","0.95","0.01","ratio",0,"{\"path\":\"$.ppg.enabled\",\"equals\":true}",true},
            {"$.ppg.low_frequency_amplitude_modulation_hz","Low-frequency amplitude modulation frequency","ppg_physiology","number","number","0.1","0.000001","1","0.01","Hz",0,"{\"path\":\"$.ppg.enabled\",\"equals\":true}",true},
            {"$.ppg.rise_time_variation_ratio","Rise-time variation","ppg_physiology","number","number","0","0","0.9","0.01","ratio",0,"{\"path\":\"$.ppg.enabled\",\"equals\":true}",true},
            {"$.ppg.decay_time_variation_ratio","Decay-time variation","ppg_physiology","number","number","0","0","0.9","0.01","ratio",0,"{\"path\":\"$.ppg.enabled\",\"equals\":true}",true},
            {"$.ppg.pac_pulse_amplitude_scale","PAC-linked pulse amplitude","ppg_physiology","number","slider","1","0","1","0.01","ratio",0,"{\"path\":\"$.ppg.enabled\",\"equals\":true}",true},
            {"$.ppg.pvc_pulse_amplitude_scale","PVC-linked pulse amplitude","ppg_physiology","number","slider","1","0","1","0.01","ratio",0,"{\"path\":\"$.ppg.enabled\",\"equals\":true}",true},
            {"$.ppg.paced_pulse_amplitude_scale","Paced-beat pulse amplitude","ppg_physiology","number","slider","1","0","1","0.01","ratio",0,"{\"path\":\"$.ppg.enabled\",\"equals\":true}",true},
            {"$.ppg.perfusion_episodes","Perfusion episodes","ppg_physiology","ppg_perfusion_episode_array","episode_editor","[]","0","64",0,"items",0,"{\"path\":\"$.ppg.enabled\",\"equals\":true}",true},
            {"$.ppg.optical.enabled","Paired red/infrared optical model","ppg_optical","boolean","toggle","false",0,0,0,"",0,"{\"path\":\"$.ppg.enabled\",\"equals\":true}",true},
            {"$.ppg.optical.profile_id","Optical site/device profile","ppg_optical","string","select","\"custom\"",0,0,0,"","[\"custom\",\"finger_transmissive_v1\",\"wrist_reflectance_v1\",\"ear_reflectance_v1\"]","{\"path\":\"$.ppg.optical.enabled\",\"equals\":true}",true},
            {"$.ppg.optical.calibration_id","Calibration identifier","ppg_optical","string","text","\"engineering_linear_v1\"",0,0,0,"",0,"{\"path\":\"$.ppg.optical.enabled\",\"equals\":true}",true},
            {"$.ppg.optical.calibration_intercept_percent","SpO2 calibration intercept","ppg_optical","number","number","110","-1000","1000","0.1","%",0,"{\"path\":\"$.ppg.optical.enabled\",\"equals\":true}",true},
            {"$.ppg.optical.calibration_slope_percent","SpO2 calibration slope","ppg_optical","number","number","-25","-1000","-0.000001","0.1","%/ratio",0,"{\"path\":\"$.ppg.optical.enabled\",\"equals\":true}",true},
            {"$.ppg.optical.minimum_spo2_percent","Minimum calibrated SpO2","ppg_optical","number","number","70","0","100","0.1","%",0,"{\"path\":\"$.ppg.optical.enabled\",\"equals\":true}",true},
            {"$.ppg.optical.maximum_spo2_percent","Maximum calibrated SpO2","ppg_optical","number","number","100","0","100","0.1","%",0,"{\"path\":\"$.ppg.optical.enabled\",\"equals\":true}",true},
            {"$.ppg.optical.baseline_spo2_percent","Baseline SpO2 target","ppg_optical","number","number","97","0","100","0.1","%",0,"{\"path\":\"$.ppg.optical.enabled\",\"equals\":true}",true},
            {"$.ppg.optical.infrared_perfusion_index_percent","Infrared perfusion index","ppg_optical","number","number","2","0.000001","100","0.1","%",0,"{\"path\":\"$.ppg.optical.enabled\",\"equals\":true}",true},
            {"$.ppg.optical.oxygenation_episodes","Oxygenation episodes","ppg_optical","ppg_oxygenation_episode_array","episode_editor","[]","0","64",0,"items",0,"{\"path\":\"$.ppg.optical.enabled\",\"equals\":true}",true},
            {"$.ppg.optical.red.dc_au","Red DC level","ppg_optical_red","number","number","1","0.000001","100","0.01","a.u.",0,"{\"path\":\"$.ppg.optical.enabled\",\"equals\":true}",true},
            {"$.ppg.optical.red.sensor_gain","Red sensor gain","ppg_optical_red","number","number","1","0.000001","100","0.01","ratio",0,"{\"path\":\"$.ppg.optical.enabled\",\"equals\":true}",true},
            {"$.ppg.optical.red.delay_ms","Red delay offset","ppg_optical_red","number","number","8","0","2000","1","ms",0,"{\"path\":\"$.ppg.optical.enabled\",\"equals\":true}",true},
            {"$.ppg.optical.red.noise_std_au","Red sensor noise","ppg_optical_red","number","number","0","0","100","0.001","a.u.",0,"{\"path\":\"$.ppg.optical.enabled\",\"equals\":true}",true},
            {"$.ppg.optical.red.ambient_offset_au","Red ambient offset","ppg_optical_red","number","number","0","-100","100","0.01","a.u.",0,"{\"path\":\"$.ppg.optical.enabled\",\"equals\":true}",true},
            {"$.ppg.optical.red.motion_sensitivity","Red motion sensitivity","ppg_optical_red","number","number","1.2","0","100","0.1","ratio",0,"{\"path\":\"$.ppg.optical.enabled\",\"equals\":true}",true},
            {"$.ppg.optical.red.ambient_sensitivity","Red ambient sensitivity","ppg_optical_red","number","number","1.1","0","100","0.1","ratio",0,"{\"path\":\"$.ppg.optical.enabled\",\"equals\":true}",true},
            {"$.ppg.optical.red.crosstalk_ratio","Red crosstalk","ppg_optical_red","number","number","0","0","1","0.01","ratio",0,"{\"path\":\"$.ppg.optical.enabled\",\"equals\":true}",true},
            {"$.ppg.optical.red.minimum_output_au","Red output minimum","ppg_optical_red","number","number","0","-100","100","0.01","a.u.",0,"{\"path\":\"$.ppg.optical.enabled\",\"equals\":true}",true},
            {"$.ppg.optical.red.maximum_output_au","Red output maximum","ppg_optical_red","number","number","5","-100","100","0.01","a.u.",0,"{\"path\":\"$.ppg.optical.enabled\",\"equals\":true}",true},
            {"$.ppg.optical.red.quantization_bits","Red quantization","ppg_optical_red","integer","number","0","0","24","1","bits",0,"{\"path\":\"$.ppg.optical.enabled\",\"equals\":true}",true},
            {"$.ppg.optical.red.seed","Red sensor seed","ppg_optical_red","uint64_string","text","\"5787203995898823729\"",0,0,0,"",0,"{\"path\":\"$.ppg.optical.enabled\",\"equals\":true}",true},
            {"$.ppg.optical.infrared.dc_au","Infrared DC level","ppg_optical_infrared","number","number","1.2","0.000001","100","0.01","a.u.",0,"{\"path\":\"$.ppg.optical.enabled\",\"equals\":true}",true},
            {"$.ppg.optical.infrared.sensor_gain","Infrared sensor gain","ppg_optical_infrared","number","number","1","0.000001","100","0.01","ratio",0,"{\"path\":\"$.ppg.optical.enabled\",\"equals\":true}",true},
            {"$.ppg.optical.infrared.delay_ms","Infrared delay offset","ppg_optical_infrared","number","number","12","0","2000","1","ms",0,"{\"path\":\"$.ppg.optical.enabled\",\"equals\":true}",true},
            {"$.ppg.optical.infrared.noise_std_au","Infrared sensor noise","ppg_optical_infrared","number","number","0","0","100","0.001","a.u.",0,"{\"path\":\"$.ppg.optical.enabled\",\"equals\":true}",true},
            {"$.ppg.optical.infrared.ambient_offset_au","Infrared ambient offset","ppg_optical_infrared","number","number","0","-100","100","0.01","a.u.",0,"{\"path\":\"$.ppg.optical.enabled\",\"equals\":true}",true},
            {"$.ppg.optical.infrared.motion_sensitivity","Infrared motion sensitivity","ppg_optical_infrared","number","number","0.8","0","100","0.1","ratio",0,"{\"path\":\"$.ppg.optical.enabled\",\"equals\":true}",true},
            {"$.ppg.optical.infrared.ambient_sensitivity","Infrared ambient sensitivity","ppg_optical_infrared","number","number","0.7","0","100","0.1","ratio",0,"{\"path\":\"$.ppg.optical.enabled\",\"equals\":true}",true},
            {"$.ppg.optical.infrared.crosstalk_ratio","Infrared crosstalk","ppg_optical_infrared","number","number","0","0","1","0.01","ratio",0,"{\"path\":\"$.ppg.optical.enabled\",\"equals\":true}",true},
            {"$.ppg.optical.infrared.minimum_output_au","Infrared output minimum","ppg_optical_infrared","number","number","0","-100","100","0.01","a.u.",0,"{\"path\":\"$.ppg.optical.enabled\",\"equals\":true}",true},
            {"$.ppg.optical.infrared.maximum_output_au","Infrared output maximum","ppg_optical_infrared","number","number","5","-100","100","0.01","a.u.",0,"{\"path\":\"$.ppg.optical.enabled\",\"equals\":true}",true},
            {"$.ppg.optical.infrared.quantization_bits","Infrared quantization","ppg_optical_infrared","integer","number","0","0","24","1","bits",0,"{\"path\":\"$.ppg.optical.enabled\",\"equals\":true}",true},
            {"$.ppg.optical.infrared.seed","Infrared sensor seed","ppg_optical_infrared","uint64_string","text","\"5787203995748675889\"",0,0,0,"",0,"{\"path\":\"$.ppg.optical.enabled\",\"equals\":true}",true},
            {"$.randomization.enabled","Controlled randomization","randomization","boolean","toggle","false",0,0,0,"",0,0,true},
            {"$.randomization.seed","Randomization seed","randomization","uint64_string","text","\"5927117558752822833\"",0,0,0,"",0,"{\"path\":\"$.randomization.enabled\",\"equals\":true}",true},
            {"$.randomization.envelopes","Parameter envelopes","randomization","randomization_envelope_array","envelope_editor","[]","1","32",0,"items",0,"{\"path\":\"$.randomization.enabled\",\"equals\":true}",true},
            {"$.physiology.respiration_frequency_hz","Respiration frequency","physiology","number","number","0.25","0.000001","1","0.01","Hz",0,0,true},
            {"$.physiology.respiratory_rr_amplitude_seconds","Respiratory RR modulation","physiology","number","number","0","0","2","0.001","s",0,0,true},
            {"$.physiology.ecg_baseline_amplitude_mv","Respiratory ECG baseline","physiology","number","number","0","0","5","0.01","mV",0,0,true},
            {"$.physiology.ppg_amplitude_modulation_ratio","Respiratory PPG amplitude modulation","physiology","number","number","0","0","1","0.01","ratio",0,"{\"path\":\"$.ppg.enabled\",\"equals\":true}",true},
            {"$.physiology.activity_start_seconds","Activity start","physiology","number","number","0","0","86400","0.1","s",0,0,true},
            {"$.physiology.activity_duration_seconds","Activity duration","physiology","number","number","0","0","86400","0.1","s",0,0,true},
            {"$.physiology.activity_intensity","Activity intensity","physiology","number","slider","0","0","1","0.01","ratio",0,0,true},
            {"$.physiology.seed","Physiology seed","physiology","uint64_string","text","\"5784961499917731377\"",0,0,0,"",0,0,true},
            {"$.output.compact","Compact long-duration output","output","boolean","toggle","false",0,0,0,"",0,0,true},
            {"$.output.retain_source_channels","Retain internal source channels","output","boolean","toggle","true",0,0,0,"",0,0,true},
            {"$.output.include_waveform_csv","Include waveform CSV","output","boolean","toggle","true",0,0,0,"",0,0,true},
            {"$.output.include_edf_bdf","Include EDF/BDF","output","boolean","toggle","true",0,0,0,"",0,0,true},
            {"$.wearable.ecg.enabled","Wearable ECG stream","wearable","boolean","toggle","true",0,0,0,"",0,0,true},
            {"$.wearable.ecg.sample_rate_hz","Wearable ECG sample rate","wearable","integer","number","250","1","1000000","1","Hz",0,"{\"path\":\"$.wearable.ecg.enabled\",\"equals\":true}",true},
            {"$.wearable.ecg.clock_offset_ms","Wearable ECG clock offset","wearable","number","number","0","-86400000","86400000","0.1","ms",0,"{\"path\":\"$.wearable.ecg.enabled\",\"equals\":true}",true},
            {"$.wearable.ecg.clock_drift_ppm","Wearable ECG clock drift","wearable","number","number","0","-100000","100000","1","ppm",0,"{\"path\":\"$.wearable.ecg.enabled\",\"equals\":true}",true},
            {"$.wearable.ecg.timestamp_jitter_ms","Wearable ECG timestamp jitter","wearable","number","number","0","0","4.5","0.01","ms",0,"{\"path\":\"$.wearable.ecg.enabled\",\"equals\":true}",true},
            {"$.wearable.ecg.packet_size_samples","Wearable ECG packet size","wearable","integer","number","25","1","1000000","1","samples",0,"{\"path\":\"$.wearable.ecg.enabled\",\"equals\":true}",true},
            {"$.wearable.ecg.packet_loss_probability","Wearable ECG packet loss","wearable","number","slider","0","0","1","0.001","probability",0,"{\"path\":\"$.wearable.ecg.enabled\",\"equals\":true}",true},
            {"$.wearable.ecg.packet_loss_burst_packets","Wearable ECG loss burst","wearable","integer","number","1","1","1000","1","packets",0,"{\"path\":\"$.wearable.ecg.enabled\",\"equals\":true}",true},
            {"$.wearable.ecg.seed","Wearable ECG seed","wearable","uint64_string","text","\"77001\"",0,0,0,"",0,"{\"path\":\"$.wearable.ecg.enabled\",\"equals\":true}",true},
            {"$.wearable.ppg.enabled","Wearable PPG stream","wearable","boolean","toggle","false",0,0,0,"",0,0,true},
            {"$.wearable.ppg.sample_rate_hz","Wearable PPG sample rate","wearable","integer","number","100","1","1000000","1","Hz",0,"{\"path\":\"$.wearable.ppg.enabled\",\"equals\":true}",true},
            {"$.wearable.ppg.clock_offset_ms","Wearable PPG clock offset","wearable","number","number","0","-86400000","86400000","0.1","ms",0,"{\"path\":\"$.wearable.ppg.enabled\",\"equals\":true}",true},
            {"$.wearable.ppg.clock_drift_ppm","Wearable PPG clock drift","wearable","number","number","0","-100000","100000","1","ppm",0,"{\"path\":\"$.wearable.ppg.enabled\",\"equals\":true}",true},
            {"$.wearable.ppg.timestamp_jitter_ms","Wearable PPG timestamp jitter","wearable","number","number","0","0","4.5","0.01","ms",0,"{\"path\":\"$.wearable.ppg.enabled\",\"equals\":true}",true},
            {"$.wearable.ppg.packet_size_samples","Wearable PPG packet size","wearable","integer","number","10","1","1000000","1","samples",0,"{\"path\":\"$.wearable.ppg.enabled\",\"equals\":true}",true},
            {"$.wearable.ppg.packet_loss_probability","Wearable PPG packet loss","wearable","number","slider","0","0","1","0.001","probability",0,"{\"path\":\"$.wearable.ppg.enabled\",\"equals\":true}",true},
            {"$.wearable.ppg.packet_loss_burst_packets","Wearable PPG loss burst","wearable","integer","number","1","1","1000","1","packets",0,"{\"path\":\"$.wearable.ppg.enabled\",\"equals\":true}",true},
            {"$.wearable.ppg.seed","Wearable PPG seed","wearable","uint64_string","text","\"77002\"",0,0,0,"",0,"{\"path\":\"$.wearable.ppg.enabled\",\"equals\":true}",true},
            {"$.wearable.accelerometer.enabled","Wearable accelerometer stream","wearable","boolean","toggle","false",0,0,0,"",0,0,true},
            {"$.wearable.accelerometer.sample_rate_hz","Wearable accelerometer sample rate","wearable","integer","number","50","1","1000000","1","Hz",0,"{\"path\":\"$.wearable.accelerometer.enabled\",\"equals\":true}",true},
            {"$.wearable.accelerometer.clock_offset_ms","Wearable accelerometer clock offset","wearable","number","number","0","-86400000","86400000","0.1","ms",0,"{\"path\":\"$.wearable.accelerometer.enabled\",\"equals\":true}",true},
            {"$.wearable.accelerometer.clock_drift_ppm","Wearable accelerometer clock drift","wearable","number","number","0","-100000","100000","1","ppm",0,"{\"path\":\"$.wearable.accelerometer.enabled\",\"equals\":true}",true},
            {"$.wearable.accelerometer.timestamp_jitter_ms","Wearable accelerometer timestamp jitter","wearable","number","number","0","0","4.5","0.01","ms",0,"{\"path\":\"$.wearable.accelerometer.enabled\",\"equals\":true}",true},
            {"$.wearable.accelerometer.packet_size_samples","Wearable accelerometer packet size","wearable","integer","number","10","1","1000000","1","samples",0,"{\"path\":\"$.wearable.accelerometer.enabled\",\"equals\":true}",true},
            {"$.wearable.accelerometer.packet_loss_probability","Wearable accelerometer packet loss","wearable","number","slider","0","0","1","0.001","probability",0,"{\"path\":\"$.wearable.accelerometer.enabled\",\"equals\":true}",true},
            {"$.wearable.accelerometer.packet_loss_burst_packets","Wearable accelerometer loss burst","wearable","integer","number","1","1","1000","1","packets",0,"{\"path\":\"$.wearable.accelerometer.enabled\",\"equals\":true}",true},
            {"$.wearable.accelerometer.seed","Wearable accelerometer seed","wearable","uint64_string","text","\"77003\"",0,0,0,"",0,"{\"path\":\"$.wearable.accelerometer.enabled\",\"equals\":true}",true},
            {"$.artifacts","Artifacts","artifacts","artifact_array","artifact_editor","[]","0","128",0,"items",0,0,true}
        };

        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << "{\"schema_version\":1,\"metadata_version\":" << json_string(metadata_version)
               << ",\"scenario_schema_version\":6,\"supported_scenario_schema_versions\":[2,3,4,5,6],\"groups\":["
               << "{\"id\":\"identity\",\"label\":\"Identity\"},{\"id\":\"render\",\"label\":\"Render\"},{\"id\":\"ecg\",\"label\":\"ECG\"},"
               << "{\"id\":\"episode\",\"label\":\"Episode\"},{\"id\":\"hrv\",\"label\":\"HRV\"},{\"id\":\"ppg\",\"label\":\"PPG\"},"
               << "{\"id\":\"ppg_stress\",\"label\":\"PPG Timing Stress\"},{\"id\":\"ppg_physiology\",\"label\":\"PPG Physiology\"},{\"id\":\"ppg_optical\",\"label\":\"PPG Optical Physiology\"},{\"id\":\"ppg_optical_red\",\"label\":\"PPG Red Sensor\"},{\"id\":\"ppg_optical_infrared\",\"label\":\"PPG Infrared Sensor\"},{\"id\":\"randomization\",\"label\":\"Randomization\"},"
               << "{\"id\":\"physiology\",\"label\":\"Physiology\"},{\"id\":\"output\",\"label\":\"Output\"},{\"id\":\"wearable\",\"label\":\"Wearable Timebase\"},{\"id\":\"artifacts\",\"label\":\"Artifacts\"}],\"fields\":[";
        for (std::size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i)
        {
            if (i) output << ',';
            write_field(output, fields[i]);
        }
        output << "],\"condition_item_fields\":["
               << "{\"name\":\"code\",\"value_type\":\"string\",\"control\":\"condition_select\"},"
               << "{\"name\":\"severity\",\"value_type\":\"number\",\"control\":\"slider\",\"minimum\":0.000001,\"maximum\":1,\"step\":0.01,\"default\":1,\"enabled_when\":\"selected condition has variable_severity=true\"}],"
               << "\"randomization_envelope_item_fields\":["
               << "{\"name\":\"parameter\",\"value_type\":\"string\",\"control\":\"select\",\"options\":[\"ecg.heart_rate_bpm\",\"ecg.rr_variability_seconds\",\"ecg.morphology.p_amplitude_mv\",\"ecg.morphology.q_amplitude_mv\",\"ecg.morphology.r_amplitude_mv\",\"ecg.morphology.s_amplitude_mv\",\"ecg.morphology.t_amplitude_mv\",\"ecg.morphology.qrs_axis_degrees\",\"ecg.morphology.t_axis_degrees\",\"ecg.morphology.qrs_duration_ms\",\"ecg.morphology.qt_interval_ms\",\"ppg.pulse_delay_ms\",\"ppg.amplitude_au\",\"ppg.optical.baseline_spo2_percent\",\"ppg.optical.infrared_perfusion_index_percent\",\"hrv.target_sdnn_seconds\",\"hrv.lf_hf_ratio\",\"physiology.activity_intensity\"]},"
               << "{\"name\":\"minimum\",\"value_type\":\"number\",\"control\":\"number\"},"
               << "{\"name\":\"maximum\",\"value_type\":\"number\",\"control\":\"number\"}],"
               << "\"ppg_perfusion_episode_item_fields\":["
               << "{\"name\":\"start_seconds\",\"value_type\":\"number\",\"control\":\"number\",\"minimum\":0,\"unit\":\"s\"},"
               << "{\"name\":\"duration_seconds\",\"value_type\":\"number\",\"control\":\"number\",\"exclusive_minimum\":0,\"unit\":\"s\"},"
               << "{\"name\":\"amplitude_scale\",\"value_type\":\"number\",\"control\":\"slider\",\"exclusive_minimum\":0,\"maximum\":1,\"step\":0.01},"
               << "{\"name\":\"rise_time_scale\",\"value_type\":\"number\",\"control\":\"number\",\"minimum\":0.25,\"maximum\":4,\"step\":0.05},"
               << "{\"name\":\"decay_time_scale\",\"value_type\":\"number\",\"control\":\"number\",\"minimum\":0.25,\"maximum\":4,\"step\":0.05},"
               << "{\"name\":\"weak_pulse_every_n_beats\",\"value_type\":\"integer\",\"control\":\"number\",\"minimum\":0},"
               << "{\"name\":\"weak_pulse_amplitude_scale\",\"value_type\":\"number\",\"control\":\"slider\",\"exclusive_minimum\":0,\"maximum\":1,\"step\":0.01},"
               << "{\"name\":\"missing_pulse_every_n_beats\",\"value_type\":\"integer\",\"control\":\"number\",\"minimum\":0}],"
               << "\"ppg_oxygenation_episode_item_fields\":["
               << "{\"name\":\"start_seconds\",\"value_type\":\"number\",\"control\":\"number\",\"minimum\":0,\"unit\":\"s\"},"
               << "{\"name\":\"duration_seconds\",\"value_type\":\"number\",\"control\":\"number\",\"exclusive_minimum\":0,\"unit\":\"s\"},"
               << "{\"name\":\"transition_seconds\",\"value_type\":\"number\",\"control\":\"number\",\"minimum\":0,\"unit\":\"s\"},"
               << "{\"name\":\"target_spo2_percent\",\"value_type\":\"number\",\"control\":\"number\",\"minimum\":0,\"maximum\":100,\"step\":0.1,\"unit\":\"%\"}],"
               << "\"conditions\":[";
        const ecg_condition_info* conditions = ecg_condition_catalog();
        for (unsigned int i = 0; i < ecg_condition_catalog_size(); ++i)
        {
            const ecg_condition_info& condition = conditions[i];
            output << (i ? "," : "") << "{\"code\":" << json_string(condition.scp_code)
                   << ",\"name\":" << json_string(condition.name)
                   << ",\"category\":" << json_string(category_name(condition.category))
                   << ",\"support\":" << json_string(condition_support_name(condition.support))
                   << ",\"variable_severity\":" << (ecg_condition_supports_variable_severity(condition.code) ? "true" : "false")
                   << ",\"statement_roles\":[";
            bool first = true;
            if (condition.diagnostic_statement) { output << "\"diagnostic\""; first = false; }
            if (condition.form_statement) { output << (first ? "" : ",") << "\"form\""; first = false; }
            if (condition.rhythm_statement) output << (first ? "" : ",") << "\"rhythm\"";
            output << "]}";
        }
        output << "],\"artifacts\":[";
        for (int type = signal_quality_ecg_baseline_wander; type <= signal_quality_ppg_sensor_saturation; ++type)
        {
            const signal_quality_artifact_type artifact = static_cast<signal_quality_artifact_type>(type);
            const bool ppg = signal_quality_artifact_is_ppg(artifact);
            output << (type ? "," : "") << "{\"type\":" << json_string(signal_quality_artifact_type_name(artifact))
                   << ",\"channel_family\":" << json_string(ppg ? "ppg" : "ecg")
                   << ",\"item_fields\":["
                   << "{\"name\":\"start_seconds\",\"value_type\":\"number\",\"control\":\"number\",\"minimum\":0,\"unit\":\"s\"},"
                   << "{\"name\":\"duration_seconds\",\"value_type\":\"number\",\"control\":\"number\",\"exclusive_minimum\":0,\"unit\":\"s\"},"
                   << "{\"name\":\"severity\",\"value_type\":\"number\",\"control\":\"slider\",\"minimum\":0,\"maximum\":1,\"step\":0.01},"
                   << "{\"name\":\"seed\",\"value_type\":\"uint64_string\",\"control\":\"text\",\"serialization\":\"json_integer\"},"
                   << "{\"name\":\"channels\",\"value_type\":\"string_array\",\"control\":\"channel_picker\",\"options\":"
                   << (ppg ? "[\"all_ppg\"]" : "[\"all_ecg\",\"I\",\"II\",\"III\",\"aVR\",\"aVL\",\"aVF\",\"V1\",\"V2\",\"V3\",\"V4\",\"V5\",\"V6\"]")
                   << "}]}";
        }
        output << "],\"targets\":["
               << "{\"name\":\"r_peak\",\"support\":\"local_scoring\",\"requires\":[]},"
               << "{\"name\":\"ppg_systolic_peak\",\"support\":\"local_scoring\",\"requires\":[\"ppg.enabled\"]},"
               << "{\"name\":\"ppg_pulse_onset\",\"support\":\"local_scoring\",\"requires\":[\"ppg.enabled\"]},"
               << "{\"name\":\"ecg_beat_classification\",\"support\":\"local_scoring\",\"requires\":[]},"
               << "{\"name\":\"hrv\",\"support\":\"local_scoring\",\"requires\":[\"hrv.enabled\",\"duration_seconds>=300\"]},"
               << "{\"name\":\"rhythm_episode\",\"support\":\"local_scoring\",\"requires\":[\"ecg.episode_type!=none\"]},"
               << "{\"name\":\"signal_quality\",\"support\":\"local_scoring\",\"requires\":[\"artifacts.length>0\"]},"
               << "{\"name\":\"ecg_delineation\",\"support\":\"local_scoring\",\"requires\":[]},"
               << "{\"name\":\"morphology_assertions\",\"support\":\"local_scoring\",\"requires\":[\"ecg.conditions\"]},"
               << "{\"name\":\"ecg_ppg_alignment\",\"support\":\"local_scoring\",\"requires\":[\"ppg.enabled\"]},"
               << "{\"name\":\"ppg_optical\",\"support\":\"local_scoring\",\"requires\":[\"ppg.optical.enabled\"]}],"
               << "\"cross_field_rules\":["
               << "{\"id\":\"sample_count\",\"expression\":\"duration_seconds * sample_rate_hz is an integer in [1,4294967295]\",\"message\":\"Duration and sample rate must produce a positive 32-bit sample count.\"},"
               << "{\"id\":\"hrv_window\",\"expression\":\"!hrv.enabled || duration_seconds >= 300\",\"message\":\"Enabled spectral HRV requires at least 300 seconds.\"},"
               << "{\"id\":\"hrv_respiration_amplitude\",\"expression\":\"hrv.respiratory_amplitude_seconds <= sqrt(2) * hrv.target_sdnn_seconds\",\"message\":\"Respiratory RR amplitude cannot exceed the requested total SDNN envelope.\"},"
               << "{\"id\":\"rr_bounds\",\"expression\":\"hrv.minimum_rr_seconds < 60/hrv.target_mean_hr_bpm < hrv.maximum_rr_seconds\",\"message\":\"Mean RR must lie strictly inside the configured RR bounds.\"},"
               << "{\"id\":\"ppg_width\",\"expression\":\"ppg.rise_time_ms + ppg.decay_time_ms <= 5000\",\"message\":\"Combined PPG rise and decay time cannot exceed 5000 ms.\"},"
               << "{\"id\":\"ppg_dicrotic\",\"expression\":\"ppg.dicrotic_delay_ms <= ppg.decay_time_ms\",\"message\":\"Dicrotic delay cannot exceed decay time.\"},"
               << "{\"id\":\"episode_bounds\",\"expression\":\"ecg.episode_type == 'none' || ecg.episode_start_seconds + ecg.episode_duration_seconds <= duration_seconds\",\"message\":\"Episode must fit inside the rendered duration.\"},"
               << "{\"id\":\"artifact_bounds\",\"expression\":\"forall artifact: start_seconds + duration_seconds <= duration_seconds\",\"message\":\"Every artifact must fit inside the scenario.\"},"
               << "{\"id\":\"activity_bounds\",\"expression\":\"physiology.activity_start_seconds + physiology.activity_duration_seconds <= duration_seconds\",\"message\":\"Activity interval must fit inside the scenario.\"},"
               << "{\"id\":\"ppg_stress_requires_ppg\",\"expression\":\"ppg.enabled || (ppg.pulse_delay_variation_ms == 0 && ppg.missing_pulse_every_n_beats == 0 && physiology.ppg_amplitude_modulation_ratio == 0)\",\"message\":\"PPG stress controls require an enabled PPG channel.\"},"
               << "{\"id\":\"ppg_physiology_requires_ppg\",\"expression\":\"ppg.enabled || (ppg.pulse_delay_jitter_ms == 0 && ppg.low_frequency_amplitude_modulation_ratio == 0 && ppg.rise_time_variation_ratio == 0 && ppg.decay_time_variation_ratio == 0 && ppg.pac_pulse_amplitude_scale == 1 && ppg.pvc_pulse_amplitude_scale == 1 && ppg.paced_pulse_amplitude_scale == 1 && ppg.perfusion_episodes.length == 0)\",\"message\":\"PPG physiology controls require an enabled PPG channel.\"},"
               << "{\"id\":\"ppg_perfusion_bounds\",\"expression\":\"forall episode: start_seconds + duration_seconds <= duration_seconds and episodes do not overlap\",\"message\":\"Perfusion episodes must fit inside the scenario and must not overlap.\"},"
               << "{\"id\":\"ppg_optical_requires_ppg\",\"expression\":\"!ppg.optical.enabled || ppg.enabled\",\"message\":\"Optical red/infrared channels require an enabled PPG source.\"},"
               << "{\"id\":\"ppg_oxygenation_bounds\",\"expression\":\"forall ppg.optical oxygenation episode: start_seconds + duration_seconds <= duration_seconds and episodes do not overlap\",\"message\":\"Oxygenation episodes must fit inside the scenario and must not overlap.\"},"
               << "{\"id\":\"wearable_sources\",\"expression\":\"all wearable streams disabled || (wearable.ecg.enabled && (!wearable.ppg.enabled || ppg.enabled) && (!wearable.accelerometer.enabled || physiology.activity_intensity > 0 || artifacts contain PPG motion))\",\"message\":\"Enabled wearable streams require ECG plus every selected latent source.\"},"
               << "{\"id\":\"wearable_rates\",\"expression\":\"all enabled wearable sample rates <= sample_rate_hz and timestamp_jitter_ms <= 450/sample_rate_hz\",\"message\":\"Wearable rates and jitter must preserve supported resampling and monotonic timestamps.\"},"
               << "{\"id\":\"compact_output\",\"expression\":\"!output.compact || (!output.retain_source_channels && !output.include_waveform_csv && !output.include_edf_bdf)\",\"message\":\"Compact output omits source channels, waveform CSV, EDF and BDF.\"}"
               << "]}";
        return output.str();
    }

    std::string scenario_template_catalog_json()
    {
        static const char* templates[] = {
            "{\"template_id\":\"ecg_rpeak_clean\",\"name\":\"Clean R-peak baseline\",\"description\":\"Regular clean ECG for detector integration and smoke tests.\",\"difficulty\":\"smoke\",\"feature_tags\":[\"ecg\",\"r_peak\",\"clean\"],\"targets\":[\"r_peak\"],\"editable_paths\":[\"$.duration_seconds\",\"$.sample_rate_hz\",\"$.seed\",\"$.ecg.heart_rate_bpm\"],\"scenario\":{\"schema_version\":2,\"scenario_id\":\"ecg_rpeak_clean\",\"name\":\"Clean R-peak baseline\",\"description\":\"Template-generated clean ECG.\",\"author\":\"Synsigra\",\"tags\":[\"template\",\"r_peak\",\"clean\"],\"duration_seconds\":30,\"sample_rate_hz\":500,\"seed\":7101,\"ecg\":{\"heart_rate_bpm\":70,\"rr_variability_seconds\":0,\"ectopic_every_n_beats\":0,\"second_degree_av_pattern\":\"unspecified\",\"q_wave_territory\":\"unspecified\",\"episode_type\":\"none\",\"episode_start_seconds\":2,\"episode_duration_seconds\":4,\"episode_rate_bpm\":170,\"flutter_conduction_pattern\":\"fixed\",\"pacing_mode\":\"ventricular\",\"pacing_non_capture_every_n_beats\":0,\"fidelity_policy\":\"allow_parameterized\",\"conditions\":[{\"code\":\"NORM\",\"severity\":1}]},\"ppg\":{\"enabled\":false,\"pulse_delay_ms\":180,\"rise_time_ms\":120,\"decay_time_ms\":300,\"amplitude_au\":1,\"baseline_au\":0,\"dicrotic_delay_ms\":180,\"dicrotic_width_ms\":80,\"dicrotic_amplitude_ratio\":0.15}}}",
            "{\"template_id\":\"ecg_rpeak_artifact\",\"name\":\"R-peak artifact stress\",\"description\":\"ECG baseline wander, powerline and EMG intervals for robust detector QA.\",\"difficulty\":\"stress\",\"feature_tags\":[\"ecg\",\"r_peak\",\"artifact\"],\"targets\":[\"r_peak\",\"signal_quality\"],\"editable_paths\":[\"$.duration_seconds\",\"$.sample_rate_hz\",\"$.seed\",\"$.artifacts\"],\"scenario\":{\"schema_version\":2,\"scenario_id\":\"ecg_rpeak_artifact\",\"name\":\"R-peak artifact stress\",\"description\":\"Template-generated ECG artifact stress case.\",\"author\":\"Synsigra\",\"tags\":[\"template\",\"r_peak\",\"artifact\"],\"duration_seconds\":30,\"sample_rate_hz\":500,\"seed\":7102,\"ecg\":{\"heart_rate_bpm\":72,\"rr_variability_seconds\":0.01,\"ectopic_every_n_beats\":0,\"second_degree_av_pattern\":\"unspecified\",\"q_wave_territory\":\"unspecified\",\"episode_type\":\"none\",\"episode_start_seconds\":2,\"episode_duration_seconds\":4,\"episode_rate_bpm\":170,\"flutter_conduction_pattern\":\"fixed\",\"pacing_mode\":\"ventricular\",\"pacing_non_capture_every_n_beats\":0,\"fidelity_policy\":\"allow_parameterized\",\"conditions\":[{\"code\":\"NORM\",\"severity\":1}]},\"ppg\":{\"enabled\":false,\"pulse_delay_ms\":180,\"rise_time_ms\":120,\"decay_time_ms\":300,\"amplitude_au\":1,\"baseline_au\":0,\"dicrotic_delay_ms\":180,\"dicrotic_width_ms\":80,\"dicrotic_amplitude_ratio\":0.15},\"artifacts\":[{\"type\":\"ecg_baseline_wander\",\"start_seconds\":2,\"duration_seconds\":24,\"severity\":0.45,\"seed\":71021,\"channels\":[\"all_ecg\"]},{\"type\":\"ecg_emg_noise\",\"start_seconds\":10,\"duration_seconds\":5,\"severity\":0.6,\"seed\":71022,\"channels\":[\"II\",\"V2\",\"V5\"]}]}}",
            "{\"template_id\":\"ecg_hrv_benchmark\",\"name\":\"Five-minute HRV benchmark\",\"description\":\"Balanced deterministic LF/HF and respiratory RR modulation.\",\"difficulty\":\"benchmark\",\"feature_tags\":[\"ecg\",\"hrv\",\"lf_hf\",\"respiration\"],\"targets\":[\"hrv\",\"r_peak\"],\"editable_paths\":[\"$.duration_seconds\",\"$.sample_rate_hz\",\"$.seed\",\"$.hrv\"],\"scenario\":{\"schema_version\":2,\"scenario_id\":\"ecg_hrv_benchmark\",\"name\":\"Five-minute HRV benchmark\",\"description\":\"Template-generated HRV benchmark.\",\"author\":\"Synsigra\",\"tags\":[\"template\",\"hrv\",\"benchmark\"],\"duration_seconds\":300,\"sample_rate_hz\":100,\"seed\":7103,\"ecg\":{\"heart_rate_bpm\":60,\"rr_variability_seconds\":0.05,\"ectopic_every_n_beats\":0,\"second_degree_av_pattern\":\"unspecified\",\"q_wave_territory\":\"unspecified\",\"episode_type\":\"none\",\"episode_start_seconds\":2,\"episode_duration_seconds\":4,\"episode_rate_bpm\":170,\"flutter_conduction_pattern\":\"fixed\",\"pacing_mode\":\"ventricular\",\"pacing_non_capture_every_n_beats\":0,\"fidelity_policy\":\"allow_parameterized\",\"conditions\":[{\"code\":\"NORM\",\"severity\":1}]},\"hrv\":{\"enabled\":true,\"target_mean_hr_bpm\":60,\"target_sdnn_seconds\":0.05,\"lf_hf_ratio\":1,\"lf_center_hz\":0.1,\"lf_bandwidth_hz\":0.04,\"hf_center_hz\":0.25,\"hf_bandwidth_hz\":0.12,\"respiratory_frequency_hz\":0.25,\"respiratory_amplitude_seconds\":0.03,\"minimum_rr_seconds\":0.5,\"maximum_rr_seconds\":1.5,\"seed\":7103},\"ppg\":{\"enabled\":false,\"pulse_delay_ms\":180,\"rise_time_ms\":120,\"decay_time_ms\":300,\"amplitude_au\":1,\"baseline_au\":0,\"dicrotic_delay_ms\":180,\"dicrotic_width_ms\":80,\"dicrotic_amplitude_ratio\":0.15}}}",
            "{\"template_id\":\"ecg_beat_ectopy\",\"name\":\"PAC/PVC beat classification\",\"description\":\"Periodic ectopy template for beat detector and classifier QA.\",\"difficulty\":\"regression\",\"feature_tags\":[\"ecg\",\"beat_classification\",\"ectopy\"],\"targets\":[\"r_peak\",\"ecg_beat_classification\"],\"editable_paths\":[\"$.duration_seconds\",\"$.seed\",\"$.ecg.ectopic_every_n_beats\",\"$.ecg.conditions\"],\"scenario\":{\"schema_version\":2,\"scenario_id\":\"ecg_beat_ectopy\",\"name\":\"PVC beat classification\",\"description\":\"Template-generated periodic PVC case.\",\"author\":\"Synsigra\",\"tags\":[\"template\",\"beat_classification\",\"pvc\"],\"duration_seconds\":30,\"sample_rate_hz\":500,\"seed\":7104,\"ecg\":{\"heart_rate_bpm\":72,\"rr_variability_seconds\":0,\"ectopic_every_n_beats\":5,\"second_degree_av_pattern\":\"unspecified\",\"q_wave_territory\":\"unspecified\",\"episode_type\":\"none\",\"episode_start_seconds\":2,\"episode_duration_seconds\":4,\"episode_rate_bpm\":170,\"flutter_conduction_pattern\":\"fixed\",\"pacing_mode\":\"ventricular\",\"pacing_non_capture_every_n_beats\":0,\"fidelity_policy\":\"allow_parameterized\",\"conditions\":[{\"code\":\"PVC\",\"severity\":0.8}]},\"ppg\":{\"enabled\":false,\"pulse_delay_ms\":180,\"rise_time_ms\":120,\"decay_time_ms\":300,\"amplitude_au\":1,\"baseline_au\":0,\"dicrotic_delay_ms\":180,\"dicrotic_width_ms\":80,\"dicrotic_amplitude_ratio\":0.15}}}",
            "{\"template_id\":\"ecg_ppg_peak\",\"name\":\"Linked ECG/PPG peaks\",\"description\":\"Clean linked ECG and green PPG for peak and timing QA.\",\"difficulty\":\"smoke\",\"feature_tags\":[\"ecg\",\"ppg\",\"peak\",\"timing\"],\"targets\":[\"r_peak\",\"ppg_systolic_peak\",\"ecg_ppg_alignment\"],\"editable_paths\":[\"$.duration_seconds\",\"$.sample_rate_hz\",\"$.seed\",\"$.ppg\"],\"scenario\":{\"schema_version\":2,\"scenario_id\":\"ecg_ppg_peak\",\"name\":\"Linked ECG PPG peaks\",\"description\":\"Template-generated linked ECG and PPG.\",\"author\":\"Synsigra\",\"tags\":[\"template\",\"ecg\",\"ppg\"],\"duration_seconds\":30,\"sample_rate_hz\":500,\"seed\":7105,\"ecg\":{\"heart_rate_bpm\":70,\"rr_variability_seconds\":0.015,\"ectopic_every_n_beats\":0,\"second_degree_av_pattern\":\"unspecified\",\"q_wave_territory\":\"unspecified\",\"episode_type\":\"none\",\"episode_start_seconds\":2,\"episode_duration_seconds\":4,\"episode_rate_bpm\":170,\"flutter_conduction_pattern\":\"fixed\",\"pacing_mode\":\"ventricular\",\"pacing_non_capture_every_n_beats\":0,\"fidelity_policy\":\"allow_parameterized\",\"conditions\":[{\"code\":\"NORM\",\"severity\":1}]},\"ppg\":{\"enabled\":true,\"pulse_delay_ms\":180,\"rise_time_ms\":120,\"decay_time_ms\":300,\"amplitude_au\":1,\"baseline_au\":0,\"dicrotic_delay_ms\":180,\"dicrotic_width_ms\":80,\"dicrotic_amplitude_ratio\":0.15}}}",
            "{\"template_id\":\"ecg_psvt_transition\",\"name\":\"PSVT transition episode\",\"description\":\"Sinus baseline with explicit PSVT onset and offset ground truth.\",\"difficulty\":\"stress\",\"feature_tags\":[\"ecg\",\"rhythm\",\"psvt\",\"transition\"],\"targets\":[\"r_peak\",\"ecg_beat_classification\"],\"editable_paths\":[\"$.duration_seconds\",\"$.seed\",\"$.ecg.episode_start_seconds\",\"$.ecg.episode_duration_seconds\",\"$.ecg.episode_rate_bpm\"],\"scenario\":{\"schema_version\":2,\"scenario_id\":\"ecg_psvt_transition\",\"name\":\"PSVT transition episode\",\"description\":\"Template-generated PSVT episode.\",\"author\":\"Synsigra\",\"tags\":[\"template\",\"rhythm\",\"psvt\",\"transition\"],\"duration_seconds\":30,\"sample_rate_hz\":500,\"seed\":7106,\"ecg\":{\"heart_rate_bpm\":72,\"rr_variability_seconds\":0,\"ectopic_every_n_beats\":0,\"second_degree_av_pattern\":\"unspecified\",\"q_wave_territory\":\"unspecified\",\"episode_type\":\"psvt\",\"episode_start_seconds\":10,\"episode_duration_seconds\":10,\"episode_rate_bpm\":180,\"flutter_conduction_pattern\":\"fixed\",\"pacing_mode\":\"ventricular\",\"pacing_non_capture_every_n_beats\":0,\"fidelity_policy\":\"allow_parameterized\",\"conditions\":[{\"code\":\"PSVT\",\"severity\":1}]},\"ppg\":{\"enabled\":false,\"pulse_delay_ms\":180,\"rise_time_ms\":120,\"decay_time_ms\":300,\"amplitude_au\":1,\"baseline_au\":0,\"dicrotic_delay_ms\":180,\"dicrotic_width_ms\":80,\"dicrotic_amplitude_ratio\":0.15}}}",
            "{\"template_id\":\"wearable_ecg_ppg_stress\",\"name\":\"Wearable ECG/PPG stress\",\"description\":\"Reproducible cardiorespiratory, activity and ECG/PPG synchronization stress.\",\"difficulty\":\"stress\",\"feature_tags\":[\"ecg\",\"ppg\",\"wearable\",\"respiration\",\"activity\",\"timing\"],\"targets\":[\"r_peak\",\"ppg_systolic_peak\",\"ecg_ppg_alignment\"],\"editable_paths\":[\"$.duration_seconds\",\"$.sample_rate_hz\",\"$.randomization\",\"$.physiology\",\"$.ppg\"],\"scenario\":{\"schema_version\":3,\"scenario_id\":\"wearable_ecg_ppg_stress\",\"name\":\"Wearable ECG PPG stress\",\"description\":\"Template-generated multimodal wearable stress case.\",\"author\":\"Synsigra\",\"tags\":[\"activity\",\"ppg\",\"respiration\",\"template\",\"wearable\"],\"duration_seconds\":60,\"sample_rate_hz\":250,\"seed\":7107,\"ecg\":{\"heart_rate_bpm\":72,\"rr_variability_seconds\":0.025,\"ectopic_every_n_beats\":0,\"second_degree_av_pattern\":\"unspecified\",\"q_wave_territory\":\"unspecified\",\"episode_type\":\"none\",\"episode_start_seconds\":2,\"episode_duration_seconds\":4,\"episode_rate_bpm\":170,\"flutter_conduction_pattern\":\"fixed\",\"pacing_mode\":\"ventricular\",\"pacing_non_capture_every_n_beats\":0,\"fidelity_policy\":\"allow_parameterized\",\"conditions\":[{\"code\":\"NORM\",\"severity\":1}]},\"ppg\":{\"enabled\":true,\"pulse_delay_ms\":180,\"rise_time_ms\":120,\"decay_time_ms\":300,\"amplitude_au\":1,\"baseline_au\":0,\"dicrotic_delay_ms\":180,\"dicrotic_width_ms\":80,\"dicrotic_amplitude_ratio\":0.15,\"pulse_delay_variation_ms\":25,\"pulse_delay_variation_hz\":0.1,\"missing_pulse_every_n_beats\":9,\"seed\":71071},\"randomization\":{\"enabled\":true,\"seed\":71072,\"envelopes\":[{\"parameter\":\"ecg.heart_rate_bpm\",\"minimum\":65,\"maximum\":85},{\"parameter\":\"ppg.pulse_delay_ms\",\"minimum\":150,\"maximum\":230}]},\"physiology\":{\"respiration_frequency_hz\":0.22,\"respiratory_rr_amplitude_seconds\":0.035,\"ecg_baseline_amplitude_mv\":0.08,\"ppg_amplitude_modulation_ratio\":0.18,\"activity_start_seconds\":20,\"activity_duration_seconds\":25,\"activity_intensity\":0.5,\"seed\":71073},\"output\":{\"compact\":false,\"retain_source_channels\":true,\"include_waveform_csv\":true,\"include_edf_bdf\":true}}}",
            "{\"template_id\":\"ppg_perfusion_stress\",\"name\":\"PPG physiology and perfusion stress\",\"description\":\"Variable PTT and morphology with low-perfusion, weak-pulse and missing-pulse ground truth.\",\"difficulty\":\"stress\",\"feature_tags\":[\"ppg\",\"perfusion\",\"weak_pulse\",\"missing_pulse\",\"ptt\",\"morphology\"],\"targets\":[\"ppg_systolic_peak\",\"ecg_ppg_alignment\"],\"editable_paths\":[\"$.duration_seconds\",\"$.sample_rate_hz\",\"$.ppg\"],\"scenario\":{\"schema_version\":4,\"scenario_id\":\"ppg_perfusion_stress\",\"name\":\"PPG physiology and perfusion stress\",\"description\":\"Template-generated PPG physiology-v2 case.\",\"author\":\"Synsigra\",\"tags\":[\"perfusion\",\"ppg\",\"stress\",\"template\"],\"duration_seconds\":60,\"sample_rate_hz\":250,\"seed\":7108,\"ecg\":{\"heart_rate_bpm\":70,\"rr_variability_seconds\":0.02,\"ectopic_every_n_beats\":0,\"second_degree_av_pattern\":\"unspecified\",\"q_wave_territory\":\"unspecified\",\"episode_type\":\"none\",\"episode_start_seconds\":2,\"episode_duration_seconds\":4,\"episode_rate_bpm\":170,\"flutter_conduction_pattern\":\"fixed\",\"pacing_mode\":\"ventricular\",\"pacing_non_capture_every_n_beats\":0,\"fidelity_policy\":\"allow_parameterized\",\"conditions\":[{\"code\":\"NORM\",\"severity\":1}]},\"ppg\":{\"enabled\":true,\"pulse_delay_ms\":185,\"rise_time_ms\":120,\"decay_time_ms\":300,\"amplitude_au\":1,\"baseline_au\":0,\"dicrotic_delay_ms\":180,\"dicrotic_width_ms\":80,\"dicrotic_amplitude_ratio\":0.15,\"pulse_delay_variation_ms\":12,\"pulse_delay_variation_hz\":0.08,\"missing_pulse_every_n_beats\":0,\"seed\":71081,\"pulse_delay_jitter_ms\":10,\"low_frequency_amplitude_modulation_ratio\":0.2,\"low_frequency_amplitude_modulation_hz\":0.05,\"rise_time_variation_ratio\":0.15,\"decay_time_variation_ratio\":0.2,\"perfusion_episodes\":[{\"start_seconds\":15,\"duration_seconds\":30,\"amplitude_scale\":0.4,\"rise_time_scale\":1.3,\"decay_time_scale\":1.2,\"weak_pulse_every_n_beats\":3,\"weak_pulse_amplitude_scale\":0.3,\"missing_pulse_every_n_beats\":7}]},\"randomization\":{\"enabled\":false,\"seed\":71082,\"envelopes\":[]},\"physiology\":{\"respiration_frequency_hz\":0.22,\"respiratory_rr_amplitude_seconds\":0.025,\"ecg_baseline_amplitude_mv\":0.03,\"ppg_amplitude_modulation_ratio\":0.15,\"activity_start_seconds\":0,\"activity_duration_seconds\":0,\"activity_intensity\":0,\"seed\":71083},\"output\":{\"compact\":false,\"retain_source_channels\":true,\"include_waveform_csv\":true,\"include_edf_bdf\":true}}}",
            "{\"template_id\":\"ppg_arrhythmia_pulse_loss\",\"name\":\"Arrhythmia-linked PPG pulse loss\",\"description\":\"Periodic PVC ECG beats with deterministic missing PPG pulses for pulse detector false-positive and exclusion-window QA.\",\"difficulty\":\"stress\",\"feature_tags\":[\"ecg\",\"ppg\",\"arrhythmia\",\"pvc\",\"pulse_loss\",\"missing_pulse\"],\"targets\":[\"ppg_systolic_peak\",\"ppg_pulse_onset\",\"ecg_beat_classification\"],\"editable_paths\":[\"$.duration_seconds\",\"$.sample_rate_hz\",\"$.ecg.ectopic_every_n_beats\",\"$.ppg.pvc_pulse_amplitude_scale\",\"$.ppg.pac_pulse_amplitude_scale\",\"$.ppg.paced_pulse_amplitude_scale\"],\"scenario\":{\"schema_version\":4,\"scenario_id\":\"ppg_arrhythmia_pulse_loss\",\"name\":\"Arrhythmia-linked PPG pulse loss\",\"description\":\"Template-generated PVC-linked missing pulse case for engineering QA.\",\"author\":\"Synsigra\",\"tags\":[\"arrhythmia\",\"missing_pulse\",\"ppg\",\"pvc\",\"template\"],\"duration_seconds\":40,\"sample_rate_hz\":250,\"seed\":7109,\"ecg\":{\"heart_rate_bpm\":72,\"rr_variability_seconds\":0.01,\"ectopic_every_n_beats\":5,\"second_degree_av_pattern\":\"unspecified\",\"q_wave_territory\":\"unspecified\",\"episode_type\":\"none\",\"episode_start_seconds\":2,\"episode_duration_seconds\":4,\"episode_rate_bpm\":170,\"flutter_conduction_pattern\":\"fixed\",\"pacing_mode\":\"ventricular\",\"pacing_non_capture_every_n_beats\":0,\"fidelity_policy\":\"allow_parameterized\",\"conditions\":[{\"code\":\"PVC\",\"severity\":0.8}]},\"ppg\":{\"enabled\":true,\"pulse_delay_ms\":185,\"rise_time_ms\":120,\"decay_time_ms\":300,\"amplitude_au\":1,\"baseline_au\":0,\"dicrotic_delay_ms\":180,\"dicrotic_width_ms\":80,\"dicrotic_amplitude_ratio\":0.15,\"pulse_delay_variation_ms\":8,\"pulse_delay_variation_hz\":0.08,\"missing_pulse_every_n_beats\":0,\"seed\":71091,\"pulse_delay_jitter_ms\":4,\"low_frequency_amplitude_modulation_ratio\":0.05,\"low_frequency_amplitude_modulation_hz\":0.05,\"rise_time_variation_ratio\":0.05,\"decay_time_variation_ratio\":0.05,\"pac_pulse_amplitude_scale\":1,\"pvc_pulse_amplitude_scale\":0,\"paced_pulse_amplitude_scale\":1,\"perfusion_episodes\":[]},\"randomization\":{\"enabled\":false,\"seed\":71092,\"envelopes\":[]},\"physiology\":{\"respiration_frequency_hz\":0.22,\"respiratory_rr_amplitude_seconds\":0.015,\"ecg_baseline_amplitude_mv\":0.02,\"ppg_amplitude_modulation_ratio\":0.05,\"activity_start_seconds\":0,\"activity_duration_seconds\":0,\"activity_intensity\":0,\"seed\":71093},\"output\":{\"compact\":false,\"retain_source_channels\":true,\"include_waveform_csv\":true,\"include_edf_bdf\":true}}}"
        };
        std::ostringstream output;
        output << "{\"schema_version\":1,\"catalog_version\":" << json_string(template_version)
               << ",\"difficulty_values\":[\"smoke\",\"regression\",\"stress\",\"benchmark\"],\"templates\":[";
        for (std::size_t i = 0; i < sizeof(templates) / sizeof(templates[0]); ++i)
            output << (i ? "," : "") << templates[i];
        output << "]}";
        return output.str();
    }

    bool analyze_scenario_pack(const ecg_pack_manifest& manifest, const std::vector<ecg_scenario_document>& scenarios, scenario_pack_analysis& analysis)
    {
        scenario_pack_analysis fresh;
        fresh.pack_id = manifest.pack_id;
        fresh.pack_version = manifest.version;
        if (scenarios.size() != manifest.scenarios.size())
        {
            fresh.messages.push_back(make_message(true, "CASE_COUNT_MISMATCH", "$.scenarios", "Parsed scenario count does not match the pack manifest."));
            analysis = fresh;
            return false;
        }
        for (std::size_t i = 0; i < scenarios.size(); ++i)
        {
            const ecg_pack_scenario& pack_case = manifest.scenarios[i];
            const ecg_scenario_document& document = scenarios[i];
            ecg_scenario_json_result document_validation;
            if (!write_ecg_scenario_json(document, document_validation))
            {
                for (std::size_t message_index = 0; message_index < document_validation.messages.size(); ++message_index)
                {
                    const ecg_scenario_json_message& source = document_validation.messages[message_index];
                    fresh.messages.push_back(make_message(true, "INVALID_SCENARIO", "$.scenarios[" + index_string(i) + "]" + (source.path.empty() ? "" : source.path.substr(1)), source.message));
                }
            }
            scenario_pack_case_analysis item;
            item.case_id = pack_case.id;
            item.scenario_id = document.scenario_id;
            item.duration_seconds = document.duration_seconds;
            item.sampling_rate_hz = document.ecg.sampling_rate_hz();
            item.sample_count = document.sample_count();
            item.wearable_timebase = !wearable_stream_config_is_default(document.wearable.ecg)
                || !wearable_stream_config_is_default(document.wearable.ppg)
                || !wearable_stream_config_is_default(document.wearable.accelerometer);
            bool has_accelerometer = document.physiology.activity_intensity > 0.0;
            for (std::size_t artifact = 0; artifact < document.signal_quality.artifacts.size(); ++artifact)
                if (signal_quality_artifact_is_motion(document.signal_quality.artifacts[artifact].type))
                    has_accelerometer = true;
            item.channel_count = clinical_lead_count + (document.ppg.enabled ? 1u + (document.ppg.optical.enabled ? 2u : 0u) : 0u) + (has_accelerometer ? 1u : 0u);
            item.targets = pack_case.targets;
            item.estimated_package_bytes = estimate_case_package_bytes(document, item.channel_count, item.estimated_waveform_csv_bytes, item.estimated_binary_signal_bytes);
            item.estimated_peak_memory_bytes = estimate_case_peak_memory_bytes(document);
            fresh.total_duration_seconds += item.duration_seconds;
            fresh.total_sample_count += item.sample_count;
            fresh.estimated_package_bytes += item.estimated_package_bytes;
            fresh.estimated_peak_memory_bytes = std::max(fresh.estimated_peak_memory_bytes, item.estimated_peak_memory_bytes);
            for (std::size_t target_index = 0; target_index < item.targets.size(); ++target_index)
            {
                const std::string& target_name = item.targets[target_index];
                scenario_pack_target_analysis* target = find_target(fresh.targets, target_name);
                ++target->case_count;
                const std::string path = "$.scenarios[" + index_string(i) + "].targets[" + index_string(target_index) + "]";
                if (target->support == scenario_target_unsupported)
                    fresh.messages.push_back(make_message(true, "UNSUPPORTED_TARGET", path, "Target is not known by the authoring contract: " + target_name));
                else
                {
                    std::string incompatibility;
                    if (!target_compatible(target_name, document, incompatibility))
                        fresh.messages.push_back(make_message(true, "TARGET_INCOMPATIBLE", path, incompatibility));
                }
            }
            fresh.cases.push_back(item);
        }
        for (std::size_t i = 0; i < fresh.targets.size(); ++i)
            if (fresh.targets[i].support == scenario_target_reference_only)
                fresh.messages.push_back(make_message(false, "REFERENCE_ONLY_TARGET", "$.targets", fresh.targets[i].target + " exports ground truth but has no local scoring policy."));
        fresh.case_count = static_cast<unsigned int>(fresh.cases.size());
        fresh.success = true;
        for (std::size_t i = 0; i < fresh.messages.size(); ++i)
            if (fresh.messages[i].error)
                fresh.success = false;
        analysis = fresh;
        return fresh.success;
    }

    std::string scenario_pack_analysis_json(const scenario_pack_analysis& analysis)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10)
               << "{\"schema_version\":1,\"analysis_version\":\"synsigra_pack_analysis_v1\",\"success\":" << (analysis.success ? "true" : "false")
               << ",\"metadata_type\":\"synsigra_pack_analysis\""
               << ",\"pack_id\":" << json_string(analysis.pack_id)
               << ",\"pack_version\":" << json_string(analysis.pack_version)
               << ",\"scoring_mode\":" << json_string(analysis_scoring_mode(analysis))
               << ",\"recommended_verifier_profile\":" << json_string(recommended_profile_for_analysis(analysis))
               << ",\"generator_compatibility\":{\"pack_schema_version\":1,\"scenario_schema_versions\":[2,3,4,5,6],\"challenge_package_contract\":\"synsigra_challenge_package_v2\",\"scoring_manifest_contract\":\"synsigra_scoring_manifest_v2\"}"
               << ",\"summary\":{\"case_count\":" << analysis.case_count
               << ",\"total_duration_seconds\":" << analysis.total_duration_seconds
               << ",\"total_sample_count\":" << analysis.total_sample_count
               << ",\"estimated_package_bytes\":" << analysis.estimated_package_bytes
               << ",\"estimated_peak_memory_bytes\":" << analysis.estimated_peak_memory_bytes << "},\"scoreable_targets\":";
        write_target_contract_array(output, analysis, true);
        output << ",\"reference_only_targets\":";
        write_target_contract_array(output, analysis, false);
        output << ",\"submission_output_schemas\":";
        write_submission_output_schemas(output, analysis);
        output << ",\"output_artifacts\":";
        write_output_artifacts(output, analysis);
        output << ",\"targets\":[";
        for (std::size_t i = 0; i < analysis.targets.size(); ++i)
            output << (i ? "," : "") << "{\"target\":" << json_string(analysis.targets[i].target)
                   << ",\"support\":" << json_string(scenario_target_support_name(analysis.targets[i].support))
                   << ",\"case_count\":" << analysis.targets[i].case_count << '}';
        output << "],\"cases\":[";
        for (std::size_t i = 0; i < analysis.cases.size(); ++i)
        {
            const scenario_pack_case_analysis& item = analysis.cases[i];
            output << (i ? "," : "") << "{\"case_id\":" << json_string(item.case_id)
                   << ",\"scenario_id\":" << json_string(item.scenario_id)
                   << ",\"duration_seconds\":" << item.duration_seconds
                   << ",\"sampling_rate_hz\":" << item.sampling_rate_hz
                   << ",\"sample_count\":" << item.sample_count
                   << ",\"channel_count\":" << item.channel_count
                   << ",\"wearable_timebase\":" << (item.wearable_timebase ? "true" : "false")
                   << ",\"estimated_waveform_csv_bytes\":" << item.estimated_waveform_csv_bytes
                   << ",\"estimated_binary_signal_bytes\":" << item.estimated_binary_signal_bytes
                   << ",\"estimated_package_bytes\":" << item.estimated_package_bytes
                   << ",\"estimated_peak_memory_bytes\":" << item.estimated_peak_memory_bytes
                   << ",\"targets\":";
            write_string_array(output, item.targets);
            output << ",\"scoreable_targets\":";
            write_string_array(output, targets_by_support(item.targets, analysis, scenario_target_local_scoring));
            output << ",\"reference_only_targets\":";
            write_string_array(output, targets_by_support(item.targets, analysis, scenario_target_reference_only));
            output << '}';
        }
        output << "],\"messages\":[";
        for (std::size_t i = 0; i < analysis.messages.size(); ++i)
            output << (i ? "," : "") << "{\"severity\":" << json_string(analysis.messages[i].error ? "error" : "warning")
                   << ",\"code\":" << json_string(analysis.messages[i].code)
                   << ",\"path\":" << json_string(analysis.messages[i].path)
                   << ",\"message\":" << json_string(analysis.messages[i].message) << '}';
        output << "]}";
        return output.str();
    }
}
