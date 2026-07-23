#include "../src/ecg_beat_classification.h"
#include "../src/ecg_export.h"

#include <cmath>
#include <iostream>
#include <string>

namespace
{
    bool check(bool condition, const char* name)
    {
        if (!condition)
            std::cerr << "FAIL: " << name << '\n';
        return condition;
    }

    bool render_pvc(signal_synth::ecg_render_bundle& render)
    {
        signal_synth::ecg_scenario_document document;
        document.scenario_id = "beat_class_pvc";
        document.duration_seconds = 10.0;
        document.ecg.clear_conditions();
        document.ecg.add_condition(signal_synth::ecg_condition_pvc, 0.7);
        document.ecg.set_ectopic_every_n_beats(3);
        signal_synth::ecg_document_render_result result;
        return signal_synth::render_ecg_document(document, render, result);
    }

    bool render_vt(signal_synth::ecg_render_bundle& render)
    {
        signal_synth::clinical_ecg_config config;
        config.rhythm.rhythm = signal_synth::clinical_rhythm_ventricular_tachycardia;
        config.rhythm.heart_rate_bpm = 150.0;
        render.document.scenario_id = "beat_class_vt";
        return signal_synth::clinical_ecg_generator(config).generate(2500, render.record);
    }

    signal_synth::detection_io_document predictions_from_render(const signal_synth::ecg_render_bundle& render)
    {
        signal_synth::detection_io_document predictions;
        predictions.target_name = "ecg_beat_classification";
        predictions.has_compare_target = true;
        predictions.compare_target = signal_synth::ecg_compare_beat_classification;
        predictions.algorithm.name = "unit_beat_classifier";
        predictions.algorithm.version = "1.0";
        for (unsigned int i = 0; i < render.record.beat_count(); ++i)
        {
            const signal_synth::clinical_beat_annotation& beat = render.record.beats()[i];
            if (!beat.qrs_present)
                continue;
            signal_synth::detection_io_event event;
            event.time_seconds = beat.r_peak_time_seconds;
            event.label = signal_synth::ecg_beat_class_name(signal_synth::ecg_beat_class_from_origin(beat.origin));
            event.original_index = i;
            predictions.events.push_back(event);
        }
        return predictions;
    }
}

int main()
{
    bool ok = true;
    ok &= check(signal_synth::ecg_beat_class_from_origin(signal_synth::clinical_origin_conducted) == signal_synth::ecg_beat_normal
        && signal_synth::ecg_beat_class_from_origin(signal_synth::clinical_origin_pac) == signal_synth::ecg_beat_supraventricular_ectopic
        && signal_synth::ecg_beat_class_from_origin(signal_synth::clinical_origin_pvc) == signal_synth::ecg_beat_ventricular_ectopic
        && signal_synth::ecg_beat_class_from_origin(signal_synth::clinical_origin_paced) == signal_synth::ecg_beat_paced
        && signal_synth::ecg_beat_class_from_origin(signal_synth::clinical_origin_atrial_paced) == signal_synth::ecg_beat_paced
        && signal_synth::ecg_beat_class_from_origin(signal_synth::clinical_origin_junctional_escape) == signal_synth::ecg_beat_escape
        && signal_synth::ecg_beat_class_from_origin(signal_synth::clinical_origin_ventricular_escape) == signal_synth::ecg_beat_escape
        && signal_synth::ecg_beat_class_from_origin(signal_synth::clinical_origin_fusion) == signal_synth::ecg_beat_fusion
        && signal_synth::ecg_beat_class_from_origin(signal_synth::clinical_origin_vt) == signal_synth::ecg_beat_unscored, "origin_class_mapping");

    signal_synth::ecg_render_bundle render;
    ok &= check(render_pvc(render), "render_pvc_classification_case");
    signal_synth::detection_io_document perfect = predictions_from_render(render);
    signal_synth::detection_io_result io_result;
    signal_synth::detection_io_document parsed;
    ok &= check(signal_synth::write_detection_json_v1(perfect, io_result) && signal_synth::parse_detection_json_v1(io_result.canonical_json, parsed, io_result), "classification_detection_roundtrip");

    signal_synth::ecg_beat_classification_options options;
    signal_synth::ecg_beat_classification_result result;
    ok &= check(signal_synth::score_ecg_beat_classification(render, parsed, options, result) && result.success, "perfect_classification_scores");
    ok &= check(result.correct_count == result.scored_ground_truth_count && result.accuracy == 1.0 && result.micro_f1_score == 1.0 && result.classes[signal_synth::ecg_beat_normal].f1_score == 1.0 && result.classes[signal_synth::ecg_beat_ventricular_ectopic].f1_score == 1.0, "perfect_classification_metrics");

    signal_synth::detection_io_document wrong = perfect;
    std::size_t ventricular_index = 0;
    while (ventricular_index < wrong.events.size() && wrong.events[ventricular_index].label != "ventricular_ectopic")
        ++ventricular_index;
    ok &= check(ventricular_index < wrong.events.size(), "ventricular_prediction_present");
    if (ventricular_index < wrong.events.size())
        wrong.events[ventricular_index].label = "normal";
    wrong.events[0].time_seconds += 0.020;
    ok &= check(signal_synth::score_ecg_beat_classification(render, wrong, options, result), "wrong_class_scores");
    ok &= check(result.confusion[signal_synth::ecg_beat_ventricular_ectopic][signal_synth::ecg_beat_normal] == 1u
        && result.classes[signal_synth::ecg_beat_ventricular_ectopic].false_negative_count == 1u
        && result.classes[signal_synth::ecg_beat_normal].false_positive_count == 1u
        && result.mean_absolute_error_seconds > 0.0, "wrong_class_confusion_and_timing");

    signal_synth::detection_io_document incomplete = perfect;
    incomplete.events.pop_back();
    signal_synth::detection_io_event extra;
    extra.time_seconds = 20.0;
    extra.label = "normal";
    extra.original_index = static_cast<unsigned int>(incomplete.events.size());
    incomplete.events.push_back(extra);
    ok &= check(signal_synth::score_ecg_beat_classification(render, incomplete, options, result)
        && result.unmatched_ground_truth.size() == 1u && result.unmatched_predictions.size() == 1u
        && result.micro_precision < 1.0 && result.micro_recall < 1.0, "unmatched_events_score");

    signal_synth::detection_io_document invalid = perfect;
    invalid.events[0].label = "pvc";
    std::vector<signal_synth::ecg_classified_beat_event> events;
    std::vector<std::string> messages;
    ok &= check(!signal_synth::beat_classification_events_from_detection(invalid, events, messages) && !messages.empty(), "noncanonical_label_rejected");

    signal_synth::ecg_render_bundle vt_render;
    ok &= check(render_vt(vt_render), "render_unscored_vt_case");
    signal_synth::detection_io_document vt_predictions = predictions_from_render(vt_render);
    for (std::size_t i = 0; i < vt_predictions.events.size(); ++i)
        vt_predictions.events[i].label = "normal";
    ok &= check(signal_synth::score_ecg_beat_classification(vt_render, vt_predictions, options, result)
        && result.scored_ground_truth_count == 0u && result.unscored_match_count == result.matched_count
        && result.classes[signal_synth::ecg_beat_unscored].ground_truth_count == vt_render.record.beat_count(), "unscored_ground_truth_is_neutral");

    ok &= check(signal_synth::score_ecg_beat_classification(render, perfect, options, result), "report_source_score");
    const std::string json = signal_synth::ecg_beat_classification_result_json(render, result);
    const std::string csv = signal_synth::ecg_beat_classification_result_csv(result);
    const std::string html = signal_synth::ecg_beat_classification_report_html(render, result);
    ok &= check(json.find("\"score_type\":\"ecg_beat_classification_qa\"") != std::string::npos && json.find("\"confusion_matrix\"") != std::string::npos, "classification_json_report");
    ok &= check(csv.find("row_type,class,scored") == 0 && csv.find("actual_class,predicted_class,count") != std::string::npos, "classification_csv_report");
    const std::string notice = "Synthetic engineering QA evidence; not diagnosis, nor clinical evidence";
    ok &= check(html.find("ECG Beat Classification QA Report") != std::string::npos && html.find(notice) != std::string::npos
        && html.find(notice) == html.rfind(notice) && html.find("background:#f3f4f6") != std::string::npos,
        "classification_html_report");
    return ok ? 0 : 1;
}
