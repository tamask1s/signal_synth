#include "../src/interval_io.h"

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
    signal_synth::interval_output_document document;
    signal_synth::interval_io_result result;
    const std::string valid = "{\"schema_version\":1,\"algorithm\":{\"name\":\"detector\",\"version\":\"2\"},\"target\":\"rhythm_episode\",\"intervals\":[{\"start_seconds\":3,\"end_seconds\":5,\"label\":\"psvt\",\"confidence\":0.8},{\"start_seconds\":1,\"end_seconds\":2,\"label\":\"svarr\",\"channel\":\"global\"}]}";
    ok &= check(signal_synth::parse_interval_json_v1(valid, document, result) && result.success, "json_parse");
    ok &= check(document.intervals.size() == 2 && document.intervals[0].start_seconds == 1.0 && document.intervals[1].start_seconds == 3.0, "canonical_order");
    ok &= check(document.intervals[1].channel == "global" && document.intervals[1].has_confidence, "defaults_and_optional_confidence");
    ok &= check(result.canonical_json.find("\"target\":\"rhythm_episode\"") != std::string::npos && result.canonical_csv.find("start_seconds,end_seconds,label,channel,confidence") == 0, "canonical_outputs");

    signal_synth::interval_output_document round_trip;
    signal_synth::interval_io_result round_trip_result;
    ok &= check(signal_synth::parse_interval_json_v1(result.canonical_json, round_trip, round_trip_result) && round_trip_result.canonical_json == result.canonical_json, "json_round_trip");
    ok &= check(signal_synth::parse_interval_csv_v1(result.canonical_csv, "rhythm_episode", round_trip, round_trip_result) && round_trip.intervals.size() == 2, "csv_round_trip");

    const std::string empty = "{\"schema_version\":1,\"algorithm\":{\"name\":\"none\",\"version\":\"1\"},\"target\":\"signal_quality\",\"intervals\":[]}";
    ok &= check(signal_synth::parse_interval_json_v1(empty, document, result) && document.intervals.empty(), "empty_predictions_valid");

    const std::string duplicate = "{\"schema_version\":1,\"algorithm\":{\"name\":\"x\",\"version\":\"1\"},\"target\":\"signal_quality\",\"intervals\":[{\"start_seconds\":1,\"end_seconds\":2,\"label\":\"noise\"},{\"start_seconds\":1,\"end_seconds\":2,\"label\":\"noise\"}]}";
    ok &= check(!signal_synth::parse_interval_json_v1(duplicate, document, result) && !result.messages.empty() && result.messages.back().code == signal_synth::interval_io_duplicate, "duplicate_rejected");

    const std::string malformed_bounds = "start_seconds,end_seconds,label\n2,1,noise\n";
    ok &= check(!signal_synth::parse_interval_csv_v1(malformed_bounds, "signal_quality", document, result), "invalid_bounds_rejected");
    const std::string unknown_column = "start_seconds,end_seconds,label,extra\n1,2,noise,x\n";
    ok &= check(!signal_synth::parse_interval_csv_v1(unknown_column, "signal_quality", document, result), "unknown_csv_column_rejected");
    const std::string physical_rhythm = "start_seconds,end_seconds,label,channel\n1,2,psvt,II\n";
    ok &= check(!signal_synth::parse_interval_csv_v1(physical_rhythm, "rhythm_episode", document, result), "rhythm_channel_rejected");
    const std::string unknown_json = "{\"schema_version\":1,\"algorithm\":{\"name\":\"x\",\"version\":\"1\"},\"target\":\"signal_quality\",\"intervals\":[],\"extra\":1}";
    ok &= check(!signal_synth::parse_interval_json_v1(unknown_json, document, result), "unknown_json_field_rejected");

    signal_synth::interval_target target;
    ok &= check(signal_synth::interval_target_from_name("rhythm_episode", target) && target == signal_synth::interval_target_rhythm_episode, "rhythm_target_name");
    ok &= check(signal_synth::interval_target_from_name("signal_quality", target) && target == signal_synth::interval_target_signal_quality, "quality_target_name");
    ok &= check(!signal_synth::interval_target_from_name("other", target), "unknown_target_name");
    return ok ? 0 : 1;
}

