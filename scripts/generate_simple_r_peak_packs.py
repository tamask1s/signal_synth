#!/usr/bin/env python3
"""Generate the simple, independent-case R-peak and RR evidence packs."""

import argparse
import hashlib
import json
import pathlib
import sys

from generate_r_peak_noise_frontier import (
    LEADS,
    NOISE_ASSET,
    NOISE_SHA256,
    OUTPUT_DIR,
    ROOT,
    scenario as frontier_scenario,
)


PACK_DIR = ROOT / "examples" / "packs"
SNR_THRESHOLDS = [
    (None, 0.98, 0.98, 0.010),
    (-0.2, 0.97, 0.95, 0.015),
    (-0.5, 0.96, 0.94, 0.018),
    (-1, 0.95, 0.92, 0.020),
    (-2, 0.93, 0.90, 0.025),
    (-3, 0.90, 0.85, 0.030),
    (-4, 0.85, 0.80, 0.040),
    (-5, 0.78, 0.73, 0.055),
    (-6, 0.74, 0.69, 0.070),
    (-7, 0.70, 0.65, 0.085),
    (-8, 0.65, 0.60, 0.100),
    (-9, 0.62, 0.58, 0.120),
    (-10, 0.60, 0.55, 0.140),
    (-11, 0.55, 0.50, 0.160),
]
STRESS_CASES = [
    ("clean_70", "../scenarios/packs/rpeak_clean_70.json", 0.98, 0.98, 0.020, 0.025, 0.010, 0.98),
    ("slow_45", "../scenarios/packs/rpeak_slow_45.json", 0.98, 0.98, 0.020, 0.025, 0.010, 0.98),
    ("fast_120", "../scenarios/packs/rpeak_fast_120.json", 0.98, 0.98, 0.020, 0.025, 0.010, 0.98),
    ("baseline_powerline", "../scenarios/packs/sq_baseline_powerline.json", 0.80, 0.75, 0.040, 0.030, 0.015, 0.75),
    ("moderate_noise", "../scenarios/packs/rpeak_rr_moderate_noise_v2.json", 0.90, 0.85, 0.040, 0.040, 0.025, 0.80),
    ("variable_rate", "../scenarios/packs/rpeak_rr_variable_rate_v2.json", 0.92, 0.90, 0.030, 0.030, 0.020, 0.85),
    ("mobitz_ii_pauses", "../scenarios/packs/rpeak_rr_mobitz_ii_pauses_v2.json", 0.92, 0.90, 0.030, 0.035, 0.020, 0.85),
    ("combined_stress", "../scenarios/packs/rpeak_rr_combined_stress_v2.json", 0.78, 0.72, 0.050, 0.060, 0.040, 0.65),
]


def case_id(snr_db):
    if snr_db is None:
        return "clean"
    level = "{:g}".format(abs(snr_db)).replace(".", "p")
    return "snr_m{}".format(level)


def continuous_noise_intervals(snr_db):
    """Cover the complete record with adjacent, untapered calibrated noise."""
    channels = ["baseline_wander", "muscle", "electrode_motion"]
    offsets = [0.0, 0.25, 0.5, 0.75]
    return [
        {
            "asset_id": "synsigra_project_noise_v1",
            "asset_channel": channels[index % len(channels)],
            "start_seconds": 3.0 * index,
            "duration_seconds": 3.0,
            "asset_offset_seconds": offsets[index % len(offsets)],
            "target_snr_db": snr_db,
            "taper_seconds": 0.0,
            "clip_limit_mv": 0.0,
            "channels": LEADS,
        }
        for index in range(20)
    ]


def ladder_scenario(snr_db):
    document = frontier_scenario(-3, 0.0, 0.0)
    suffix = case_id(snr_db)
    label = "clean reference" if snr_db is None else "{} dB SNR".format(snr_db)
    document.update({
        "scenario_id": "rpeak_rr_snr_ladder_{}_v8".format(suffix),
        "name": "R-peak and RR simple SNR ladder — {}".format(label),
        "description": (
            "Paired 60-second R-peak and RR case with identical variable-rate, "
            "periodic-PVC cardiac truth. {}"
        ).format(
            "No noise is applied."
            if snr_db is None
            else "Calibrated external noise is present continuously at {} dB; its source type changes every three seconds without clean gaps.".format(snr_db)
        ),
        "tags": [
            "r_peak", "rr_interval", "snr_ladder", "per_case_verdict",
            "variable_rate", "pvc", "all_lead", "evidence",
        ] + ([] if snr_db is None else ["external_noise", "continuous_noise", "calibrated_snr"]),
        "artifacts": [],
    })
    if snr_db is None:
        document.pop("external_noise", None)
    else:
        document["external_noise"]["intervals"] = continuous_noise_intervals(snr_db)
    return document


def profile(
    pack_id, name, description, f1, component, timing,
    rr_error_limits, rr_tolerance_min=None,
):
    rr_overall = {
        "prediction_match_fraction": {"min": component},
        "status_match_fraction": {"min": 0.90},
        "truth_match_fraction": {"min": component},
    }
    if rr_tolerance_min is not None:
        rr_overall["tolerance_pass_fraction"] = {"min": rr_tolerance_min}
    return {
        "schema_version": 1,
        "profile_id": "{}_{}".format(pack_id, name),
        "description": description,
        "targets": {
            "r_peak": {
                "total": {
                    "f1_score": {"min": f1},
                    "mean_absolute_error_seconds": {"max": timing},
                    "positive_predictive_value": {"min": component},
                    "sensitivity": {"min": component},
                }
            },
            "rr_interval": {
                "overall": rr_overall,
                "rr_interval": rr_error_limits,
            },
        },
    }


def protocol(pack_id, context, cases, profiles, truth_scope):
    return {
        "schema_version": 2,
        "contract": "synsigra_verification_protocol_v2",
        "protocol_id": pack_id,
        "pack_id": pack_id,
        "context_of_use": context,
        "scoring_contract": "synsigra_local_verification_v3",
        "verdict_scope": "per_case",
        "required_case_targets": [
            {"case_id": item["id"], "targets": ["r_peak", "rr_interval"]}
            for item in cases
        ],
        "acceptance_strata": [
            {
                "id": item["id"],
                "case_ids": [item["id"]],
                "acceptance_profile": case_profile,
            }
            for item, case_profile in zip(cases, profiles)
        ],
        "stress_strata": [
            {"id": item["id"], "case_ids": [item["id"]]}
            for item in cases
        ],
        "truth_policy": {
            "event_scoreability": "Complete in-record constructed QRS events are scoreable; every exclusion is reported.",
            "rr_definition": "Difference between consecutive scoreable construction R-peak times; the first beat has no in-record RR value.",
            "rr_association": "Valid RR intervals are anchored by their ending R-peak time and associated when their overlap covers more than half of the shorter interval. FP splits and FN merges create local comparison pairs; unique interval coverage cannot exceed 100%.",
            "rr_statistics_boundary": "RR median is the robust ladder gate. RR tolerance percentage, MAE and P95 remain visible diagnostics. This is a Synsigra engineering policy, not an ANSI/AAMI EC57 RR requirement.",
            "artifact_overlap": "Noise and artifacts remain scoreable because each complete case is the unit of evaluation.",
            "event_reporting_basis": "R-peak TP, FP, FN, sensitivity and positive predictive value use a 150 ms closest-event window, comparable to the default WFDB bxb reporting convention. Synsigra does not claim bxb identity or EC57 conformity.",
            "literature_context": "Thresholds are pre-specified for this exact deterministic synthetic protocol, informed by PhysioNet MIT-BIH Noise Stress Test Database and peer-reviewed noisy-QRS benchmarks; SNR labels are not directly interchangeable across protocols.",
            "scope": truth_scope,
            "boundary": "Only construction events crossing the record boundary are excluded; no blanket edge window is used.",
        },
        "evidence_boundary": "Synthetic engineering QA evidence for R-peak detection and beat-to-beat RR measurement; not diagnosis, clinical validation, or regulatory qualification.",
    }


def ladder_documents():
    pack_id = "r_peak_rr_snr_ladder_v1"
    cases = [
        {
            "id": case_id(snr_db),
            "path": "../scenarios/packs/rpeak_rr_snr_ladder_{}_v8.json".format(case_id(snr_db)),
            "targets": ["r_peak", "rr_interval"],
        }
        for snr_db, _f1, _component, _rr_median in SNR_THRESHOLDS
    ]
    profiles = []
    for snr_db, f1, component, rr_median in SNR_THRESHOLDS:
        label = "clean" if snr_db is None else "{} dB".format(snr_db)
        profiles.append(profile(
            pack_id,
            case_id(snr_db),
            "{} is judged independently over the complete 60-second signal; no bins or pooled cases.".format(label),
            f1,
            component,
            0.02 if snr_db is None else 0.04,
            {"median_absolute_error": {"max": rr_median}},
        ))
    pack = {
        "schema_version": 2,
        "pack_id": pack_id,
        "name": "R-peak + RR Simple SNR Ladder",
        "version": "2.0",
        "description": "Independent clean and continuous-noise cases at -0.2, -0.5, and every integer SNR from -1 through -11 dB. Each complete case has its own R-peak and RR verdict; nothing is pooled.",
        "verification_protocol_path": "{}_expectations.json".format(pack_id),
        "targets": ["r_peak", "rr_interval"],
        "scenarios": cases,
    }
    expectations = protocol(
        pack_id,
        "Simple synthetic engineering comparison of R-peak and RR robustness across independent clean and continuous-noise SNR cases.",
        cases,
        profiles,
        "R-peak event and RR measurement files are required; signal quality, HRV, classification, and delineation are outside the evidence matrix.",
    )
    return pack, expectations


def stress_documents():
    pack_id = "r_peak_rr_simple_stress_v1"
    cases = [
        {"id": item[0], "path": item[1], "targets": ["r_peak", "rr_interval"]}
        for item in STRESS_CASES
    ]
    profiles = [
        profile(
            pack_id,
            case_name,
            "{} is judged independently over its complete signal; no bins or pooled cases.".format(case_name.replace("_", " ")),
            f1,
            component,
            timing,
            {
                "mean_absolute_error": {"max": rr_mae},
                "median_absolute_error": {"max": rr_median},
            },
            rr_tolerance,
        )
        for (
            case_name, _path, f1, component, timing, rr_mae,
            rr_median, rr_tolerance,
        ) in STRESS_CASES
    ]
    pack = {
        "schema_version": 2,
        "pack_id": pack_id,
        "name": "R-peak + RR Simple Case Stress",
        "version": "2.0",
        "description": "Eight independent clean, rate-boundary, variable-rate, physiologic-pause, moderate-noise, and combined-stress cases. Each complete case has a plain R-peak and RR verdict; nothing is pooled.",
        "verification_protocol_path": "{}_expectations.json".format(pack_id),
        "targets": ["r_peak", "rr_interval"],
        "scenarios": cases,
    }
    expectations = protocol(
        pack_id,
        "Simple synthetic engineering evidence for R-peak detection and RR measurement across independent rate and acquisition-stress cases.",
        cases,
        profiles,
        "R-peak event and RR measurement files are required; no signal-quality output or claim is requested.",
    )
    return pack, expectations


def encoded(document):
    return json.dumps(document, indent=2, sort_keys=False) + "\n"


def ppg_disabled():
    return {
        "enabled": False,
        "pulse_delay_ms": 180,
        "rise_time_ms": 120,
        "decay_time_ms": 300,
        "amplitude_au": 1,
        "baseline_au": 0,
        "dicrotic_delay_ms": 180,
        "dicrotic_width_ms": 80,
        "dicrotic_amplitude_ratio": 0.15,
    }


def stress_scenario(
    scenario_id, name, description, duration, seed, ecg, artifacts=None,
):
    return {
        "schema_version": 2,
        "scenario_id": scenario_id,
        "name": name,
        "description": description,
        "author": "Synsigra",
        "tags": [
            "r_peak", "rr_interval", "simple_stress", "per_case_verdict",
        ],
        "duration_seconds": duration,
        "sample_rate_hz": 500,
        "seed": seed,
        "ecg": ecg,
        "ppg": ppg_disabled(),
        "artifacts": artifacts or [],
    }


def additional_stress_scenarios():
    common = {
        "q_wave_territory": "unspecified",
        "flutter_conduction_pattern": "fixed",
        "pacing_mode": "ventricular",
        "pacing_non_capture_every_n_beats": 0,
        "fidelity_policy": "allow_parameterized",
    }
    moderate_ecg = dict(common, **{
        "heart_rate_bpm": 76,
        "rr_variability_seconds": 0.035,
        "ectopic_every_n_beats": 13,
        "second_degree_av_pattern": "unspecified",
        "rhythm_episodes": [],
        "conditions": [{"code": "PVC", "severity": 0.55}],
    })
    variable_ecg = dict(common, **{
        "heart_rate_bpm": 65,
        "rr_variability_seconds": 0,
        "ectopic_every_n_beats": 0,
        "second_degree_av_pattern": "unspecified",
        "rhythm_episodes": [
            {"type": "afib", "start_seconds": 7, "duration_seconds": 12, "transition_seconds": 0.3, "rate_bpm": 95, "seed": 941211},
            {"type": "psvt", "start_seconds": 26, "duration_seconds": 9, "transition_seconds": 0.25, "rate_bpm": 135, "seed": 941212},
            {"type": "afib", "start_seconds": 42, "duration_seconds": 11, "transition_seconds": 0.3, "rate_bpm": 105, "seed": 941213},
        ],
        "conditions": [{"code": "SR", "severity": 1}],
    })
    pause_ecg = dict(common, **{
        "heart_rate_bpm": 72,
        "rr_variability_seconds": 0,
        "ectopic_every_n_beats": 0,
        "second_degree_av_pattern": "mobitz_ii",
        "rhythm_episodes": [],
        "conditions": [{"code": "2AVB", "severity": 1}],
    })
    combined_ecg = dict(common, **{
        "heart_rate_bpm": 78,
        "rr_variability_seconds": 0,
        "ectopic_every_n_beats": 0,
        "second_degree_av_pattern": "unspecified",
        "rhythm_episodes": [
            {"type": "afib", "start_seconds": 12, "duration_seconds": 12, "transition_seconds": 0.3, "rate_bpm": 105, "seed": 941411},
            {"type": "psvt", "start_seconds": 38, "duration_seconds": 8, "transition_seconds": 0.25, "rate_bpm": 145, "seed": 941412},
        ],
        "conditions": [{"code": "SR", "severity": 1}],
    })
    return [
        stress_scenario(
            "rpeak_rr_moderate_noise_v2",
            "R-peak and RR moderate mixed noise",
            "Moderate full-record baseline and powerline interference with two EMG bursts, variable RR, and periodic PVCs.",
            45, 941100, moderate_ecg,
            [
                {"type": "ecg_baseline_wander", "start_seconds": 0, "duration_seconds": 45, "severity": 0.38, "seed": 941111, "channels": ["all_ecg"]},
                {"type": "ecg_powerline", "start_seconds": 0, "duration_seconds": 45, "severity": 0.20, "seed": 941112, "channels": ["all_ecg"]},
                {"type": "ecg_emg_noise", "start_seconds": 8, "duration_seconds": 7, "severity": 0.42, "seed": 941113, "channels": ["all_ecg"]},
                {"type": "ecg_emg_noise", "start_seconds": 29, "duration_seconds": 8, "severity": 0.48, "seed": 941114, "channels": ["all_ecg"]},
            ],
        ),
        stress_scenario(
            "rpeak_rr_variable_rate_v2",
            "R-peak and RR highly variable rate",
            "Sinus baseline with irregular AF episodes, a 135 bpm PSVT episode, deterministic transitions, and no acquisition artifact.",
            60, 941200, variable_ecg,
        ),
        stress_scenario(
            "rpeak_rr_mobitz_ii_pauses_v2",
            "R-peak and RR physiologic dropped-QRS pauses",
            "Mobitz II conduction creates deterministic non-conducted atrial events. No R-peak truth exists where no QRS is generated; the surrounding long RR interval remains scoreable.",
            45, 941300, pause_ecg,
        ),
        stress_scenario(
            "rpeak_rr_combined_stress_v2",
            "R-peak and RR combined stress",
            "Sinus, AF and PSVT rate changes, baseline wander, powerline interference, and EMG bursts in one independent case.",
            60, 941400, combined_ecg,
            [
                {"type": "ecg_baseline_wander", "start_seconds": 0, "duration_seconds": 60, "severity": 0.50, "seed": 941421, "channels": ["all_ecg"]},
                {"type": "ecg_powerline", "start_seconds": 0, "duration_seconds": 60, "severity": 0.25, "seed": 941422, "channels": ["all_ecg"]},
                {"type": "ecg_emg_noise", "start_seconds": 6, "duration_seconds": 9, "severity": 0.50, "seed": 941423, "channels": ["all_ecg"]},
                {"type": "ecg_emg_noise", "start_seconds": 31, "duration_seconds": 12, "severity": 0.58, "seed": 941424, "channels": ["all_ecg"]},
            ],
        ),
    ]


def outputs():
    ladder_pack, ladder_protocol = ladder_documents()
    stress_pack, stress_protocol = stress_documents()
    documents = [
        (PACK_DIR / "r_peak_rr_snr_ladder_v1.json", ladder_pack),
        (PACK_DIR / "r_peak_rr_snr_ladder_v1_expectations.json", ladder_protocol),
        (PACK_DIR / "r_peak_rr_simple_stress_v1.json", stress_pack),
        (PACK_DIR / "r_peak_rr_simple_stress_v1_expectations.json", stress_protocol),
    ]
    documents.extend(
        (
            OUTPUT_DIR / "rpeak_rr_snr_ladder_{}_v8.json".format(case_id(snr_db)),
            ladder_scenario(snr_db),
        )
        for snr_db, _f1, _component, _rr_median in SNR_THRESHOLDS
    )
    documents.extend(
        (OUTPUT_DIR / "{}.json".format(document["scenario_id"]), document)
        for document in additional_stress_scenarios()
    )
    return documents


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Fail if committed output is stale.")
    args = parser.parse_args()
    actual_hash = hashlib.sha256(NOISE_ASSET.read_bytes()).hexdigest()
    if actual_hash != NOISE_SHA256:
        raise RuntimeError("noise asset SHA-256 changed: {}".format(actual_hash))
    stale = []
    for path, document in outputs():
        content = encoded(document)
        if args.check:
            if not path.is_file() or path.read_text(encoding="utf-8") != content:
                stale.append(str(path.relative_to(ROOT)))
        else:
            path.write_text(content, encoding="utf-8")
    if stale:
        print("stale generated files: {}".format(", ".join(stale)), file=sys.stderr)
        return 1
    print("simple_r_peak_packs={}".format("checked" if args.check else "generated"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
