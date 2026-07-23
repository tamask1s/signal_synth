import json
import os
import shutil
import subprocess

import synsigra as ss


def run(command):
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = process.communicate()
    if process.returncode != 0 or stderr:
        raise RuntimeError(
            "command failed: {}\n{}\n{}".format(
                " ".join(command),
                stdout.decode("utf-8"),
                stderr.decode("utf-8"),
            )
        )
    return stdout.decode("utf-8")


def read_json(path):
    with open(path, "r") as handle:
        return json.load(handle)


def write_json(path, value):
    with open(path, "w") as handle:
        json.dump(value, handle, sort_keys=True, separators=(",", ":"))


def output_path(root, output):
    return os.path.join(root, *output["path"].split("/"))


def scoreable_r_peaks(case):
    return [
        {"time_seconds": beat["r_peak_seconds"]}
        for beat in case.annotations()["beats"]
        if beat.get("qrs_present", False) and beat.get("r_peak_scoreable", True)
    ]


def perfect_submission(challenge_dir, submission_dir, algorithm_name):
    shutil.copytree(
        os.path.join(challenge_dir, "user-output-template"),
        submission_dir,
    )
    manifest_path = os.path.join(submission_dir, "submission.json")
    manifest = read_json(manifest_path)
    manifest["algorithm"] = {"name": algorithm_name, "version": "1"}
    assert manifest["outputs"]
    assert set(item["target"] for item in manifest["outputs"]) == set(["r_peak"])
    write_json(manifest_path, manifest)
    challenge = ss.load_challenge(challenge_dir)
    for output in manifest["outputs"]:
        write_json(
            output_path(submission_dir, output),
            {
                "schema_version": 1,
                "events": scoreable_r_peaks(challenge.case(output["case_id"])),
            },
        )
    challenge.close()
    return manifest


def assert_rpeak_only_protocol(challenge, expected_cases):
    protocol = challenge.verification_protocol()
    assert protocol["contract"] == "synsigra_verification_protocol_v2"
    assert [item["case_id"] for item in protocol["required_case_targets"]] == expected_cases
    assert set(
        target
        for item in protocol["required_case_targets"]
        for target in item["targets"]
    ) == set(["r_peak"])
    scoring = challenge.scoring_manifest()
    assert set(
        item["target"]
        for case in scoring["cases"]
        for item in case["scoring"]
    ) == set(["r_peak"])


def main():
    source = os.environ["SIGNAL_SYNTH_SOURCE_DIR"]
    cli = os.environ["SIGNAL_SYNTH_CLI"]
    work = os.environ["SIGNAL_SYNTH_RPEAK_EVIDENCE_WORK_DIR"]
    if os.path.exists(work):
        shutil.rmtree(work)
    os.makedirs(work)

    run([
        os.environ.get("PYTHON", "python3"),
        os.path.join(source, "scripts", "generate_r_peak_noise_frontier.py"),
        "--check",
    ])

    evidence_challenge_dir = os.path.join(work, "detector_evidence_challenge")
    frontier_challenge_dir = os.path.join(work, "noise_frontier_challenge")
    run([
        cli, "pack", "challenge",
        os.path.join(source, "examples", "packs", "r_peak_stress_v1.json"),
        "--out", evidence_challenge_dir,
    ])
    run([
        cli, "pack", "challenge",
        os.path.join(source, "examples", "packs", "r_peak_noise_frontier_v1.json"),
        "--out", frontier_challenge_dir,
        "--noise-assets", os.path.join(source, "examples", "assets", "noise"),
    ])

    evidence_challenge = ss.load_challenge(evidence_challenge_dir)
    frontier_challenge = ss.load_challenge(frontier_challenge_dir)
    evidence_cases = ["clean_70", "slow_45", "fast_120", "baseline_powerline"]
    frontier_cases = [
        "clean_anchor", "mixed_snr_m7", "mixed_snr_m8",
        "mixed_snr_m9", "mixed_snr_m10",
    ]
    assert_rpeak_only_protocol(evidence_challenge, evidence_cases)
    assert_rpeak_only_protocol(frontier_challenge, frontier_cases)

    expected_snr = {
        "mixed_snr_m7": -7,
        "mixed_snr_m8": -8,
        "mixed_snr_m9": -9,
        "mixed_snr_m10": -10,
    }
    expected_artifact_severity = {
        "mixed_snr_m7": (0.35, 0.10),
        "mixed_snr_m8": (0.50, 0.18),
        "mixed_snr_m9": (0.65, 0.26),
        "mixed_snr_m10": (0.80, 0.34),
    }
    paired_r_peak_times = None
    mean_noise_ratios = []
    for case_id, snr_db in expected_snr.items():
        truth = read_json(os.path.join(
            frontier_challenge_dir, "cases", case_id,
            "external_noise_truth.json",
        ))
        assert truth["release_allowed"] and len(truth["intervals"]) == 18
        channels = [
            channel
            for interval in truth["intervals"]
            for channel in interval["channels"]
        ]
        assert len(channels) == 18 * 12
        assert set(channel["target_snr_db"] for channel in channels) == set([snr_db])
        assert max(
            abs(channel["achieved_snr_db"] - channel["target_snr_db"])
            for channel in channels
        ) < 1e-9
        mean_noise_ratios.append(sum(
            channel["added_noise_rms_mv"] / channel["clean_rms_mv"]
            for channel in channels
        ) / len(channels))
        annotations = frontier_challenge.case(case_id).annotations()
        assert len(annotations["beats"]) >= 70
        r_peak_times = [
            beat["r_peak_seconds"] for beat in annotations["beats"]
            if beat.get("qrs_present", False)
        ]
        if paired_r_peak_times is None:
            paired_r_peak_times = r_peak_times
        else:
            assert r_peak_times == paired_r_peak_times
        artifact_types = set(
            item["type"] for item in annotations.get("artifact_intervals", [])
        )
        assert set([
            "ecg_baseline_wander", "ecg_powerline", "ecg_external_noise",
        ]).issubset(artifact_types)
        scenario = read_json(os.path.join(
            frontier_challenge_dir, "cases", case_id, "resolved_scenario.json",
        ))
        severities = dict(
            (item["type"], item["severity"])
            for item in scenario["artifacts"]
        )
        assert (
            severities["ecg_baseline_wander"],
            severities["ecg_powerline"],
        ) == expected_artifact_severity[case_id]
    assert mean_noise_ratios == sorted(mean_noise_ratios)

    evidence_submission = os.path.join(work, "detector_evidence_submission")
    evidence_manifest = perfect_submission(
        evidence_challenge_dir, evidence_submission, "perfect-rpeak-only",
    )
    assert len(evidence_manifest["outputs"]) == 4
    evidence_result = ss.verify_package(
        evidence_challenge,
        evidence_submission,
        os.path.join(work, "detector_evidence_result"),
    )
    assert evidence_result.evidence["success"]
    assert evidence_result.evidence["verification"]["matrix_complete"]
    assert evidence_result.evidence["verification"]["evidence_eligible"]
    assert evidence_result.evidence["verification"]["evidence_passed"]
    assert evidence_result.evidence["case_target_count"] == 4
    assert set(item["target"] for item in evidence_result.evidence["results"]) == set(["r_peak"])

    frontier_submission = os.path.join(work, "frontier_submission")
    frontier_manifest = perfect_submission(
        frontier_challenge_dir, frontier_submission, "perfect-frontier-rpeak",
    )
    assert len(frontier_manifest["outputs"]) == 5
    frontier_result_dir = os.path.join(work, "frontier_result")
    frontier_result = ss.verify_package(
        frontier_challenge, frontier_submission, frontier_result_dir,
    )
    assert frontier_result.evidence["success"]
    assert frontier_result.evidence["verification"]["evidence_eligible"]
    assert frontier_result.evidence["verification"]["evidence_passed"]
    strata = frontier_result.evidence["policy"]["acceptance_strata"]
    assert [item["stratum_id"] for item in strata] == frontier_cases
    assert all(item["passed"] for item in strata)

    weak_submission = os.path.join(work, "weak_frontier_submission")
    shutil.copytree(frontier_submission, weak_submission)
    for output in frontier_manifest["outputs"]:
        if output["case_id"] == "clean_anchor":
            continue
        path = output_path(weak_submission, output)
        document = read_json(path)
        document["events"] = document["events"][::4]
        write_json(path, document)
    weak_result_dir = os.path.join(work, "weak_frontier_result")
    weak_result = ss.verify_package(
        frontier_challenge, weak_submission, weak_result_dir,
    )
    assert not weak_result.evidence["success"]
    assert weak_result.evidence["scoring_success"]
    assert weak_result.evidence["verification"]["matrix_complete"]
    assert weak_result.evidence["verification"]["evidence_eligible"]
    assert not weak_result.evidence["verification"]["evidence_passed"]
    failed_strata = [
        item["stratum_id"]
        for item in weak_result.evidence["policy"]["acceptance_strata"]
        if not item["passed"]
    ]
    assert failed_strata == [
        "mixed_snr_m7", "mixed_snr_m8", "mixed_snr_m9", "mixed_snr_m10",
    ]
    index_html = open(
        os.path.join(weak_result_dir, "index.html"), "r"
    ).read()
    assert "Case contribution breakdown" in index_html
    assert all(case_id in index_html for case_id in expected_snr)

    evidence_challenge.close()
    frontier_challenge.close()
    print("r_peak_evidence_pack_test=passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
