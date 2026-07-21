import json
import math
import os
import shutil
import subprocess

import synsigra as ss
from synsigra.delineation import DelineationScope, delineation_truth_from_annotations


def run(command):
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = process.communicate()
    if process.returncode != 0 or stderr:
        raise RuntimeError("command failed: %s\n%s\n%s" % (" ".join(command), stdout.decode("utf-8"), stderr.decode("utf-8")))
    return stdout.decode("utf-8")


def read_json(path):
    with open(path, "r") as handle:
        return json.load(handle)


def write_json(path, value):
    with open(path, "w") as handle:
        json.dump(value, handle, sort_keys=True, separators=(",", ":"))


def output_path(submission_dir, output):
    return os.path.join(submission_dir, *output["path"].split("/"))


def r_peak_events(annotations):
    return [{"time_seconds": item["r_peak_seconds"]} for item in annotations["beats"] if item.get("qrs_present", False)]


def signal_quality_intervals(annotations):
    return [{"start_seconds": item["start_seconds"], "end_seconds": item["end_seconds"], "label": item["type"], "channel": "global"} for item in annotations.get("artifact_intervals", [])]


def delineation_events(case):
    duration = case.case_summary()["render"]["duration_seconds"]
    truth = delineation_truth_from_annotations(case.annotations(), DelineationScope(["II", "V2"]), duration)
    return [{"time_seconds": item.time_seconds, "channel": item.lead, "label": item.kind} for item in truth if item.status == "present"]


def make_perfect_submission(challenge_dir, submission_dir):
    shutil.copytree(os.path.join(challenge_dir, "user-output-template"), submission_dir)
    manifest_path = os.path.join(submission_dir, "submission.json")
    manifest = read_json(manifest_path)
    manifest["algorithm"] = {"name": "rr-qtc-perfect-fixture", "version": "1"}
    write_json(manifest_path, manifest)
    challenge = ss.load_challenge(challenge_dir)
    for output in manifest["outputs"]:
        case = challenge.case(output["case_id"])
        target = output["target"]
        path = output_path(submission_dir, output)
        if target == "r_peak":
            write_json(path, {"schema_version": 1, "events": r_peak_events(case.annotations())})
        elif target == "signal_quality":
            write_json(path, {"schema_version": 1, "intervals": signal_quality_intervals(case.annotations())})
        elif target == "ecg_delineation":
            write_json(path, {"schema_version": 1, "events": delineation_events(case)})
        elif target in ("rr_interval", "qtc"):
            write_json(path, {"schema_version": 2, "contract": "synsigra_measurement_values_v2", "measurements": [dict(item["measurement"]) for item in case.measurement_truth(target)]})
        else:
            raise AssertionError("unexpected target: %s" % target)
    challenge.close()
    return manifest


def assert_rr_truth(challenge):
    artifact_rr_count = 0
    for case in challenge.cases:
        annotations = case.annotations()
        beats = [item for item in annotations["beats"] if item.get("qrs_present", False)]
        truth = case.measurement_truth("rr_interval")
        assert len(truth) == max(0, len(beats) - 1)
        for index, item in enumerate(truth):
            measurement = item["measurement"]
            expected = beats[index + 1]["r_peak_seconds"] - beats[index]["r_peak_seconds"]
            assert measurement["name"] == "rr_interval" and measurement["unit"] == "s"
            assert int(measurement["beat_index"]) == int(beats[index + 1]["beat_index"])
            assert abs(measurement["value"] - expected) < 1e-12
            for interval in annotations.get("artifact_intervals", []):
                if interval["start_seconds"] <= measurement["time_seconds"] < interval["end_seconds"]:
                    artifact_rr_count += 1
                    break
    assert artifact_rr_count > 0


def corrected_qtc(qt, rr, formula):
    if formula == "fixed":
        return qt
    if formula == "bazett":
        return qt / math.sqrt(rr)
    if formula == "fridericia":
        return qt / math.pow(rr, 1.0 / 3.0)
    if formula == "framingham":
        return qt + 0.154 * (1.0 - rr)
    if formula == "hodges":
        return qt + 0.00175 * (60.0 / rr - 60.0)
    raise AssertionError("unexpected QT formula: %s" % formula)


def assert_qtc_truth(challenge):
    expected_formula = {
        "bazett_brady_45": "bazett", "bazett_tachy_120": "bazett",
        "fridericia_brady_45": "fridericia", "fridericia_tachy_120": "fridericia",
        "framingham_brady_45": "framingham", "framingham_tachy_120": "framingham",
        "hodges_brady_45": "hodges", "hodges_tachy_120": "hodges",
        "fixed_normal_60": "fixed", "dynamic_long_qt": "fridericia",
        "difficult_t_u": "fridericia", "low_t_baseline": "fridericia",
    }
    for case in challenge.cases:
        rows = [item["measurement"] for item in case.measurement_truth("qtc")]
        by_beat = {}
        for item in rows:
            by_beat.setdefault(item["beat_index"], {})[item["name"]] = item
        complete = [item for item in by_beat.values() if all(name in item for name in ("rr_interval", "qt_interval", "qtc_interval"))]
        assert complete and len(complete) + 1 == sum(1 for item in rows if item["name"] == "qt_interval")
        for item in complete:
            formula = item["qtc_interval"]["formula"]
            assert formula == expected_formula[case.id]
            expected = corrected_qtc(item["qt_interval"]["value"], item["rr_interval"]["value"], formula)
            assert abs(item["qtc_interval"]["value"] - expected) < 1e-12


def shift_measurement(submission_dir, manifest, target, name, offset):
    changed = 0
    for output in manifest["outputs"]:
        if output["target"] != target:
            continue
        path = output_path(submission_dir, output)
        document = read_json(path)
        for item in document["measurements"]:
            if item["name"] == name and item["status"] == "valid":
                item["value"] += offset
                changed += 1
        write_json(path, document)
    assert changed > 0


def remove_artifact_r_peaks(challenge, submission_dir, manifest):
    removed = 0
    for output in manifest["outputs"]:
        if output["target"] != "r_peak":
            continue
        intervals = challenge.case(output["case_id"]).annotations().get("artifact_intervals", [])
        if not intervals:
            continue
        path = output_path(submission_dir, output)
        document = read_json(path)
        retained = []
        for event in document["events"]:
            affected = any(item["start_seconds"] <= event["time_seconds"] < item["end_seconds"] for item in intervals)
            if affected:
                removed += 1
            else:
                retained.append(event)
        document["events"] = retained
        write_json(path, document)
    assert removed > 0


def target_result(report, target):
    return next(item for item in report.evidence["targets"] if item["target"] == target)


def main():
    cli = os.environ["SIGNAL_SYNTH_CLI"]
    source = os.environ["SIGNAL_SYNTH_SOURCE_DIR"]
    work = os.environ["SIGNAL_SYNTH_RR_QTC_WORK_DIR"]
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)

    rr_challenge_dir = os.path.join(work, "rr_challenge")
    qtc_challenge_dir = os.path.join(work, "qtc_challenge")
    run([cli, "pack", "challenge", os.path.join(source, "examples", "packs", "r_peak_rr_noise_v1.json"), "--out", rr_challenge_dir, "--noise-assets", os.path.join(source, "examples", "assets", "noise")])
    run([cli, "pack", "challenge", os.path.join(source, "examples", "packs", "ecg_qtc_verification_v1.json"), "--out", qtc_challenge_dir])

    rr_challenge = ss.load_challenge(rr_challenge_dir)
    qtc_challenge = ss.load_challenge(qtc_challenge_dir)
    assert rr_challenge.verify_integrity()["ok"] and qtc_challenge.verify_integrity()["ok"]
    rr_protocol = rr_challenge.verification_protocol()
    qtc_protocol = qtc_challenge.verification_protocol()
    assert rr_protocol["contract"] == qtc_protocol["contract"] == "synsigra_verification_protocol_v2"
    assert rr_protocol["pack_id"] == "r_peak_rr_noise_v1" and rr_protocol["acceptance_profile"]["profile_id"] == "r_peak_rr_noise_v1_acceptance"
    assert qtc_protocol["pack_id"] == "ecg_qtc_verification_v1" and qtc_protocol["acceptance_profile"]["profile_id"] == "ecg_qtc_verification_v1_acceptance"
    rr_roles = dict((item["path"], item["role"]) for item in rr_challenge.manifest["files"])
    assert rr_roles["scoring_manifest.json"] == "scoring_manifest_json"
    assert rr_roles["verification_protocol.json"] == "verification_protocol_json"
    assert rr_roles["user-output-template/submission.json"] == "submission_manifest_json"
    assert rr_roles["user-output-template/formats.json"] == "submission_formats_json"
    assert_rr_truth(rr_challenge)
    assert_qtc_truth(qtc_challenge)
    external_truth = read_json(os.path.join(rr_challenge_dir, "cases", "external_extreme", "external_noise_truth.json"))
    assert external_truth["release_allowed"] and len(external_truth["intervals"]) == 3
    for interval in external_truth["intervals"]:
        assert len(interval["channels"]) == 12
        assert max(abs(item["achieved_snr_db"] - item["target_snr_db"]) for item in interval["channels"]) < 1e-9

    rr_submission = os.path.join(work, "rr_submission")
    qtc_submission = os.path.join(work, "qtc_submission")
    rr_manifest = make_perfect_submission(rr_challenge_dir, rr_submission)
    qtc_manifest = make_perfect_submission(qtc_challenge_dir, qtc_submission)
    rr_perfect = ss.verify_package(rr_challenge, rr_submission, os.path.join(work, "rr_perfect"))
    qtc_perfect = ss.verify_package(qtc_challenge, qtc_submission, os.path.join(work, "qtc_perfect"))
    assert rr_perfect.evidence["success"] and qtc_perfect.evidence["success"]
    assert rr_perfect.evidence["verification"]["mode"] == "evidence" and rr_perfect.evidence["verification"]["evidence_eligible"]
    assert rr_perfect.evidence["verification"]["protocol"]["sha256"].startswith("sha256:") and rr_perfect.evidence["verification"]["matrix_complete"]
    assert target_result(rr_perfect, "rr_interval")["policy"]["passed"]
    assert target_result(qtc_perfect, "qtc")["policy"]["passed"]

    incomplete_submission = os.path.join(work, "incomplete_submission")
    shutil.copytree(rr_submission, incomplete_submission)
    missing_output = rr_manifest["outputs"][0]
    os.remove(output_path(incomplete_submission, missing_output))
    incomplete = ss.verify_package(rr_challenge, incomplete_submission, os.path.join(work, "incomplete"))
    assert not incomplete.evidence["success"] and not incomplete.evidence["verification"]["matrix_complete"]
    assert not incomplete.evidence["verification"]["evidence_eligible"] and incomplete.evidence["status"] == "evidence_failed"

    rpeak_bad_submission = os.path.join(work, "rpeak_bad_submission")
    shutil.copytree(rr_submission, rpeak_bad_submission)
    remove_artifact_r_peaks(rr_challenge, rpeak_bad_submission, rr_manifest)
    rpeak_bad = ss.verify_package(rr_challenge, rpeak_bad_submission, os.path.join(work, "rpeak_bad"))
    rpeak_result = target_result(rpeak_bad, "r_peak")
    assert not rpeak_bad.evidence["success"] and not rpeak_result["policy"]["passed"]
    assert any(item["section"] == "artifact" and not item["passed"] for item in rpeak_result["policy"]["checks"] if item["applicable"])

    shift_measurement(rr_submission, rr_manifest, "rr_interval", "rr_interval", 0.040)
    rr_bad = ss.verify_package(rr_challenge, rr_submission, os.path.join(work, "rr_bad"))
    rr_result = target_result(rr_bad, "rr_interval")
    assert not rr_bad.evidence["success"] and not rr_result["policy"]["passed"]
    assert any(item["section"] == "rr_interval" and not item["passed"] for item in rr_result["policy"]["checks"] if item["applicable"])

    shift_measurement(qtc_submission, qtc_manifest, "qtc", "qtc_interval", 0.040)
    qtc_bad = ss.verify_package(qtc_challenge, qtc_submission, os.path.join(work, "qtc_bad"))
    qtc_result = target_result(qtc_bad, "qtc")
    assert not qtc_bad.evidence["success"] and not qtc_result["policy"]["passed"]
    assert any(item["section"] == "qtc_interval" and not item["passed"] for item in qtc_result["policy"]["checks"] if item["applicable"])

    try:
        ss.verify_package(rr_challenge, rpeak_bad_submission, os.path.join(work, "forbidden_override"), profile="smoke")
        raise AssertionError("evidence mode accepted a caller-selected profile")
    except ss.VerificationError:
        pass
    diagnostic = ss.verify_package(rr_challenge, rpeak_bad_submission, os.path.join(work, "diagnostic"), mode="diagnostic", cases=["clean_70"], targets=["r_peak"], profile="smoke")
    assert diagnostic.evidence["verification"]["mode"] == "diagnostic" and not diagnostic.evidence["verification"]["evidence_eligible"]

    rr_challenge.close()
    qtc_challenge.close()
    print("rr_qtc_pack_test=passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
