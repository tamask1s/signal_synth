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


RELEASE_PACK_IDS = [
    "advanced_rhythm_burden_v1",
    "r_peak_stress_v1",
    "hrv_v1",
    "ecg_beat_classification_v1",
    "ecg_rhythm_v1",
    "signal_quality_v1",
    "ecg_morphology_stress_v1",
    "ppg_alignment_v1",
    "combined_worst_case_v1",
    "wearable_timebase_v2",
    "ppg_benchmark_v1",
    "ppg_optical_v2",
    "ecg_delineation_v2",
    "cardiorespiratory_v1",
]


def assert_release_pack_metadata(item):
    assert item["schema_version"] == 1
    assert item["metadata_type"] == "synsigra_curated_pack_metadata"
    assert isinstance(item["version"], str) and item["version"]
    assert item["release_status"] == "beta"
    expected_date = "2026-07-17" if item["pack_id"] in ("ecg_delineation_v2", "wearable_timebase_v2", "ppg_optical_v2", "cardiorespiratory_v1", "advanced_rhythm_burden_v1") else "2026-07-06"
    assert item["release_date"] == expected_date
    assert item["recommended_for"] and item["not_recommended_for"] and item["changelog"]
    assert item["source"]["pack_fingerprint"].startswith("sha256:")
    assert item["source"]["source_content_sha256"].startswith("sha256:")
    assert item["generator_compatibility"]["challenge_package_contract"] == "synsigra_challenge_package_v2"
    assert item["generator_compatibility"]["scoring_manifest_contract"] == "synsigra_scoring_manifest_v2"
    assert item["generator_compatibility"]["submission_contract"] == "synsigra_submission_v1"
    assert item["case_count"] == len(item["case_ids"]) == len(item["cases"])
    assert item["duration"]["total_seconds"] > 0
    assert item["sampling_rates_hz"]
    assert item["channels"]["minimum_channel_count"] >= 1
    assert item["channels"]["maximum_channel_count"] >= item["channels"]["minimum_channel_count"]
    assert item["estimated_package"]["bytes"] > 0
    assert item["estimated_package"]["size_class"] in ("small", "medium", "large", "very_large")
    assert item["output_artifacts"]
    artifact_roles = [artifact["role"] for artifact in item["output_artifacts"]]
    assert "provenance_json" in artifact_roles
    assert "engineering_claim_boundary_txt" in artifact_roles
    assert item["declared_targets"]
    assert item["targets"]
    assert item["scoring_mode"] in ("local", "mixed", "reference_only")
    assert bool(item["scoreable_targets"]) == item["ui"]["scoreable_before_job"]
    assert bool(item["reference_only_targets"]) == item["ui"]["reference_only_before_job"]
    if item["scoreable_targets"]:
        assert item["submission_output_schemas"]
        assert item["supported_threshold_profiles"]
        assert item["local_verifier_smoke_tests"]
        for target in item["scoreable_targets"]:
            assert target["scoreable"] is True
            assert target["case_count"] == len(target["case_ids"])
            assert target["case_ids"]
            assert target["support"] == "local_scoring"
            assert target["score_type"] in ("event_detection", "classification", "hrv_metrics", "interval_detection", "ecg_delineation", "measurement")
            assert target["accepted_formats"]
    else:
        assert not item["submission_output_schemas"]
        assert not item["supported_threshold_profiles"]
        assert not item["local_verifier_smoke_tests"]
    for target in item["reference_only_targets"]:
        assert target["scoreable"] is False
        assert target["support"] == "reference_only"
        assert target["reference_artifacts"]
    for case in item["cases"]:
        assert case["case_id"] in item["case_ids"]
        assert case["targets"]
        assert set(case["scoreable_targets"]).issubset(set(item["targets"]))
        assert set(case["reference_only_targets"]).issubset(set(item["targets"]))


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
    assert item["scoring_mode"] == "local"
    assert item["case_count"] == 4
    assert item["case_ids"] == ["clean_70", "slow_45", "fast_120", "baseline_powerline"]
    assert item["duration"]["total_seconds"] == 100
    assert item["sampling_rates_hz"] == [500]
    assert item["channels"]["minimum_channel_count"] == 12
    assert item["channels"]["maximum_channel_count"] == 12
    assert item["estimated_package"]["bytes"] > 0
    assert item["estimated_package"]["size_class"] == "medium"
    assert item["supported_threshold_profiles"] == ["smoke", "regression", "stress", "benchmark"]
    assert item["local_verifier_smoke_tests"]
    assert item["threshold_profile_contract"]["policy_failure_exit_code"] == 1
    assert item["submission_output_schemas"] == ["point_events_json_v1", "point_events_csv_v1", "interval_events_json_v1", "interval_events_csv_v1"]
    assert item["recommended_for"] and item["not_recommended_for"] and item["changelog"]
    assert item["generator_compatibility"]["minimum_generator_version"] == "0.6.0-dev"
    assert item["generator_compatibility"]["scoring_manifest_contract"] == "synsigra_scoring_manifest_v2"
    scoreable = dict((target["target"], target) for target in item["scoreable_targets"])
    reference = dict((target["target"], target) for target in item["reference_only_targets"])
    assert sorted(scoreable.keys()) == ["r_peak", "signal_quality"]
    assert reference == {}
    assert scoreable["r_peak"]["score_type"] == "event_detection"
    assert scoreable["r_peak"]["accepted_formats"] == ["point_events_json_v1", "point_events_csv_v1"]
    assert scoreable["r_peak"]["default_tolerance_seconds"] == 0.05
    assert scoreable["r_peak"]["case_ids"] == item["case_ids"]
    assert scoreable["signal_quality"]["score_type"] == "interval_detection"
    assert scoreable["signal_quality"]["accepted_formats"] == ["interval_events_json_v1", "interval_events_csv_v1"]
    assert scoreable["signal_quality"]["default_minimum_iou"] == 0.1
    assert scoreable["signal_quality"]["case_ids"] == ["baseline_powerline"]
    baseline = [case for case in item["cases"] if case["case_id"] == "baseline_powerline"][0]
    assert baseline["scoreable_targets"] == ["r_peak", "signal_quality"]
    assert baseline["reference_only_targets"] == []
    assert item["ui"]["scoreable_before_job"]
    assert not item["ui"]["reference_only_before_job"]


def assert_ppg_benchmark_metadata(item):
    assert item["pack_id"] == "ppg_benchmark_v1"
    assert item["version"] == "1.1"
    assert item["case_count"] == 9
    assert "arrhythmia_pulse_loss" in item["case_ids"]
    assert "arrhythmia_linked_pulse_loss" in item["feature_tags"]
    assert item["scoring_mode"] == "local"
    assert sorted(target["target"] for target in item["scoreable_targets"]) == ["ppg_pulse_onset", "ppg_systolic_peak"]
    case = [case for case in item["cases"] if case["case_id"] == "arrhythmia_pulse_loss"][0]
    assert case["scoreable_targets"] == ["ppg_systolic_peak", "ppg_pulse_onset"]
    assert case["reference_only_targets"] == []


def assert_delineation_metadata(item):
    assert item["pack_id"] == "ecg_delineation_v2"
    assert item["version"] == "2.0"
    assert item["release_date"] == "2026-07-17"
    assert item["case_count"] == 8
    assert item["case_ids"] == ["clean_70", "slow_45", "fast_120", "clbbb", "ischemia_anterolateral", "afib_absent_p", "mobitz_ii_nonconducted_p", "baseline_powerline"]
    assert item["submission_output_schemas"] == ["point_events_json_v1", "point_events_csv_v1"]
    assert len(item["scoreable_targets"]) == 1
    target = item["scoreable_targets"][0]
    assert target["target"] == "ecg_delineation"
    assert target["score_type"] == "ecg_delineation"
    assert target["accepted_formats"] == ["point_events_json_v1", "point_events_csv_v1"]
    assert target["default_tolerance_seconds"] == 0.04
    assert target["case_ids"] == item["case_ids"]
    assert item["generator_compatibility"]["local_verifier_min_version"] == "0.4.0"
    assert item["reference_only_targets"] == []


def assert_ppg_optical_metadata(item):
    assert item["pack_id"] == "ppg_optical_v2"
    assert item["version"] == "1.0"
    assert item["case_ids"] == ["normoxia", "desaturation", "low_perfusion", "interference"]
    assert {"measurement_values_json_v1", "measurement_values_csv_v1", "point_events_json_v1", "point_events_csv_v1"} == set(item["submission_output_schemas"])
    assert_measurement_metadata(item, "ppg_optical", "0.6.0")
    roles = set(artifact["role"] for artifact in item["output_artifacts"])
    assert {"ppg_optical_latent_csv", "ppg_optical_truth_json", "measurement_truth_json"}.issubset(roles)


def assert_measurement_metadata(item, target, verifier_version="0.5.0"):
    contracts = dict((entry["target"], entry) for entry in item["scoreable_targets"])
    assert target in contracts
    assert contracts[target]["score_type"] == "measurement"
    assert contracts[target]["accepted_formats"] == ["measurement_values_json_v1", "measurement_values_csv_v1"]
    assert contracts[target]["default_pairing_window_seconds"] == 0.2
    assert item["generator_compatibility"]["local_verifier_min_version"] == verifier_version
    assert "measurement_truth_json" in [artifact["role"] for artifact in item["output_artifacts"]]
    assert item["reference_only_targets"] == []


def assert_cardiorespiratory_metadata(item):
    assert item["pack_id"] == "cardiorespiratory_v1"
    assert item["version"] == "1.0"
    assert item["case_ids"] == ["clean_coupling", "ptt_variation", "pulse_loss_motion"]
    assert_measurement_metadata(item, "prv", "0.6.0")
    assert_measurement_metadata(item, "respiratory_rate", "0.6.0")
    roles = set(artifact["role"] for artifact in item["output_artifacts"])
    assert {"cardiorespiratory_truth_json", "prv_tachogram_csv", "respiration_reference_csv", "measurement_truth_json"}.issubset(roles)


def assert_advanced_rhythm_metadata(item):
    assert item["pack_id"] == "advanced_rhythm_burden_v1"
    assert item["case_ids"] == ["recurrent_af_psvt", "recurrent_vt", "vf_asystole"]
    assert_measurement_metadata(item, "rhythm_burden", "0.6.0")
    targets = dict((target["target"], target) for target in item["scoreable_targets"])
    assert targets["rhythm_episode"]["score_type"] == "interval_detection"
    assert targets["rhythm_episode"]["accepted_formats"] == ["interval_events_json_v1", "interval_events_csv_v1"]


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
        assert generated["release_set_id"] == "synsigra_curated_release_2026_07_17"
        assert generated["release_set_status"] == "beta"
        assert generated["catalog_id"] == "synsigra_verification_packs"
        assert generated["catalog_version"] == "2.2"
        assert generated["pack_count"] == 14
        assert [item["pack_id"] for item in generated["packs"]] == RELEASE_PACK_IDS
        for pack_id in RELEASE_PACK_IDS:
            assert_release_pack_metadata(pack(generated, pack_id))
        assert_rpeak_metadata(pack(generated, "r_peak_stress_v1"))
        assert_ppg_benchmark_metadata(pack(generated, "ppg_benchmark_v1"))
        assert_delineation_metadata(pack(generated, "ecg_delineation_v2"))
        assert_ppg_optical_metadata(pack(generated, "ppg_optical_v2"))
        assert_cardiorespiratory_metadata(pack(generated, "cardiorespiratory_v1"))
        assert_advanced_rhythm_metadata(pack(generated, "advanced_rhythm_burden_v1"))
        assert_measurement_metadata(pack(generated, "ecg_morphology_stress_v1"), "morphology_assertions")
        assert_measurement_metadata(pack(generated, "ppg_alignment_v1"), "ecg_ppg_alignment")
        wearable = pack(generated, "wearable_timebase_v2")
        assert_measurement_metadata(wearable, "ecg_ppg_alignment", "0.6.0")
        assert {"wearable_samples_csv", "wearable_timestamp_truth_csv", "wearable_timebase_truth_json", "wearable_alignment_truth_json"}.issubset(set(artifact["role"] for artifact in wearable["output_artifacts"]))

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
