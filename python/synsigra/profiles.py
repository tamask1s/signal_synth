import copy
import json
import math
import os


class ThresholdProfileError(ValueError):
    pass


def _event_thresholds(total_f1, clean_f1, artifact_f1, max_mae):
    return {
        "total": {"f1_score": {"min": total_f1}, "mean_absolute_error_seconds": {"max": max_mae}},
        "clean": {"f1_score": {"min": clean_f1}},
        "artifact": {"f1_score": {"min": artifact_f1}},
    }


def _interval_thresholds(time_f1, temporal_iou, max_boundary_mae):
    return {"overall": {"time_f1_score": {"min": time_f1}, "temporal_iou": {"min": temporal_iou}, "mean_absolute_onset_error_seconds": {"max": max_boundary_mae}, "mean_absolute_offset_error_seconds": {"max": max_boundary_mae}}}


def _delineation_thresholds(f1_score, max_mae):
    return {"overall": {"f1_score": {"min": f1_score}, "mean_absolute_error_seconds": {"max": max_mae}}}


def _measurement_thresholds(pass_fraction, status_fraction):
    return {"overall": {"truth_match_fraction": {"min": pass_fraction}, "prediction_match_fraction": {"min": pass_fraction}, "tolerance_pass_fraction": {"min": pass_fraction}, "status_match_fraction": {"min": status_fraction}}}


def _timing_measurement_thresholds(pass_fraction, status_fraction, error_limits):
    output = _measurement_thresholds(pass_fraction, status_fraction)
    for name, maximum_error in error_limits.items():
        output[name] = {
            "truth_match_fraction": {"min": pass_fraction},
            "prediction_match_fraction": {"min": pass_fraction},
            "tolerance_pass_fraction": {"min": pass_fraction},
            "status_match_fraction": {"min": status_fraction},
            "mean_absolute_error": {"max": maximum_error},
            "p95_absolute_error": {"max": maximum_error * 2.0},
        }
    return output


HRV_MEASUREMENTS = [
    "rr_interval", "mean_rr_seconds", "mean_heart_rate_bpm", "sdnn_seconds", "rmssd_seconds", "pnn50_percent", "sd1_seconds", "sd2_seconds", "sd1_sd2_ratio",
    "vlf_power_seconds2", "lf_power_seconds2", "hf_power_seconds2", "lf_hf_ratio", "lf_normalized_units", "hf_normalized_units", "total_power_seconds2",
]


def _hrv_measurement_thresholds(pass_fraction, status_fraction):
    output = _measurement_thresholds(pass_fraction, status_fraction)
    for name in HRV_MEASUREMENTS:
        output[name] = {
            "truth_match_fraction": {"min": pass_fraction},
            "prediction_match_fraction": {"min": pass_fraction},
            "tolerance_pass_fraction": {"min": pass_fraction},
            "status_match_fraction": {"min": status_fraction},
        }
    return output


BUILTIN_THRESHOLD_PROFILES = {
    "smoke": {
        "schema_version": 1,
        "profile_id": "smoke",
        "description": "Basic integration gate with deliberately permissive quality thresholds.",
        "targets": {
            "event_detection": _event_thresholds(0.70, 0.75, 0.50, 0.080),
            "interval_detection": _interval_thresholds(0.60, 0.40, 0.500),
            "ecg_delineation": _delineation_thresholds(0.60, 0.080),
            "measurement": _measurement_thresholds(0.70, 0.70),
            "rr_interval": _timing_measurement_thresholds(0.70, 0.70, {"rr_interval": 0.025}),
            "qtc": _timing_measurement_thresholds(0.70, 0.70, {"rr_interval": 0.025, "qt_interval": 0.035, "qtc_interval": 0.035}),
            "ecg_beat_classification": {"summary": {"micro_f1_score": {"min": 0.70}}, "per_class": {"f1_score": {"min": 0.50}}},
            "hrv": _hrv_measurement_thresholds(0.70, 0.70),
        },
    },
    "regression": {
        "schema_version": 1,
        "profile_id": "regression",
        "description": "Default release regression gate for established algorithms.",
        "targets": {
            "event_detection": _event_thresholds(0.90, 0.95, 0.70, 0.050),
            "interval_detection": _interval_thresholds(0.85, 0.75, 0.200),
            "ecg_delineation": _delineation_thresholds(0.85, 0.040),
            "measurement": _measurement_thresholds(0.90, 0.90),
            "rr_interval": _timing_measurement_thresholds(0.90, 0.90, {"rr_interval": 0.015}),
            "qtc": _timing_measurement_thresholds(0.90, 0.90, {"rr_interval": 0.015, "qt_interval": 0.025, "qtc_interval": 0.025}),
            "ecg_beat_classification": {"summary": {"micro_f1_score": {"min": 0.90}}, "per_class": {"f1_score": {"min": 0.80}}},
            "hrv": _hrv_measurement_thresholds(0.90, 0.90),
        },
    },
    "stress": {
        "schema_version": 1,
        "profile_id": "stress",
        "description": "Gate for deliberately difficult artifact and transition scenarios.",
        "targets": {
            "event_detection": _event_thresholds(0.75, 0.90, 0.60, 0.080),
            "interval_detection": _interval_thresholds(0.65, 0.50, 0.400),
            "ecg_delineation": _delineation_thresholds(0.65, 0.080),
            "measurement": _measurement_thresholds(0.75, 0.75),
            "rr_interval": _timing_measurement_thresholds(0.75, 0.75, {"rr_interval": 0.025}),
            "qtc": _timing_measurement_thresholds(0.75, 0.75, {"rr_interval": 0.025, "qt_interval": 0.035, "qtc_interval": 0.035}),
            "ecg_beat_classification": {"summary": {"micro_f1_score": {"min": 0.75}}, "per_class": {"f1_score": {"min": 0.60}}},
            "hrv": _hrv_measurement_thresholds(0.75, 0.75),
        },
    },
    "benchmark": {
        "schema_version": 1,
        "profile_id": "benchmark",
        "description": "Strict comparison profile for controlled benchmark releases.",
        "targets": {
            "event_detection": _event_thresholds(0.95, 0.98, 0.80, 0.030),
            "interval_detection": _interval_thresholds(0.95, 0.90, 0.100),
            "ecg_delineation": _delineation_thresholds(0.95, 0.020),
            "measurement": _measurement_thresholds(0.95, 0.95),
            "rr_interval": _timing_measurement_thresholds(0.95, 0.95, {"rr_interval": 0.010}),
            "qtc": _timing_measurement_thresholds(0.95, 0.95, {"rr_interval": 0.010, "qt_interval": 0.015, "qtc_interval": 0.015}),
            "ecg_beat_classification": {"summary": {"micro_f1_score": {"min": 0.95}}, "per_class": {"f1_score": {"min": 0.90}}},
            "hrv": _hrv_measurement_thresholds(0.95, 0.95),
        },
    },
}

PROFILE_SCHEMA = {
    "event_detection": {
        "total": set(["sensitivity", "positive_predictive_value", "f1_score", "mean_absolute_error_seconds", "median_absolute_error_seconds", "rms_error_seconds", "max_absolute_error_seconds"]),
        "clean": set(["sensitivity", "positive_predictive_value", "f1_score", "mean_absolute_error_seconds", "median_absolute_error_seconds", "rms_error_seconds", "max_absolute_error_seconds"]),
        "artifact": set(["sensitivity", "positive_predictive_value", "f1_score", "mean_absolute_error_seconds", "median_absolute_error_seconds", "rms_error_seconds", "max_absolute_error_seconds"]),
    },
    "ecg_beat_classification": {
        "summary": set(["accuracy", "micro_precision", "micro_recall", "micro_f1_score"]),
        "per_class": set(["precision", "recall", "f1_score"]),
    },
    "interval_detection": {
        "overall": set(["time_sensitivity", "time_precision", "time_f1_score", "temporal_iou", "event_sensitivity", "event_precision", "false_alarms_per_hour", "mean_absolute_onset_error_seconds", "mean_absolute_offset_error_seconds"]),
    },
    "ecg_delineation": {
        "overall": set(["sensitivity", "positive_predictive_value", "f1_score", "within_tolerance_fraction", "mean_absolute_error_seconds", "p95_absolute_error_seconds"]),
    },
    "measurement": {
        "overall": set(["truth_match_fraction", "prediction_match_fraction", "tolerance_pass_fraction", "status_match_fraction", "assertion_agreement_fraction"]),
    },
}
PROFILE_SCHEMA["r_peak"] = PROFILE_SCHEMA["event_detection"]
PROFILE_SCHEMA["ppg_systolic_peak"] = PROFILE_SCHEMA["event_detection"]
PROFILE_SCHEMA["ppg_pulse_onset"] = PROFILE_SCHEMA["event_detection"]
PROFILE_SCHEMA["rhythm_episode"] = PROFILE_SCHEMA["interval_detection"]
PROFILE_SCHEMA["signal_quality"] = PROFILE_SCHEMA["interval_detection"]
PROFILE_SCHEMA["morphology_assertions"] = PROFILE_SCHEMA["measurement"]
PROFILE_SCHEMA["ecg_ppg_alignment"] = PROFILE_SCHEMA["measurement"]
PROFILE_SCHEMA["ppg_optical"] = PROFILE_SCHEMA["measurement"]
PROFILE_SCHEMA["prv"] = PROFILE_SCHEMA["measurement"]
PROFILE_SCHEMA["respiratory_rate"] = PROFILE_SCHEMA["measurement"]
PROFILE_SCHEMA["rhythm_burden"] = PROFILE_SCHEMA["measurement"]

TIMING_MEASUREMENT_METRICS = set(["truth_match_fraction", "prediction_match_fraction", "tolerance_pass_fraction", "status_match_fraction", "assertion_agreement_fraction", "mean_absolute_error", "root_mean_square_error", "p95_absolute_error", "maximum_absolute_error"])
PROFILE_SCHEMA["rr_interval"] = {"overall": PROFILE_SCHEMA["measurement"]["overall"], "rr_interval": TIMING_MEASUREMENT_METRICS}
PROFILE_SCHEMA["qtc"] = {"overall": PROFILE_SCHEMA["measurement"]["overall"], "rr_interval": TIMING_MEASUREMENT_METRICS, "qt_interval": TIMING_MEASUREMENT_METRICS, "qtc_interval": TIMING_MEASUREMENT_METRICS}
PROFILE_SCHEMA["hrv"] = {"overall": PROFILE_SCHEMA["measurement"]["overall"]}
for _hrv_measurement in HRV_MEASUREMENTS:
    PROFILE_SCHEMA["hrv"][_hrv_measurement] = TIMING_MEASUREMENT_METRICS


def load_threshold_profile(profile="regression"):
    if profile is None:
        profile = "regression"
    if isinstance(profile, dict):
        document = copy.deepcopy(profile)
    elif profile in BUILTIN_THRESHOLD_PROFILES:
        document = copy.deepcopy(BUILTIN_THRESHOLD_PROFILES[profile])
    elif isinstance(profile, str) and os.path.isfile(profile):
        with open(profile, "r") as handle:
            document = json.load(handle)
    else:
        raise ThresholdProfileError("unknown threshold profile: %s" % profile)
    _validate_profile(document)
    return document


def threshold_profile_names():
    return sorted(BUILTIN_THRESHOLD_PROFILES.keys())


def _validate_profile(document):
    if not isinstance(document, dict) or document.get("schema_version") != 1:
        raise ThresholdProfileError("threshold profile must be a schema_version 1 JSON object")
    allowed_fields = set(["schema_version", "profile_id", "description", "targets"])
    unknown_fields = sorted(set(document) - allowed_fields)
    if unknown_fields:
        raise ThresholdProfileError("threshold profile contains unknown fields: %s" % ", ".join(unknown_fields))
    if not isinstance(document.get("profile_id"), str) or not document["profile_id"]:
        raise ThresholdProfileError("threshold profile_id must be a non-empty string")
    if "description" in document and not isinstance(document["description"], str):
        raise ThresholdProfileError("threshold profile description must be a string")
    targets = document.get("targets")
    if not isinstance(targets, dict) or not targets:
        raise ThresholdProfileError("threshold profile targets must be a non-empty object")
    for target_name, sections in targets.items():
        if target_name not in PROFILE_SCHEMA:
            raise ThresholdProfileError("unknown threshold target: %s" % target_name)
        if not isinstance(sections, dict) or not sections:
            raise ThresholdProfileError("threshold target %s must be a non-empty object" % target_name)
        for section_name, metrics in sections.items():
            if section_name not in PROFILE_SCHEMA[target_name]:
                raise ThresholdProfileError("unknown threshold section: %s/%s" % (target_name, section_name))
            if not isinstance(metrics, dict) or not metrics:
                raise ThresholdProfileError("threshold section %s/%s must be a non-empty object" % (target_name, section_name))
            for metric_name, limits in metrics.items():
                if metric_name not in PROFILE_SCHEMA[target_name][section_name]:
                    raise ThresholdProfileError("unknown threshold metric: %s/%s/%s" % (target_name, section_name, metric_name))
                if not isinstance(limits, dict) or not limits or any(name not in ("min", "max") for name in limits):
                    raise ThresholdProfileError("threshold %s/%s/%s must define min and/or max" % (target_name, section_name, metric_name))
                for operator, value in limits.items():
                    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(float(value)):
                        raise ThresholdProfileError("threshold %s/%s/%s/%s must be finite" % (target_name, section_name, metric_name, operator))
                if "min" in limits and "max" in limits and float(limits["min"]) > float(limits["max"]):
                    raise ThresholdProfileError("threshold %s/%s/%s has min greater than max" % (target_name, section_name, metric_name))
