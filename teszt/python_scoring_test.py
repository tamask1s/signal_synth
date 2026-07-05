import json
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile

import synsigra as ss


def run(command):
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = process.communicate()
    if process.returncode != 0:
        raise RuntimeError("command failed: %s\n%s\n%s" % (" ".join(command), stdout.decode("utf-8"), stderr.decode("utf-8")))
    if stderr:
        raise RuntimeError("stderr was not empty: %s" % stderr.decode("utf-8"))
    return stdout.decode("utf-8")


def read_json(path):
    with open(path, "r") as handle:
        return json.load(handle)


def write_json(path, data):
    with open(path, "w") as handle:
        json.dump(data, handle, sort_keys=True, separators=(",", ":"))


def create_challenge_with_cli(source_dir, work_dir, cli):
    pack_source_dir = os.path.join(work_dir, "pack_source")
    scenario_dir = os.path.join(pack_source_dir, "scenarios")
    os.makedirs(scenario_dir)
    shutil.copyfile(os.path.join(source_dir, "examples", "scenarios", "ecg_clean.json"), os.path.join(scenario_dir, "clean_ecg.json"))
    shutil.copyfile(os.path.join(source_dir, "examples", "scenarios", "ecg_ppg_clean.json"), os.path.join(scenario_dir, "ppg_clean.json"))
    shutil.copyfile(os.path.join(source_dir, "examples", "scenarios", "hrv", "hrv_mild_variability.json"), os.path.join(scenario_dir, "hrv_mild.json"))
    pack_path = os.path.join(pack_source_dir, "pack.json")
    write_json(pack_path, {
        "schema_version": 1,
        "pack_id": "python_scoring_challenge",
        "name": "Python Scoring Challenge",
        "version": "1",
        "description": "Python scoring integration test challenge.",
        "targets": ["r_peak", "ppg_systolic_peak", "ecg_beat_classification", "hrv"],
        "scenarios": [
            {"id": "clean_ecg", "path": "scenarios/clean_ecg.json", "targets": ["r_peak", "ecg_beat_classification"]},
            {"id": "ppg_clean", "path": "scenarios/ppg_clean.json", "targets": ["ppg_systolic_peak"]},
            {"id": "hrv_mild", "path": "scenarios/hrv_mild.json", "targets": ["hrv"]},
        ],
    })
    challenge_dir = os.path.join(work_dir, "challenge")
    run([cli, "pack", "challenge", pack_path, "--out", challenge_dir])
    return challenge_dir


def rpeak_detections(annotations):
    return [{"time_seconds": item["r_peak_seconds"], "label": "r"} for item in annotations["beats"] if item.get("qrs_present", False)]


def ppg_detections(annotations):
    return [
        {"time_seconds": item["time_seconds"], "label": "ppg_peak"}
        for item in annotations.get("ppg_fiducials", [])
        if item.get("kind") == "systolic_peak" and item.get("source") == "measurement"
    ]


def beat_classifications(annotations):
    return [
        {"time_seconds": item["r_peak_seconds"], "label": item["beat_class"]}
        for item in annotations["beats"]
        if item.get("qrs_present", False)
    ]


def hrv_output(case, perturb=False):
    truth = case.hrv_metrics()
    accepted = [item for item in truth["tachogram"] if not item["excluded"]]
    mean_rr = truth["time_domain"]["mean_rr_seconds"]
    intervals = [{"beat_time_seconds": item["beat_time_seconds"], "rr_seconds": item["rr_seconds"]} for item in accepted]
    if perturb:
        mean_rr += 0.040
        intervals = intervals[1:]
        intervals[0]["rr_seconds"] += 0.030
    return {
        "schema_version": 1,
        "algorithm": {"name": "python_test_hrv", "version": "1"},
        "metrics": {"mean_rr_seconds": mean_rr, "rmssd_seconds": truth["time_domain"]["rmssd_seconds"]},
        "rr_intervals": intervals,
    }


def write_detections(path, target, events):
    write_json(path, {
        "schema_version": 1,
        "algorithm": {"name": "python_test_detector", "version": "1"},
        "target": target,
        "events": events,
    })


def zip_directory(source, archive_path):
    with zipfile.ZipFile(archive_path, "w") as archive:
        for root, dirs, files in os.walk(source):
            dirs.sort()
            files.sort()
            for name in files:
                full = os.path.join(root, name)
                archive.write(full, os.path.relpath(full, source))


def main():
    cli = os.environ["SIGNAL_SYNTH_CLI"]
    source_dir = os.environ["SIGNAL_SYNTH_SOURCE_DIR"]
    work_dir = os.environ.get("SIGNAL_SYNTH_PY_WORK_DIR") or tempfile.mkdtemp(prefix="synsigra_py_")
    if os.path.exists(work_dir):
        shutil.rmtree(work_dir)
    os.makedirs(work_dir)

    challenge_dir = create_challenge_with_cli(source_dir, work_dir, cli)

    assert ss.threshold_profile_names() == ["benchmark", "regression", "smoke", "stress"]
    assert ss.load_threshold_profile("regression")["profile_id"] == "regression"
    try:
        ss.load_threshold_profile({"schema_version": 1, "profile_id": "broken", "targets": {"event_detection": {"total": {"f1_score": {"minimum": 0.9}}}}})
        raise AssertionError("invalid threshold operator was accepted")
    except ss.ThresholdProfileError:
        pass

    challenge = ss.load_challenge(challenge_dir)
    assert challenge.package_id == "python_scoring_challenge"
    assert challenge.case_ids() == ["clean_ecg", "ppg_clean", "hrv_mild"]
    assert len(challenge.case("clean_ecg").waveform()) > 0
    assert "II_mv" in challenge.case("clean_ecg").waveform().columns
    integrity = challenge.verify_integrity()
    assert integrity["ok"] and integrity["checked_file_count"] > 0
    scoring_manifest = challenge.scoring_manifest()
    assert scoring_manifest["cases"][0]["case_summary_path"] == "cases/clean_ecg/case_summary.json"
    assert scoring_manifest["cases"][0]["scoring"][0]["score_command"].startswith("signal-synth compare rpeaks")
    case_summary = challenge.case("clean_ecg").case_summary()
    assert case_summary["case_id"] == "clean_ecg"
    assert case_summary["render"]["sample_rate_hz"] == 500
    assert "r_peak" in case_summary["targets"]
    assert challenge.case("clean_ecg").metadata()["scenario"]["id"] == "ecg_clean_001"
    assert challenge.case("clean_ecg").ground_truth_metrics()["beats"]["count"] == 12
    assert challenge.case("clean_ecg").hrv_metrics()["counts"]["accepted"] == 12
    assert isinstance(challenge.case("clean_ecg").warnings()["issues"], list)

    tampered_dir = os.path.join(work_dir, "tampered")
    shutil.copytree(challenge_dir, tampered_dir)
    with open(os.path.join(tampered_dir, "cases", "clean_ecg", "waveform.csv"), "a") as handle:
        handle.write("tamper\n")
    tampered = ss.load_challenge(tampered_dir)
    try:
        tampered.verify_integrity()
        raise AssertionError("tampered package was accepted")
    except ss.ChallengeIntegrityError:
        pass
    tampered.close()

    archive_path = os.path.join(work_dir, "challenge.synsigra")
    zip_directory(challenge_dir, archive_path)
    archive_challenge = ss.load_challenge(archive_path)
    assert archive_challenge.case("clean_ecg").scenario_id == challenge.case("clean_ecg").scenario_id
    archive_challenge.close()

    detections_dir = os.path.join(work_dir, "detections")
    os.makedirs(detections_dir)
    rpeak_path = os.path.join(detections_dir, "clean_ecg.json")
    rpeak_recommended_path = os.path.join(detections_dir, "clean_ecg_r_peak.json")
    ppg_path = os.path.join(detections_dir, "ppg_clean.json")
    beat_class_path = os.path.join(detections_dir, "clean_ecg_beat_classes.json")
    beat_class_recommended_path = os.path.join(detections_dir, "clean_ecg_ecg_beat_classification.json")
    hrv_path = os.path.join(detections_dir, "hrv_mild.json")
    write_detections(rpeak_path, "r_peak", rpeak_detections(challenge.case("clean_ecg").annotations()))
    shutil.copyfile(rpeak_path, rpeak_recommended_path)
    write_detections(ppg_path, "ppg_systolic_peak", ppg_detections(challenge.case("ppg_clean").annotations()))
    write_detections(beat_class_path, "ecg_beat_classification", beat_classifications(challenge.case("clean_ecg").annotations()))
    shutil.copyfile(beat_class_path, beat_class_recommended_path)
    write_json(hrv_path, hrv_output(challenge.case("hrv_mild")))

    rpeak_detections_doc = ss.load_detections(rpeak_path, target="r_peak")
    ppg_detections_doc = ss.load_detections(ppg_path, target="ppg_systolic_peak")
    beat_class_doc = ss.load_detections(beat_class_path, target="ecg_beat_classification")
    assert len(rpeak_detections_doc) > 0
    assert len(ppg_detections_doc) > 0
    assert len(beat_class_doc) == len(rpeak_detections_doc)

    rpeak_report = ss.compare_rpeaks(challenge.case("clean_ecg"), rpeak_detections_doc, cli_path=cli)
    assert rpeak_report.json["comparison"]["metrics"]["total"]["f1_score"] == 1
    assert "not diagnosis" in rpeak_report.html and "clinical validation certification" in rpeak_report.html

    direct_dir = os.path.join(work_dir, "direct_compare")
    run([cli, "compare", "rpeaks", challenge.case("clean_ecg").scenario_path, rpeak_path, "--out", direct_dir])
    assert read_json(os.path.join(direct_dir, "comparison.json")) == rpeak_report.json

    ppg_report = ss.compare_ppg_peaks(challenge.case("ppg_clean"), ppg_detections_doc, cli_path=cli)
    assert ppg_report.json["comparison"]["metrics"]["total"]["f1_score"] == 1

    beat_class_report = ss.compare_beat_classes(challenge.case("clean_ecg"), beat_class_doc, cli_path=cli)
    assert beat_class_report.json["summary"]["accuracy"] == 1
    assert beat_class_report.json["summary"]["micro_f1_score"] == 1
    assert "ECG Beat Classification QA Report" in beat_class_report.html

    hrv_annotations = challenge.case("clean_ecg").annotations()
    accepted_rr = [item for item in hrv_annotations["rr_tachogram"] if not item["excluded"]]
    mean_rr = sum(item["rr_seconds"] for item in accepted_rr) / len(accepted_rr)
    hrv_report = ss.score_hrv(challenge.case("clean_ecg"), {
        "schema_version": 1,
        "algorithm": {"name": "python_hrv_test", "version": "1"},
        "metrics": {"mean_rr_seconds": mean_rr},
        "rr_intervals": [
            {"beat_time_seconds": item["beat_time_seconds"], "rr_seconds": item["rr_seconds"]}
            for item in accepted_rr
        ],
    }, cli_path=cli)
    assert hrv_report.json["metric_pass_fraction"] == 1
    assert hrv_report.json["rr"]["missing_count"] == 0
    assert "HRV Algorithm QA Score" in hrv_report.html

    local_verify_dir = os.path.join(work_dir, "local_verify")
    local_report = ss.verify_package(archive_path, detections_dir, local_verify_dir)
    assert local_report.summary["success"]
    assert local_report.summary["package"]["package_id"] == "python_scoring_challenge"
    assert local_report.summary["case_target_count"] == 4
    assert local_report.summary["passed_case_target_count"] == 4
    assert local_report.summary["policy"]["profile_id"] == "regression"
    assert local_report.summary["policy"]["passed"]
    assert read_json(os.path.join(local_verify_dir, "verification", "clean_ecg_r_peak", "comparison.json"))["comparison"]["metrics"]["total"]["f1_score"] == 1
    assert read_json(os.path.join(local_verify_dir, "verification", "ppg_clean", "comparison.json"))["comparison"]["metrics"]["total"]["f1_score"] == 1
    assert read_json(os.path.join(local_verify_dir, "verification", "clean_ecg_ecg_beat_classification", "comparison.json"))["summary"]["micro_f1_score"] == 1
    assert read_json(os.path.join(local_verify_dir, "verification", "hrv_mild", "comparison.json"))["metric_pass_fraction"] == 1
    assert next(item for item in local_report.summary["targets"] if item["target"] == "ecg_beat_classification")["confusion_matrix"]["labels"] == ["normal", "supraventricular_ectopic", "ventricular_ectopic", "paced", "escape", "unscored"]
    with open(os.path.join(local_verify_dir, "verification_report.html"), "r") as handle:
        assert "Synsigra Local Verification Report" in handle.read()

    cli_verify_dir = os.path.join(work_dir, "cli_verify")
    cli_output = run([sys.executable, "-m", "synsigra.cli", "verify", archive_path, detections_dir, cli_verify_dir])
    assert "status=passed" in cli_output
    assert read_json(os.path.join(cli_verify_dir, "verification_summary.json"))["success"]

    degraded_dir = os.path.join(work_dir, "degraded")
    os.makedirs(degraded_dir)
    degraded_rpeaks = rpeak_detections(challenge.case("clean_ecg").annotations())
    degraded_rpeaks[0]["time_seconds"] += 0.020
    degraded_rpeaks = degraded_rpeaks[:-1]
    degraded_rpeaks.append({"time_seconds": 100.0, "label": "r"})
    degraded_rpeak_path = os.path.join(degraded_dir, "clean_ecg_r_peak.json")
    write_detections(degraded_rpeak_path, "r_peak", degraded_rpeaks)

    degraded_classes = beat_classifications(challenge.case("clean_ecg").annotations())
    degraded_classes[0]["label"] = "ventricular_ectopic"
    degraded_classes = degraded_classes[:-1]
    degraded_class_path = os.path.join(degraded_dir, "clean_ecg_ecg_beat_classification.json")
    write_detections(degraded_class_path, "ecg_beat_classification", degraded_classes)

    degraded_hrv_path = os.path.join(degraded_dir, "hrv_mild.json")
    write_json(degraded_hrv_path, hrv_output(challenge.case("hrv_mild"), perturb=True))

    degraded_verify_dir = os.path.join(work_dir, "degraded_verify")
    degraded_report = ss.verify_package(challenge_dir, degraded_dir, degraded_verify_dir, cases=["clean_ecg", "hrv_mild"], profile="regression")
    assert not degraded_report.summary["success"]
    assert degraded_report.summary["scoring_success"]
    assert degraded_report.summary["policy"]["failed_check_count"] > 0
    degraded_rpeak_summary = next(item for item in degraded_report.summary["targets"] if item["target"] == "r_peak")
    assert degraded_rpeak_summary["total"]["mean_absolute_error_seconds"] > 0

    cpp_rpeak_dir = os.path.join(work_dir, "cpp_degraded_rpeak")
    run([cli, "compare", "rpeaks", challenge.case("clean_ecg").scenario_path, degraded_rpeak_path, "--out", cpp_rpeak_dir])
    python_rpeak = read_json(os.path.join(degraded_verify_dir, "verification", "clean_ecg_r_peak", "comparison.json"))["comparison"]
    cpp_rpeak = read_json(os.path.join(cpp_rpeak_dir, "comparison.json"))["comparison"]
    for key in ("target", "tolerance_seconds", "success", "metrics", "matches", "false_positives", "false_negatives"):
        assert python_rpeak[key] == cpp_rpeak[key]

    cpp_class_dir = os.path.join(work_dir, "cpp_degraded_class")
    run([cli, "compare", "beat-classes", challenge.case("clean_ecg").scenario_path, degraded_class_path, "--out", cpp_class_dir])
    python_class = read_json(os.path.join(degraded_verify_dir, "verification", "clean_ecg_ecg_beat_classification", "comparison.json"))
    cpp_class = read_json(os.path.join(cpp_class_dir, "comparison.json"))
    for key in ("summary", "classes", "confusion_matrix", "matches", "unmatched_ground_truth", "unmatched_predictions"):
        assert python_class[key] == cpp_class[key]

    cpp_hrv_dir = os.path.join(work_dir, "cpp_degraded_hrv")
    run([cli, "hrv", "score", challenge.case("hrv_mild").scenario_path, degraded_hrv_path, "--out", cpp_hrv_dir])
    python_hrv = read_json(os.path.join(degraded_verify_dir, "verification", "hrv_mild", "comparison.json"))
    cpp_hrv = read_json(os.path.join(cpp_hrv_dir, "hrv_score.json"))
    assert python_hrv["metric_pass_fraction"] == cpp_hrv["metric_pass_fraction"]
    assert python_hrv["metrics"] == cpp_hrv["metrics"]
    for key in ("evaluated", "ground_truth_count", "user_count", "matched_count", "missing_count", "extra_count", "passed_count", "time_tolerance_seconds", "absolute_tolerance_seconds", "relative_tolerance_percent", "mean_absolute_error_seconds", "rms_error_seconds", "max_absolute_error_seconds"):
        assert python_hrv["rr"][key] == cpp_hrv["rr"][key]

    failed_cli_dir = os.path.join(work_dir, "failed_cli")
    failed_cli = subprocess.Popen([sys.executable, "-m", "synsigra.cli", "verify", challenge_dir, degraded_dir, failed_cli_dir, "--case", "clean_ecg", "--profile", "regression"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    failed_stdout, failed_stderr = failed_cli.communicate()
    assert failed_cli.returncode == 1
    assert not failed_stderr
    assert "failed_policy_check_count=" in failed_stdout.decode("utf-8")

    exported_dir = os.path.join(work_dir, "exported_report")
    rpeak_report.write(exported_dir)
    assert os.path.exists(os.path.join(exported_dir, "comparison_report.html"))

    challenge.close()
    print("python_scoring_test=passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
