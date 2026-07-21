#!/usr/bin/env python3
"""Export SaaS-ingestable curated pack metadata from core pack definitions."""

import argparse
import hashlib
import json
import os
import subprocess
import sys


EXPORTER_VERSION = "synsigra_curated_pack_metadata_export_v1"
METADATA_TYPE = "synsigra_curated_pack_catalog"
DEFAULT_GENERATOR_COMPATIBILITY = {
    "minimum_generator_version": "0.10.0-dev",
    "pack_schema_version": 2,
    "scenario_schema_versions": [2, 3, 4, 5, 6, 7, 8, 9],
    "challenge_package_contract": "synsigra_challenge_package_v3",
    "scoring_manifest_contract": "synsigra_scoring_manifest_v3",
    "submission_contract": "synsigra_submission_v1",
    "verification_protocol_contract": "synsigra_verification_protocol_v2",
    "local_verifier_min_version": "0.11.0",
}


TARGET_CONTRACTS = {
    "r_peak": {
        "scoreable": True,
        "score_type": "event_detection",
        "accepted_formats": ["point_events_json_v1", "point_events_csv_v1"],
        "default_tolerance_seconds": 0.05,
        "primary_metric": "f1_score",
        "description": "R-peak detector event timing against exact synthetic beat annotations.",
    },
    "ppg_systolic_peak": {
        "scoreable": True,
        "score_type": "event_detection",
        "accepted_formats": ["point_events_json_v1", "point_events_csv_v1"],
        "default_tolerance_seconds": 0.08,
        "primary_metric": "f1_score",
        "description": "PPG systolic peak event timing against exact synthetic pulse annotations.",
    },
    "ppg_pulse_onset": {
        "scoreable": True,
        "score_type": "event_detection",
        "accepted_formats": ["point_events_json_v1", "point_events_csv_v1"],
        "default_tolerance_seconds": 0.05,
        "primary_metric": "f1_score",
        "description": "PPG pulse onset event timing against exact synthetic pulse annotations.",
    },
    "ecg_beat_classification": {
        "scoreable": True,
        "score_type": "classification",
        "accepted_formats": ["point_events_json_v1", "point_events_csv_v1"],
        "default_tolerance_seconds": 0.075,
        "primary_metric": "micro_f1_score",
        "description": "Beat-level ECG class labels with exact synthetic beat timing.",
    },
    "hrv": {
        "scoreable": True,
        "score_type": "measurement",
        "accepted_formats": ["measurement_values_json_v2", "measurement_values_csv_v2"],
        "primary_metric": "metric_pass_fraction",
        "description": "HRV metric and RR-interval scoring against exact synthetic tachogram ground truth.",
    },
    "signal_quality": {
        "scoreable": True,
        "score_type": "interval_detection",
        "accepted_formats": ["interval_events_json_v1", "interval_events_csv_v1"],
        "default_minimum_iou": 0.1,
        "primary_metric": "time_f1_score",
        "description": "Signal-quality interval labels and channels scored against exact synthetic artifact ground truth.",
    },
    "rhythm_episode": {
        "scoreable": True,
        "score_type": "interval_detection",
        "accepted_formats": ["interval_events_json_v1", "interval_events_csv_v1"],
        "default_minimum_iou": 0.1,
        "primary_metric": "time_f1_score",
        "description": "Rhythm episode labels and boundaries scored against exact synthetic episode ground truth.",
    },
    "ecg_delineation": {
        "scoreable": True,
        "score_type": "ecg_delineation",
        "accepted_formats": ["point_events_json_v1", "point_events_csv_v1"],
        "default_tolerance_seconds": 0.04,
        "primary_metric": "f1_score",
        "description": "Lead-specific P, QRS, J-point, and T fiducial timing against exact synthetic construction ground truth.",
    },
    "rr_interval": {
        "scoreable": True,
        "score_type": "measurement",
        "accepted_formats": ["measurement_values_json_v2", "measurement_values_csv_v2"],
        "default_pairing_window_seconds": 0.2,
        "primary_metric": "tolerance_pass_fraction",
        "description": "Every observable beat-to-beat R-R interval scored independently of artifact-exclusion policy.",
    },
    "qtc": {
        "scoreable": True,
        "score_type": "measurement",
        "accepted_formats": ["measurement_values_json_v2", "measurement_values_csv_v2"],
        "default_pairing_window_seconds": 0.2,
        "primary_metric": "tolerance_pass_fraction",
        "description": "Beat-level RR, QT and formula-explicit QTc measurements scored against exact synthetic timing truth.",
    },
    "morphology_assertions": {
        "scoreable": True,
        "score_type": "measurement",
        "accepted_formats": ["measurement_values_json_v2", "measurement_values_csv_v2"],
        "default_pairing_window_seconds": 0.2,
        "primary_metric": "tolerance_pass_fraction",
        "description": "Beat-, lead-, record-, axis-, and phenotype-level ECG measurements scored against exact synthetic ground truth.",
    },
    "ecg_ppg_alignment": {
        "scoreable": True,
        "score_type": "measurement",
        "accepted_formats": ["measurement_values_json_v2", "measurement_values_csv_v2"],
        "default_pairing_window_seconds": 0.2,
        "primary_metric": "tolerance_pass_fraction",
        "description": "Per-beat PTT and ECG-to-PPG peak delay scored as paired-signal measurements.",
    },
    "ppg_optical": {
        "scoreable": True,
        "score_type": "measurement",
        "accepted_formats": ["measurement_values_json_v2", "measurement_values_csv_v2"],
        "default_pairing_window_seconds": 0.2,
        "primary_metric": "tolerance_pass_fraction",
        "description": "Per-pulse red/infrared AC/DC, perfusion-index, ratio-of-ratios and oxygenation-trajectory measurements.",
    },
    "prv": {
        "scoreable": True,
        "score_type": "measurement",
        "accepted_formats": ["measurement_values_json_v2", "measurement_values_csv_v2"],
        "default_pairing_window_seconds": 0.2,
        "primary_metric": "tolerance_pass_fraction",
        "description": "Pulse-interval variability metrics and accepted/excluded PPG interval measurements against explicit PRV truth.",
    },
    "respiratory_rate": {
        "scoreable": True,
        "score_type": "measurement",
        "accepted_formats": ["measurement_values_json_v2", "measurement_values_csv_v2"],
        "default_pairing_window_seconds": 0.2,
        "primary_metric": "tolerance_pass_fraction",
        "description": "Respiratory-rate record and trajectory measurements against the deterministic coupling reference.",
    },
    "rhythm_burden": {
        "scoreable": True,
        "score_type": "measurement",
        "accepted_formats": ["measurement_values_json_v2", "measurement_values_csv_v2"],
        "default_pairing_window_seconds": 0.2,
        "primary_metric": "tolerance_pass_fraction",
        "description": "Overall and class-specific episode duration, fraction and count measurements against exact interval truth.",
    },
}

LOCAL_VERIFIER_SMOKE_TESTS = {
    "r_peak": [
        {
            "test_id": "TEST-PYTHON-SCORING-001",
            "scope": "local event-detection scoring smoke test",
        },
    ],
    "ppg_systolic_peak": [
        {
            "test_id": "TEST-PYTHON-SCORING-001",
            "scope": "local PPG event-detection scoring smoke test",
        }
    ],
    "ppg_pulse_onset": [
        {
            "test_id": "TEST-PYTHON-SCORING-001",
            "scope": "local PPG onset event-detection scoring smoke test",
        }
    ],
    "ecg_beat_classification": [
        {
            "test_id": "TEST-PYTHON-SCORING-001",
            "scope": "local beat-classification scoring smoke test",
        }
    ],
    "hrv": [
        {
            "test_id": "TEST-HRV-GENERIC-001",
            "scope": "generic measurement-path HRV scoring smoke test",
        },
        {
            "test_id": "TEST-PYTHON-SCORING-001",
            "scope": "local HRV user-output scoring smoke test",
        },
    ],
    "rhythm_episode": [
        {"test_id": "TEST-INTERVAL-SCORING-001", "scope": "core rhythm-episode interval scoring smoke test"},
        {"test_id": "TEST-PYTHON-SCORING-001", "scope": "local rhythm-episode interval scoring parity test"},
    ],
    "signal_quality": [
        {"test_id": "TEST-INTERVAL-SCORING-001", "scope": "core signal-quality interval scoring smoke test"},
        {"test_id": "TEST-PYTHON-SCORING-001", "scope": "local signal-quality interval scoring parity test"},
    ],
    "ecg_delineation": [
        {"test_id": "TEST-DELINEATION-SCORING-001", "scope": "core lead-specific delineation scoring smoke test"},
        {"test_id": "TEST-DELINEATION-PYTHON-001", "scope": "generator-free delineation scoring and C++/Python parity test"},
    ],
    "rr_interval": [
        {"test_id": "TEST-RR-QTC-PACK-001", "scope": "generator-free observable RR measurement scoring with negative controls"},
    ],
    "qtc": [
        {"test_id": "TEST-RR-QTC-PACK-001", "scope": "generator-free formula-explicit QT/QTc scoring with negative controls"},
    ],
    "morphology_assertions": [
        {"test_id": "TEST-MEASUREMENT-SCORING-001", "scope": "core morphology measurement scoring smoke test"},
        {"test_id": "TEST-MEASUREMENT-PYTHON-001", "scope": "generator-free morphology measurement scoring parity test"},
    ],
    "ecg_ppg_alignment": [
        {"test_id": "TEST-MEASUREMENT-SCORING-001", "scope": "core paired-signal measurement scoring smoke test"},
        {"test_id": "TEST-MEASUREMENT-PYTHON-001", "scope": "generator-free ECG/PPG measurement scoring parity test"},
    ],
    "ppg_optical": [
        {"test_id": "TEST-PPG-OPTICAL-V2-001", "scope": "core optical physiology and measurement-truth smoke test"},
        {"test_id": "TEST-MEASUREMENT-PYTHON-001", "scope": "generator-free optical measurement scoring parity test"},
    ],
    "prv": [
        {"test_id": "TEST-CARDIORESPIRATORY-001", "scope": "core PRV derivation, exclusion and measurement scoring smoke test"},
        {"test_id": "TEST-CARDIORESPIRATORY-PYTHON-001", "scope": "generator-free PRV measurement scoring test"},
    ],
    "respiratory_rate": [
        {"test_id": "TEST-CARDIORESPIRATORY-001", "scope": "core deterministic respiratory reference and measurement scoring smoke test"},
        {"test_id": "TEST-CARDIORESPIRATORY-PYTHON-001", "scope": "generator-free respiratory-rate measurement scoring test"},
    ],
    "rhythm_burden": [
        {"test_id": "TEST-ECG-EPISODE-001", "scope": "core multi-episode burden measurement and critical-state scoring test"},
        {"test_id": "TEST-MEASUREMENT-PYTHON-001", "scope": "generator-free measurement scoring engine parity test"},
    ],
}


def read_json(path):
    with open(path, "r") as handle:
        return json.load(handle)


def write_json(path, value):
    text = json.dumps(value, indent=2, sort_keys=True)
    if path == "-":
        sys.stdout.write(text)
        sys.stdout.write("\n")
        return
    parent = os.path.dirname(path)
    if parent and not os.path.isdir(parent):
        os.makedirs(parent)
    with open(path, "w") as handle:
        handle.write(text)
        handle.write("\n")


def canonical_hash(value):
    encoded = json.dumps(value, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return "sha256:" + hashlib.sha256(encoded).hexdigest()


def run(command):
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = process.communicate()
    stdout_text = stdout.decode("utf-8")
    stderr_text = stderr.decode("utf-8")
    if process.returncode != 0:
        raise RuntimeError("command failed: %s\n%s\n%s" % (" ".join(command), stdout_text, stderr_text))
    if stderr_text:
        raise RuntimeError("stderr was not empty: %s" % stderr_text)
    return stdout_text


def run_json(command):
    return json.loads(run(command))


def run_key_values(command):
    result = {}
    for line in run(command).splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            result[key] = value
    return result


def abs_pack_path(catalog_path, entry):
    return os.path.normpath(os.path.join(os.path.dirname(catalog_path), entry["path"]))


def size_class(bytes_value):
    value = int(bytes_value)
    if value < 10 * 1024 * 1024:
        return "small"
    if value < 100 * 1024 * 1024:
        return "medium"
    if value < 1024 * 1024 * 1024:
        return "large"
    return "very_large"


def unique(values):
    output = []
    for value in values:
        if value not in output:
            output.append(value)
    return output


def case_ids_for_target(analysis, target):
    return [item["case_id"] for item in analysis["cases"] if target in item.get("targets", [])]


def effective_targets(analysis):
    return [item["target"] for item in analysis["targets"]]


def effective_scoring_mode(scoreable_targets, reference_targets):
    if scoreable_targets and reference_targets:
        return "mixed"
    if scoreable_targets:
        return "local"
    return "reference_only"


def contract_for_target(target, analysis):
    base = dict(TARGET_CONTRACTS.get(target, {
        "scoreable": False,
        "score_type": "generated_reference_only",
        "reference_artifacts": ["annotations_json", "case_summary_json"],
        "description": "Generated reference output. No local scoring policy is defined yet.",
    }))
    base["target"] = target
    base["case_ids"] = case_ids_for_target(analysis, target)
    return base


def target_contracts(analysis):
    scoreable = []
    reference_only = []
    for item in analysis["targets"]:
        contract = contract_for_target(item["target"], analysis)
        contract["case_count"] = item["case_count"]
        contract["support"] = item["support"]
        if contract.get("scoreable") and item["support"] == "local_scoring":
            scoreable.append(contract)
        else:
            contract["scoreable"] = False
            reference_only.append(contract)
    return scoreable, reference_only


def supported_profiles(entry, scoreable_targets):
    explicit = entry.get("supported_threshold_profiles")
    if explicit is not None:
        return list(explicit)
    if not scoreable_targets:
        return []
    return ["smoke", "regression", "stress", "benchmark"]


def channel_families(entry, analysis):
    families = []
    for modality in entry.get("modality", []):
        if modality == "ECG":
            families.append({"name": "standard_12_lead_ecg", "channel_count": 12, "channels": ["I", "II", "III", "aVR", "aVL", "aVF", "V1", "V2", "V3", "V4", "V5", "V6"]})
        if modality == "PPG":
            families.append({"name": "green_ppg", "channel_count": 1, "channels": ["ppg_green"]})
    if "ppg_optical" in effective_targets(analysis):
        families.append({"name": "paired_optical_ppg", "channel_count": 2, "channels": ["ppg_red", "ppg_infrared"]})
    if any(item.get("channel_count", 0) > 13 for item in analysis.get("cases", [])):
        families.append({"name": "motion_reference", "channel_count": 1, "channels": ["accel_motion"]})
    return families


def output_artifacts(scoreable_targets, reference_targets, analysis, has_verification_protocol):
    artifacts = [
        {"role": "manifest_json", "required": True},
        {"role": "scoring_manifest_json", "required": True},
        {"role": "provenance_json", "required": True},
        {"role": "engineering_claim_boundary_txt", "required": True},
        {"role": "case_summary_json", "required": True},
        {"role": "annotations_json", "required": True},
        {"role": "waveform_csv", "required": True},
        {"role": "wfdb", "required": True},
        {"role": "edf_bdf", "required": True},
    ]
    roles = set(item["role"] for item in artifacts)
    if has_verification_protocol:
        artifacts.append({"role": "verification_protocol_json", "required": True})
        roles.add("verification_protocol_json")
    for item in analysis.get("output_artifacts", []):
        if item["role"] not in roles:
            artifacts.append(dict(item))
            roles.add(item["role"])
    if any(item["target"] == "hrv" for item in scoreable_targets):
        for role in ("hrv_metrics_json", "rr_tachogram_csv"):
            if role not in roles:
                artifacts.append({"role": role, "required": True})
                roles.add(role)
    if any(item["score_type"] == "measurement" for item in scoreable_targets):
        artifacts.append({"role": "measurement_truth_json", "required": True})
    if reference_targets:
        artifacts.append({"role": "reference_ground_truth", "required": True, "targets": [item["target"] for item in reference_targets]})
    return artifacts


def submission_output_schemas(scoreable_targets):
    schemas = []
    for target in scoreable_targets:
        for schema in target.get("accepted_formats", []):
            if schema not in schemas:
                schemas.append(schema)
    return schemas


def local_verifier_smoke_tests(scoreable_targets):
    tests = []
    seen = set()
    for target in scoreable_targets:
        for item in LOCAL_VERIFIER_SMOKE_TESTS.get(target["target"], []):
            key = (item["test_id"], item["scope"])
            if key in seen:
                continue
            seen.add(key)
            enriched = dict(item)
            enriched["target"] = target["target"]
            tests.append(enriched)
    return tests


def case_metadata(case, scoreable_targets, reference_targets):
    targets = list(case.get("targets", []))
    return {
        "case_id": case["case_id"],
        "scenario_id": case["scenario_id"],
        "targets": targets,
        "scoreable_targets": [target["target"] for target in scoreable_targets if target["target"] in targets],
        "reference_only_targets": [target["target"] for target in reference_targets if target["target"] in targets],
        "duration_seconds": case["duration_seconds"],
        "sampling_rate_hz": case["sampling_rate_hz"],
        "sample_count": case["sample_count"],
        "channel_count": case["channel_count"],
        "external_noise": bool(case.get("external_noise", False)),
        "external_noise_release_allowed": bool(case.get("external_noise_release_allowed", True)),
        "external_noise_asset_ids": list(case.get("external_noise_asset_ids", [])),
        "estimated_waveform_csv_bytes": case["estimated_waveform_csv_bytes"],
        "estimated_binary_signal_bytes": case["estimated_binary_signal_bytes"],
        "estimated_package_bytes": case["estimated_package_bytes"],
        "estimated_peak_memory_bytes": case["estimated_peak_memory_bytes"],
    }


def stable_relpath(path, source_root):
    return os.path.relpath(os.path.abspath(path), os.path.abspath(source_root)).replace(os.sep, "/")


def verification_protocol_metadata(pack_path, pack, source_root):
    relative_path = pack.get("verification_protocol_path", "")
    if not relative_path:
        return {"available": False, "artifact_role": "", "source_path": "", "source_content_sha256": "", "document": None}
    protocol_path = os.path.join(os.path.dirname(pack_path), relative_path)
    protocol = read_json(protocol_path)
    required = set(["schema_version", "contract", "protocol_id", "pack_id", "context_of_use", "scoring_contract", "required_case_targets", "acceptance_profile", "stress_strata", "truth_policy", "evidence_boundary"])
    if not isinstance(protocol, dict) or set(protocol) != required:
        raise RuntimeError("verification protocol has an incomplete envelope: %s" % protocol_path)
    if protocol["schema_version"] != 2 or protocol["contract"] != "synsigra_verification_protocol_v2" or protocol["pack_id"] != pack["pack_id"] or protocol["scoring_contract"] != "synsigra_local_verification_v3":
        raise RuntimeError("verification protocol identity does not match pack: %s" % protocol_path)
    expected_matrix = [(item["id"], item["targets"]) for item in pack["scenarios"]]
    protocol_matrix = [(item.get("case_id"), item.get("targets")) for item in protocol["required_case_targets"]]
    if protocol_matrix != expected_matrix:
        raise RuntimeError("verification protocol case-target matrix does not match pack: %s" % protocol_path)
    return {
        "available": True,
        "artifact_role": "verification_protocol_json",
        "source_path": stable_relpath(protocol_path, source_root),
        "source_content_sha256": canonical_hash(protocol),
        "document": protocol,
    }


def pack_metadata(catalog_path, source_root, entry, pack, analysis, validate_info):
    scoreable_targets, reference_targets = target_contracts(analysis)
    case_ids = [item["case_id"] for item in analysis["cases"]]
    sampling_rates = unique([item["sampling_rate_hz"] for item in analysis["cases"]])
    channel_counts = [item["channel_count"] for item in analysis["cases"]]
    durations = [item["duration_seconds"] for item in analysis["cases"]]
    supported_threshold_profiles = supported_profiles(entry, scoreable_targets)
    protocol = verification_protocol_metadata(abs_pack_path(catalog_path, entry), pack, source_root)
    metadata = {
        "schema_version": 1,
        "metadata_type": "synsigra_curated_pack_metadata",
        "metadata_version": EXPORTER_VERSION,
        "pack_id": pack["pack_id"],
        "name": pack["name"],
        "version": pack["version"],
        "description": pack["description"],
        "release_status": entry.get("release_status", "beta"),
        "release_date": entry.get("release_date", ""),
        "deprecation_message": entry.get("deprecation_message", ""),
        "changelog": list(entry.get("changelog", [])),
        "modality": list(entry.get("modality", [])),
        "difficulty": list(entry.get("difficulty", [])),
        "feature_tags": list(entry.get("feature_tags", [])),
        "catalog_scoring_mode": entry.get("scoring_mode", ""),
        "scoring_mode": effective_scoring_mode(scoreable_targets, reference_targets),
        "recommended_profile": entry.get("recommended_profile"),
        "supported_threshold_profiles": supported_threshold_profiles,
        "recommended_for": list(entry.get("recommended_for", [])),
        "not_recommended_for": list(entry.get("not_recommended_for", [])),
        "source": {
            "catalog_path": stable_relpath(catalog_path, source_root),
            "pack_path": entry["path"],
            "pack_fingerprint": validate_info.get("pack_fingerprint", ""),
            "source_content_sha256": canonical_hash(pack),
        },
        "generator_compatibility": dict(DEFAULT_GENERATOR_COMPATIBILITY),
        "verification_protocol": protocol,
        "declared_targets": list(pack.get("targets", [])),
        "targets": effective_targets(analysis),
        "scoreable_targets": scoreable_targets,
        "reference_only_targets": reference_targets,
        "submission_output_schemas": submission_output_schemas(scoreable_targets),
        "local_verifier_smoke_tests": local_verifier_smoke_tests(scoreable_targets),
        "threshold_profile_contract": {
            "supported_profiles": supported_threshold_profiles,
            "recommended_profile": entry.get("recommended_profile"),
            "policy_failure_exit_code": 1,
        },
        "cases": [case_metadata(item, scoreable_targets, reference_targets) for item in analysis["cases"]],
        "case_count": analysis["summary"]["case_count"],
        "case_ids": case_ids,
        "duration": {
            "total_seconds": analysis["summary"]["total_duration_seconds"],
            "minimum_case_seconds": min(durations) if durations else 0,
            "maximum_case_seconds": max(durations) if durations else 0,
        },
        "sampling_rates_hz": sampling_rates,
        "channels": {
            "minimum_channel_count": min(channel_counts) if channel_counts else 0,
            "maximum_channel_count": max(channel_counts) if channel_counts else 0,
            "families": channel_families(entry, analysis),
        },
        "estimated_package": {
            "bytes": analysis["summary"]["estimated_package_bytes"],
            "size_class": size_class(analysis["summary"]["estimated_package_bytes"]),
            "peak_memory_bytes": analysis["summary"]["estimated_peak_memory_bytes"],
        },
        "output_artifacts": output_artifacts(scoreable_targets, reference_targets, analysis, protocol["available"]),
        "ui": {
            "primary_badges": unique(list(entry.get("difficulty", [])) + list(entry.get("feature_tags", []))[:3]),
            "scoreable_before_job": bool(scoreable_targets),
            "reference_only_before_job": bool(reference_targets),
        },
        "limitations": {
            "intended_use": "Synthetic biosignal engineering QA and algorithm verification",
            "not_for": "Diagnosis, patient monitoring, clinical validation certification, or standalone conformity assessment",
        },
        "messages": list(analysis.get("messages", [])),
    }
    metadata["generator_compatibility"].update(entry.get("generator_compatibility", {}))
    return metadata


def export_metadata(catalog_path, cli, pack_ids, source_root):
    catalog = read_json(catalog_path)
    selected = set(pack_ids or [])
    packs = []
    for entry in catalog["packs"]:
        if selected and entry["pack_id"] not in selected:
            continue
        pack_path = abs_pack_path(catalog_path, entry)
        pack = read_json(pack_path)
        validate_info = run_key_values([cli, "pack", "validate", pack_path])
        analysis = run_json([cli, "pack", "analyze", pack_path])
        if not analysis.get("success", False):
            raise RuntimeError("pack analysis failed for %s" % pack_path)
        packs.append(pack_metadata(catalog_path, source_root, entry, pack, analysis, validate_info))
    missing = sorted(selected - set(item["pack_id"] for item in packs))
    if missing:
        raise RuntimeError("pack id not found in catalog: %s" % ", ".join(missing))
    return {
        "schema_version": 1,
        "metadata_type": METADATA_TYPE,
        "metadata_version": EXPORTER_VERSION,
        "release_set_id": "synsigra_curated_release_2026_07_18",
        "release_set_status": "beta",
        "catalog_id": catalog.get("catalog_id", ""),
        "catalog_version": catalog.get("version", ""),
        "product": catalog.get("product", ""),
        "intended_use": catalog.get("intended_use", ""),
        "not_for": catalog.get("not_for", ""),
        "source_catalog_sha256": canonical_hash(catalog),
        "pack_count": len(packs),
        "packs": packs,
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description="Export SaaS-ingestable Synsigra curated-pack metadata.")
    parser.add_argument("--catalog", default="examples/catalog/verification_packs_v1.json", help="Input curated pack catalog JSON.")
    parser.add_argument("--cli", default=os.environ.get("SIGNAL_SYNTH_CLI", "signal-synth"), help="signal-synth CLI path.")
    parser.add_argument("--source-root", default=os.getcwd(), help="Project root used for stable source paths in metadata.")
    parser.add_argument("--pack-id", action="append", default=[], help="Restrict export to one pack id. Can be repeated.")
    parser.add_argument("--out", default="-", help="Output JSON path or '-' for stdout.")
    args = parser.parse_args(argv)
    document = export_metadata(args.catalog, args.cli, args.pack_id, args.source_root)
    write_json(args.out, document)
    return 0


if __name__ == "__main__":
    sys.exit(main())
