import json
import os
import shutil
import subprocess


def run(command):
    return subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)


cli = os.environ["SIGNAL_SYNTH_CLI"]
pack = os.environ["SIGNAL_SYNTH_PACK_EXAMPLE"]
scenario = os.environ["SIGNAL_SYNTH_EXAMPLE"]
work_dir = os.environ["SIGNAL_SYNTH_INTEGRATION_WORK_DIR"]

contract_process = run([cli, "contract"])
assert contract_process.returncode == 0
assert contract_process.stderr == ""
contract = json.loads(contract_process.stdout)
assert contract_process.stdout.strip() == json.dumps(contract, separators=(",", ":"))
assert contract["schema_version"] == 1
assert contract["contract"] == "synsigra_core_integration_v3"
assert contract["external_noise"]["scenario_schema_version"] == 8
assert contract["contracts"]["cpp_facade"] == "1.1.0"
assert contract["contracts"]["challenge_package"] == "synsigra_challenge_package_v2"
assert contract["contracts"]["scoring_manifest"] == "synsigra_scoring_manifest_v2"
assert contract["contracts"]["submission"] == "synsigra_submission_v1"
assert contract["contracts"]["submission_formats"] == "synsigra_submission_formats_v1"
assert contract["cli"]["challenge_success_media_type"] == "application/json"
assert contract["cli"]["comparison_targets"] == ["r_peak", "ppg_systolic_peak", "ppg_pulse_onset", "ecg_beat_classification"]
assert contract["cli"]["interval_targets"] == ["rhythm_episode", "signal_quality"]
assert contract["cli"]["interval_output_schemas"] == ["interval_json_v1", "interval_csv_v1"]
assert contract["cli"]["delineation_targets"] == ["ecg_delineation"]
assert contract["cli"]["delineation_output_schemas"] == ["point_events_json_v1", "point_events_csv_v1"]
assert contract["cli"]["hrv_targets"] == ["hrv"]
assert contract["cli"]["measurement_targets"] == ["morphology_assertions", "ecg_ppg_alignment", "ppg_optical", "prv", "respiratory_rate", "rhythm_burden"]
assert contract["cli"]["customer_verification_command"].startswith("synsigra-verify")
assert contract["cli"]["customer_output_schemas"] == ["point_events_json_v1", "point_events_csv_v1", "interval_events_json_v1", "interval_events_csv_v1", "hrv_metrics_json_v1", "measurement_values_json_v1", "measurement_values_csv_v1"]

shutil.rmtree(work_dir, ignore_errors=True)
os.makedirs(work_dir)
challenge_dir = os.path.join(work_dir, "challenge")
challenge_process = run([cli, "pack", "challenge", pack, "--out", challenge_dir])
assert challenge_process.returncode == 0
assert challenge_process.stderr == ""
receipt = json.loads(challenge_process.stdout)
assert challenge_process.stdout.strip() == json.dumps(receipt, separators=(",", ":"))
assert receipt["schema_version"] == 1
assert receipt["contract"] == contract["contract"]
assert receipt["status"] == "challenge_rendered"
assert receipt["output_directory"] == challenge_dir
assert receipt["package_id"] == "r_peak_stress_v1"
assert receipt["scenario_count"] == 4
assert receipt["pack_fingerprint"].startswith("sha256:")
assert receipt["package_fingerprint"].startswith("sha256:")
assert receipt["generator"] == contract["generator"]
assert receipt["contracts"]["challenge_package"] == contract["contracts"]["challenge_package"]
assert receipt["contracts"]["scoring_manifest"] == contract["contracts"]["scoring_manifest"]

with open(os.path.join(challenge_dir, "manifest.json"), "r") as manifest_file:
    manifest = json.load(manifest_file)
assert manifest["package_id"] == receipt["package_id"]
assert os.path.isfile(os.path.join(challenge_dir, "user-output-template", "submission.json"))
with open(os.path.join(challenge_dir, "scoring_manifest.json"), "r") as handle:
    scoring_manifest = json.load(handle)
assert scoring_manifest["submission_contract_version"] == "synsigra_submission_v1"
assert scoring_manifest["submission_template_path"] == "user-output-template/submission.json"
assert scoring_manifest["submission_format_contract_path"] == "user-output-template/formats.json"
with open(os.path.join(challenge_dir, "user-output-template", "formats.json"), "r") as handle:
    format_contract = json.load(handle)
assert format_contract["contract"] == "synsigra_submission_formats_v1"
assert [item["name"] for item in format_contract["formats"]] == contract["cli"]["customer_output_schemas"]
for case in scoring_manifest["cases"]:
    for entry in case["scoring"]:
        if entry["supported"]:
            assert entry["accepted_formats"]
            assert entry["recommended_format"] in entry["accepted_formats"]
            assert entry["recommended_path"].startswith("outputs/%s/" % case["case_id"])
            assert "score_command" not in entry
            assert not any(key.startswith("accepted_") and key != "accepted_formats" for key in entry)

legacy_process = run([cli, "compare", "rpeaks", scenario, os.path.join(work_dir, "missing.json"), "--out", os.path.join(work_dir, "compare")])
assert legacy_process.returncode == 2
assert legacy_process.stdout == ""
assert legacy_process.stderr.startswith("error=COMPARE_TARGET_FAILED")

print("integration_contract_test=passed")
