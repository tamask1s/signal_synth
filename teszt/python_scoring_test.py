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


def copy_case(source_dir, scenario_name, case_root, case_id, cli):
    scenario_source = os.path.join(source_dir, "examples", "scenarios", scenario_name)
    scenario_dir = os.path.join(case_root, case_id)
    os.makedirs(scenario_dir)
    scenario_path = os.path.join(scenario_dir, "scenario.json")
    shutil.copyfile(scenario_source, scenario_path)
    render_dir = os.path.join(scenario_dir, "render")
    run([cli, "render", scenario_path, "--out", render_dir])
    shutil.copyfile(os.path.join(render_dir, "waveform.csv"), os.path.join(scenario_dir, "waveform.csv"))
    shutil.copyfile(os.path.join(render_dir, "annotations.json"), os.path.join(scenario_dir, "annotations.json"))
    annotations = read_json(os.path.join(scenario_dir, "annotations.json"))
    return {
        "id": case_id,
        "scenario_id": read_json(scenario_path)["scenario_id"],
        "scenario_path": "cases/%s/scenario.json" % case_id,
        "document_fingerprint": annotations["document_fingerprint"],
        "render_identity": annotations["render_identity"],
        "files": [
            "cases/%s/scenario.json" % case_id,
            "cases/%s/waveform.csv" % case_id,
            "cases/%s/annotations.json" % case_id,
        ],
    }


def write_manifest(challenge_dir, cases):
    files = []
    for item in cases:
        for path in item["files"]:
            role = "scenario_json"
            media_type = "application/json"
            if path.endswith("waveform.csv"):
                role = "waveform_csv"
                media_type = "text/csv"
            elif path.endswith("annotations.json"):
                role = "annotations_json"
            files.append({
                "path": path,
                "role": role,
                "media_type": media_type,
                "sha256": "sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
                "size_bytes": 1,
                "required": True,
            })
    manifest = {
        "schema_version": 1,
        "package_id": "python_scoring_challenge",
        "name": "Python Scoring Challenge",
        "version": "1",
        "description": "Python scoring integration test challenge.",
        "package_type": "scenario_pack",
        "ground_truth_included": True,
        "waveform_formats": ["csv"],
        "generator_version": "test",
        "usage_restrictions": "engineering algorithm QA only",
        "not_for": "diagnosis, patient monitoring, clinical validation certificate, or standalone conformity assessment",
        "files": files,
        "cases": cases,
    }
    write_json(os.path.join(challenge_dir, "manifest.json"), manifest)


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

    challenge_dir = os.path.join(work_dir, "challenge")
    os.makedirs(os.path.join(challenge_dir, "cases"))
    ecg_case = copy_case(source_dir, "ecg_clean.json", os.path.join(challenge_dir, "cases"), "clean_ecg", cli)
    ppg_case = copy_case(source_dir, "ecg_ppg_clean.json", os.path.join(challenge_dir, "cases"), "ppg_clean", cli)
    write_manifest(challenge_dir, [ecg_case, ppg_case])

    challenge = ss.load_challenge(challenge_dir)
    assert challenge.package_id == "python_scoring_challenge"
    assert challenge.case_ids() == ["clean_ecg", "ppg_clean"]
    assert len(challenge.case("clean_ecg").waveform()) > 0
    assert "II_mv" in challenge.case("clean_ecg").waveform().columns

    archive_path = os.path.join(work_dir, "challenge.synsigra")
    zip_directory(challenge_dir, archive_path)
    archive_challenge = ss.load_challenge(archive_path)
    assert archive_challenge.case("clean_ecg").scenario_id == challenge.case("clean_ecg").scenario_id
    archive_challenge.close()

    detections_dir = os.path.join(work_dir, "detections")
    os.makedirs(detections_dir)
    rpeak_path = os.path.join(detections_dir, "clean_ecg.json")
    ppg_path = os.path.join(detections_dir, "ppg_clean.json")
    beat_class_path = os.path.join(detections_dir, "clean_ecg_beat_classes.json")
    write_detections(rpeak_path, "r_peak", rpeak_detections(challenge.case("clean_ecg").annotations()))
    write_detections(ppg_path, "ppg_systolic_peak", ppg_detections(challenge.case("ppg_clean").annotations()))
    write_detections(beat_class_path, "ecg_beat_classification", beat_classifications(challenge.case("clean_ecg").annotations()))

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

    exported_dir = os.path.join(work_dir, "exported_report")
    rpeak_report.write(exported_dir)
    assert os.path.exists(os.path.join(exported_dir, "comparison_report.html"))

    challenge.close()
    print("python_scoring_test=passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
