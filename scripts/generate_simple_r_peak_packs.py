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
    (None, 0.98, 0.98),
    (-1, 0.95, 0.92),
    (-2, 0.93, 0.90),
    (-3, 0.90, 0.85),
    (-4, 0.85, 0.80),
    (-5, 0.78, 0.73),
    (-6, 0.74, 0.69),
    (-7, 0.70, 0.65),
    (-8, 0.65, 0.60),
    (-9, 0.62, 0.58),
    (-10, 0.60, 0.55),
    (-11, 0.55, 0.50),
]
STRESS_CASES = [
    ("clean_70", "../scenarios/packs/rpeak_clean_70.json", 0.98, 0.98, 0.02),
    ("slow_45", "../scenarios/packs/rpeak_slow_45.json", 0.98, 0.98, 0.02),
    ("fast_120", "../scenarios/packs/rpeak_fast_120.json", 0.98, 0.98, 0.02),
    ("baseline_powerline", "../scenarios/packs/sq_baseline_powerline.json", 0.80, 0.75, 0.04),
]


def case_id(snr_db):
    return "clean" if snr_db is None else "snr_m{}".format(abs(snr_db))


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


def profile(pack_id, name, description, f1, component, timing, rr_mae=0.025):
    rr_p95 = 0.06 if rr_mae > 0.025 else 0.05
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
                "overall": {
                    "prediction_match_fraction": {"min": component},
                    "status_match_fraction": {"min": 0.90},
                    "tolerance_pass_fraction": {"min": component},
                    "truth_match_fraction": {"min": component},
                },
                "rr_interval": {
                    "mean_absolute_error": {"max": rr_mae},
                    "p95_absolute_error": {"max": rr_p95},
                },
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
            "artifact_overlap": "Noise and artifacts remain scoreable because each complete case is the unit of evaluation.",
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
        for snr_db, _f1, _component in SNR_THRESHOLDS
    ]
    profiles = []
    for snr_db, f1, component in SNR_THRESHOLDS:
        label = "clean" if snr_db is None else "{} dB".format(snr_db)
        profiles.append(profile(
            pack_id,
            case_id(snr_db),
            "{} is judged independently over the complete 60-second signal; no bins or pooled cases.".format(label),
            f1,
            component,
            0.02 if snr_db is None else 0.04,
            0.030 if snr_db == -11 else 0.025,
        ))
    pack = {
        "schema_version": 2,
        "pack_id": pack_id,
        "name": "R-peak + RR Simple SNR Ladder",
        "version": "1.0",
        "description": "Independent clean and continuous-noise cases at every integer SNR from -1 through -11 dB. Each complete case has its own R-peak and RR verdict; nothing is pooled.",
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
        )
        for case_name, _path, f1, component, timing in STRESS_CASES
    ]
    pack = {
        "schema_version": 2,
        "pack_id": pack_id,
        "name": "R-peak + RR Simple Case Stress",
        "version": "1.0",
        "description": "Four independent clean, slow-rate, fast-rate, and baseline-plus-powerline cases. Each complete case has a plain R-peak and RR verdict; nothing is pooled.",
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
        for snr_db, _f1, _component in SNR_THRESHOLDS
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
