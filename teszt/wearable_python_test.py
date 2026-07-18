import json
import os
import shutil
import subprocess

import synsigra


def run(command):
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = process.communicate()
    if process.returncode != 0 or stderr:
        raise RuntimeError("command failed: %s\n%s\n%s" % (" ".join(command), stdout.decode("utf-8"), stderr.decode("utf-8")))
    return stdout.decode("utf-8")


def main():
    source = os.environ["SIGNAL_SYNTH_SOURCE_DIR"]
    cli = os.environ["SIGNAL_SYNTH_CLI"]
    work = os.environ["SIGNAL_SYNTH_WEARABLE_WORK_DIR"]
    if os.path.exists(work):
        shutil.rmtree(work)
    os.makedirs(work)
    challenge_dir = os.path.join(work, "challenge")
    run([cli, "pack", "challenge", os.path.join(source, "examples", "packs", "wearable_timebase_v2.json"), "--out", challenge_dir])
    challenge = synsigra.load_challenge(challenge_dir)
    assert challenge.verify_integrity()["ok"]
    case = challenge.case("independent_clocks")
    ecg = case.wearable_samples("ecg")
    ppg = case.wearable_samples("ppg")
    accelerometer = case.wearable_samples("accelerometer")
    timestamps = case.wearable_timestamp_truth()
    timebase = case.wearable_timebase_truth()
    alignment = case.wearable_alignment_truth()
    realism = case.realism_metrics()
    realism_table = case.realism_table()
    population = challenge.realism_population()
    assert len(ecg) > len(ppg) > len(accelerometer) > 0
    assert timebase["contract"] == "synsigra_wearable_timebase_v3"
    assert [stream["kind"] for stream in timebase["streams"]] == ["ecg", "ppg", "accelerometer"]
    assert [stream["profile_id"] for stream in timebase["streams"]] == ["clinical_12lead_reference_v1", "custom", "synthetic_activity_v1"]
    assert len(timestamps) == sum(stream["sample_count"] for stream in timebase["streams"])
    assert alignment["events"] and alignment["events"][0]["ecg_r"]["present"]
    assert realism["contract"] == "synsigra_realism_metrics_v1" and realism["single_score"] is None
    assert len(realism_table) == len(realism["metrics"])
    assert population["contract"] == "synsigra_realism_population_v1" and population["case_count"] == len(challenge.cases)
    profile_case = challenge.case("patch_wrist")
    profile_truth = profile_case.wearable_timebase_truth()
    assert profile_truth["streams"][0]["profile_id"] == "patch_left_chest_vector_v1"
    assert profile_truth["streams"][0]["resolved_profile"]["placement"] == "left_chest_patch_engineering_vector"
    assert profile_truth["streams"][1]["profile_id"] == "wrist_reflectance_v1"
    assert profile_case.wearable_samples("ecg").columns == ["sample_index", "packet_index", "device_timestamp_seconds", "ecg_patch_left_chest"]
    with open(os.path.join(challenge_dir, "manifest.json"), "r") as handle:
        manifest = json.load(handle)
    roles = [item["role"] for item in manifest["files"] if item["path"] in case.files]
    assert roles.count("wearable_samples_csv") == 3
    assert "wearable_timestamp_truth_csv" in roles
    assert "wearable_timebase_truth_json" in roles
    assert "wearable_alignment_truth_json" in roles
    try:
        case.wearable_samples("temperature")
        raise AssertionError("unknown wearable stream was accepted")
    except ValueError:
        pass
    challenge.close()
    print("wearable_python_test=passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
