#include "../src/delineation_io.h"

#include <iostream>
#include <string>

namespace
{
    bool check(bool condition, const char* name)
    {
        if (!condition) std::cerr << "FAIL: " << name << '\n';
        return condition;
    }
}

int main()
{
    bool ok = true;
    const std::string valid = "{\"schema_version\":1,\"events\":[{\"time_seconds\":1.25,\"sample_index\":625,\"channel\":\"II\",\"label\":\"qrs_onset\",\"confidence\":0.9}]}";
    signal_synth::delineation_output_document document;
    signal_synth::delineation_io_result result;
    ok &= check(signal_synth::parse_delineation_point_events_json_v1(valid, document, result) && result.success, "json_parse");
    ok &= check(document.events.size() == 1u && document.events[0].lead == "II" && document.events[0].kind == signal_synth::delineation_qrs_onset, "event_parse");
    ok &= check(document.events[0].has_sample_index && document.events[0].sample_index == 625u, "sample_index_parse");
    ok &= check(document.events[0].has_confidence && document.events[0].confidence == 0.9, "confidence_parse");
    ok &= check(result.canonical_json.find("\"sample_index\":625") != std::string::npos && result.canonical_json.find("\"channel\":\"II\"") != std::string::npos && result.canonical_json.find("beat_index") == std::string::npos, "identity_free_json");
    ok &= check(result.canonical_csv.find("time_seconds,sample_index,channel,label,confidence") == 0u, "csv_header");

    signal_synth::delineation_output_document round_trip;
    signal_synth::delineation_io_result round_trip_result;
    ok &= check(signal_synth::parse_delineation_point_events_json_v1(result.canonical_json, round_trip, round_trip_result) && round_trip_result.canonical_json == result.canonical_json, "json_round_trip");
    ok &= check(signal_synth::parse_delineation_point_events_csv_v1(result.canonical_csv, round_trip, round_trip_result) && round_trip_result.canonical_csv == result.canonical_csv, "csv_round_trip");

    const std::string empty = "{\"schema_version\":1,\"events\":[]}";
    ok &= check(signal_synth::parse_delineation_point_events_json_v1(empty, document, result) && document.events.empty(), "empty_predictions_valid");
    const std::string duplicate = "{\"schema_version\":1,\"events\":[{\"time_seconds\":1,\"channel\":\"II\",\"label\":\"p_peak\"},{\"time_seconds\":1,\"channel\":\"II\",\"label\":\"p_peak\"}]}";
    ok &= check(!signal_synth::parse_delineation_point_events_json_v1(duplicate, document, result), "duplicate_event_rejected");
    const std::string legacy_identity = "{\"schema_version\":1,\"events\":[{\"time_seconds\":1,\"channel\":\"II\",\"label\":\"p_peak\",\"beat_index\":\"1\"}]}";
    ok &= check(!signal_synth::parse_delineation_point_events_json_v1(legacy_identity, document, result), "generator_identity_rejected");
    const std::string unknown_label = "{\"schema_version\":1,\"events\":[{\"time_seconds\":1,\"channel\":\"II\",\"label\":\"u_peak\"}]}";
    ok &= check(!signal_synth::parse_delineation_point_events_json_v1(unknown_label, document, result), "unknown_label_rejected");
    const std::string unknown_lead = "{\"schema_version\":1,\"events\":[{\"time_seconds\":1,\"channel\":\"ECG\",\"label\":\"p_peak\"}]}";
    ok &= check(!signal_synth::parse_delineation_point_events_json_v1(unknown_lead, document, result), "unknown_lead_rejected");
    const std::string missing_channel_csv = "time_seconds,sample_index,channel,label,confidence\n1,,,p_peak,\n";
    ok &= check(!signal_synth::parse_delineation_point_events_csv_v1(missing_channel_csv, document, result), "csv_channel_required");
    const std::string unknown_column = "time_seconds,sample_index,channel,label,confidence,extra\n1,,II,p_peak,,x\n";
    ok &= check(!signal_synth::parse_delineation_point_events_csv_v1(unknown_column, document, result), "unknown_csv_column_rejected");
    return ok ? 0 : 1;
}
