import json
import os
import subprocess
import sys


def load_json(path):
    with open(path, "r") as handle:
        return json.load(handle)


def run(command):
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = process.communicate()
    if process.returncode != 0:
        raise RuntimeError("command failed: %s\n%s\n%s" % (" ".join(command), stdout.decode("utf-8"), stderr.decode("utf-8")))
    if stderr:
        raise RuntimeError("stderr was not empty: %s" % stderr.decode("utf-8"))
    return stdout.decode("utf-8")


def main():
    source_dir = os.environ["SIGNAL_SYNTH_SOURCE_DIR"]
    cli = os.environ["SIGNAL_SYNTH_CLI"]
    catalog_path = os.path.join(source_dir, "examples", "catalog", "verification_packs_v1.json")
    catalog = load_json(catalog_path)
    assert catalog["schema_version"] == 1
    assert catalog["catalog_id"] == "synsigra_verification_packs"
    assert catalog["version"] == "1.0"
    assert "clinical validation" in catalog["not_for"].lower()

    expected = set([
        "r_peak_stress_v1", "hrv_v1", "ecg_beat_classification_v1", "ecg_rhythm_v1",
        "signal_quality_v1", "ecg_morphology_stress_v1", "ppg_alignment_v1", "combined_worst_case_v1",
    ])
    seen = set()
    for entry in catalog["packs"]:
        pack_id = entry["pack_id"]
        assert pack_id not in seen
        seen.add(pack_id)
        assert entry["modality"] and entry["targets"] and entry["difficulty"] and entry["feature_tags"]
        assert entry["scoring_mode"] in ("local", "mixed", "reference_only")
        if entry["scoring_mode"] == "local":
            assert entry["recommended_profile"] in ("smoke", "regression", "stress", "benchmark")
        pack_path = os.path.normpath(os.path.join(os.path.dirname(catalog_path), entry["path"]))
        assert os.path.isfile(pack_path)
        assert run([cli, "pack", "validate", pack_path]).startswith("status=valid\n")
        pack = load_json(pack_path)
        assert pack["pack_id"] == pack_id
        assert set(pack["targets"]) == set(entry["targets"])
        for case in pack["scenarios"]:
            scenario_path = os.path.normpath(os.path.join(os.path.dirname(pack_path), case["path"]))
            assert os.path.isfile(scenario_path)
            assert run([cli, "validate", scenario_path]).startswith("status=valid\n")
    assert seen == expected
    print("verification_catalog_test=passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
