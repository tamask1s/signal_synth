import csv
import json
import os
import shutil
import subprocess

import synsigra as ss


def run(command):
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = process.communicate()
    if process.returncode != 0 or stderr:
        raise RuntimeError("command failed: %s\n%s\n%s" % (" ".join(command), stdout.decode("utf-8"), stderr.decode("utf-8")))
    return stdout.decode("utf-8")


def read_json(path):
    with open(path, "r") as handle:
        return json.load(handle)


def write_json(path, data):
    with open(path, "w") as handle:
        json.dump(data, handle, sort_keys=True, separators=(",", ":"))


def truth_for_target(path, target):
    document = read_json(path)
    matches = [item for item in document["targets"] if item["target"] == target]
    assert len(matches) == 1
    return matches[0]["measurements"]


def rr_measurement(value, endpoint):
    return {
        "name": "rr_interval",
        "value": value,
        "unit": "s",
        "status": "valid",
        "scope": "beat",
        "time_seconds": endpoint,
        "method_id": "observable_r_peak_difference",
        "preprocessing_policy_id": "none",
    }


def rr_truth(value, endpoint):
    return {
        "measurement": rr_measurement(value, endpoint),
        "absolute_tolerance": 0.025,
        "relative_tolerance_percent": 0,
        "error_model": "linear",
        "reason": "Deterministic R-peak interval fixture.",
        "original_index": int(endpoint),
    }


def assert_local_rr_split_merge_scoring():
    truth = [rr_truth(1.0, 1.0), rr_truth(1.0, 2.0)]

    split = ss.score_measurements(
        truth,
        [
            rr_measurement(0.5, 0.5),
            rr_measurement(0.5, 1.0),
            rr_measurement(1.0, 2.0),
        ],
        "rr_interval",
    )
    assert split["options"]["rr_pairing_method"] == "peak_anchored_interval_overlap"
    assert split["overall"]["matched_count"] == 3
    assert split["overall"]["covered_truth_count"] == 2
    assert split["overall"]["matched_prediction_count"] == 3
    assert split["overall"]["truth_match_fraction"] == 1.0
    assert split["overall"]["prediction_match_fraction"] == 1.0
    assert split["overall"]["missing_count"] == 0
    assert split["overall"]["extra_count"] == 0
    assert split["overall"]["error"]["median_absolute"] == 0.5
    assert set(item["pairing_method"] for item in split["matches"]) == set([
        "rr_peak_anchored_interval_overlap",
    ])

    merged = ss.score_measurements(
        truth,
        [rr_measurement(2.0, 2.0)],
        "rr_interval",
    )
    assert merged["overall"]["matched_count"] == 2
    assert merged["overall"]["covered_truth_count"] == 2
    assert merged["overall"]["matched_prediction_count"] == 1
    assert merged["overall"]["truth_match_fraction"] == 1.0
    assert merged["overall"]["prediction_match_fraction"] == 1.0
    assert merged["overall"]["missing_count"] == 0
    assert merged["overall"]["extra_count"] == 0
    assert merged["overall"]["error"]["median_absolute"] == 1.0


def main():
    source = os.environ["SIGNAL_SYNTH_SOURCE_DIR"]
    cli = os.environ["SIGNAL_SYNTH_CLI"]
    work = os.environ["SIGNAL_SYNTH_MEASUREMENT_WORK_DIR"]
    if os.path.exists(work):
        shutil.rmtree(work)
    os.makedirs(work)
    source_dir = os.path.join(work, "source")
    scenarios = os.path.join(source_dir, "scenarios")
    os.makedirs(scenarios)
    morphology_source = os.path.join(source, "examples", "scenarios", "catalog", "morph_clbbb.json")
    alignment_source = os.path.join(source, "examples", "scenarios", "packs", "ppg_delay_180.json")
    shutil.copyfile(morphology_source, os.path.join(scenarios, "morphology.json"))
    shutil.copyfile(alignment_source, os.path.join(scenarios, "alignment.json"))
    pack_path = os.path.join(source_dir, "pack.json")
    write_json(pack_path, {
        "schema_version": 2, "pack_id": "measurement_python_test", "name": "Measurement Python Test", "version": "1", "description": "Measurement verifier parity fixture.",
        "targets": ["morphology_assertions", "ecg_ppg_alignment"],
        "scenarios": [
            {"id": "morphology", "path": "scenarios/morphology.json", "targets": ["morphology_assertions"]},
            {"id": "alignment", "path": "scenarios/alignment.json", "targets": ["ecg_ppg_alignment"]},
        ],
    })
    challenge_dir = os.path.join(work, "challenge")
    run([cli, "pack", "challenge", pack_path, "--out", challenge_dir])
    challenge = ss.load_challenge(challenge_dir)
    assert challenge.verify_integrity()["ok"]
    assert challenge.case("morphology").measurement_truth("morphology_assertions")
    manifest = read_json(os.path.join(challenge_dir, "manifest.json"))
    roles = [item["role"] for item in manifest["files"] if item["path"].endswith("measurement_truth.json")]
    assert roles == ["measurement_truth_json", "measurement_truth_json"]

    submission_dir = os.path.join(work, "submission")
    shutil.copytree(os.path.join(challenge_dir, "user-output-template"), submission_dir)
    submission_path = os.path.join(submission_dir, "submission.json")
    submission = read_json(submission_path)
    submission["algorithm"] = {"name": "perfect_measurement_fixture", "version": "1"}
    write_json(submission_path, submission)
    for output in submission["outputs"]:
        truth_path = os.path.join(challenge_dir, "cases", output["case_id"], "measurement_truth.json")
        truth = truth_for_target(truth_path, output["target"])
        measurements = [dict(item["measurement"]) for item in truth]
        write_json(os.path.join(submission_dir, *output["path"].split("/")), {"schema_version": 2, "contract": "synsigra_measurement_values_v2", "measurements": measurements})

    verify_dir = os.path.join(work, "verify")
    report = ss.verify_package(challenge, submission_dir, verify_dir, mode="diagnostic", profile="regression")
    assert report.evidence["success"] and report.evidence["scoring_version"] == "synsigra-python-local-v10"
    assert report.evidence["contract"] == "synsigra_local_verification_v3" and not report.evidence["verification"]["evidence_eligible"]
    assert set(item["target"] for item in report.evidence["targets"]) == set(["morphology_assertions", "ecg_ppg_alignment"])
    for item in report.evidence["targets"]:
        assert item["overall"]["tolerance_pass_fraction"] == 1.0
        assert item["overall"]["status_match_fraction"] == 1.0
        assert item["policy"]["passed"]

    morphology_output = os.path.join(submission_dir, "outputs", "morphology", "morphology_assertions.json")
    evidence = read_json(os.path.join(verify_dir, "evidence.json"))
    python_case_report = next(
        item["comparison"] for item in evidence["results"]
        if item["case_id"] == "morphology" and item["target"] == "morphology_assertions"
    )
    assert python_case_report["tolerance_rules"]
    assert all(item["unit"] for item in python_case_report["by_measurement_context"])
    assert all("ground_truth_value" in item and "prediction_value" in item and "effective_tolerance" in item for item in python_case_report["matches"] if item["numeric_pair"])
    cpp_dir = os.path.join(work, "cpp")
    run([cli, "measurement", "score", "morphology_assertions", morphology_source, morphology_output, "--out", cpp_dir])
    cpp_report = read_json(os.path.join(cpp_dir, "measurement_score.json"))
    assert all(item["unit"] for item in cpp_report["by_measurement_context"])
    assert all(
        "absolute_tolerance" in item and
        "relative_tolerance_percent" in item and
        "effective_tolerance" in item
        for item in cpp_report["matches"] if item["numeric_pair"]
    )
    for name in ("ground_truth_count", "prediction_count", "matched_count", "covered_truth_count", "matched_prediction_count", "numeric_pair_count", "tolerance_pass_count", "status_match_count", "missing_count", "extra_count", "truth_match_fraction", "prediction_match_fraction", "tolerance_pass_fraction", "status_match_fraction"):
        assert python_case_report["overall"][name] == cpp_report["overall"][name], name
    for name in ("bias", "mean_absolute", "root_mean_square", "median_absolute", "p95_absolute", "maximum_absolute"):
        assert abs(python_case_report["overall"]["error"][name] - cpp_report["overall"]["error"][name]) < 1e-15, name

    write_json(morphology_output, {"schema_version": 2, "contract": "synsigra_measurement_values_v2", "measurements": []})
    empty_report = ss.verify_package(challenge, submission_dir, os.path.join(work, "verify_empty"), mode="diagnostic", profile="regression")
    morphology_target = [item for item in empty_report.evidence["targets"] if item["target"] == "morphology_assertions"][0]
    assert not empty_report.evidence["success"] and not morphology_target["policy"]["passed"]
    assert morphology_target["overall"]["truth_match_fraction"] == 0.0
    write_json(morphology_output, {"schema_version": 2, "contract": "synsigra_measurement_values_v2", "measurements": [dict(item["measurement"]) for item in truth_for_target(os.path.join(challenge_dir, "cases", "morphology", "measurement_truth.json"), "morphology_assertions")]})

    first_records = read_json(morphology_output)["measurements"][:8]
    csv_path = os.path.join(work, "measurements.csv")
    with open(csv_path, "w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=["name", "value", "unit", "status", "scope", "time_seconds", "beat_index", "window_start_seconds", "window_end_seconds", "channel", "formula", "method_id", "preprocessing_policy_id", "confidence"])
        writer.writeheader()
        for item in first_records:
            writer.writerow(item)
    assert len(ss.load_measurements(csv_path, "measurement_values_csv_v2")) == len(first_records)

    duplicate_path = os.path.join(work, "duplicate.json")
    with open(duplicate_path, "w") as handle:
        handle.write('{"schema_version":2,"schema_version":2,"contract":"synsigra_measurement_values_v2","measurements":[]}')
    try:
        ss.load_measurements(duplicate_path)
        raise AssertionError("duplicate JSON key was accepted")
    except ss.MeasurementError:
        pass

    legacy_path = os.path.join(work, "legacy_v1.json")
    write_json(legacy_path, {"schema_version": 1, "measurements": []})
    try:
        ss.load_measurements(legacy_path)
        raise AssertionError("measurement v1 input was accepted")
    except ss.MeasurementError:
        pass
    assert_local_rr_split_merge_scoring()
    challenge.close()
    print("measurement_python_test=passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
