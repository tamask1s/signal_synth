import json
import os
import shutil
import subprocess
import sys
import tempfile


def read_json(path):
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


def pack(document, pack_id):
    for item in document["packs"]:
        if item["pack_id"] == pack_id:
            return item
    raise AssertionError("pack not found: %s" % pack_id)


def assert_rpeak_metadata(item):
    assert item["schema_version"] == 1
    assert item["metadata_type"] == "synsigra_curated_pack_metadata"
    assert item["pack_id"] == "r_peak_stress_v1"
    assert item["version"] == "1.0"
    assert item["release_status"] == "beta"
    assert item["release_date"] == "2026-07-06"
    assert item["declared_targets"] == ["r_peak"]
    assert item["targets"] == ["r_peak", "signal_quality"]
    assert item["catalog_scoring_mode"] == "local"
    assert item["scoring_mode"] == "mixed"
    assert item["case_count"] == 4
    assert item["case_ids"] == ["clean_70", "slow_45", "fast_120", "baseline_powerline"]
    assert item["duration"]["total_seconds"] == 100
    assert item["sampling_rates_hz"] == [500]
    assert item["channels"]["minimum_channel_count"] == 12
    assert item["channels"]["maximum_channel_count"] == 12
    assert item["estimated_package"]["bytes"] > 0
    assert item["estimated_package"]["size_class"] == "medium"
    assert item["supported_threshold_profiles"] == ["smoke", "regression", "stress", "benchmark"]
    assert item["threshold_profile_contract"]["policy_failure_exit_code"] == 1
    assert item["detector_output_schemas"] == ["detection_json_v1", "detection_csv_v2"]
    assert item["recommended_for"] and item["not_recommended_for"] and item["changelog"]
    assert item["generator_compatibility"]["minimum_generator_version"] == "0.5.0-dev"
    assert item["generator_compatibility"]["scoring_manifest_contract"] == "synsigra_scoring_manifest_v1"
    scoreable = dict((target["target"], target) for target in item["scoreable_targets"])
    reference = dict((target["target"], target) for target in item["reference_only_targets"])
    assert sorted(scoreable.keys()) == ["r_peak"]
    assert sorted(reference.keys()) == ["signal_quality"]
    assert scoreable["r_peak"]["score_type"] == "event_detection"
    assert scoreable["r_peak"]["accepted_detection_formats"] == ["detection_json_v1", "detection_csv_v2"]
    assert scoreable["r_peak"]["default_tolerance_seconds"] == 0.05
    assert scoreable["r_peak"]["case_ids"] == item["case_ids"]
    assert reference["signal_quality"]["score_type"] == "generated_reference_only"
    assert reference["signal_quality"]["case_ids"] == ["baseline_powerline"]
    baseline = [case for case in item["cases"] if case["case_id"] == "baseline_powerline"][0]
    assert baseline["scoreable_targets"] == ["r_peak"]
    assert baseline["reference_only_targets"] == ["signal_quality"]
    assert item["ui"]["scoreable_before_job"]
    assert item["ui"]["reference_only_before_job"]


def main():
    source_dir = os.environ["SIGNAL_SYNTH_SOURCE_DIR"]
    cli = os.environ["SIGNAL_SYNTH_CLI"]
    script = os.path.join(source_dir, "scripts", "export_curated_pack_metadata.py")
    catalog = os.path.join(source_dir, "examples", "catalog", "verification_packs_v1.json")
    expected_path = os.path.join(source_dir, "examples", "catalog", "curated_pack_metadata_v1.json")
    work_dir = tempfile.mkdtemp(prefix="synsigra_pack_metadata_")
    try:
        output_path = os.path.join(work_dir, "curated_pack_metadata_v1.json")
        run([sys.executable, script, "--cli", cli, "--catalog", catalog, "--source-root", source_dir, "--out", output_path])
        generated = read_json(output_path)
        expected = read_json(expected_path)
        assert generated == expected
        assert generated["schema_version"] == 1
        assert generated["metadata_type"] == "synsigra_curated_pack_catalog"
        assert generated["metadata_version"] == "synsigra_curated_pack_metadata_export_v1"
        assert generated["catalog_id"] == "synsigra_verification_packs"
        assert generated["catalog_version"] == "1.2"
        assert generated["pack_count"] == 10
        assert_rpeak_metadata(pack(generated, "r_peak_stress_v1"))

        filtered_path = os.path.join(work_dir, "rpeak_only.json")
        run([sys.executable, script, "--cli", cli, "--catalog", catalog, "--source-root", source_dir, "--pack-id", "r_peak_stress_v1", "--out", filtered_path])
        filtered = read_json(filtered_path)
        assert filtered["pack_count"] == 1
        assert_rpeak_metadata(filtered["packs"][0])
    finally:
        shutil.rmtree(work_dir)
    print("pack_metadata_export_test=passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
