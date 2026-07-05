#include <signal_synth/clinical_ecg.h>
#include <signal_synth/challenge_assembly.h>
#include <signal_synth/challenge_package.h>
#include <signal_synth/detection_io.h>
#include <signal_synth/ecg_model.h>
#include <signal_synth/ecg_export.h>
#include <signal_synth/ecg_wfdb_export.h>
#include <signal_synth/ecg_edf_bdf_export.h>
#include <signal_synth/hrv_metrics.h>
#include <signal_synth/hrv_scoring.h>
#include <signal_synth/ecg_beat_classification.h>
#include <signal_synth/ecg_compare.h>
#include <signal_synth/ecg_morphology.h>
#include <signal_synth/ecg_scenario.h>
#include <signal_synth/ecg_scenario_json.h>
#include <signal_synth/ecg_pack.h>
#include <signal_synth/ecg_pack_score.h>
#include <signal_synth/signal_synth.h>
#include <signal_synth/ppg_model.h>
#include <signal_synth/signal_quality.h>
#include <signal_synth/scenario_authoring.h>
#include <signal_synth/synsigra_api.h>

#include <string>

int main()
{
    signal_synth::qrs_params legacy;
    signal_synth::ecg_model_config model;
    signal_synth::clinical_ecg_config clinical;
    signal_synth::challenge_package_manifest challenge_package;
    signal_synth::challenge_package_file challenge_file;
    signal_synth::challenge_package_build_options challenge_options;
    signal_synth::challenge_package_input_file challenge_input_file;
    signal_synth::detection_io_document detection_document;
    signal_synth::detection_io_event detection_event;
    signal_synth::clinical_ecg_record record;
    signal_synth::ecg_morphology_report morphology;
    signal_synth::ecg_qa_scenario scenario;
    signal_synth::ecg_scenario_report report;
    signal_synth::ecg_scenario_engine engine;
    signal_synth::ecg_scenario_document document;
    signal_synth::ecg_scenario_json_result json_result;
    signal_synth::ecg_pack_manifest pack;
    signal_synth::ecg_pack_json_result pack_result;
    signal_synth::ecg_pack_score_summary pack_score_summary;
    signal_synth::ecg_compare_options compare_options;
    signal_synth::ecg_compare_result compare_result;
    signal_synth::ppg_config ppg;
    signal_synth::signal_quality_artifact_config artifact;
    signal_synth::synsigra_validation_result facade_validation;
    signal_synth::synsigra_compare_options facade_compare_options;
    signal_synth::wfdb_export_bundle wfdb_bundle;
    signal_synth::edf_bdf_export_bundle edf_bdf_bundle;
    signal_synth::hrv_analysis_result hrv_analysis;
    signal_synth::hrv_score_result hrv_score;
    signal_synth::ecg_beat_classification_result beat_classification;
    signal_synth::scenario_pack_analysis authoring_analysis;

    if (legacy.amplitude_r <= 0.0 || !signal_synth::ecg_model(model).valid() || !signal_synth::clinical_ecg_generator(clinical).valid())
        return 1;
    if (challenge_package.schema_version != 1 || challenge_file.role != signal_synth::challenge_file_other)
        return 12;
    if (challenge_options.package_type != signal_synth::challenge_package_scenario_pack || challenge_input_file.role != signal_synth::challenge_file_other)
        return 19;
    if (detection_document.schema_version != 1 || detection_event.has_confidence)
        return 13;
    if (signal_synth::ecg_condition_catalog_size() != signal_synth::ecg_condition_count)
        return 2;
    if (!scenario.add_condition(signal_synth::ecg_condition_norm) || !engine.validate(scenario, report))
        return 3;
    if (record.sample_count() != 0 || morphology.entry_count() != 0)
        return 4;
    if (!signal_synth::write_ecg_scenario_json(document, json_result))
        return 5;
    pack.pack_id = "smoke_pack";
    pack.name = "Smoke Pack";
    pack.version = "1";
    pack.description = "Package smoke pack";
    pack.targets.push_back("smoke");
    signal_synth::ecg_pack_scenario pack_scenario;
    pack_scenario.id = "clean";
    pack_scenario.path = "ecg_clean.json";
    pack_scenario.targets.push_back("smoke");
    pack.scenarios.push_back(pack_scenario);
    if (!signal_synth::write_ecg_pack_json(pack, pack_result))
        return 9;
    if (pack_score_summary.success || std::string(signal_synth::ecg_pack_score_summary_csv(pack_score_summary)).empty())
        return 14;
    if (compare_options.target != signal_synth::ecg_compare_r_peak || signal_synth::ecg_compare_default_tolerance_seconds(compare_options.target) <= 0.0 || compare_result.success)
        return 10;
    if (std::string(signal_synth::signal_synth_generator_version()).empty())
        return 6;
    if (ppg.pulse_delay_ms <= 0.0)
        return 7;
    if (artifact.severity <= 0.0)
        return 8;
    if (facade_validation.success || facade_compare_options.target != signal_synth::synsigra_compare_r_peak || std::string(signal_synth::synsigra_api_version()).empty())
        return 11;
    if (authoring_analysis.success || std::string(signal_synth::scenario_authoring_metadata_version()).empty())
        return 20;
    if (!wfdb_bundle.artifacts.empty() || !edf_bdf_bundle.artifacts.empty())
        return 15;
    if (hrv_analysis.metric_definition_version.empty() || hrv_analysis.interpolation_rate_hz <= 0.0)
        return 16;
    if (hrv_score.success || std::string(signal_synth::hrv_metric_name(signal_synth::hrv_metric_sdnn_seconds)).empty())
        return 17;
    if (beat_classification.success || std::string(signal_synth::ecg_beat_class_name(signal_synth::ecg_beat_normal)) != "normal")
        return 18;
    return 0;
}
