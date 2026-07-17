import json
import os
import shutil
import subprocess
import tempfile

from synsigra.challenge import load_challenge
from synsigra.local_verify import verify_package
from synsigra.submission import SubmissionError, load_submission


def write_json(path, document):
    with open(path, "w") as handle:
        json.dump(document, handle, sort_keys=True, separators=(",", ":"))


def rejected(path, package, scoring_manifest):
    try:
        load_submission(path, package, scoring_manifest)
        raise AssertionError("invalid submission was accepted")
    except SubmissionError:
        pass


cli = os.environ["SIGNAL_SYNTH_CLI"]
pack = os.environ["SIGNAL_SYNTH_PACK_EXAMPLE"]
work = tempfile.mkdtemp(prefix="synsigra_submission_")
try:
    challenge_dir = os.path.join(work, "challenge")
    process = subprocess.run([cli, "pack", "challenge", pack, "--out", challenge_dir], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    assert process.returncode == 0 and not process.stderr
    package = load_challenge(challenge_dir)
    scoring_manifest = package.scoring_manifest()
    submission_dir = os.path.join(work, "submission")
    shutil.copytree(os.path.join(challenge_dir, "user-output-template"), submission_dir)
    manifest_path = os.path.join(submission_dir, "submission.json")
    manifest = json.load(open(manifest_path, "r"))
    rejected(submission_dir, package, scoring_manifest)

    manifest["algorithm"] = {"name": "strict-test", "version": "1"}
    write_json(manifest_path, manifest)
    submission = load_submission(submission_dir, package, scoring_manifest)
    assert len(submission.outputs) == 5
    assert submission.algorithm == {"name": "strict-test", "version": "1"}

    changed = json.loads(json.dumps(manifest))
    changed["challenge"]["pack_fingerprint"] = "sha256:" + "0" * 64
    write_json(manifest_path, changed)
    rejected(submission_dir, package, scoring_manifest)

    changed = json.loads(json.dumps(manifest))
    changed["outputs"][0]["format"] = "detection_json_v1"
    write_json(manifest_path, changed)
    rejected(submission_dir, package, scoring_manifest)

    changed = json.loads(json.dumps(manifest))
    changed["outputs"][0]["path"] = "../escape.json"
    write_json(manifest_path, changed)
    rejected(submission_dir, package, scoring_manifest)

    changed = json.loads(json.dumps(manifest))
    changed["outputs"] = changed["outputs"][:-1]
    write_json(manifest_path, changed)
    rejected(submission_dir, package, scoring_manifest)

    changed = json.loads(json.dumps(manifest))
    changed["outputs"].append(json.loads(json.dumps(changed["outputs"][0])))
    write_json(manifest_path, changed)
    rejected(submission_dir, package, scoring_manifest)

    write_json(manifest_path, manifest)
    unexpected_path = os.path.join(submission_dir, "unexpected.txt")
    with open(unexpected_path, "w") as handle:
        handle.write("unexpected")
    rejected(submission_dir, package, scoring_manifest)
    os.remove(unexpected_path)

    first = manifest["outputs"][0]
    first_path = os.path.join(submission_dir, *first["path"].split("/"))
    os.remove(first_path)
    missing = verify_package(package, submission_dir, os.path.join(work, "missing"), cases=[first["case_id"]], targets=[first["target"]])
    assert missing.summary["cases"][0]["status"] == "missing_output"
    assert "%s/%s" % (first["case_id"], first["target"]) in missing.summary["cases"][0]["message"]

    write_json(first_path, {"schema_version": 1, "events": [{"time_seconds": "invalid"}]})
    malformed = verify_package(package, submission_dir, os.path.join(work, "malformed"), cases=[first["case_id"]], targets=[first["target"]])
    assert malformed.summary["cases"][0]["status"] == "scoring_error"
    assert "%s/%s" % (first["case_id"], first["target"]) in malformed.summary["cases"][0]["message"]
    package.close()
finally:
    shutil.rmtree(work)

print("submission_test=passed")
