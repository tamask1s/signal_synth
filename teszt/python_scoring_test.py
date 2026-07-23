import csv
import json
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile
from html.parser import HTMLParser

import synsigra as ss
from synsigra.delineation import DelineationScope, delineation_truth_from_annotations


class LinkParser(HTMLParser):
    def __init__(self):
        HTMLParser.__init__(self)
        self.hrefs = []

    def handle_starttag(self, tag, attrs):
        if tag == "a":
            href = dict(attrs).get("href")
            if href:
                self.hrefs.append(href)


def html_links(path):
    parser = LinkParser()
    with open(path, "r") as handle:
        parser.feed(handle.read())
    return parser.hrefs


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


def verification_evidence(root):
    return read_json(os.path.join(root, "evidence.json"))


def case_comparison(root, case_id, target):
    matches = [
        item["comparison"]
        for item in verification_evidence(root)["results"]
        if item["case_id"] == case_id and item["target"] == target
    ]
    assert len(matches) == 1
    return matches[0]


def write_json(path, data):
    with open(path, "w") as handle:
        json.dump(data, handle, sort_keys=True, separators=(",", ":"))


def create_challenge_with_cli(source_dir, work_dir, cli):
    pack_source_dir = os.path.join(work_dir, "pack_source")
    scenario_dir = os.path.join(pack_source_dir, "scenarios")
    os.makedirs(scenario_dir)
    shutil.copyfile(os.path.join(source_dir, "examples", "scenarios", "ecg_clean.json"), os.path.join(scenario_dir, "clean_ecg.json"))
    shutil.copyfile(os.path.join(source_dir, "examples", "scenarios", "ecg_ppg_clean.json"), os.path.join(scenario_dir, "ppg_clean.json"))
    shutil.copyfile(os.path.join(source_dir, "examples", "scenarios", "ppg_perfusion_stress_v4.json"), os.path.join(scenario_dir, "ppg_stress.json"))
    shutil.copyfile(os.path.join(source_dir, "examples", "scenarios", "ppg_motion_accelerometer_v4.json"), os.path.join(scenario_dir, "ppg_motion.json"))
    shutil.copyfile(os.path.join(source_dir, "examples", "scenarios", "hrv", "hrv_mild_variability.json"), os.path.join(scenario_dir, "hrv_mild.json"))
    shutil.copyfile(os.path.join(source_dir, "examples", "scenarios", "catalog", "rhythm_psvt_episode.json"), os.path.join(scenario_dir, "rhythm_episode.json"))
    shutil.copyfile(os.path.join(source_dir, "examples", "scenarios", "packs", "sq_baseline_powerline.json"), os.path.join(scenario_dir, "signal_quality.json"))
    pack_path = os.path.join(pack_source_dir, "pack.json")
    write_json(pack_path, {
        "schema_version": 2,
        "pack_id": "python_scoring_challenge",
        "name": "Python Scoring Challenge",
        "version": "1",
        "description": "Python scoring integration test challenge.",
        "targets": ["r_peak", "ppg_systolic_peak", "ecg_beat_classification", "hrv", "rhythm_episode", "signal_quality", "ecg_delineation"],
        "scenarios": [
            {"id": "clean_ecg", "path": "scenarios/clean_ecg.json", "targets": ["r_peak", "ecg_beat_classification", "ecg_delineation"]},
            {"id": "ppg_clean", "path": "scenarios/ppg_clean.json", "targets": ["ppg_systolic_peak", "ppg_pulse_onset"]},
            {"id": "ppg_stress", "path": "scenarios/ppg_stress.json", "targets": ["ppg_systolic_peak"]},
            {"id": "ppg_motion", "path": "scenarios/ppg_motion.json", "targets": ["ppg_systolic_peak"]},
            {"id": "hrv_mild", "path": "scenarios/hrv_mild.json", "targets": ["hrv"]},
            {"id": "rhythm_episode", "path": "scenarios/rhythm_episode.json", "targets": ["rhythm_episode"]},
            {"id": "signal_quality", "path": "scenarios/signal_quality.json", "targets": ["signal_quality"]},
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


def ppg_onset_detections(annotations):
    return [
        {"time_seconds": item["time_seconds"], "label": "ppg_onset"}
        for item in annotations.get("ppg_fiducials", [])
        if item.get("kind") == "pulse_onset" and item.get("source") == "measurement"
    ]


def beat_classifications(annotations):
    return [
        {"time_seconds": item["r_peak_seconds"], "label": item["beat_class"]}
        for item in annotations["beats"]
        if item.get("qrs_present", False)
    ]


def hrv_output(case, perturb=False):
    measurements = [dict(item["measurement"]) for item in case.measurement_truth("hrv")]
    if perturb:
        for item in measurements:
            if item["name"] == "mean_rr_seconds":
                item["value"] += 0.040
        rr_indices = [index for index, item in enumerate(measurements) if item["name"] == "rr_interval"]
        del measurements[rr_indices[0]]
        measurements[rr_indices[1] - 1]["value"] += 0.030
    return {"schema_version": 2, "contract": "synsigra_measurement_values_v2", "measurements": measurements}


def write_detections(path, target, events):
    write_json(path, {
        "schema_version": 1,
        "algorithm": {"name": "python_test_detector", "version": "1"},
        "target": target,
        "events": events,
    })


def write_intervals(path, target, intervals):
    write_json(path, {
        "schema_version": 1,
        "algorithm": {"name": "python_test_interval_detector", "version": "1"},
        "target": target,
        "intervals": intervals,
    })


def write_point_events(path, events):
    write_json(path, {"schema_version": 1, "events": events})


def write_point_events_csv(path, events):
    with open(path, "w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=["time_seconds", "channel", "label"])
        writer.writeheader()
        for event in events:
            writer.writerow({name: event.get(name, "") for name in writer.fieldnames})


def write_interval_events(path, intervals):
    write_json(path, {"schema_version": 1, "intervals": intervals})


def submission_paths(challenge_dir, destination):
    shutil.copytree(os.path.join(challenge_dir, "user-output-template"), destination)
    manifest_path = os.path.join(destination, "submission.json")
    manifest = read_json(manifest_path)
    manifest["algorithm"] = {"name": "python_test_algorithm", "version": "1"}
    write_json(manifest_path, manifest)
    return dict(((item["case_id"], item["target"]), os.path.join(destination, *item["path"].split("/"))) for item in manifest["outputs"])


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
    assert challenge.case_ids() == ["clean_ecg", "ppg_clean", "ppg_stress", "ppg_motion", "hrv_mild", "rhythm_episode", "signal_quality"]
    provenance = read_json(os.path.join(challenge_dir, "provenance.json"))
    assert provenance["metadata_type"] == "synsigra_package_provenance"
    assert provenance["generator"]["version"] == "0.10.0-dev"
    assert provenance["verifier"]["version"] == "0.13.0"
    assert provenance["verifier"]["package_contract_version"] == "synsigra_challenge_package_v3"
    assert "clinical validation" in provenance["claim_boundary"]["not_for"]
    assert os.path.exists(os.path.join(challenge_dir, "ENGINEERING_CLAIM_BOUNDARY.txt"))
    case_provenance = read_json(os.path.join(challenge_dir, "cases", "clean_ecg", "provenance.json"))
    assert case_provenance["metadata_type"] == "synsigra_export_provenance"
    assert case_provenance["scenario"]["id"] == "ecg_clean_001"
    assert len(challenge.case("clean_ecg").waveform()) > 0
    assert "II_mv" in challenge.case("clean_ecg").waveform().columns
    integrity = challenge.verify_integrity()
    assert integrity["ok"] and integrity["checked_file_count"] > 0
    scoring_manifest = challenge.scoring_manifest()
    assert scoring_manifest["cases"][0]["case_summary_path"] == "cases/clean_ecg/case_summary.json"
    assert scoring_manifest["submission_contract_version"] == "synsigra_submission_v1"
    assert scoring_manifest["submission_template_path"] == "user-output-template/submission.json"
    assert scoring_manifest["submission_format_contract_path"] == "user-output-template/formats.json"
    format_contract = read_json(os.path.join(challenge_dir, "user-output-template", "formats.json"))
    assert format_contract["contract"] == "synsigra_submission_formats_v2"
    assert format_contract["target_adapters"]["ecg_beat_classification"]["required_record_fields"] == ["time_seconds", "label"]
    assert format_contract["target_adapters"]["ecg_delineation"]["required_record_fields"] == ["time_seconds", "channel", "label"]
    assert scoring_manifest["cases"][0]["scoring"][0]["accepted_formats"] == ["point_events_json_v1", "point_events_csv_v1"]
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

    direct_inputs_dir = os.path.join(work_dir, "direct_inputs")
    os.makedirs(direct_inputs_dir)
    rpeak_path = os.path.join(direct_inputs_dir, "clean_ecg.json")
    ppg_path = os.path.join(direct_inputs_dir, "ppg_clean.json")
    ppg_onset_path = os.path.join(direct_inputs_dir, "ppg_clean_ppg_pulse_onset.json")
    ppg_stress_path = os.path.join(direct_inputs_dir, "ppg_stress.json")
    ppg_motion_path = os.path.join(direct_inputs_dir, "ppg_motion.json")
    beat_class_path = os.path.join(direct_inputs_dir, "clean_ecg_beat_classes.json")
    hrv_path = os.path.join(direct_inputs_dir, "hrv_mild.json")
    rhythm_interval_path = os.path.join(direct_inputs_dir, "rhythm_episode.json")
    quality_interval_path = os.path.join(direct_inputs_dir, "signal_quality.json")
    write_detections(rpeak_path, "r_peak", rpeak_detections(challenge.case("clean_ecg").annotations()))
    write_detections(ppg_path, "ppg_systolic_peak", ppg_detections(challenge.case("ppg_clean").annotations()))
    write_detections(ppg_onset_path, "ppg_pulse_onset", ppg_onset_detections(challenge.case("ppg_clean").annotations()))
    write_detections(ppg_stress_path, "ppg_systolic_peak", ppg_detections(challenge.case("ppg_stress").annotations()))
    write_detections(ppg_motion_path, "ppg_systolic_peak", ppg_detections(challenge.case("ppg_motion").annotations()))
    write_detections(beat_class_path, "ecg_beat_classification", beat_classifications(challenge.case("clean_ecg").annotations()))
    write_json(hrv_path, hrv_output(challenge.case("hrv_mild")))
    write_intervals(rhythm_interval_path, "rhythm_episode", [{"start_seconds": item["start_seconds"], "end_seconds": item["end_seconds"], "label": item["kind"], "channel": "global"} for item in challenge.case("rhythm_episode").annotations()["episodes"] if item.get("present", True) and item["kind"] in ("psvt", "svarr")])
    write_intervals(quality_interval_path, "signal_quality", [{"start_seconds": item["start_seconds"], "end_seconds": item["end_seconds"], "label": item["type"], "channel": "global"} for item in challenge.case("signal_quality").annotations()["artifact_intervals"]])

    submission_dir = os.path.join(work_dir, "submission")
    output_paths = submission_paths(challenge_dir, submission_dir)
    submission_manifest_path = os.path.join(submission_dir, "submission.json")
    submission_manifest = read_json(submission_manifest_path)
    motion_output = next(item for item in submission_manifest["outputs"] if item["case_id"] == "ppg_motion" and item["target"] == "ppg_systolic_peak")
    os.remove(output_paths[("ppg_motion", "ppg_systolic_peak")])
    motion_output["format"] = "point_events_csv_v1"
    motion_output["path"] = os.path.splitext(motion_output["path"])[0] + ".csv"
    write_json(submission_manifest_path, submission_manifest)
    output_paths[("ppg_motion", "ppg_systolic_peak")] = os.path.join(submission_dir, *motion_output["path"].split("/"))
    write_point_events(output_paths[("clean_ecg", "r_peak")], rpeak_detections(challenge.case("clean_ecg").annotations()))
    write_point_events(output_paths[("clean_ecg", "ecg_beat_classification")], beat_classifications(challenge.case("clean_ecg").annotations()))
    delineation_scope = DelineationScope(["II", "V2"])
    delineation_truth = delineation_truth_from_annotations(challenge.case("clean_ecg").annotations(), delineation_scope, 10.0)
    write_point_events(output_paths[("clean_ecg", "ecg_delineation")], [{"time_seconds": item.time_seconds, "channel": item.lead, "label": item.kind} for item in delineation_truth if item.status == "present"])
    write_point_events(output_paths[("ppg_clean", "ppg_systolic_peak")], ppg_detections(challenge.case("ppg_clean").annotations()))
    write_point_events(output_paths[("ppg_clean", "ppg_pulse_onset")], ppg_onset_detections(challenge.case("ppg_clean").annotations()))
    write_point_events(output_paths[("ppg_stress", "ppg_systolic_peak")], ppg_detections(challenge.case("ppg_stress").annotations()))
    write_point_events_csv(output_paths[("ppg_motion", "ppg_systolic_peak")], ppg_detections(challenge.case("ppg_motion").annotations()))
    write_json(output_paths[("hrv_mild", "hrv")], hrv_output(challenge.case("hrv_mild")))
    write_interval_events(output_paths[("rhythm_episode", "rhythm_episode")], [{"start_seconds": item["start_seconds"], "end_seconds": item["end_seconds"], "label": item["kind"], "channel": "global"} for item in challenge.case("rhythm_episode").annotations()["episodes"] if item.get("present", True) and item["kind"] in ("psvt", "svarr")])
    write_interval_events(output_paths[("signal_quality", "signal_quality")], [{"start_seconds": item["start_seconds"], "end_seconds": item["end_seconds"], "label": item["type"], "channel": "global"} for item in challenge.case("signal_quality").annotations()["artifact_intervals"]])

    rpeak_detections_doc = ss.load_detections(rpeak_path, target="r_peak")
    ppg_detections_doc = ss.load_detections(ppg_path, target="ppg_systolic_peak")
    beat_class_doc = ss.load_detections(beat_class_path, target="ecg_beat_classification")
    assert len(rpeak_detections_doc) > 0
    assert len(ppg_detections_doc) > 0
    assert len(beat_class_doc) == len(rpeak_detections_doc)
    rhythm_intervals_doc = ss.load_intervals(rhythm_interval_path, target="rhythm_episode")
    quality_intervals_doc = ss.load_intervals(quality_interval_path, target="signal_quality")
    assert len(rhythm_intervals_doc) == 1 and len(quality_intervals_doc) > 0

    rpeak_report = ss.compare_rpeaks(challenge.case("clean_ecg"), rpeak_detections_doc, cli_path=cli)
    assert rpeak_report.json["comparison"]["metrics"]["total"]["f1_score"] == 1
    assert rpeak_report.html.count("Synthetic engineering QA evidence; not diagnosis, nor clinical evidence") == 1

    direct_dir = os.path.join(work_dir, "direct_compare")
    run([cli, "compare", "r_peak", challenge.case("clean_ecg").scenario_path, rpeak_path, "--out", direct_dir])
    assert read_json(os.path.join(direct_dir, "comparison.json")) == rpeak_report.json

    ppg_report = ss.compare_ppg_peaks(challenge.case("ppg_clean"), ppg_detections_doc, cli_path=cli)
    assert ppg_report.json["comparison"]["metrics"]["total"]["f1_score"] == 1
    assert ppg_report.json["comparison"]["metrics"]["pulse_timing"]["matched_interval_count"] > 0
    onset_report = ss.compare_ppg_onsets(challenge.case("ppg_clean"), ss.load_detections(ppg_onset_path, target="ppg_pulse_onset"), cli_path=cli)
    assert onset_report.json["comparison"]["metrics"]["total"]["f1_score"] == 1
    assert onset_report.json["comparison"]["metrics"]["pulse_timing"]["mean_absolute_interval_error_seconds"] == 0
    ppg_stress_report = ss.compare_ppg_peaks(challenge.case("ppg_stress"), ss.load_detections(ppg_stress_path, target="ppg_systolic_peak"), cli_path=cli)
    assert ppg_stress_report.json["comparison"]["metrics"]["low_perfusion"]["ground_truth_count"] > 0
    assert ppg_stress_report.json["comparison"]["metrics"]["weak"]["ground_truth_count"] > 0
    assert ppg_stress_report.json["comparison"]["metrics"]["missing_pulse"]["opportunity_count"] > 0

    beat_class_report = ss.compare_beat_classes(challenge.case("clean_ecg"), beat_class_doc, cli_path=cli)
    assert beat_class_report.json["summary"]["accuracy"] == 1
    assert beat_class_report.json["summary"]["micro_f1_score"] == 1
    assert "ECG Beat Classification QA Report" in beat_class_report.html

    hrv_truth = ss.load_measurement_truth(os.path.join(challenge_dir, "cases", "hrv_mild", "measurement_truth.json"), "hrv")
    hrv_predictions = ss.load_measurements(hrv_path)
    hrv_report = ss.score_measurements(hrv_truth, hrv_predictions, "hrv")
    assert hrv_report["contract"] == "synsigra_measurement_score_v2"
    assert hrv_report["overall"]["tolerance_pass_fraction"] == 1
    assert not hrv_report["missing_ground_truth_indices"] and not hrv_report["extra_prediction_indices"]

    rhythm_interval_report = ss.score_rhythm_episodes(challenge.case("rhythm_episode"), rhythm_intervals_doc, cli_path=cli)
    assert rhythm_interval_report.json["overall"]["time_f1_score"] == 1
    quality_interval_report = ss.score_signal_quality(challenge.case("signal_quality"), quality_intervals_doc, cli_path=cli)
    assert quality_interval_report.json["overall"]["time_f1_score"] == 1

    local_verify_dir = os.path.join(work_dir, "local_verify")
    local_report = ss.verify_package(archive_path, submission_dir, local_verify_dir, mode="diagnostic")
    assert local_report.evidence["success"]
    assert local_report.evidence["package"]["package_id"] == "python_scoring_challenge"
    assert local_report.evidence["case_target_count"] == 10
    assert local_report.evidence["completed_case_target_count"] == 10
    assert local_report.evidence["policy"]["profile_id"] == "regression"
    assert local_report.evidence["policy"]["passed"]
    assert case_comparison(local_verify_dir, "clean_ecg", "r_peak")["comparison"]["metrics"]["total"]["f1_score"] == 1
    rpeak_submission = case_comparison(local_verify_dir, "clean_ecg", "r_peak")["submission_output"]
    assert rpeak_submission["format"] == "point_events_json_v1"
    assert rpeak_submission["algorithm"] == {"name": "python_test_algorithm", "version": "1"}
    assert rpeak_submission["target"] == "r_peak" and rpeak_submission["sha256"].startswith("sha256:")
    assert case_comparison(local_verify_dir, "ppg_clean", "ppg_systolic_peak")["comparison"]["metrics"]["total"]["f1_score"] == 1
    local_onset = case_comparison(local_verify_dir, "ppg_clean", "ppg_pulse_onset")["comparison"]
    cpp_onset_dir = os.path.join(work_dir, "cpp_ppg_onset")
    run([cli, "compare", "ppg_pulse_onset", challenge.case("ppg_clean").scenario_path, ppg_onset_path, "--out", cpp_onset_dir])
    cpp_onset = read_json(os.path.join(cpp_onset_dir, "comparison.json"))["comparison"]
    for key in (
        "target",
        "tolerance_seconds",
        "success",
        "metrics",
        "matches",
        "false_positives",
        "false_negatives",
        "excluded_ground_truth",
        "excluded_detections",
    ):
        assert local_onset[key] == cpp_onset[key]
    stress_metrics = case_comparison(local_verify_dir, "ppg_stress", "ppg_systolic_peak")["comparison"]["metrics"]
    assert stress_metrics["low_perfusion"]["ground_truth_count"] > 0
    assert stress_metrics["weak"]["ground_truth_count"] > 0
    assert stress_metrics["missing_pulse"]["opportunity_count"] > 0
    motion_metrics = case_comparison(local_verify_dir, "ppg_motion", "ppg_systolic_peak")["comparison"]["metrics"]
    assert motion_metrics["motion"]["ground_truth_count"] > 0
    assert motion_metrics["motion"]["true_positive_count"] == motion_metrics["motion"]["ground_truth_count"]
    motion_submission = case_comparison(local_verify_dir, "ppg_motion", "ppg_systolic_peak")["submission_output"]
    assert motion_submission["format"] == "point_events_csv_v1"
    assert motion_submission["algorithm"] == {"name": "python_test_algorithm", "version": "1"}
    assert motion_submission["target"] == "ppg_systolic_peak" and motion_submission["sha256"].startswith("sha256:")
    cpp_stress_dir = os.path.join(work_dir, "cpp_ppg_stress")
    run([cli, "compare", "ppg_systolic_peak", challenge.case("ppg_stress").scenario_path, ppg_stress_path, "--out", cpp_stress_dir])
    python_stress = case_comparison(local_verify_dir, "ppg_stress", "ppg_systolic_peak")["comparison"]
    cpp_stress = read_json(os.path.join(cpp_stress_dir, "comparison.json"))["comparison"]
    for key in (
        "target",
        "tolerance_seconds",
        "success",
        "metrics",
        "matches",
        "false_positives",
        "false_negatives",
        "excluded_ground_truth",
        "excluded_detections",
    ):
        assert python_stress[key] == cpp_stress[key]
    cpp_motion_dir = os.path.join(work_dir, "cpp_ppg_motion")
    run([cli, "compare", "ppg_systolic_peak", challenge.case("ppg_motion").scenario_path, ppg_motion_path, "--out", cpp_motion_dir])
    python_motion = case_comparison(local_verify_dir, "ppg_motion", "ppg_systolic_peak")["comparison"]
    cpp_motion = read_json(os.path.join(cpp_motion_dir, "comparison.json"))["comparison"]
    for key in (
        "target",
        "tolerance_seconds",
        "success",
        "metrics",
        "matches",
        "false_positives",
        "false_negatives",
        "excluded_ground_truth",
        "excluded_detections",
    ):
        assert python_motion[key] == cpp_motion[key]
    assert case_comparison(local_verify_dir, "clean_ecg", "ecg_beat_classification")["summary"]["micro_f1_score"] == 1
    delineation_report = case_comparison(local_verify_dir, "clean_ecg", "ecg_delineation")
    assert delineation_report["overall"]["f1_score"] == 1
    assert delineation_report["schema_version"] == 2
    assert any(item["anchor_type"] == "atrial_event" for item in delineation_report["truth"])
    assert case_comparison(local_verify_dir, "hrv_mild", "hrv")["overall"]["tolerance_pass_fraction"] == 1
    local_rhythm_interval = case_comparison(local_verify_dir, "rhythm_episode", "rhythm_episode")
    local_quality_interval = case_comparison(local_verify_dir, "signal_quality", "signal_quality")
    assert local_rhythm_interval["overall"]["time_f1_score"] == 1
    assert local_quality_interval["overall"]["time_f1_score"] == 1
    for local_interval, direct_report in ((local_rhythm_interval, rhythm_interval_report.json), (local_quality_interval, quality_interval_report.json)):
        for key in ("target", "options", "overall", "classes", "confusion_matrix", "matches", "false_positive_indices", "false_negative_indices"):
            assert local_interval[key] == direct_report[key]
    assert next(item for item in local_report.evidence["targets"] if item["target"] == "ecg_beat_classification")["confusion_matrix"]["labels"] == ["normal", "supraventricular_ectopic", "ventricular_ectopic", "paced", "escape", "fusion", "unscored"]
    assert local_report.evidence["hrv_pipeline"]["available"] and local_report.evidence["hrv_pipeline"]["complete"]
    assert [item["stage"] for item in local_report.evidence["hrv_pipeline"]["stages"]] == ["r_peak_detection", "rr_interval_reconstruction", "hrv_metric_computation", "signal_quality_interval_detection"]
    assert all(item["score"] == 1 for item in local_report.evidence["hrv_pipeline"]["stages"])
    assert len([os.path.join(root, name) for root, _dirs, names in os.walk(local_verify_dir) for name in names]) == 12
    with open(os.path.join(local_verify_dir, "index.html"), "r") as handle:
        html = handle.read()
        assert "Synsigra verification evidence" in html and "Pipeline trace" in html
        assert "href=\"details/clean_ecg_r_peak.html\"" in html
        assert html.count("Synthetic engineering QA evidence; not diagnosis, nor clinical evidence") == 1
    html_paths = set(item["report_path"] for item in local_report.evidence["results"])
    assert set(link for link in html_links(os.path.join(local_verify_dir, "index.html")) if link.endswith(".html")) == html_paths
    for item in local_report.evidence["results"]:
        detail_path = os.path.join(local_verify_dir, item["report_path"])
        assert os.path.isfile(detail_path)
        detail_html = open(detail_path, "r").read()
        assert "href=\"../index.html\"" in detail_html
        assert detail_html.count("Synthetic engineering QA evidence; not diagnosis, nor clinical evidence") == 1
        for href in html_links(detail_path):
            target = href.split("#", 1)[0]
            if target:
                assert os.path.isfile(os.path.normpath(os.path.join(os.path.dirname(detail_path), target)))

    cli_verify_dir = os.path.join(work_dir, "cli_verify")
    cli_output = run([sys.executable, "-m", "synsigra.cli", "verify", archive_path, submission_dir, cli_verify_dir, "--mode", "diagnostic"])
    assert "status=diagnostic_passed" in cli_output
    assert read_json(os.path.join(cli_verify_dir, "evidence.json"))["success"]

    degraded_dir = os.path.join(work_dir, "degraded")
    shutil.copytree(submission_dir, degraded_dir)
    degraded_outputs = dict(((item["case_id"], item["target"]), os.path.join(degraded_dir, *item["path"].split("/"))) for item in read_json(os.path.join(degraded_dir, "submission.json"))["outputs"])
    degraded_direct_dir = os.path.join(work_dir, "degraded_direct")
    os.makedirs(degraded_direct_dir)
    degraded_rpeaks = rpeak_detections(challenge.case("clean_ecg").annotations())
    degraded_rpeaks[0]["time_seconds"] += 0.020
    degraded_rpeaks = degraded_rpeaks[:-1]
    degraded_rpeaks.append({"time_seconds": 100.0, "label": "r"})
    degraded_rpeak_path = os.path.join(degraded_direct_dir, "clean_ecg_r_peak.json")
    write_detections(degraded_rpeak_path, "r_peak", degraded_rpeaks)
    write_point_events(degraded_outputs[("clean_ecg", "r_peak")], degraded_rpeaks)

    degraded_classes = beat_classifications(challenge.case("clean_ecg").annotations())
    degraded_classes[0]["label"] = "ventricular_ectopic"
    degraded_classes = degraded_classes[:-1]
    degraded_class_path = os.path.join(degraded_direct_dir, "clean_ecg_ecg_beat_classification.json")
    write_detections(degraded_class_path, "ecg_beat_classification", degraded_classes)
    write_point_events(degraded_outputs[("clean_ecg", "ecg_beat_classification")], degraded_classes)

    degraded_hrv_path = os.path.join(degraded_direct_dir, "hrv_mild.json")
    write_json(degraded_hrv_path, hrv_output(challenge.case("hrv_mild"), perturb=True))
    write_json(degraded_outputs[("hrv_mild", "hrv")], hrv_output(challenge.case("hrv_mild"), perturb=True))

    degraded_verify_dir = os.path.join(work_dir, "degraded_verify")
    degraded_report = ss.verify_package(challenge_dir, degraded_dir, degraded_verify_dir, mode="diagnostic", cases=["clean_ecg", "hrv_mild"], profile="regression")
    assert not degraded_report.evidence["success"]
    assert degraded_report.evidence["scoring_success"]
    assert degraded_report.evidence["policy"]["failed_check_count"] > 0
    degraded_rpeak_summary = next(item for item in degraded_report.evidence["targets"] if item["target"] == "r_peak")
    assert degraded_rpeak_summary["total"]["mean_absolute_error_seconds"] > 0

    cpp_rpeak_dir = os.path.join(work_dir, "cpp_degraded_rpeak")
    run([cli, "compare", "r_peak", challenge.case("clean_ecg").scenario_path, degraded_rpeak_path, "--out", cpp_rpeak_dir])
    python_rpeak = case_comparison(degraded_verify_dir, "clean_ecg", "r_peak")["comparison"]
    cpp_rpeak = read_json(os.path.join(cpp_rpeak_dir, "comparison.json"))["comparison"]
    for key in (
        "target",
        "tolerance_seconds",
        "success",
        "metrics",
        "matches",
        "false_positives",
        "false_negatives",
        "excluded_ground_truth",
        "excluded_detections",
    ):
        assert python_rpeak[key] == cpp_rpeak[key]

    cpp_class_dir = os.path.join(work_dir, "cpp_degraded_class")
    run([cli, "compare", "ecg_beat_classification", challenge.case("clean_ecg").scenario_path, degraded_class_path, "--out", cpp_class_dir])
    python_class = case_comparison(degraded_verify_dir, "clean_ecg", "ecg_beat_classification")
    cpp_class = read_json(os.path.join(cpp_class_dir, "comparison.json"))
    for key in ("summary", "classes", "confusion_matrix", "matches", "unmatched_ground_truth", "unmatched_predictions"):
        assert python_class[key] == cpp_class[key]

    cpp_hrv_dir = os.path.join(work_dir, "cpp_degraded_hrv")
    run([cli, "measurement", "score", "hrv", challenge.case("hrv_mild").scenario_path, degraded_hrv_path, "--out", cpp_hrv_dir])
    python_hrv = case_comparison(degraded_verify_dir, "hrv_mild", "hrv")
    cpp_hrv = read_json(os.path.join(cpp_hrv_dir, "measurement_score.json"))
    assert python_hrv["contract"] == cpp_hrv["contract"] == "synsigra_measurement_score_v2"
    for key in ("ground_truth_count", "prediction_count", "matched_count", "numeric_pair_count", "tolerance_pass_count", "missing_count", "extra_count", "truth_match_fraction", "prediction_match_fraction", "tolerance_pass_fraction", "status_match_fraction"):
        assert python_hrv["overall"][key] == cpp_hrv["overall"][key]

    failed_cli_dir = os.path.join(work_dir, "failed_cli")
    failed_cli = subprocess.Popen([sys.executable, "-m", "synsigra.cli", "verify", challenge_dir, degraded_dir, failed_cli_dir, "--mode", "diagnostic", "--case", "clean_ecg", "--profile", "regression"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
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
