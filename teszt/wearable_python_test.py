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
    assert timebase["contract"] == "synsigra_wearable_timebase_v2"
    assert [stream["kind"] for stream in timebase["streams"]] == ["ecg", "ppg", "accelerometer"]
    assert len(timestamps) == sum(stream["sample_count"] for stream in timebase["streams"])
    assert alignment["events"] and alignment["events"][0]["ecg_r"]["present"]
    assert realism["contract"] == "synsigra_realism_metrics_v1" and realism["single_score"] is None
    assert len(realism_table) == len(realism["metrics"])
    assert population["contract"] == "synsigra_realism_population_v1" and population["case_count"] == len(challenge.cases)
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
