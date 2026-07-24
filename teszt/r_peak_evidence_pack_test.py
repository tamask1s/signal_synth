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
    assert set(item["target"] for item in manifest["outputs"]) == set([
        "r_peak", "rr_interval",
    ])
    write_json(manifest_path, manifest)
    challenge = ss.load_challenge(challenge_dir)
    for output in manifest["outputs"]:
        case = challenge.case(output["case_id"])
        if output["target"] == "r_peak":
            document = {
                "schema_version": 1,
                "events": scoreable_r_peaks(case),
            }
        elif output["target"] == "rr_interval":
            document = {
                "schema_version": 2,
                "contract": "synsigra_measurement_values_v2",
                "measurements": [
                    dict(item["measurement"])
                    for item in case.measurement_truth("rr_interval")
                ],
            }
        else:
            raise AssertionError("unexpected target: {}".format(output["target"]))
        write_json(output_path(submission_dir, output), document)
    challenge.close()
    return manifest


def assert_detector_protocol(challenge, expected_cases):
    protocol = challenge.verification_protocol()
    assert protocol["contract"] == "synsigra_verification_protocol_v2"
    assert [item["case_id"] for item in protocol["required_case_targets"]] == expected_cases
    assert set(
        target
        for item in protocol["required_case_targets"]
        for target in item["targets"]
    ) == set(["r_peak", "rr_interval"])
    scoring = challenge.scoring_manifest()
    assert set(
        item["target"]
        for case in scoring["cases"]
        for item in case["scoring"]
    ) == set(["r_peak", "rr_interval"])


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
    run([
        os.environ.get("PYTHON", "python3"),
        os.path.join(source, "scripts", "generate_simple_r_peak_packs.py"),
        "--check",
    ])

    evidence_challenge_dir = os.path.join(work, "detector_evidence_challenge")
    frontier_challenge_dir = os.path.join(work, "noise_frontier_challenge")
    simple_challenge_dir = os.path.join(work, "simple_stress_challenge")
    ladder_challenge_dir = os.path.join(work, "snr_ladder_challenge")
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
    run([
        cli, "pack", "challenge",
        os.path.join(source, "examples", "packs", "r_peak_rr_simple_stress_v1.json"),
        "--out", simple_challenge_dir,
    ])
    run([
        cli, "pack", "challenge",
        os.path.join(source, "examples", "packs", "r_peak_rr_snr_ladder_v1.json"),
        "--out", ladder_challenge_dir,
        "--noise-assets", os.path.join(source, "examples", "assets", "noise"),
    ])

    evidence_challenge = ss.load_challenge(evidence_challenge_dir)
    frontier_challenge = ss.load_challenge(frontier_challenge_dir)
    simple_challenge = ss.load_challenge(simple_challenge_dir)
    ladder_challenge = ss.load_challenge(ladder_challenge_dir)
    evidence_cases = ["clean_70", "slow_45", "fast_120", "baseline_powerline"]
    frontier_cases = [
        "clean_anchor", "mixed_snr_m3", "mixed_snr_m4", "mixed_snr_m5",
        "mixed_snr_m7", "mixed_snr_m8", "mixed_snr_m9",
        "mixed_snr_m10", "mixed_snr_m11",
    ]
    assert_detector_protocol(evidence_challenge, evidence_cases)
    assert_detector_protocol(frontier_challenge, frontier_cases)
    ladder_cases = ["clean"] + ["snr_m{}".format(level) for level in range(1, 12)]
    assert_detector_protocol(simple_challenge, evidence_cases)
    assert_detector_protocol(ladder_challenge, ladder_cases)
    for challenge in (simple_challenge, ladder_challenge):
        protocol = challenge.verification_protocol()
        assert protocol["verdict_scope"] == "per_case"
        assert "acceptance_profile" not in protocol
        assert [item["case_ids"] for item in protocol["acceptance_strata"]] == [
            [case_id] for case_id in challenge.case_ids()
        ]
    frontier_protocol = frontier_challenge.verification_protocol()
    strata_by_id = dict(
        (item["id"], item) for item in frontier_protocol["acceptance_strata"]
    )
    expected_f1 = {
        "clean_anchor": 0.98,
        "mixed_snr_m3": 0.90,
        "mixed_snr_m4": 0.85,
        "mixed_snr_m5": 0.78,
        "mixed_snr_m7": 0.70,
        "mixed_snr_m8": 0.65,
        "mixed_snr_m9": 0.62,
        "mixed_snr_m10": 0.60,
        "mixed_snr_m11": 0.55,
    }
    for case_id, f1_minimum in expected_f1.items():
        targets = strata_by_id[case_id]["acceptance_profile"]["targets"]
        score_bin = "total" if case_id == "clean_anchor" else "artifact"
        assert targets["r_peak"][score_bin]["f1_score"]["min"] == f1_minimum
        expected_rr_mae = 0.030 if case_id == "mixed_snr_m11" else 0.025
        assert (
            targets["rr_interval"]["rr_interval"]["mean_absolute_error"]["max"]
            == expected_rr_mae
        )

    expected_snr = {
        "mixed_snr_m3": -3,
        "mixed_snr_m4": -4,
        "mixed_snr_m5": -5,
        "mixed_snr_m7": -7,
        "mixed_snr_m8": -8,
        "mixed_snr_m9": -9,
        "mixed_snr_m10": -10,
        "mixed_snr_m11": -11,
    }
    expected_artifact_severity = {
        "mixed_snr_m3": (0.08, 0.02),
        "mixed_snr_m4": (0.16, 0.04),
        "mixed_snr_m5": (0.25, 0.07),
        "mixed_snr_m7": (0.35, 0.10),
        "mixed_snr_m8": (0.50, 0.18),
        "mixed_snr_m9": (0.65, 0.26),
        "mixed_snr_m10": (0.80, 0.34),
        "mixed_snr_m11": (0.95, 0.42),
    }
    paired_r_peak_times = None
    paired_rr_truth = None
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
        rr_truth = [
            item["measurement"]
            for item in frontier_challenge.case(case_id).measurement_truth("rr_interval")
        ]
        assert rr_truth and all(item["name"] == "rr_interval" for item in rr_truth)
        if paired_rr_truth is None:
            paired_rr_truth = rr_truth
        else:
            assert rr_truth == paired_rr_truth
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

    ladder_r_peak_times = None
    for level in range(1, 12):
        ladder_case_id = "snr_m{}".format(level)
        truth = read_json(os.path.join(
            ladder_challenge_dir, "cases", ladder_case_id,
            "external_noise_truth.json",
        ))
        assert len(truth["intervals"]) == 20
        assert truth["intervals"][0]["start_seconds"] == 0
        assert truth["intervals"][-1]["end_seconds"] == 60
        assert all(item["taper_seconds"] == 0 for item in truth["intervals"])
        assert all(
            channel["target_snr_db"] == -level
            for interval in truth["intervals"]
            for channel in interval["channels"]
        )
        r_peak_times = [
            beat["r_peak_seconds"]
            for beat in ladder_challenge.case(ladder_case_id).annotations()["beats"]
            if beat.get("qrs_present", False)
        ]
        if ladder_r_peak_times is None:
            ladder_r_peak_times = r_peak_times
        else:
            assert r_peak_times == ladder_r_peak_times
    clean_ladder_r_peaks = [
        beat["r_peak_seconds"]
        for beat in ladder_challenge.case("clean").annotations()["beats"]
        if beat.get("qrs_present", False)
    ]
    assert clean_ladder_r_peaks == ladder_r_peak_times

    evidence_submission = os.path.join(work, "detector_evidence_submission")
    evidence_manifest = perfect_submission(
        evidence_challenge_dir, evidence_submission, "perfect-rpeak-rr",
    )
    assert len(evidence_manifest["outputs"]) == 8
    evidence_result = ss.verify_package(
        evidence_challenge,
        evidence_submission,
        os.path.join(work, "detector_evidence_result"),
    )
    assert evidence_result.evidence["success"]
    assert evidence_result.evidence["verification"]["matrix_complete"]
    assert evidence_result.evidence["verification"]["evidence_eligible"]
    assert evidence_result.evidence["verification"]["evidence_passed"]
    assert evidence_result.evidence["case_target_count"] == 8
    assert set(item["target"] for item in evidence_result.evidence["results"]) == set([
        "r_peak", "rr_interval",
    ])

    frontier_submission = os.path.join(work, "frontier_submission")
    frontier_manifest = perfect_submission(
        frontier_challenge_dir, frontier_submission, "perfect-frontier-rpeak-rr",
    )
    assert len(frontier_manifest["outputs"]) == 18
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

    simple_submission = os.path.join(work, "simple_submission")
    perfect_submission(
        simple_challenge_dir, simple_submission, "perfect-simple-rpeak-rr",
    )
    simple_result = ss.verify_package(
        simple_challenge,
        simple_submission,
        os.path.join(work, "simple_result"),
    )
    assert simple_result.evidence["success"]
    assert simple_result.evidence["policy"]["profile_id"] == "per_case_profiles"
    assert len(simple_result.evidence["policy"]["acceptance_strata"]) == 4

    ladder_submission = os.path.join(work, "ladder_submission")
    ladder_manifest = perfect_submission(
        ladder_challenge_dir, ladder_submission, "perfect-ladder-rpeak-rr",
    )
    ladder_result_dir = os.path.join(work, "ladder_result")
    ladder_result = ss.verify_package(
        ladder_challenge, ladder_submission, ladder_result_dir,
    )
    assert ladder_result.evidence["success"]
    assert ladder_result.evidence["verification"]["protocol"]["verdict_scope"] == "per_case"
    assert all(
        check.get("scope") == "acceptance_stratum"
        for check in ladder_result.evidence["policy"]["checks"]
    )
    ladder_html = open(os.path.join(ladder_result_dir, "index.html"), "r").read()
    assert "Per-case verdicts" in ladder_html
    assert "No case pooling or cross-case averaging was used." in ladder_html
    assert "Aggregate values are calculated" not in ladder_html
    assert "Case contribution breakdown" not in ladder_html
    assert all(case_id in ladder_html for case_id in ladder_cases)

    weak_ladder_submission = os.path.join(work, "weak_ladder_submission")
    shutil.copytree(ladder_submission, weak_ladder_submission)
    weak_output = next(
        output for output in ladder_manifest["outputs"]
        if output["case_id"] == "snr_m1" and output["target"] == "r_peak"
    )
    weak_path = output_path(weak_ladder_submission, weak_output)
    weak_document = read_json(weak_path)
    weak_document["events"] = weak_document["events"][::4]
    write_json(weak_path, weak_document)
    weak_ladder = ss.verify_package(
        ladder_challenge,
        weak_ladder_submission,
        os.path.join(work, "weak_ladder_result"),
    )
    assert not weak_ladder.evidence["success"]
    assert [
        item["stratum_id"]
        for item in weak_ladder.evidence["policy"]["acceptance_strata"]
        if not item["passed"]
    ] == ["snr_m1"]

    weak_submission = os.path.join(work, "weak_frontier_submission")
    shutil.copytree(frontier_submission, weak_submission)
    for output in frontier_manifest["outputs"]:
        if output["target"] != "r_peak" or output["case_id"] == "clean_anchor":
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
        "mixed_snr_m3", "mixed_snr_m4", "mixed_snr_m5",
        "mixed_snr_m7", "mixed_snr_m8", "mixed_snr_m9",
        "mixed_snr_m10", "mixed_snr_m11",
    ]
    index_html = open(
        os.path.join(weak_result_dir, "index.html"), "r"
    ).read()
    assert "Case contribution breakdown" in index_html
    assert all(case_id in index_html for case_id in expected_snr)

    evidence_challenge.close()
    frontier_challenge.close()
    simple_challenge.close()
    ladder_challenge.close()
    print("r_peak_evidence_pack_test=passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
