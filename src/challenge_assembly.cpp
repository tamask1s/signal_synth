#include "challenge_assembly.h"

#include <algorithm>

namespace
{
    void add_format_once(std::vector<std::string>& formats, const char* value)
    {
        if (std::find(formats.begin(), formats.end(), value) == formats.end())
            formats.push_back(value);
    }

    void derive_format(signal_synth::challenge_file_role role, std::vector<std::string>& formats)
    {
        switch (role)
        {
        case signal_synth::challenge_file_waveform_csv:
            add_format_once(formats, "csv");
            break;
        case signal_synth::challenge_file_wearable_samples_csv:
            add_format_once(formats, "wearable_csv");
            break;
        case signal_synth::challenge_file_wfdb_header:
        case signal_synth::challenge_file_wfdb_signal:
        case signal_synth::challenge_file_wfdb_annotation:
            add_format_once(formats, "wfdb");
            break;
        case signal_synth::challenge_file_edf:
            add_format_once(formats, "edf");
            break;
        case signal_synth::challenge_file_bdf:
            add_format_once(formats, "bdf");
            break;
        default:
            break;
        }
    }

    void append_manifest_file(signal_synth::challenge_package_manifest& manifest, const signal_synth::challenge_package_input_file& source)
    {
        signal_synth::challenge_package_file file;
        file.path = source.path;
        file.role = source.role;
        file.media_type = source.media_type;
        file.sha256 = signal_synth::challenge_package_content_sha256(source.content);
        file.size_bytes = static_cast<unsigned long long>(source.content.size());
        file.required = source.required;
        manifest.files.push_back(file);
    }
}

namespace signal_synth
{
    challenge_package_input_file::challenge_package_input_file() : role(challenge_file_other), required(true)
    {
    }

    challenge_package_build_options::challenge_package_build_options()
        : package_type(challenge_package_scenario_pack), usage_restrictions(challenge_package_default_usage_restrictions()), not_for(challenge_package_default_not_for())
    {
    }

    challenge_package_build_result::challenge_package_build_result() : success(false)
    {
    }

    const char* challenge_package_default_usage_restrictions()
    {
        return "engineering algorithm QA only";
    }

    const char* challenge_package_default_not_for()
    {
        return "diagnosis, patient monitoring, clinical validation certificate, or standalone conformity assessment";
    }

    challenge_file_role challenge_file_role_for_export_artifact(const std::string& artifact_name)
    {
        if (artifact_name == "scenario.json")
            return challenge_file_scenario_json;
        if (artifact_name == "metadata.json" || artifact_name == "provenance.json" || artifact_name == "resolved_scenario.json" || artifact_name == "randomization.json" || artifact_name == "warnings.json" || artifact_name == "wfdb_metadata.json" || artifact_name == "edf_bdf_metadata.json" || artifact_name == "case_summary.json" || artifact_name == "scoring_manifest.json")
            return challenge_file_metadata_json;
        if (artifact_name == "waveform.csv")
            return challenge_file_waveform_csv;
        if (artifact_name == "annotations.json")
            return challenge_file_annotations_json;
        if (artifact_name == "ground_truth_metrics.json" || artifact_name == "hrv_metrics.json")
            return challenge_file_ground_truth_metrics_json;
        if (artifact_name == "measurement_truth.json")
            return challenge_file_measurement_truth_json;
        if (artifact_name == "wearable_ecg_samples.csv" || artifact_name == "wearable_ppg_samples.csv" || artifact_name == "wearable_accelerometer_samples.csv")
            return challenge_file_wearable_samples_csv;
        if (artifact_name == "wearable_timestamp_truth.csv")
            return challenge_file_wearable_timestamp_truth_csv;
        if (artifact_name == "wearable_timebase_truth.json")
            return challenge_file_wearable_timebase_truth_json;
        if (artifact_name == "wearable_alignment_truth.json")
            return challenge_file_wearable_alignment_truth_json;
        if (artifact_name == "realism_metrics.json")
            return challenge_file_realism_metrics_json;
        if (artifact_name == "realism_metrics.csv")
            return challenge_file_realism_metrics_csv;
        if (artifact_name == "realism_report.html")
            return challenge_file_realism_report_html;
        if (artifact_name == "ppg_optical_latent.csv")
            return challenge_file_ppg_optical_latent_csv;
        if (artifact_name == "ppg_optical_truth.json")
            return challenge_file_ppg_optical_truth_json;
        if (artifact_name == "cardiorespiratory_truth.json")
            return challenge_file_cardiorespiratory_truth_json;
        if (artifact_name == "prv_tachogram.csv")
            return challenge_file_prv_tachogram_csv;
        if (artifact_name == "respiration_reference.csv")
            return challenge_file_respiration_reference_csv;
        if (artifact_name == "report.html")
            return challenge_file_report_html;
        if (artifact_name == "README.txt" || artifact_name == "ENGINEERING_CLAIM_BOUNDARY.txt")
            return challenge_file_readme;
        if (artifact_name == "synsigra.hea")
            return challenge_file_wfdb_header;
        if (artifact_name == "synsigra.dat")
            return challenge_file_wfdb_signal;
        if (artifact_name == "synsigra.atr")
            return challenge_file_wfdb_annotation;
        if (artifact_name == "synsigra.edf")
            return challenge_file_edf;
        if (artifact_name == "synsigra.bdf")
            return challenge_file_bdf;
        return challenge_file_other;
    }

    bool build_challenge_package_manifest(const challenge_package_build_options& options, const std::vector<challenge_package_case_input>& cases, challenge_package_build_result& result)
    {
        challenge_package_build_result fresh;
        challenge_package_manifest manifest;
        manifest.package_id = options.package_id;
        manifest.name = options.name;
        manifest.version = options.version;
        manifest.description = options.description;
        manifest.package_type = options.package_type;
        manifest.ground_truth_included = true;
        manifest.waveform_formats = options.waveform_formats;
        manifest.generator_version = options.generator_version;
        manifest.usage_restrictions = options.usage_restrictions;
        manifest.not_for = options.not_for;

        for (std::size_t i = 0; i < options.package_files.size(); ++i)
        {
            append_manifest_file(manifest, options.package_files[i]);
            derive_format(options.package_files[i].role, manifest.waveform_formats);
        }

        for (std::size_t i = 0; i < cases.size(); ++i)
        {
            challenge_package_case item;
            item.id = cases[i].id;
            item.scenario_id = cases[i].scenario_id;
            item.scenario_path = cases[i].scenario_path;
            item.document_fingerprint = cases[i].document_fingerprint;
            item.render_identity = cases[i].render_identity;
            for (std::size_t f = 0; f < cases[i].files.size(); ++f)
            {
                append_manifest_file(manifest, cases[i].files[f]);
                derive_format(cases[i].files[f].role, manifest.waveform_formats);
                item.files.push_back(cases[i].files[f].path);
            }
            manifest.cases.push_back(item);
        }

        if (manifest.waveform_formats.empty())
            add_format_once(manifest.waveform_formats, "csv");

        if (!write_challenge_package_json(manifest, fresh.manifest_json))
        {
            fresh.manifest = manifest;
            result = fresh;
            return false;
        }
        fresh.manifest = manifest;
        fresh.success = true;
        result = fresh;
        return true;
    }
}
