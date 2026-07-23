#!/usr/bin/env python3
"""Generate the paired, graded R-peak and RR noise-frontier scenarios."""

import argparse
import hashlib
import json
import pathlib
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]
OUTPUT_DIR = ROOT / "examples" / "scenarios" / "packs"
NOISE_ASSET = ROOT / "examples" / "assets" / "noise" / "synsigra_project_noise_v1.csv"
NOISE_SHA256 = "f19abdb3f3ff38a71e3d7fbad9543dcb218964c71cdd7fa2251a10e1f6bcd3cc"
LEADS = ["I", "II", "III", "aVR", "aVL", "aVF", "V1", "V2", "V3", "V4", "V5", "V6"]
TIERS = [
    (-3, 0.08, 0.02),
    (-4, 0.16, 0.04),
    (-5, 0.25, 0.07),
    (-7, 0.35, 0.10),
    (-8, 0.50, 0.18),
    (-9, 0.65, 0.26),
    (-10, 0.80, 0.34),
    (-11, 0.95, 0.42),
]


def noise_intervals(snr_db):
    channels = ["baseline_wander", "muscle", "electrode_motion"]
    offsets = [0.0, 0.25, 0.5, 0.75]
    return [
        {
            "asset_id": "synsigra_project_noise_v1",
            "asset_channel": channels[index % len(channels)],
            "start_seconds": 3.0 + 3.0 * index,
            "duration_seconds": 3.0,
            "asset_offset_seconds": offsets[index % len(offsets)],
            "target_snr_db": snr_db,
            "taper_seconds": 0.15,
            "clip_limit_mv": 0.0,
            "channels": LEADS,
        }
        for index in range(18)
    ]


def scenario(snr_db, baseline_severity, powerline_severity):
    tier = "m{}".format(abs(snr_db))
    return {
        "schema_version": 8,
        "scenario_id": "rpeak_noise_frontier_{}_v8".format(tier),
        "name": "R-peak and RR noise frontier {} dB SNR".format(snr_db),
        "description": (
            "Paired 60-second all-lead R-peak and RR stress case at {} dB target SNR. "
            "Calibrated baseline, muscle and electrode-motion segments are "
            "combined with persistent analytic baseline wander and powerline "
            "interference; more-negative tiers are harder."
        ).format(snr_db),
        "author": "Synsigra",
        "tags": [
            "r_peak", "rr_interval", "noise_frontier", "external_noise", "calibrated_snr",
            "baseline_wander", "powerline", "all_lead", "evidence",
        ],
        "duration_seconds": 60.0,
        "sample_rate_hz": 500,
        "seed": 930100,
        "ecg": {
            "heart_rate_bpm": 78.0,
            "rr_variability_seconds": 0.04,
            "ectopic_every_n_beats": 11,
            "second_degree_av_pattern": "unspecified",
            "q_wave_territory": "unspecified",
            "rhythm_episodes": [],
            "flutter_conduction_pattern": "fixed",
            "pacing_mode": "ventricular",
            "pacing_non_capture_every_n_beats": 0,
            "fidelity_policy": "allow_parameterized",
            "conditions": [{"code": "PVC", "severity": 0.7}],
        },
        "ppg": {
            "enabled": False,
            "pulse_delay_ms": 180.0,
            "rise_time_ms": 120.0,
            "decay_time_ms": 300.0,
            "amplitude_au": 1.0,
            "baseline_au": 0.0,
            "dicrotic_delay_ms": 180.0,
            "dicrotic_width_ms": 80.0,
            "dicrotic_amplitude_ratio": 0.15,
            "pulse_delay_variation_ms": 0.0,
            "pulse_delay_variation_hz": 0.0,
            "missing_pulse_every_n_beats": 0,
            "seed": 930200,
            "pulse_delay_jitter_ms": 0.0,
            "low_frequency_amplitude_modulation_ratio": 0.0,
            "low_frequency_amplitude_modulation_hz": 0.1,
            "rise_time_variation_ratio": 0.0,
            "decay_time_variation_ratio": 0.0,
            "pac_pulse_amplitude_scale": 1.0,
            "pvc_pulse_amplitude_scale": 1.0,
            "paced_pulse_amplitude_scale": 1.0,
            "perfusion_episodes": [],
            "optical": {
                "enabled": False,
                "profile_id": "custom",
                "calibration_id": "engineering_linear_v1",
                "calibration_intercept_percent": 110.0,
                "calibration_slope_percent": -25.0,
                "minimum_spo2_percent": 70.0,
                "maximum_spo2_percent": 100.0,
                "baseline_spo2_percent": 97.0,
                "infrared_perfusion_index_percent": 2.0,
                "red": {
                    "dc_au": 1.0, "sensor_gain": 1.0, "delay_ms": 8.0,
                    "noise_std_au": 0.0, "ambient_offset_au": 0.0,
                    "motion_sensitivity": 1.2, "ambient_sensitivity": 1.1,
                    "crosstalk_ratio": 0.0, "minimum_output_au": 0.0,
                    "maximum_output_au": 5.0, "quantization_bits": 0,
                    "seed": 930201,
                },
                "infrared": {
                    "dc_au": 1.2, "sensor_gain": 1.0, "delay_ms": 12.0,
                    "noise_std_au": 0.0, "ambient_offset_au": 0.0,
                    "motion_sensitivity": 0.8, "ambient_sensitivity": 0.7,
                    "crosstalk_ratio": 0.0, "minimum_output_au": 0.0,
                    "maximum_output_au": 5.0, "quantization_bits": 0,
                    "seed": 930202,
                },
                "oxygenation_episodes": [],
            },
        },
        "randomization": {"enabled": False, "seed": 930300, "envelopes": []},
        "physiology": {
            "respiration_frequency_hz": 0.25,
            "respiratory_rr_amplitude_seconds": 0.0,
            "ecg_baseline_amplitude_mv": 0.0,
            "ppg_amplitude_modulation_ratio": 0.0,
            "ppg_delay_modulation_ms": 0.0,
            "accelerometer_respiration_amplitude_g": 0.0,
            "activity_start_seconds": 0.0,
            "activity_duration_seconds": 0.0,
            "activity_intensity": 0.0,
            "seed": 930400,
        },
        "output": {
            "compact": False,
            "retain_source_channels": True,
            "include_waveform_csv": True,
            "include_edf_bdf": True,
        },
        "artifacts": [
            {
                "type": "ecg_baseline_wander",
                "start_seconds": 1.0,
                "duration_seconds": 58.0,
                "severity": baseline_severity,
                "seed": 930501,
                "channels": ["all_ecg"],
            },
            {
                "type": "ecg_powerline",
                "start_seconds": 0.0,
                "duration_seconds": 60.0,
                "severity": powerline_severity,
                "seed": 930502,
                "channels": ["all_ecg"],
            },
        ],
        "external_noise": {
            "assets": [
                {
                    "id": "synsigra_project_noise_v1",
                    "source_uri": "project://signal_synth/examples/assets/noise/synsigra_project_noise_v1.csv",
                    "license": "Synsigra project-owned CC0-1.0 test fixture",
                    "content_sha256": "sha256:" + NOISE_SHA256,
                    "sample_rate_hz": 50,
                    "channels": ["baseline_wander", "muscle", "electrode_motion"],
                    "redistribution": "source_and_output",
                }
            ],
            "intervals": noise_intervals(snr_db),
        },
    }


def encoded(document):
    return json.dumps(document, sort_keys=False, separators=(",", ":")) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check", action="store_true",
        help="Fail if committed scenarios differ from deterministic output.",
    )
    args = parser.parse_args()
    actual_hash = hashlib.sha256(NOISE_ASSET.read_bytes()).hexdigest()
    if actual_hash != NOISE_SHA256:
        raise RuntimeError("noise asset SHA-256 changed: {}".format(actual_hash))
    mismatches = []
    for snr_db, baseline_severity, powerline_severity in TIERS:
        path = OUTPUT_DIR / "rpeak_noise_frontier_m{}_v8.json".format(abs(snr_db))
        content = encoded(scenario(snr_db, baseline_severity, powerline_severity))
        if args.check:
            if not path.is_file() or path.read_text(encoding="utf-8") != content:
                mismatches.append(str(path.relative_to(ROOT)))
        else:
            path.write_text(content, encoding="utf-8")
    if mismatches:
        print("stale generated scenarios: {}".format(", ".join(mismatches)), file=sys.stderr)
        return 1
    print("r_peak_noise_frontier_scenarios={}".format("checked" if args.check else "generated"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
