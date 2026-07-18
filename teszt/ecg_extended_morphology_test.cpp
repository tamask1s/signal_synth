#include "../src/delineation_scoring.h"
#include "../src/ecg_beat_classification.h"
#include "../src/ecg_export.h"
#include "../src/measurement_scoring.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    bool check(bool condition, const char* name)
    {
        if (!condition) std::cerr << "FAIL: " << name << '\n';
        return condition;
    }

    signal_synth::ecg_scenario_document base_document(const char* id)
    {
        signal_synth::ecg_scenario_document document;
        document.schema_version = 7;
        document.scenario_id = id;
        document.name = "Extended ECG morphology";
        document.duration_seconds = 10.0;
        document.ecg.clear_conditions();
        document.ecg.add_condition(signal_synth::ecg_condition_sr);
        document.ecg.set_sampling_rate_hz(500);
        document.ecg.set_heart_rate_bpm(70.0);
        document.ecg.set_seed(82001);
        return document;
    }

    bool render(const signal_synth::ecg_scenario_document& document, signal_synth::ecg_render_bundle& output)
    {
        signal_synth::ecg_document_render_result result;
        return signal_synth::render_ecg_document(document, output, result);
    }

    bool has_fiducial(const signal_synth::clinical_ecg_record& record, signal_synth::clinical_fiducial_kind kind, int lead, signal_synth::clinical_fiducial_source source)
    {
        const signal_synth::clinical_fiducial_annotation* items = record.fiducials();
        for (unsigned int i = 0; items && i < record.fiducial_count(); ++i)
            if (items[i].kind == kind && items[i].lead_index == lead && items[i].source == source && items[i].present) return true;
        return false;
    }

    bool smooth_overlay(const signal_synth::clinical_ecg_record& baseline, const signal_synth::clinical_ecg_record& modified, unsigned int lead)
    {
        const double* clean = baseline.lead_data(lead);
        const double* changed = modified.lead_data(lead);
        if (!clean || !changed || baseline.sample_count() != modified.sample_count()) return false;
        double maximum_step = 0.0;
        for (unsigned int i = 1; i < baseline.sample_count(); ++i)
        {
            const double previous = changed[i - 1] - clean[i - 1];
            const double current = changed[i] - clean[i];
            maximum_step = std::max(maximum_step, std::fabs(current - previous));
        }
        return maximum_step < 0.05;
    }

    bool has_delineation_kind(const std::vector<signal_synth::delineation_truth_point>& truth, signal_synth::delineation_kind kind, const char* lead)
    {
        for (std::size_t i = 0; i < truth.size(); ++i)
            if (truth[i].kind == kind && truth[i].lead == lead && truth[i].status == signal_synth::delineation_truth_present) return true;
        return false;
    }

    bool has_measurement(const std::vector<signal_synth::measurement_truth>& truth, const char* name, const char* formula, const char* lead)
    {
        for (std::size_t i = 0; i < truth.size(); ++i)
            if (truth[i].measurement.name == name && truth[i].measurement.formula == formula && truth[i].measurement.channel == lead && truth[i].measurement.status == signal_synth::measurement_valid) return true;
        return false;
    }
}

int main()
{
    bool ok = true;
    signal_synth::ecg_scenario_document baseline_document = base_document("extended_baseline");
    signal_synth::ecg_scenario_document document = baseline_document;
    document.scenario_id = "extended_components";
    const unsigned int lead_ii = signal_synth::ecg_morphology_lead_mask(signal_synth::clinical_lead_ii);
    const unsigned int lead_v2 = signal_synth::ecg_morphology_lead_mask(signal_synth::clinical_lead_v2);
    const unsigned int lead_v5 = signal_synth::ecg_morphology_lead_mask(signal_synth::clinical_lead_v5);
    ok &= check(document.ecg.add_morphology_component(signal_synth::ecg_component_p_biphasic, lead_ii, -0.08, 55.0, 35.0), "add_p_biphasic");
    ok &= check(document.ecg.add_morphology_component(signal_synth::ecg_component_p_notch, lead_v2, 0.07, 38.0, 22.0), "add_p_notch");
    ok &= check(document.ecg.add_morphology_component(signal_synth::ecg_component_r_prime, lead_v2, 0.22, 48.0, 30.0), "add_r_prime");
    ok &= check(document.ecg.add_morphology_component(signal_synth::ecg_component_qrs_fragment, lead_v5, -0.09, 22.0, 16.0), "add_qrs_fragment");
    ok &= check(document.ecg.add_morphology_component(signal_synth::ecg_component_t_biphasic, lead_ii, -0.12, 90.0, 60.0), "add_t_biphasic");
    ok &= check(document.ecg.add_morphology_component(signal_synth::ecg_component_t_notch, lead_v5, 0.08, 55.0, 28.0), "add_t_notch");
    ok &= check(document.ecg.add_morphology_component(signal_synth::ecg_component_u_wave, lead_ii | lead_v5, 0.08, 35.0, 90.0), "add_u_wave");
    ok &= check(!document.ecg.add_morphology_component(signal_synth::ecg_component_u_wave, lead_ii, 0.08, 40.0, 90.0), "duplicate_type_lead_rejected");
    ok &= check(!document.ecg.add_morphology_component(signal_synth::ecg_component_p_notch, lead_ii, 0.01, 20.0, 20.0), "subthreshold_component_rejected");

    signal_synth::ecg_render_bundle baseline;
    signal_synth::ecg_render_bundle modified;
    ok &= check(render(baseline_document, baseline), "render_baseline");
    ok &= check(render(document, modified), "render_extended_components");
    ok &= check(has_fiducial(modified.record, signal_synth::clinical_p_secondary_peak, signal_synth::clinical_lead_ii, signal_synth::clinical_fiducial_construction)
        && has_fiducial(modified.record, signal_synth::clinical_p_notch, signal_synth::clinical_lead_v2, signal_synth::clinical_fiducial_lead_measurement)
        && has_fiducial(modified.record, signal_synth::clinical_r_prime, signal_synth::clinical_lead_v2, signal_synth::clinical_fiducial_lead_measurement)
        && has_fiducial(modified.record, signal_synth::clinical_qrs_fragment, signal_synth::clinical_lead_v5, signal_synth::clinical_fiducial_lead_measurement)
        && has_fiducial(modified.record, signal_synth::clinical_t_secondary_peak, signal_synth::clinical_lead_ii, signal_synth::clinical_fiducial_lead_measurement)
        && has_fiducial(modified.record, signal_synth::clinical_t_notch, signal_synth::clinical_lead_v5, signal_synth::clinical_fiducial_lead_measurement)
        && has_fiducial(modified.record, signal_synth::clinical_u_peak, signal_synth::clinical_lead_ii, signal_synth::clinical_fiducial_lead_measurement), "component_fiducials_present");
    ok &= check(!has_fiducial(modified.record, signal_synth::clinical_r_prime, signal_synth::clinical_lead_i, signal_synth::clinical_fiducial_construction), "component_lead_selection");
    ok &= check(smooth_overlay(baseline.record, modified.record, signal_synth::clinical_lead_ii)
        && smooth_overlay(baseline.record, modified.record, signal_synth::clinical_lead_v2)
        && smooth_overlay(baseline.record, modified.record, signal_synth::clinical_lead_v5), "component_overlays_are_smooth");

    signal_synth::ecg_scenario_json_result written;
    signal_synth::ecg_scenario_document parsed;
    signal_synth::ecg_scenario_json_result parsed_result;
    ok &= check(signal_synth::write_ecg_scenario_json(document, written) && written.canonical_json.find("\"extended_morphology\"") != std::string::npos
        && signal_synth::parse_ecg_scenario_json(written.canonical_json, parsed, parsed_result)
        && parsed.ecg.morphology_component_count() == document.ecg.morphology_component_count()
        && parsed_result.canonical_json == written.canonical_json, "schema_v7_roundtrip");

    signal_synth::delineation_evaluation_scope scope;
    scope.leads.push_back("II"); scope.leads.push_back("V2"); scope.leads.push_back("V5");
    std::vector<signal_synth::delineation_truth_point> delineation;
    std::vector<std::string> messages;
    ok &= check(signal_synth::delineation_ground_truth_from_render(modified, scope, delineation, messages)
        && has_delineation_kind(delineation, signal_synth::delineation_p_secondary_peak, "II")
        && has_delineation_kind(delineation, signal_synth::delineation_r_prime, "V2")
        && has_delineation_kind(delineation, signal_synth::delineation_t_notch, "V5")
        && has_delineation_kind(delineation, signal_synth::delineation_u_onset, "II")
        && has_delineation_kind(delineation, signal_synth::delineation_u_peak, "II")
        && has_delineation_kind(delineation, signal_synth::delineation_u_offset, "II"), "extended_delineation_truth");
    signal_synth::delineation_output_document perfect_delineation;
    for (std::size_t i = 0; i < delineation.size(); ++i)
        if (delineation[i].status == signal_synth::delineation_truth_present)
        {
            signal_synth::delineation_event event;
            event.lead = delineation[i].lead; event.kind = delineation[i].kind; event.time_seconds = delineation[i].time_seconds; event.original_index = static_cast<unsigned int>(perfect_delineation.events.size());
            perfect_delineation.events.push_back(event);
        }
    signal_synth::delineation_score_options delineation_options;
    signal_synth::delineation_score_result delineation_score;
    ok &= check(signal_synth::score_delineation_output_to_render(modified, perfect_delineation, scope, delineation_options, delineation_score) && delineation_score.total.f1_score == 1.0, "extended_delineation_perfect_score");

    std::vector<signal_synth::measurement_truth> measurements;
    ok &= check(signal_synth::measurement_ground_truth_from_render(modified, "morphology_assertions", measurements, messages)
        && has_measurement(measurements, "component_amplitude", "p_biphasic", "II")
        && has_measurement(measurements, "component_r_offset", "r_prime", "V2")
        && has_measurement(measurements, "component_amplitude", "u_wave", "V5"), "extended_measurement_truth");
    std::vector<signal_synth::measurement_value> perfect_measurements;
    for (std::size_t i = 0; i < measurements.size(); ++i) perfect_measurements.push_back(measurements[i].measurement);
    signal_synth::measurement_score_options measurement_options;
    signal_synth::measurement_score_result measurement_score;
    ok &= check(signal_synth::score_measurements("morphology_assertions", measurements, perfect_measurements, measurement_options, measurement_score)
        && measurement_score.total.tolerance_pass_fraction == 1.0, "extended_measurement_perfect_score");

    signal_synth::ecg_scenario_document fusion_document = base_document("fusion_beats");
    ok &= check(fusion_document.ecg.set_fusion_beats(4, 0.45), "configure_fusion_beats");
    signal_synth::ecg_render_bundle fusion;
    ok &= check(render(fusion_document, fusion), "render_fusion_beats");
    unsigned int fusion_count = 0;
    bool cadence = true;
    for (unsigned int i = 0; i < fusion.record.beat_count(); ++i)
    {
        const signal_synth::clinical_beat_annotation& beat = fusion.record.beats()[i];
        const bool expected = (beat.beat_index + 1u) % 4u == 0u;
        cadence = cadence && ((beat.origin == signal_synth::clinical_origin_fusion) == expected);
        if (expected)
        {
            ++fusion_count;
            cadence = cadence && beat.p_present && std::fabs(beat.fusion_ventricular_fraction - 0.45) < 1e-12
                && signal_synth::ecg_beat_class_from_origin(beat.origin) == signal_synth::ecg_beat_fusion;
        }
    }
    ok &= check(fusion_count > 0u && cadence, "fusion_cadence_and_classification");
    measurements.clear();
    ok &= check(signal_synth::measurement_ground_truth_from_render(fusion, "morphology_assertions", measurements, messages)
        && has_measurement(measurements, "fusion_ventricular_fraction", "", ""), "fusion_measurement_truth");
    return ok ? 0 : 1;
}
