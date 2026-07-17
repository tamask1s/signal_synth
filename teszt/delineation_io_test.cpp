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
    const std::string valid = "{\"schema_version\":1,\"algorithm\":{\"name\":\"delineator\",\"version\":\"2\"},\"target\":\"ecg_delineation\",\"scope\":{\"mode\":\"selected_beats\",\"beat_indices\":[\"18446744073709551615\",\"2\"],\"leads\":[\"V2\",\"II\"]},\"events\":[{\"beat_index\":\"2\",\"lead\":\"II\",\"kind\":\"qrs_onset\",\"time_seconds\":1.25,\"confidence\":0.9}]}";
    signal_synth::delineation_output_document document;
    signal_synth::delineation_io_result result;
    ok &= check(signal_synth::parse_delineation_json_v1(valid, document, result) && result.success, "json_parse");
    ok &= check(document.beat_indices.size() == 2u && document.beat_indices[1] == 18446744073709551615ULL, "uint64_identity");
    ok &= check(document.leads.size() == 2u && document.leads[0] == "II" && document.leads[1] == "V2", "lead_order");
    ok &= check(document.events.size() == 1u && document.events[0].kind == signal_synth::delineation_qrs_onset, "event_parse");
    ok &= check(result.canonical_json.find("\"beat_index\":\"2\"") != std::string::npos, "json_identity_is_string");
    ok &= check(result.canonical_csv.find("row_type,scope_mode,evaluated_beat_index") == 0u, "csv_header");

    signal_synth::delineation_output_document round_trip;
    signal_synth::delineation_io_result round_trip_result;
    ok &= check(signal_synth::parse_delineation_json_v1(result.canonical_json, round_trip, round_trip_result) && round_trip_result.canonical_json == result.canonical_json, "json_round_trip");
    ok &= check(signal_synth::parse_delineation_csv_v1(result.canonical_csv, round_trip, round_trip_result) && round_trip_result.canonical_csv == result.canonical_csv, "csv_round_trip");

    const std::string empty = "{\"schema_version\":1,\"algorithm\":{\"name\":\"none\",\"version\":\"1\"},\"target\":\"ecg_delineation\",\"scope\":{\"mode\":\"all_beats\",\"leads\":[\"II\"]},\"events\":[]}";
    ok &= check(signal_synth::parse_delineation_json_v1(empty, document, result) && document.events.empty(), "empty_predictions_valid");
    const std::string leading_zero = "{\"schema_version\":1,\"algorithm\":{\"name\":\"x\",\"version\":\"1\"},\"target\":\"ecg_delineation\",\"scope\":{\"mode\":\"selected_beats\",\"beat_indices\":[\"01\"],\"leads\":[\"II\"]},\"events\":[]}";
    ok &= check(!signal_synth::parse_delineation_json_v1(leading_zero, document, result), "noncanonical_identity_rejected");
    const std::string duplicate = "{\"schema_version\":1,\"algorithm\":{\"name\":\"x\",\"version\":\"1\"},\"target\":\"ecg_delineation\",\"scope\":{\"mode\":\"all_beats\",\"leads\":[\"II\"]},\"events\":[{\"beat_index\":\"0\",\"lead\":\"II\",\"kind\":\"p_peak\",\"time_seconds\":1},{\"beat_index\":\"0\",\"lead\":\"II\",\"kind\":\"p_peak\",\"time_seconds\":1.1}]}";
    ok &= check(!signal_synth::parse_delineation_json_v1(duplicate, document, result), "duplicate_identity_rejected");
    const std::string outside_scope = "{\"schema_version\":1,\"algorithm\":{\"name\":\"x\",\"version\":\"1\"},\"target\":\"ecg_delineation\",\"scope\":{\"mode\":\"selected_beats\",\"beat_indices\":[\"0\"],\"leads\":[\"II\"]},\"events\":[{\"beat_index\":\"1\",\"lead\":\"II\",\"kind\":\"p_peak\",\"time_seconds\":1}]}";
    ok &= check(!signal_synth::parse_delineation_json_v1(outside_scope, document, result), "event_scope_rejected");
    const std::string no_scope_csv = "row_type,scope_mode,evaluated_beat_index,beat_index,lead,kind,time_seconds\nevent,,,0,II,r_peak,1\n";
    ok &= check(!signal_synth::parse_delineation_csv_v1(no_scope_csv, document, result), "csv_scope_required");
    const std::string unknown_column = "row_type,scope_mode,evaluated_beat_index,beat_index,lead,kind,time_seconds,extra\nscope,all_beats,,,II,,,x\n";
    ok &= check(!signal_synth::parse_delineation_csv_v1(unknown_column, document, result), "unknown_csv_column_rejected");
    const std::string incomplete_scope = "row_type,scope_mode,evaluated_beat_index,beat_index,lead,kind,time_seconds\nscope,selected_beats,1,,II,,\nscope,selected_beats,2,,V2,,\n";
    ok &= check(!signal_synth::parse_delineation_csv_v1(incomplete_scope, document, result), "incomplete_scope_grid_rejected");
    return ok ? 0 : 1;
}
