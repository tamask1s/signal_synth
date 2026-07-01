#include <signal_synth/clinical_ecg.h>
#include <signal_synth/ecg_model.h>
#include <signal_synth/ecg_export.h>
#include <signal_synth/ecg_morphology.h>
#include <signal_synth/ecg_scenario.h>
#include <signal_synth/ecg_scenario_json.h>
#include <signal_synth/signal_synth.h>

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
    if (std::string(signal_synth::signal_synth_generator_version()).empty())
        return 6;
    return 0;
}
