#include "../src/detection_io.h"
#include "../src/ecg_export.h"

#include <cmath>
#include <iostream>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    bool check(bool condition, const char* name)
    {
        if (!condition)
            std::cerr << "FAIL: " << name << '\n';
        return condition;
    }

    bool render_document(signal_synth::ecg_render_bundle& render)
    {
        signal_synth::ecg_scenario_document document;
        document.scenario_id = "detection_io_clean";
        document.duration_seconds = 8.0;
        signal_synth::ecg_export_result result;
        return signal_synth::render_ecg_document(document, render, result) && result.success;
    }

    signal_synth::detection_io_document detection_document_from_render(const signal_synth::ecg_render_bundle& render)
    {
        signal_synth::detection_io_document document;
        document.target_name = "r_peak";
        document.algorithm.name = "unit_detector";
        document.algorithm.version = "1.0";
        for (unsigned int i = 0; i < render.record.beat_count(); ++i)
        {
            signal_synth::detection_io_event event;
            event.time_seconds = render.record.beats()[i].r_peak_time_seconds;
            event.label = "r";
            event.channel = "II";
            event.has_sample_index = true;
            event.sample_index = static_cast<unsigned int>(std::floor(event.time_seconds * render.record.sampling_rate_hz() + 0.5));
            event.has_confidence = true;
            event.confidence = 0.99;
            event.original_index = i;
            document.events.push_back(event);
        }
        return document;
    }

    bool compare_document(const signal_synth::ecg_render_bundle& render, const signal_synth::detection_io_document& document, signal_synth::ecg_compare_result& compare_result)
    {
        signal_synth::detection_io_result io_result;
        std::vector<signal_synth::ecg_detected_event> events;
        if (!signal_synth::detection_events_for_compare(document, events, io_result))
            return false;
        signal_synth::ecg_compare_options options;
        options.target = signal_synth::ecg_compare_r_peak;
        return signal_synth::compare_detections_to_render(render, events, options, compare_result);
    }

    std::string text(double value)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output.precision(17);
        output << value;
        return output.str();
    }

    std::string text(unsigned int value)
    {
        std::ostringstream output;
        output << value;
        return output.str();
    }
}

int main()
{
    bool ok = true;
    signal_synth::ecg_render_bundle render;
    ok &= check(render_document(render), "render_document");

    signal_synth::detection_io_document source = detection_document_from_render(render);
    signal_synth::detection_io_result result;
    ok &= check(signal_synth::write_detection_json_v1(source, result) && result.success && result.canonical_json.find("\"target\":\"r_peak\"") != std::string::npos, "write_detection_json");
    const std::string json = result.canonical_json;

    signal_synth::detection_io_document parsed_json;
    ok &= check(signal_synth::parse_detection_json_v1(json, parsed_json, result) && result.success, "parse_detection_json");
    ok &= check(parsed_json.algorithm.name == "unit_detector" && parsed_json.events.size() == source.events.size() && parsed_json.events[0].has_sample_index, "json_shape");

    std::string csv = "time_seconds,sample_index,channel,label,confidence\n";
    for (std::size_t i = 0; i < source.events.size(); ++i)
    {
        const signal_synth::detection_io_event& event = source.events[i];
        csv += text(event.time_seconds) + "," + text(event.sample_index) + ",II,r,0.99\n";
    }
    signal_synth::detection_io_document parsed_csv;
    ok &= check(signal_synth::parse_detection_csv_v2(csv, "r_peak", parsed_csv, result) && result.success, "parse_detection_csv");
    ok &= check(parsed_csv.events.size() == source.events.size() && parsed_csv.events[0].original_index == 0 && parsed_csv.events[0].channel == "II", "csv_shape");

    signal_synth::ecg_compare_result json_compare;
    signal_synth::ecg_compare_result csv_compare;
    ok &= check(compare_document(render, parsed_json, json_compare) && json_compare.success, "json_compare");
    ok &= check(compare_document(render, parsed_csv, csv_compare) && csv_compare.success, "csv_compare");
    ok &= check(json_compare.total.true_positive_count == csv_compare.total.true_positive_count && json_compare.total.f1_score == csv_compare.total.f1_score, "csv_json_score_identity");

    signal_synth::detection_io_document reversed;
    reversed.target_name = "r_peak";
    reversed.algorithm.name = "reverse_order";
    reversed.algorithm.version = "1.0";
    reversed.events.push_back(source.events[1]);
    reversed.events.back().original_index = 0;
    reversed.events.push_back(source.events[0]);
    reversed.events.back().original_index = 1;
    ok &= check(compare_document(render, reversed, json_compare) && json_compare.matches.size() >= 2, "reversed_compare");
    ok &= check(json_compare.matches[0].detection_index == 1 && json_compare.matches[1].detection_index == 0, "original_detection_index_preserved");

    const std::string quoted_csv = "time_seconds,channel,label\n1.0,\"lead,II\",\"r,label\"\n";
    ok &= check(signal_synth::parse_detection_csv_v2(quoted_csv, "r_peak", parsed_csv, result) && parsed_csv.events[0].channel == "lead,II" && parsed_csv.events[0].label == "r,label", "quoted_csv");

    const std::string duplicate_header = "time_seconds,time_seconds\n1.0,1.1\n";
    ok &= check(!signal_synth::parse_detection_csv_v2(duplicate_header, "r_peak", parsed_csv, result) && !result.messages.empty(), "reject_duplicate_header");
    const std::string unknown_header = "time_seconds,extra\n1.0,x\n";
    ok &= check(!signal_synth::parse_detection_csv_v2(unknown_header, "r_peak", parsed_csv, result) && !result.messages.empty(), "reject_unknown_header");
    const std::string bad_confidence = "time_seconds,confidence\n1.0,1.5\n";
    ok &= check(!signal_synth::parse_detection_csv_v2(bad_confidence, "r_peak", parsed_csv, result) && !result.messages.empty(), "reject_bad_confidence");
    const std::string duplicate_event = "time_seconds,channel,label\n1.0,II,r\n1.0,II,r\n";
    ok &= check(!signal_synth::parse_detection_csv_v2(duplicate_event, "r_peak", parsed_csv, result) && !result.messages.empty(), "reject_duplicate_event");
    const std::string bad_json = "{\"schema_version\":1,\"algorithm\":{\"name\":\"a\",\"version\":\"1\"},\"target\":\"r_peak\",\"events\":[{\"time_seconds\":01}]}";
    ok &= check(!signal_synth::parse_detection_json_v1(bad_json, parsed_json, result) && !result.messages.empty(), "reject_invalid_json_number");
    const std::string unknown_json_field = "{\"schema_version\":1,\"algorithm\":{\"name\":\"a\",\"version\":\"1\"},\"target\":\"r_peak\",\"events\":[],\"extra\":1}";
    ok &= check(!signal_synth::parse_detection_json_v1(unknown_json_field, parsed_json, result) && !result.messages.empty(), "reject_unknown_json_field");
    signal_synth::ecg_compare_target target;
    ok &= check(signal_synth::detection_compare_target_from_name("ppg_systolic_peak", target) && target == signal_synth::ecg_compare_ppg_systolic_peak, "target_parse");

    return ok ? 0 : 1;
}
