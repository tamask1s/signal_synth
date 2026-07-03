#include <signal_synth/clinical_ecg.h>
#include <signal_synth/ecg_model.h>
#include <signal_synth/ecg_export.h>
#include <signal_synth/ecg_compare.h>
#include <signal_synth/ecg_morphology.h>
#include <signal_synth/ecg_scenario.h>
#include <signal_synth/ecg_scenario_json.h>
#include <signal_synth/ecg_pack.h>
#include <signal_synth/signal_synth.h>
#include <signal_synth/ppg_model.h>
#include <signal_synth/signal_quality.h>

#include <string>

int main()
{
    signal_synth::qrs_params legacy;
    signal_synth::ecg_model_config model;
    signal_synth::clinical_ecg_config clinical;
    signal_synth::clinical_ecg_record record;
    signal_synth::ecg_morphology_report morphology;
    signal_synth::ecg_qa_scenario scenario;
    signal_synth::ecg_scenario_report report;
    signal_synth::ecg_scenario_engine engine;
    signal_synth::ecg_scenario_document document;
    signal_synth::ecg_scenario_json_result json_result;
    signal_synth::ecg_pack_manifest pack;
    signal_synth::ecg_pack_json_result pack_result;
    signal_synth::ecg_compare_options compare_options;
    signal_synth::ecg_compare_result compare_result;
    signal_synth::ppg_config ppg;
    signal_synth::signal_quality_artifact_config artifact;

    if (legacy.amplitude_r <= 0.0 || !signal_synth::ecg_model(model).valid() || !signal_synth::clinical_ecg_generator(clinical).valid())
        return 1;
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
    if (compare_options.target != signal_synth::ecg_compare_r_peak || signal_synth::ecg_compare_default_tolerance_seconds(compare_options.target) <= 0.0 || compare_result.success)
        return 10;
    if (std::string(signal_synth::signal_synth_generator_version()).empty())
        return 6;
    if (ppg.pulse_delay_ms <= 0.0)
        return 7;
    if (artifact.severity <= 0.0)
        return 8;
    return 0;
}
