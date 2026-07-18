#include "../src/measurement_io.h"

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
    const std::string json = "{\"schema_version\":1,\"measurements\":["
        "{\"name\":\"qtc_interval\",\"value\":0.42,\"unit\":\"s\",\"status\":\"valid\",\"scope\":\"beat\",\"time_seconds\":1.2,\"beat_index\":\"18446744073709551615\",\"formula\":\"fridericia\",\"confidence\":0.9},"
        "{\"name\":\"t_amplitude\",\"unit\":\"mV\",\"status\":\"absent\",\"scope\":\"beat_lead\",\"time_seconds\":1.2,\"channel\":\"II\"},"
        "{\"name\":\"qrs_axis\",\"value\":45,\"unit\":\"deg\",\"status\":\"valid\",\"scope\":\"record\"},"
        "{\"name\":\"lf_normalized_units\",\"value\":42,\"unit\":\"nu\",\"status\":\"valid\",\"scope\":\"record\"},"
        "{\"name\":\"pulse_transit_time\",\"value\":0.18,\"unit\":\"s\",\"status\":\"valid\",\"scope\":\"paired_signal\",\"time_seconds\":1.2,\"channel\":\"ecg_r_to_ppg_green_onset\"}]}";
    signal_synth::measurement_output_document document;
    signal_synth::measurement_io_result result;
    ok &= check(signal_synth::parse_measurement_values_json_v1(json, document, result) && result.success, "json_parse");
    ok &= check(document.measurements.size() == 5u && document.measurements[0].has_beat_index && document.measurements[0].beat_index == 18446744073709551615ULL, "uint64_string");
    ok &= check(!document.measurements[1].has_value && document.measurements[1].status == signal_synth::measurement_absent, "explicit_absent");
    ok &= check(result.canonical_csv.find("name,value,unit,status,scope,time_seconds,beat_index,channel,formula,confidence\n") == 0u, "csv_header");

    signal_synth::measurement_output_document round_trip;
    signal_synth::measurement_io_result round_trip_result;
    ok &= check(signal_synth::parse_measurement_values_json_v1(result.canonical_json, round_trip, round_trip_result) && round_trip_result.canonical_json == result.canonical_json, "json_round_trip");
    ok &= check(signal_synth::parse_measurement_values_csv_v1(result.canonical_csv, round_trip, round_trip_result) && round_trip_result.canonical_csv == result.canonical_csv, "csv_round_trip");
    ok &= check(signal_synth::parse_measurement_values_json_v1("{\"schema_version\":1,\"measurements\":[]}", document, result) && document.measurements.empty(), "empty_valid");

    ok &= check(!signal_synth::parse_measurement_values_json_v1("{\"schema_version\":1,\"measurements\":[{\"name\":\"qtc_interval\",\"value\":0.4,\"unit\":\"s\",\"status\":\"valid\",\"scope\":\"beat\",\"time_seconds\":1}]}", document, result), "qtc_formula_required");
    ok &= check(!signal_synth::parse_measurement_values_json_v1("{\"schema_version\":1,\"measurements\":[{\"name\":\"qrs_axis\",\"unit\":\"deg\",\"status\":\"valid\",\"scope\":\"record\"}]}", document, result), "valid_requires_value");
    ok &= check(!signal_synth::parse_measurement_values_json_v1("{\"schema_version\":1,\"measurements\":[{\"name\":\"t_amplitude\",\"value\":0,\"unit\":\"mV\",\"status\":\"absent\",\"scope\":\"beat_lead\",\"time_seconds\":1,\"channel\":\"II\"}]}", document, result), "absent_forbids_value");
    ok &= check(!signal_synth::parse_measurement_values_json_v1("{\"schema_version\":1,\"measurements\":[{\"name\":\"qrs_duration\",\"value\":0.1,\"unit\":\"ms\",\"status\":\"valid\",\"scope\":\"beat\",\"time_seconds\":1}]}", document, result), "unit_validated");
    ok &= check(!signal_synth::parse_measurement_values_json_v1("{\"schema_version\":1,\"measurements\":[{\"name\":\"qrs_axis\",\"value\":1,\"unit\":\"deg\",\"status\":\"valid\",\"scope\":\"record\",\"extra\":1}]}", document, result), "unknown_field_rejected");
    ok &= check(!signal_synth::parse_measurement_values_json_v1("{\"schema_version\":1,\"measurements\":[{\"name\":\"qrs_axis\",\"value\":1,\"unit\":\"deg\",\"status\":\"valid\",\"scope\":\"record\"},{\"name\":\"qrs_axis\",\"value\":2,\"unit\":\"deg\",\"status\":\"valid\",\"scope\":\"record\"}]}", document, result), "duplicate_rejected");
    ok &= check(!signal_synth::parse_measurement_values_csv_v1("value,name,unit,status,scope,time_seconds,beat_index,channel,formula,confidence\n1,qrs_axis,deg,valid,record,,,,,\n", document, result), "csv_header_order");
    ok &= check(signal_synth::parse_measurement_values_json_v1("{\"schema_version\":1,\"measurements\":[{\"name\":\"assertion.norm.rhythm.0\",\"value\":1,\"unit\":\"bool\",\"status\":\"valid\",\"scope\":\"record\"}]}", document, result), "boolean_assertion_unit");
    ok &= check(!signal_synth::parse_measurement_values_json_v1("{\"schema_version\":1,\"measurements\":[{\"name\":\"assertion.norm.rhythm.0\",\"value\":0.5,\"unit\":\"bool\",\"status\":\"valid\",\"scope\":\"record\"}]}", document, result), "boolean_assertion_value");
    return ok ? 0 : 1;
}
