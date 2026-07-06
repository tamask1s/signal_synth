import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile

import synsigra as ss


FIXTURE_RELATIVE = os.path.join("python", "tests", "fixtures", "golden", "r_peak_stress_v1")
ISSUE_URL = "https://github.com/tamask1s/signal_synth/issues/66"


def read_json(path):
    with open(path, "r") as handle:
        return json.load(handle)


def write_json(path, data):
    with open(path, "w") as handle:
        json.dump(data, handle, indent=2, sort_keys=True)
        handle.write("\n")


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        while True:
            block = handle.read(1024 * 1024)
            if not block:
                break
            digest.update(block)
    return "sha256:" + digest.hexdigest()


def deterministic_zip(source_dir, archive_path):
    with zipfile.ZipFile(archive_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for root, dirs, files in os.walk(source_dir):
            dirs.sort()
            files.sort()
            for name in files:
                full_path = os.path.join(root, name)
                relative = os.path.relpath(full_path, source_dir).replace(os.sep, "/")
                info = zipfile.ZipInfo(relative)
                info.date_time = (2026, 1, 1, 0, 0, 0)
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o644 << 16
                with open(full_path, "rb") as handle:
                    archive.writestr(info, handle.read())


def run_verify(package_path, detections_dir, output_dir):
    command = [
        sys.executable, "-m", "synsigra.cli", "verify", package_path, detections_dir, output_dir,
        "--target", "r_peak", "--profile", "regression", "--force"
    ]
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = process.communicate()
    return process.returncode, stdout.decode("utf-8"), stderr.decode("utf-8")


def metric_subset(metrics):
    return {
        "ground_truth_count": metrics.get("ground_truth_count", 0),
        "detection_count": metrics.get("detection_count", 0),
        "true_positive_count": metrics.get("true_positive_count", 0),
        "false_positive_count": metrics.get("false_positive_count", 0),
        "false_negative_count": metrics.get("false_negative_count", 0),
        "sensitivity": round(float(metrics.get("sensitivity", 0.0)), 12),
        "positive_predictive_value": round(float(metrics.get("positive_predictive_value", 0.0)), 12),
        "f1_score": round(float(metrics.get("f1_score", 0.0)), 12),
        "mean_absolute_error_seconds": round(float(metrics.get("mean_absolute_error_seconds", 0.0)), 12),
        "max_absolute_error_seconds": round(float(metrics.get("max_absolute_error_seconds", 0.0)), 12),
    }


def normalize_summary(summary):
    targets = []
    for target in summary.get("targets", []):
        item = {
            "target": target.get("target", ""),
            "score_type": target.get("score_type", ""),
            "case_count": target.get("case_count", 0),
            "passed_case_count": target.get("passed_case_count", 0),
            "failed_case_count": target.get("failed_case_count", 0),
            "total": metric_subset(target.get("total", {})),
            "clean": metric_subset(target.get("clean", {})),
            "artifact": metric_subset(target.get("artifact", {})),
            "policy": {
                "profile_id": target.get("policy", {}).get("profile_id", ""),
                "passed": target.get("policy", {}).get("passed", False),
                "failed_checks": [
                    {
                        "section": check.get("section", ""),
                        "metric": check.get("metric", ""),
                        "operator": check.get("operator", ""),
                        "threshold": check.get("threshold", 0.0),
                        "actual": round(float(check.get("actual", 0.0)), 12) if check.get("actual") is not None else None,
                    }
                    for check in target.get("policy", {}).get("checks", [])
                    if check.get("applicable", False) and not check.get("passed", False)
                ],
            },
        }
        targets.append(item)
    cases = []
    for case in summary.get("cases", []):
        metrics = case.get("metrics", {})
        cases.append({
            "case_id": case.get("case_id", ""),
            "scenario_id": case.get("scenario_id", ""),
            "target": case.get("target", ""),
            "status": case.get("status", ""),
            "success": case.get("success", False),
            "score_type": case.get("score_type", ""),
            "report_path": case.get("report_path", ""),
            "detection_input_sha256": case.get("detection_input_sha256", ""),
            "total": metric_subset(metrics.get("total", {}) if isinstance(metrics, dict) else {}),
            "clean": metric_subset(metrics.get("clean", {}) if isinstance(metrics, dict) else {}),
            "artifact": metric_subset(metrics.get("artifact", {}) if isinstance(metrics, dict) else {}),
        })
    return {
        "schema_version": summary.get("schema_version", 0),
        "summary_type": summary.get("summary_type", ""),
        "scoring_version": summary.get("scoring_version", ""),
        "status": summary.get("status", ""),
        "success": summary.get("success", False),
        "scoring_success": summary.get("scoring_success", False),
        "package": {
            "package_id": summary.get("package", {}).get("package_id", ""),
            "version": summary.get("package", {}).get("version", ""),
            "pack_fingerprint": summary.get("package", {}).get("pack_fingerprint", ""),
            "generator_version": summary.get("package", {}).get("generator_version", ""),
            "ground_truth_included": summary.get("package", {}).get("ground_truth_included", False),
            "usage_restrictions": summary.get("package", {}).get("usage_restrictions", ""),
            "not_for": summary.get("package", {}).get("not_for", ""),
        },
        "integrity": {
            "checked_file_count": summary.get("integrity", {}).get("checked_file_count", 0),
            "total_bytes": summary.get("integrity", {}).get("total_bytes", 0),
            "ok": summary.get("integrity", {}).get("ok", False),
        },
        "case_target_count": summary.get("case_target_count", 0),
        "scored_case_target_count": summary.get("scored_case_target_count", 0),
        "passed_case_target_count": summary.get("passed_case_target_count", 0),
        "failed_case_target_count": summary.get("failed_case_target_count", 0),
        "policy": {
            "profile_id": summary.get("policy", {}).get("profile_id", ""),
            "passed": summary.get("policy", {}).get("passed", False),
            "target_count": summary.get("policy", {}).get("target_count", 0),
            "failed_check_count": summary.get("policy", {}).get("failed_check_count", 0),
        },
        "targets": targets,
        "cases": cases,
    }


def assert_or_update(path, actual, update):
    if update:
        write_json(path, actual)
        return
    expected = read_json(path)
    assert actual == expected


def assert_contract(fixture_dir, contract, archive_path, challenge):
    assert contract["schema_version"] == 1
    assert contract["issue_url"] == ISSUE_URL
    assert contract["pack_id"] == "r_peak_stress_v1"
    assert contract["pack_version"] == "1.0"
    assert contract["target"] == "r_peak"
    assert contract["case_ids"] == ["clean_70", "slow_45", "fast_120", "baseline_powerline"]
    assert contract["pack_fingerprint"].startswith("sha256:")
    assert contract["full_package_fingerprint"].startswith("sha256:")
    assert contract["full_package_waveform_formats"] == ["csv", "wfdb", "edf", "bdf"]
    assert sha256_file(os.path.join(fixture_dir, "challenge", "manifest.json")) == contract["manifest_sha256"]
    assert sha256_file(os.path.join(fixture_dir, "challenge", "scoring_manifest.json")) == contract["scoring_manifest_sha256"]
    assert sha256_file(os.path.join(fixture_dir, "challenge", "pack.json")) == contract["pack_json_sha256"]
    assert sha256_file(os.path.join(fixture_dir, "challenge", "summary.json")) == contract["summary_json_sha256"]
    if contract.get("compact_archive_sha256"):
        assert sha256_file(archive_path) == contract["compact_archive_sha256"]
    assert challenge.package_id == contract["pack_id"]
    assert challenge.version == contract["pack_version"]
    assert challenge.case_ids() == contract["case_ids"]
    scoring_manifest = challenge.scoring_manifest()
    assert scoring_manifest["package_id"] == contract["pack_id"]
    assert scoring_manifest["pack_fingerprint"] == contract["pack_fingerprint"]
    assert scoring_manifest["targets"][0]["target"] == "r_peak"
    assert scoring_manifest["targets"][0]["cases"] == contract["case_ids"]
    for case in scoring_manifest["cases"]:
        assert case["case_id"] in contract["case_ids"]
        assert case["scoring"][0]["target"] == "r_peak"
        assert case["scoring"][0]["score_type"] == "event_detection"
        assert "detection_json_v1" in case["scoring"][0]["accepted_detection_formats"]
        assert "detection_csv_v2" in case["scoring"][0]["accepted_detection_formats"]
        assert case["scoring"][0]["score_command"].startswith("signal-synth compare rpeaks")


def normalize_pack_contract(pack):
    return {
        "schema_version": pack.get("schema_version"),
        "pack_id": pack.get("pack_id", ""),
        "version": pack.get("version", ""),
        "targets": list(pack.get("targets", [])),
        "scenarios": [
            {
                "id": item.get("id", ""),
                "targets": list(item.get("targets", [])),
                "scenario_file": os.path.basename(item.get("path", "")),
            }
            for item in pack.get("scenarios", [])
        ],
    }


def assert_pack_contract(contract, pack_path):
    pack_contract = normalize_pack_contract(read_json(pack_path))
    assert pack_contract == {
        "schema_version": 1,
        "pack_id": contract["pack_id"],
        "version": contract["pack_version"],
        "targets": [contract["target"]],
        "scenarios": [
            {"id": "clean_70", "targets": ["r_peak"], "scenario_file": "rpeak_clean_70.json"},
            {"id": "slow_45", "targets": ["r_peak"], "scenario_file": "rpeak_slow_45.json"},
            {"id": "fast_120", "targets": ["r_peak"], "scenario_file": "rpeak_fast_120.json"},
            {"id": "baseline_powerline", "targets": ["r_peak", "signal_quality"], "scenario_file": "sq_baseline_powerline.json"},
        ],
    }


def assert_optional_saas_contract(contract):
    saas_dir = os.environ.get("SIGNAL_SYNTH_SAAS_DIR")
    if not saas_dir:
        return
    assert_pack_contract(contract, os.path.join(saas_dir, "packs", contract["pack_id"] + ".json"))


def maybe_update_archive_hash(contract_path, contract, archive_path, update):
    archive_sha = sha256_file(archive_path)
    if update:
        contract["compact_archive_sha256"] = archive_sha
        write_json(contract_path, contract)
    else:
        assert contract.get("compact_archive_sha256") == archive_sha


def main():
    source_dir = os.environ.get("SIGNAL_SYNTH_SOURCE_DIR") or os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    fixture_dir = os.path.join(source_dir, FIXTURE_RELATIVE)
    contract_path = os.path.join(fixture_dir, "golden_contract.json")
    contract = read_json(contract_path)
    update = os.environ.get("SYNSIGRA_UPDATE_GOLDEN") == "1"
    work_dir = tempfile.mkdtemp(prefix="synsigra_golden_")
    try:
        assert_pack_contract(contract, os.path.join(source_dir, contract["source_pack_path"]))
        assert_optional_saas_contract(contract)
        archive_path = os.path.join(work_dir, "r_peak_stress_v1_compact.synsigra")
        deterministic_zip(os.path.join(fixture_dir, "challenge"), archive_path)
        maybe_update_archive_hash(contract_path, contract, archive_path, update)
        with ss.load_challenge(archive_path) as challenge:
            integrity = challenge.verify_integrity()
            assert integrity["ok"]
            assert_contract(fixture_dir, contract, archive_path, challenge)

        pass_output = os.path.join(work_dir, "pass_verify")
        pass_code, pass_stdout, pass_stderr = run_verify(archive_path, os.path.join(fixture_dir, "detections", "pass"), pass_output)
        assert pass_code == 0
        assert pass_stderr == ""
        assert "status=passed" in pass_stdout
        pass_summary = normalize_summary(read_json(os.path.join(pass_output, "verification_summary.json")))
        assert pass_summary["status"] == "passed"
        assert pass_summary["success"]
        assert_or_update(os.path.join(fixture_dir, contract["expected"]["pass_summary"]), pass_summary, update)

        fail_output = os.path.join(work_dir, "fail_verify")
        fail_code, fail_stdout, fail_stderr = run_verify(archive_path, os.path.join(fixture_dir, "detections", "fail"), fail_output)
        assert fail_code == 1
        assert fail_stderr == ""
        assert "status=failed" in fail_stdout
        fail_summary = normalize_summary(read_json(os.path.join(fail_output, "verification_summary.json")))
        assert fail_summary["status"] == "failed"
        assert not fail_summary["success"]
        assert fail_summary["scoring_success"]
        assert fail_summary["policy"]["failed_check_count"] > 0
        assert_or_update(os.path.join(fixture_dir, contract["expected"]["fail_summary"]), fail_summary, update)
    finally:
        shutil.rmtree(work_dir)
    print("golden_challenge_fixture_test=passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
