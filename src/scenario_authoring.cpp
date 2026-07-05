#include "scenario_authoring.h"

#include "clinical_ecg.h"
#include "signal_quality.h"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace
{
    const char* metadata_version = "synsigra_authoring_v1";
    const char* template_version = "synsigra_templates_v1";

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
        if (target == "ppg_systolic_peak" && !document.ppg.enabled)
        {
            message = "PPG systolic-peak scoring requires ppg.enabled=true.";
            return false;
        }
        if (target == "ecg_ppg_alignment" && !document.ppg.enabled)
        {
            message = "ECG/PPG alignment reference requires ppg.enabled=true.";
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
        return true;
    }

    unsigned long long estimate_case_package_bytes(unsigned int sample_count, unsigned int channel_count, unsigned long long& csv_bytes, unsigned long long& binary_bytes)
    {
        const unsigned long long samples = sample_count;
        const unsigned long long channels = channel_count;
        csv_bytes = 256u + samples * (channels + 1u) * 18u;
        binary_bytes = samples * channels * 7u;
        return csv_bytes + binary_bytes + 262144u;
    }

    void write_string_array(std::ostringstream& output, const std::vector<std::string>& values)
    {
        output << '[';
        for (std::size_t i = 0; i < values.size(); ++i)
            output << (i ? "," : "") << json_string(values[i]);
        output << ']';
    }
}

namespace signal_synth
{
    scenario_pack_analysis_message::scenario_pack_analysis_message() : error(false), code(), path(), message() {}
    scenario_pack_case_analysis::scenario_pack_case_analysis()
        : case_id(), scenario_id(), duration_seconds(0.0), sampling_rate_hz(0), sample_count(0), channel_count(0), estimated_waveform_csv_bytes(0), estimated_binary_signal_bytes(0), estimated_package_bytes(0), targets() {}
    scenario_pack_target_analysis::scenario_pack_target_analysis() : target(), support(scenario_target_unsupported), case_count(0) {}
    scenario_pack_analysis::scenario_pack_analysis()
        : success(false), pack_id(), pack_version(), case_count(0), total_duration_seconds(0.0), total_sample_count(0), estimated_package_bytes(0), cases(), targets(), messages() {}

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
        if (target == "r_peak" || target == "ppg_systolic_peak" || target == "ecg_beat_classification" || target == "hrv")
            return scenario_target_local_scoring;
        if (target == "signal_quality" || target == "morphology_assertions" || target == "ecg_ppg_alignment")
            return scenario_target_reference_only;
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
            {"$.artifacts","Artifacts","artifacts","artifact_array","artifact_editor","[]","0","128",0,"items",0,0,true}
        };

        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << "{\"schema_version\":1,\"metadata_version\":" << json_string(metadata_version)
               << ",\"scenario_schema_version\":2,\"groups\":["
               << "{\"id\":\"identity\",\"label\":\"Identity\"},{\"id\":\"render\",\"label\":\"Render\"},{\"id\":\"ecg\",\"label\":\"ECG\"},"
               << "{\"id\":\"episode\",\"label\":\"Episode\"},{\"id\":\"hrv\",\"label\":\"HRV\"},{\"id\":\"ppg\",\"label\":\"PPG\"},{\"id\":\"artifacts\",\"label\":\"Artifacts\"}],\"fields\":[";
        for (std::size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i)
        {
            if (i) output << ',';
            write_field(output, fields[i]);
        }
        output << "],\"condition_item_fields\":["
               << "{\"name\":\"code\",\"value_type\":\"string\",\"control\":\"condition_select\"},"
               << "{\"name\":\"severity\",\"value_type\":\"number\",\"control\":\"slider\",\"minimum\":0.000001,\"maximum\":1,\"step\":0.01,\"default\":1,\"enabled_when\":\"selected condition has variable_severity=true\"}],"
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
        for (int type = signal_quality_ecg_baseline_wander; type <= signal_quality_ecg_adc_clipping; ++type)
        {
            const signal_quality_artifact_type artifact = static_cast<signal_quality_artifact_type>(type);
            const bool ppg = artifact == signal_quality_ppg_dropout;
            output << (type ? "," : "") << "{\"type\":" << json_string(signal_quality_artifact_type_name(artifact))
                   << ",\"channel_family\":" << json_string(ppg ? "ppg" : "ecg")
                   << ",\"item_fields\":["
                   << "{\"name\":\"start_seconds\",\"value_type\":\"number\",\"control\":\"number\",\"minimum\":0,\"unit\":\"s\"},"
                   << "{\"name\":\"duration_seconds\",\"value_type\":\"number\",\"control\":\"number\",\"exclusive_minimum\":0,\"unit\":\"s\"},"
                   << "{\"name\":\"severity\",\"value_type\":\"number\",\"control\":\"slider\",\"minimum\":0,\"maximum\":1,\"step\":0.01},"
                   << "{\"name\":\"seed\",\"value_type\":\"uint64_string\",\"control\":\"text\",\"serialization\":\"json_integer\"},"
                   << "{\"name\":\"channels\",\"value_type\":\"string_array\",\"control\":\"channel_picker\",\"options\":"
                   << (ppg ? "[\"ppg_green\"]" : "[\"all_ecg\",\"I\",\"II\",\"III\",\"aVR\",\"aVL\",\"aVF\",\"V1\",\"V2\",\"V3\",\"V4\",\"V5\",\"V6\"]")
                   << "}]}";
        }
        output << "],\"targets\":["
               << "{\"name\":\"r_peak\",\"support\":\"local_scoring\",\"requires\":[]},"
               << "{\"name\":\"ppg_systolic_peak\",\"support\":\"local_scoring\",\"requires\":[\"ppg.enabled\"]},"
               << "{\"name\":\"ecg_beat_classification\",\"support\":\"local_scoring\",\"requires\":[]},"
               << "{\"name\":\"hrv\",\"support\":\"local_scoring\",\"requires\":[\"hrv.enabled\",\"duration_seconds>=300\"]},"
               << "{\"name\":\"signal_quality\",\"support\":\"reference_only\",\"requires\":[\"artifacts.length>0\"]},"
               << "{\"name\":\"morphology_assertions\",\"support\":\"reference_only\",\"requires\":[\"ecg.conditions\"]},"
               << "{\"name\":\"ecg_ppg_alignment\",\"support\":\"reference_only\",\"requires\":[\"ppg.enabled\"]}],"
               << "\"cross_field_rules\":["
               << "{\"id\":\"sample_count\",\"expression\":\"duration_seconds * sample_rate_hz is an integer in [1,4294967295]\",\"message\":\"Duration and sample rate must produce a positive 32-bit sample count.\"},"
               << "{\"id\":\"hrv_window\",\"expression\":\"!hrv.enabled || duration_seconds >= 300\",\"message\":\"Enabled spectral HRV requires at least 300 seconds.\"},"
               << "{\"id\":\"hrv_respiration_amplitude\",\"expression\":\"hrv.respiratory_amplitude_seconds <= sqrt(2) * hrv.target_sdnn_seconds\",\"message\":\"Respiratory RR amplitude cannot exceed the requested total SDNN envelope.\"},"
               << "{\"id\":\"rr_bounds\",\"expression\":\"hrv.minimum_rr_seconds < 60/hrv.target_mean_hr_bpm < hrv.maximum_rr_seconds\",\"message\":\"Mean RR must lie strictly inside the configured RR bounds.\"},"
               << "{\"id\":\"ppg_width\",\"expression\":\"ppg.rise_time_ms + ppg.decay_time_ms <= 5000\",\"message\":\"Combined PPG rise and decay time cannot exceed 5000 ms.\"},"
               << "{\"id\":\"ppg_dicrotic\",\"expression\":\"ppg.dicrotic_delay_ms <= ppg.decay_time_ms\",\"message\":\"Dicrotic delay cannot exceed decay time.\"},"
               << "{\"id\":\"episode_bounds\",\"expression\":\"ecg.episode_type == 'none' || ecg.episode_start_seconds + ecg.episode_duration_seconds <= duration_seconds\",\"message\":\"Episode must fit inside the rendered duration.\"},"
               << "{\"id\":\"artifact_bounds\",\"expression\":\"forall artifact: start_seconds + duration_seconds <= duration_seconds\",\"message\":\"Every artifact must fit inside the scenario.\"}"
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
            "{\"template_id\":\"ecg_psvt_transition\",\"name\":\"PSVT transition episode\",\"description\":\"Sinus baseline with explicit PSVT onset and offset ground truth.\",\"difficulty\":\"stress\",\"feature_tags\":[\"ecg\",\"rhythm\",\"psvt\",\"transition\"],\"targets\":[\"r_peak\",\"ecg_beat_classification\"],\"editable_paths\":[\"$.duration_seconds\",\"$.seed\",\"$.ecg.episode_start_seconds\",\"$.ecg.episode_duration_seconds\",\"$.ecg.episode_rate_bpm\"],\"scenario\":{\"schema_version\":2,\"scenario_id\":\"ecg_psvt_transition\",\"name\":\"PSVT transition episode\",\"description\":\"Template-generated PSVT episode.\",\"author\":\"Synsigra\",\"tags\":[\"template\",\"rhythm\",\"psvt\",\"transition\"],\"duration_seconds\":30,\"sample_rate_hz\":500,\"seed\":7106,\"ecg\":{\"heart_rate_bpm\":72,\"rr_variability_seconds\":0,\"ectopic_every_n_beats\":0,\"second_degree_av_pattern\":\"unspecified\",\"q_wave_territory\":\"unspecified\",\"episode_type\":\"psvt\",\"episode_start_seconds\":10,\"episode_duration_seconds\":10,\"episode_rate_bpm\":180,\"flutter_conduction_pattern\":\"fixed\",\"pacing_mode\":\"ventricular\",\"pacing_non_capture_every_n_beats\":0,\"fidelity_policy\":\"allow_parameterized\",\"conditions\":[{\"code\":\"PSVT\",\"severity\":1}]},\"ppg\":{\"enabled\":false,\"pulse_delay_ms\":180,\"rise_time_ms\":120,\"decay_time_ms\":300,\"amplitude_au\":1,\"baseline_au\":0,\"dicrotic_delay_ms\":180,\"dicrotic_width_ms\":80,\"dicrotic_amplitude_ratio\":0.15}}}"
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
            item.channel_count = clinical_lead_count + (document.ppg.enabled ? 1u : 0u);
            item.targets = pack_case.targets;
            item.estimated_package_bytes = estimate_case_package_bytes(item.sample_count, item.channel_count, item.estimated_waveform_csv_bytes, item.estimated_binary_signal_bytes);
            fresh.total_duration_seconds += item.duration_seconds;
            fresh.total_sample_count += item.sample_count;
            fresh.estimated_package_bytes += item.estimated_package_bytes;
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
               << ",\"pack_id\":" << json_string(analysis.pack_id)
               << ",\"pack_version\":" << json_string(analysis.pack_version)
               << ",\"summary\":{\"case_count\":" << analysis.case_count
               << ",\"total_duration_seconds\":" << analysis.total_duration_seconds
               << ",\"total_sample_count\":" << analysis.total_sample_count
               << ",\"estimated_package_bytes\":" << analysis.estimated_package_bytes << "},\"targets\":[";
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
                   << ",\"estimated_waveform_csv_bytes\":" << item.estimated_waveform_csv_bytes
                   << ",\"estimated_binary_signal_bytes\":" << item.estimated_binary_signal_bytes
                   << ",\"estimated_package_bytes\":" << item.estimated_package_bytes
                   << ",\"targets\":";
            write_string_array(output, item.targets);
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
