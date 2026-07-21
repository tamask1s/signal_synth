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


def write_json(path, value):
    with open(path, "w") as handle:
        json.dump(value, handle, sort_keys=True, separators=(",", ":"))


def truth_for_target(path, target):
    matches = [item for item in read_json(path)["targets"] if item["target"] == target]
    assert len(matches) == 1
    return matches[0]["measurements"]


def main():
    source = os.environ["SIGNAL_SYNTH_SOURCE_DIR"]
    cli = os.environ["SIGNAL_SYNTH_CLI"]
    work = os.environ["SIGNAL_SYNTH_CARDIORESPIRATORY_WORK_DIR"]
    if os.path.exists(work):
        shutil.rmtree(work)
    os.makedirs(os.path.join(work, "source", "scenarios"))
    shutil.copyfile(os.path.join(source, "examples", "scenarios", "packs", "cardiorespiratory_clean_v4.json"), os.path.join(work, "source", "scenarios", "clean.json"))
    pack_path = os.path.join(work, "source", "pack.json")
    write_json(pack_path, {
        "schema_version": 2, "pack_id": "cardiorespiratory_python_test", "name": "Cardiorespiratory Python Test", "version": "1", "description": "Generator-free PRV and respiratory-rate scoring fixture.",
        "targets": ["prv", "respiratory_rate"],
        "scenarios": [{"id": "clean", "path": "scenarios/clean.json", "targets": ["prv", "respiratory_rate"]}],
    })
    challenge_dir = os.path.join(work, "challenge")
    run([cli, "pack", "challenge", pack_path, "--out", challenge_dir])
    challenge = ss.load_challenge(challenge_dir)
    assert challenge.verify_integrity()["ok"]
    case = challenge.case("clean")
    cardiorespiratory = case.cardiorespiratory_truth()
    assert cardiorespiratory["contract"] == "synsigra_cardiorespiratory_truth_v1"
    assert cardiorespiratory["prv_available"] and cardiorespiratory["respiration_available"]
    assert len(case.prv_tachogram()) > 100 and len(case.respiration_reference()) == 3001
    assert case.measurement_truth("prv") and case.measurement_truth("respiratory_rate")

    submission_dir = os.path.join(work, "submission")
    shutil.copytree(os.path.join(challenge_dir, "user-output-template"), submission_dir)
    submission_path = os.path.join(submission_dir, "submission.json")
    submission = read_json(submission_path)
    submission["algorithm"] = {"name": "perfect_cardiorespiratory_fixture", "version": "1"}
    write_json(submission_path, submission)
    truth_path = os.path.join(challenge_dir, "cases", "clean", "measurement_truth.json")
    for output in submission["outputs"]:
        measurements = [dict(item["measurement"]) for item in truth_for_target(truth_path, output["target"])]
        write_json(os.path.join(submission_dir, *output["path"].split("/")), {"schema_version": 2, "contract": "synsigra_measurement_values_v2", "measurements": measurements})
    report = ss.verify_package(challenge, submission_dir, os.path.join(work, "verify"), mode="diagnostic", profile="regression")
    assert report.evidence["success"]
    assert set(item["target"] for item in report.evidence["targets"]) == set(["prv", "respiratory_rate"])
    for target in report.evidence["targets"]:
        assert target["overall"]["tolerance_pass_fraction"] == 1.0 and target["policy"]["passed"]
    challenge.close()
    print("cardiorespiratory_python_test=passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
