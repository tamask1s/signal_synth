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
            "ecg_beat_classification": {"summary": {"micro_f1_score": {"min": 0.70}}, "per_class": {"f1_score": {"min": 0.50}}},
            "hrv": {"summary": {"metric_pass_fraction": {"min": 0.70}}, "rr": {"pass_fraction": {"min": 0.70}}},
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
            "ecg_beat_classification": {"summary": {"micro_f1_score": {"min": 0.90}}, "per_class": {"f1_score": {"min": 0.80}}},
            "hrv": {"summary": {"metric_pass_fraction": {"min": 0.90}}, "rr": {"pass_fraction": {"min": 0.90}}},
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
            "ecg_beat_classification": {"summary": {"micro_f1_score": {"min": 0.75}}, "per_class": {"f1_score": {"min": 0.60}}},
            "hrv": {"summary": {"metric_pass_fraction": {"min": 0.75}}, "rr": {"pass_fraction": {"min": 0.75}}},
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
            "ecg_beat_classification": {"summary": {"micro_f1_score": {"min": 0.95}}, "per_class": {"f1_score": {"min": 0.90}}},
            "hrv": {"summary": {"metric_pass_fraction": {"min": 1.0}}, "rr": {"pass_fraction": {"min": 0.95}}},
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
    "hrv": {
        "summary": set(["metric_pass_fraction"]),
        "rr": set(["pass_fraction", "mean_absolute_error_seconds", "rms_error_seconds", "max_absolute_error_seconds"]),
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
    if not isinstance(document.get("profile_id"), str) or not document["profile_id"]:
        raise ThresholdProfileError("threshold profile_id must be a non-empty string")
    targets = document.get("targets")
    if not isinstance(targets, dict) or not targets:
        raise ThresholdProfileError("threshold profile targets must be a non-empty object")
    for target_name, sections in targets.items():
        if target_name not in PROFILE_SCHEMA:
            raise ThresholdProfileError("unknown threshold target: %s" % target_name)
        if not isinstance(sections, dict):
            raise ThresholdProfileError("threshold target %s must be an object" % target_name)
        for section_name, metrics in sections.items():
            if section_name not in PROFILE_SCHEMA[target_name]:
                raise ThresholdProfileError("unknown threshold section: %s/%s" % (target_name, section_name))
            if not isinstance(metrics, dict):
                raise ThresholdProfileError("threshold section %s/%s must be an object" % (target_name, section_name))
            for metric_name, limits in metrics.items():
                if metric_name not in PROFILE_SCHEMA[target_name][section_name]:
                    raise ThresholdProfileError("unknown threshold metric: %s/%s/%s" % (target_name, section_name, metric_name))
                if not isinstance(limits, dict) or not limits or any(name not in ("min", "max") for name in limits):
                    raise ThresholdProfileError("threshold %s/%s/%s must define min and/or max" % (target_name, section_name, metric_name))
                for operator, value in limits.items():
                    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(float(value)):
                        raise ThresholdProfileError("threshold %s/%s/%s/%s must be finite" % (target_name, section_name, metric_name, operator))
