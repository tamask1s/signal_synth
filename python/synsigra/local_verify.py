import csv
import hashlib
import html
import io
import json
import math
import os
import shlex
import shutil

from .challenge import ChallengePackage, load_challenge
from .detections import DetectionDocument, load_detections
from .profiles import load_threshold_profile


SCORING_VERSION = "synsigra-python-local-v1"
LIMITATION_TEXT = "Synthetic engineering QA evidence; not diagnosis, patient monitoring, clinical validation certification, or standalone conformity assessment."
BEAT_CLASSES = ["normal", "supraventricular_ectopic", "ventricular_ectopic", "paced", "escape", "unscored"]
SCORED_BEAT_CLASSES = set(["normal", "supraventricular_ectopic", "ventricular_ectopic", "paced", "escape"])
HRV_METRICS = [
    "mean_rr_seconds", "mean_heart_rate_bpm", "sdnn_seconds", "rmssd_seconds",
    "pnn50_percent", "sd1_seconds", "sd2_seconds", "sd1_sd2_ratio",
    "lf_power_seconds2", "hf_power_seconds2", "lf_hf_ratio", "total_power_seconds2",
]


class VerificationError(ValueError):
    pass


class VerificationReport(object):
    def __init__(self, output_dir, summary, json_report="", csv_report="", html_report=""):
        self.output_dir = output_dir
        self.summary = summary
        self.json = summary
        self.json_report = json_report
        self.csv = csv_report
        self.html = html_report


class _TruthEvent(object):
    def __init__(self, index, time_seconds, label="", in_artifact_interval=False, low_perfusion=False, weak_pulse=False, missing_pulse_window=False):
        self.index = int(index)
        self.time_seconds = float(time_seconds)
        self.label = label
        self.in_artifact_interval = bool(in_artifact_interval)
        self.low_perfusion = bool(low_perfusion)
        self.weak_pulse = bool(weak_pulse)
        self.missing_pulse_window = bool(missing_pulse_window)


def verify_package(challenge, detections_dir, out_dir, cases=None, targets=None, profile="regression", force=False):
    """Verify user detections against a Synsigra challenge package locally.

    This verifier uses only challenge package contents: manifest metadata,
    annotations, case summaries, and user detection output files. It does not
    invoke the signal generator or require generator source code.
    """
    package, owned = _as_package(challenge)
    try:
        if not os.path.isdir(detections_dir):
            raise VerificationError("detections directory does not exist: %s" % detections_dir)
        integrity = package.verify_integrity()
        scoring_manifest = package.scoring_manifest()
        threshold_profile = load_threshold_profile(profile)
        _prepare_output_dir(out_dir, force)
        selected_cases = _normalize_filter(cases)
        selected_targets = _normalize_filter(targets)
        results = []
        messages = []

        for case in package.cases:
            if selected_cases is not None and case.id not in selected_cases:
                continue
            case_summary = case.case_summary()
            annotations = case.annotations()
            entries = _case_scoring_entries(scoring_manifest, case.id, case_summary)
            for entry in entries:
                target = entry.get("target", "")
                if selected_targets is not None and target not in selected_targets:
                    continue
                if not entry.get("supported", False):
                    results.append(_unsupported_result(case, case_summary, entry, out_dir))
                    continue
                if target not in ("r_peak", "ppg_systolic_peak", "ecg_beat_classification", "hrv"):
                    results.append(_unsupported_result(case, case_summary, entry, out_dir))
                    continue
                result = _verify_case_target(package, case, case_summary, annotations, entry, detections_dir, out_dir)
                results.append(result)

        if not results:
            messages.append("no scoreable case-target pairs matched the selected filters")

        summary = _build_verification_summary(package, scoring_manifest, integrity, results, messages, threshold_profile)
        json_path = os.path.join(out_dir, "verification_summary.json")
        csv_path = os.path.join(out_dir, "verification_summary.csv")
        html_path = os.path.join(out_dir, "verification_report.html")
        _write_json(json_path, summary)
        _write_text(csv_path, _summary_csv(summary))
        _write_text(html_path, _summary_html(summary))
        return VerificationReport(out_dir, summary, json_path, csv_path, html_path)
    finally:
        if owned:
            package.close()


def _verify_case_target(package, case, case_summary, annotations, entry, detections_dir, out_dir):
    target = entry.get("target", "")
    relative_out = _target_output_relative_path(entry, case.id, case_summary.get("targets", []), target)
    target_dir = os.path.join(out_dir, relative_out)
    _ensure_dir(target_dir)
    detection_path = _find_detection_file(detections_dir, entry, case.id, case_summary.get("targets", []), target)
    if detection_path is None:
        return _error_result(package, case, case_summary, entry, target_dir, "missing_detection", "no detection file found for %s/%s" % (case.id, target))
    try:
        if target == "hrv":
            user_output = _load_hrv_user_output(detection_path)
            report_json = _score_hrv(package, case, case_summary, user_output)
            report_csv = _hrv_csv(report_json)
            report_html = _hrv_html(report_json)
            input_identity = {
                "path": detection_path,
                "format": "hrv_json_v1",
                "algorithm": dict(user_output.get("algorithm", {})),
                "event_count": len(user_output.get("rr_intervals", [])),
            }
            _write_json(os.path.join(target_dir, "comparison.json"), report_json)
            _write_text(os.path.join(target_dir, "comparison.csv"), report_csv)
            _write_text(os.path.join(target_dir, "comparison_report.html"), report_html)
            return _case_result_from_report(case, case_summary, target, relative_out, detection_path, input_identity, report_json)
        detections = load_detections(detection_path, target=target)
        if target == "ecg_beat_classification":
            report_json = _score_beat_classification(package, case, case_summary, annotations, detections, entry)
            report_csv = _beat_classification_csv(report_json)
            report_html = _beat_classification_html(report_json)
        else:
            report_json = _score_event_detection(package, case, case_summary, annotations, detections, entry)
            report_csv = _event_comparison_csv(report_json)
            report_html = _event_comparison_html(report_json)
        _write_json(os.path.join(target_dir, "comparison.json"), report_json)
        _write_text(os.path.join(target_dir, "comparison.csv"), report_csv)
        _write_text(os.path.join(target_dir, "comparison_report.html"), report_html)
        return _case_result_from_report(case, case_summary, target, relative_out, detection_path, _detection_identity(detections), report_json)
    except Exception as error:
        return _error_result(package, case, case_summary, entry, target_dir, "scoring_error", str(error), detection_path)


def _score_event_detection(package, case, case_summary, annotations, detections, entry):
    target = entry.get("target", "")
    tolerance_seconds = float(entry.get("default_tolerance_seconds", _default_tolerance_seconds(target)))
    truth = _truth_events_for_target(target, annotations, case_summary)
    detection_events = []
    for item in detections.events:
        if not _finite_non_negative(item.time_seconds):
            raise VerificationError("detection time must be finite and non-negative")
        pulse = _ppg_pulse_at_time(annotations, item.time_seconds) if target == "ppg_systolic_peak" else None
        detection_events.append(_TruthEvent(
            item.original_index, item.time_seconds, "", _in_artifact_interval(target, item.time_seconds, annotations, case_summary),
            bool(pulse and pulse.get("low_perfusion", False)),
            bool(pulse and _ppg_pulse_state(pulse) == "weak"),
            bool(pulse and _ppg_pulse_state(pulse) == "missing")))
    missing_pulse_opportunity_count = sum(1 for item in annotations.get("ppg_pulses", []) if _ppg_pulse_state(item) == "missing") if target == "ppg_systolic_peak" else 0
    result = _compare_events(target, truth, detection_events, tolerance_seconds, missing_pulse_opportunity_count)
    return {
        "schema_version": 1,
        "score_type": "event_detection_qa",
        "scoring_version": SCORING_VERSION,
        "package": _package_identity(package),
        "scenario": _scenario_identity(case, case_summary, annotations),
        "detection_input": _detection_identity(detections),
        "comparison": result,
        "notes": [LIMITATION_TEXT],
    }


def _score_beat_classification(package, case, case_summary, annotations, detections, entry):
    tolerance_seconds = float(entry.get("default_tolerance_seconds", _default_tolerance_seconds("ecg_beat_classification")))
    truth = _truth_events_for_target("ecg_beat_classification", annotations, case_summary)
    predictions = []
    for item in detections.events:
        if not _finite_non_negative(item.time_seconds):
            raise VerificationError("classification event time must be finite and non-negative")
        if item.label not in BEAT_CLASSES:
            raise VerificationError("classification event label is not a canonical ECG beat class: %s" % item.label)
        predictions.append(_TruthEvent(item.original_index, item.time_seconds, item.label, False))
    result = _score_classification_events(truth, predictions, tolerance_seconds)
    result.update({
        "schema_version": 1,
        "score_type": "ecg_beat_classification_qa",
        "scoring_version": SCORING_VERSION,
        "scenario_id": case_summary.get("scenario_id", case.scenario_id),
        "document_fingerprint": case_summary.get("document_fingerprint", case.document_fingerprint),
        "render_identity": case_summary.get("render_identity", case.render_identity),
        "package": _package_identity(package),
        "scenario": _scenario_identity(case, case_summary, annotations),
        "algorithm": {"name": detections.algorithm_name, "version": detections.algorithm_version},
        "detection_input": _detection_identity(detections),
        "intended_use": "synthetic engineering algorithm testing and QA",
        "not_for": "diagnosis, patient monitoring, clinical validation certification, or standalone conformity assessment",
    })
    return result


def _load_hrv_user_output(path):
    with open(path, "r") as handle:
        document = json.load(handle)
    if not isinstance(document, dict) or isinstance(document.get("schema_version"), bool) or document.get("schema_version") != 1:
        raise VerificationError("HRV output must be a schema_version 1 JSON object")
    allowed = set(["schema_version", "algorithm", "metrics", "rr_intervals"])
    unknown = sorted(set(document.keys()) - allowed)
    if unknown:
        raise VerificationError("unknown HRV output field: %s" % unknown[0])
    algorithm = document.get("algorithm")
    if not isinstance(algorithm, dict) or not isinstance(algorithm.get("name"), str) or not algorithm.get("name") or not isinstance(algorithm.get("version"), str):
        raise VerificationError("HRV output algorithm must contain non-empty name and string version")
    if set(algorithm.keys()) - set(["name", "version"]):
        raise VerificationError("HRV output algorithm contains unknown fields")
    metrics = document.get("metrics")
    if not isinstance(metrics, dict):
        raise VerificationError("HRV output metrics must be an object")
    for name, value in metrics.items():
        if name not in HRV_METRICS:
            raise VerificationError("unknown HRV metric: %s" % name)
        if not _finite_non_negative(value):
            raise VerificationError("HRV metric %s must be finite and non-negative" % name)
    intervals = document.get("rr_intervals", [])
    if not isinstance(intervals, list):
        raise VerificationError("HRV rr_intervals must be an array")
    for index, item in enumerate(intervals):
        if not isinstance(item, dict) or set(item.keys()) - set(["beat_time_seconds", "rr_seconds"]):
            raise VerificationError("HRV rr_intervals[%s] has invalid fields" % index)
        if not _finite_non_negative(item.get("beat_time_seconds")) or not _finite_positive(item.get("rr_seconds")):
            raise VerificationError("HRV rr_intervals[%s] requires non-negative time and positive RR" % index)
    if not metrics and not intervals:
        raise VerificationError("HRV output requires at least one metric or RR interval")
    return document


def _score_hrv(package, case, case_summary, user_output):
    truth = case.hrv_metrics()
    truth_metrics = {}
    truth_metrics.update(truth.get("time_domain", {}))
    truth_metrics.update(truth.get("frequency_domain", {}))
    metric_scores = []
    for name, user_value in user_output.get("metrics", {}).items():
        if name not in truth_metrics:
            raise VerificationError("challenge has no HRV ground truth metric: %s" % name)
        ground_truth = float(truth_metrics[name])
        user_value = float(user_value)
        absolute_error = abs(user_value - ground_truth)
        relative_error = 100.0 * absolute_error / ground_truth if ground_truth > 0.0 else (0.0 if absolute_error == 0.0 else 1.7976931348623157e308)
        absolute_tolerance, relative_tolerance = _hrv_metric_tolerances(name)
        metric_scores.append({
            "name": name,
            "unit": _hrv_metric_unit(name),
            "ground_truth": ground_truth,
            "user": user_value,
            "absolute_error": absolute_error,
            "relative_error_percent": relative_error,
            "absolute_tolerance": absolute_tolerance,
            "relative_tolerance_percent": relative_tolerance,
            "passed": absolute_error <= absolute_tolerance or relative_error <= relative_tolerance,
        })
    passed_metric_count = sum(1 for item in metric_scores if item["passed"])
    rr_score = _score_hrv_rr(truth.get("tachogram", []), user_output.get("rr_intervals", []))
    return {
        "schema_version": 1,
        "score_type": "hrv_algorithm_qa",
        "scoring_version": "synsigra-python-local-hrv-v1",
        "package": _package_identity(package),
        "scenario": _scenario_identity(case, case_summary, {}),
        "algorithm": dict(user_output.get("algorithm", {})),
        "metric_definition_version": truth.get("definition_version", ""),
        "exclusion_policy": truth.get("exclusion_policy", ""),
        "spectral_method": truth.get("spectral_method", ""),
        "limitations": LIMITATION_TEXT,
        "success": True,
        "passed_metric_count": passed_metric_count,
        "metric_pass_fraction": _ratio(passed_metric_count, len(metric_scores)),
        "metrics": metric_scores,
        "rr": rr_score,
    }


def _hrv_metric_tolerances(name):
    absolute = 0.010
    relative = 10.0
    if name == "mean_rr_seconds":
        return 0.010, 2.0
    if name == "mean_heart_rate_bpm":
        return 1.0, 2.0
    if name == "pnn50_percent":
        return 2.0, relative
    if name == "sd1_sd2_ratio":
        return 0.10, relative
    if name == "lf_hf_ratio":
        return 0.20, 15.0
    if name in ("lf_power_seconds2", "hf_power_seconds2"):
        return 0.0005, 15.0
    if name == "total_power_seconds2":
        return 0.001, 15.0
    return absolute, relative


def _hrv_metric_unit(name):
    if name == "mean_heart_rate_bpm":
        return "bpm"
    if name == "pnn50_percent":
        return "percent"
    if name in ("sd1_sd2_ratio", "lf_hf_ratio"):
        return "ratio"
    if name in ("lf_power_seconds2", "hf_power_seconds2", "total_power_seconds2"):
        return "s2"
    return "s"


def _score_hrv_rr(truth_intervals, user_intervals):
    truth = [item for item in truth_intervals if not item.get("excluded", False)]
    truth_used = [False] * len(truth)
    user_used = [False] * len(user_intervals)
    errors = []
    passed_count = 0
    while True:
        best_truth = None
        best_user = None
        best_time_error = 1.050
        for truth_index, truth_item in enumerate(truth):
            if truth_used[truth_index]:
                continue
            for user_index, user_item in enumerate(user_intervals):
                if user_used[user_index]:
                    continue
                time_error = abs(float(user_item["beat_time_seconds"]) - float(truth_item["beat_time_seconds"]))
                if time_error <= 0.050 and time_error < best_time_error:
                    best_time_error = time_error
                    best_truth = truth_index
                    best_user = user_index
        if best_truth is None:
            break
        truth_used[best_truth] = True
        user_used[best_user] = True
        absolute_error = abs(float(user_intervals[best_user]["rr_seconds"]) - float(truth[best_truth]["rr_seconds"]))
        relative_error = 100.0 * absolute_error / float(truth[best_truth]["rr_seconds"])
        if absolute_error <= 0.020 or relative_error <= 5.0:
            passed_count += 1
        errors.append(absolute_error)
    return {
        "evaluated": bool(user_intervals),
        "ground_truth_count": len(truth) if user_intervals else 0,
        "user_count": len(user_intervals),
        "matched_count": len(errors),
        "missing_count": len(truth) - len(errors) if user_intervals else 0,
        "extra_count": len(user_intervals) - len(errors),
        "passed_count": passed_count,
        "pass_fraction": _ratio(passed_count, len(truth)) if user_intervals else 0.0,
        "time_tolerance_seconds": 0.050,
        "absolute_tolerance_seconds": 0.020,
        "relative_tolerance_percent": 5.0,
        "mean_absolute_error_seconds": _mean(errors),
        "rms_error_seconds": _rms(errors),
        "max_absolute_error_seconds": max(errors) if errors else 0.0,
    }


def _compare_events(target, truth, detections, tolerance_seconds, missing_pulse_opportunity_count=0):
    if not truth and target == "ppg_systolic_peak":
        raise VerificationError("scenario has no PPG systolic peak ground truth")
    sorted_detections = sorted(detections, key=lambda item: (item.time_seconds, item.index))
    candidates = []
    for truth_index, truth_event in enumerate(truth):
        for detection_sorted_index, detection_event in enumerate(sorted_detections):
            absolute_error = abs(detection_event.time_seconds - truth_event.time_seconds)
            if absolute_error <= tolerance_seconds:
                candidates.append((absolute_error, truth_index, detection_sorted_index))
    candidates.sort()
    truth_matched = [False] * len(truth)
    detection_matched = [False] * len(sorted_detections)
    total = _empty_event_metrics()
    clean = _empty_event_metrics()
    artifact = _empty_event_metrics()
    low_perfusion = _empty_event_metrics()
    weak = _empty_event_metrics()
    total_errors = []
    clean_errors = []
    artifact_errors = []
    low_perfusion_errors = []
    weak_errors = []
    matches = []
    false_positives = []
    false_negatives = []
    for absolute_error, truth_index, detection_sorted_index in candidates:
        if truth_matched[truth_index] or detection_matched[detection_sorted_index]:
            continue
        truth_matched[truth_index] = True
        detection_matched[detection_sorted_index] = True
        truth_event = truth[truth_index]
        detection_event = sorted_detections[detection_sorted_index]
        error_seconds = detection_event.time_seconds - truth_event.time_seconds
        match = {
            "ground_truth_index": truth_event.index,
            "detection_index": detection_event.index,
            "ground_truth_time_seconds": truth_event.time_seconds,
            "detection_time_seconds": detection_event.time_seconds,
            "error_seconds": error_seconds,
            "in_artifact_interval": truth_event.in_artifact_interval,
            "low_perfusion": truth_event.low_perfusion,
            "weak_pulse": truth_event.weak_pulse,
        }
        matches.append(match)
        total["true_positive_count"] += 1
        total_errors.append(abs(error_seconds))
        if truth_event.in_artifact_interval:
            artifact["true_positive_count"] += 1
            artifact_errors.append(abs(error_seconds))
        else:
            clean["true_positive_count"] += 1
            clean_errors.append(abs(error_seconds))
        if truth_event.low_perfusion:
            low_perfusion["true_positive_count"] += 1
            low_perfusion_errors.append(abs(error_seconds))
        if truth_event.weak_pulse:
            weak["true_positive_count"] += 1
            weak_errors.append(abs(error_seconds))

    for index, truth_event in enumerate(truth):
        _bin_metrics(truth_event.in_artifact_interval, clean, artifact)["ground_truth_count"] += 1
        if truth_event.low_perfusion:
            low_perfusion["ground_truth_count"] += 1
        if truth_event.weak_pulse:
            weak["ground_truth_count"] += 1
        if not truth_matched[index]:
            event = {
                "ground_truth_index": truth_event.index, "time_seconds": truth_event.time_seconds,
                "in_artifact_interval": truth_event.in_artifact_interval,
                "low_perfusion": truth_event.low_perfusion, "weak_pulse": truth_event.weak_pulse,
            }
            false_negatives.append(event)
            total["false_negative_count"] += 1
            _bin_metrics(truth_event.in_artifact_interval, clean, artifact)["false_negative_count"] += 1
            if truth_event.low_perfusion:
                low_perfusion["false_negative_count"] += 1
            if truth_event.weak_pulse:
                weak["false_negative_count"] += 1
    for index, detection_event in enumerate(sorted_detections):
        _bin_metrics(detection_event.in_artifact_interval, clean, artifact)["detection_count"] += 1
        if detection_event.low_perfusion:
            low_perfusion["detection_count"] += 1
        if detection_event.weak_pulse:
            weak["detection_count"] += 1
        if not detection_matched[index]:
            event = {
                "detection_index": detection_event.index, "time_seconds": detection_event.time_seconds,
                "in_artifact_interval": detection_event.in_artifact_interval,
                "low_perfusion": detection_event.low_perfusion, "weak_pulse": detection_event.weak_pulse,
                "missing_pulse_window": detection_event.missing_pulse_window,
            }
            false_positives.append(event)
            total["false_positive_count"] += 1
            _bin_metrics(detection_event.in_artifact_interval, clean, artifact)["false_positive_count"] += 1
            if detection_event.low_perfusion:
                low_perfusion["false_positive_count"] += 1
            if detection_event.weak_pulse:
                weak["false_positive_count"] += 1
    total["ground_truth_count"] = len(truth)
    total["detection_count"] = len(detections)
    _finalize_event_metrics(total, total_errors)
    _finalize_event_metrics(clean, clean_errors)
    _finalize_event_metrics(artifact, artifact_errors)
    _finalize_event_metrics(low_perfusion, low_perfusion_errors)
    _finalize_event_metrics(weak, weak_errors)
    return {
        "target": target,
        "tolerance_seconds": tolerance_seconds,
        "success": True,
        "metrics": {
            "total": total, "clean": clean, "artifact": artifact,
            "low_perfusion": low_perfusion, "weak": weak,
            "missing_pulse": {
                "opportunity_count": missing_pulse_opportunity_count,
                "detection_count": sum(1 for item in detections if item.missing_pulse_window),
            },
        },
        "matches": matches,
        "false_positives": false_positives,
        "false_negatives": false_negatives,
    }


def _score_classification_events(truth, predictions, tolerance_seconds):
    candidates = []
    for truth_index, truth_event in enumerate(truth):
        for prediction_index, prediction in enumerate(predictions):
            error = abs(prediction.time_seconds - truth_event.time_seconds)
            if error <= tolerance_seconds:
                candidates.append((error, truth_index, prediction_index))
    candidates.sort()
    truth_used = [False] * len(truth)
    prediction_used = [False] * len(predictions)
    classes = dict((name, _empty_class_metrics(name != "unscored")) for name in BEAT_CLASSES)
    confusion = dict((actual, dict((predicted, 0) for predicted in BEAT_CLASSES)) for actual in BEAT_CLASSES)
    scored_ground_truth_count = 0
    scored_prediction_count = 0
    matched_count = 0
    correct_count = 0
    unscored_match_count = 0
    timing_error_sum = 0.0
    max_absolute_error_seconds = 0.0
    matches = []
    unmatched_ground_truth = []
    unmatched_predictions = []

    for event in truth:
        classes[event.label]["ground_truth_count"] += 1
        if event.label in SCORED_BEAT_CLASSES:
            scored_ground_truth_count += 1

    for absolute_error, truth_index, prediction_index in candidates:
        if truth_used[truth_index] or prediction_used[prediction_index]:
            continue
        truth_used[truth_index] = True
        prediction_used[prediction_index] = True
        truth_event = truth[truth_index]
        prediction = predictions[prediction_index]
        actual = truth_event.label
        predicted = prediction.label
        scored = actual in SCORED_BEAT_CLASSES
        correct = scored and actual == predicted
        confusion[actual][predicted] += 1
        classes[predicted]["prediction_count"] += 1
        matched_count += 1
        if not scored:
            unscored_match_count += 1
        else:
            timing_error_sum += absolute_error
            max_absolute_error_seconds = max(max_absolute_error_seconds, absolute_error)
            if predicted in SCORED_BEAT_CLASSES:
                scored_prediction_count += 1
            if correct:
                correct_count += 1
                classes[actual]["true_positive_count"] += 1
            else:
                classes[actual]["false_negative_count"] += 1
                if predicted in SCORED_BEAT_CLASSES:
                    classes[predicted]["false_positive_count"] += 1
        matches.append({
            "ground_truth_index": truth_event.index,
            "prediction_index": prediction.index,
            "ground_truth_time_seconds": truth_event.time_seconds,
            "prediction_time_seconds": prediction.time_seconds,
            "error_seconds": prediction.time_seconds - truth_event.time_seconds,
            "actual_class": actual,
            "predicted_class": predicted,
            "scored": scored,
            "correct": correct,
        })

    for index, truth_event in enumerate(truth):
        if truth_used[index]:
            continue
        unmatched_ground_truth.append({"ground_truth_index": truth_event.index, "time_seconds": truth_event.time_seconds, "actual_class": truth_event.label})
        if truth_event.label in SCORED_BEAT_CLASSES:
            classes[truth_event.label]["false_negative_count"] += 1
    for index, prediction in enumerate(predictions):
        if prediction_used[index]:
            continue
        unmatched_predictions.append({"prediction_index": prediction.index, "time_seconds": prediction.time_seconds, "predicted_class": prediction.label})
        classes[prediction.label]["prediction_count"] += 1
        if prediction.label in SCORED_BEAT_CLASSES:
            scored_prediction_count += 1
            classes[prediction.label]["false_positive_count"] += 1

    for metrics in classes.values():
        if not metrics["scored"]:
            continue
        metrics["precision"] = _ratio(metrics["true_positive_count"], metrics["true_positive_count"] + metrics["false_positive_count"])
        metrics["recall"] = _ratio(metrics["true_positive_count"], metrics["true_positive_count"] + metrics["false_negative_count"])
        metrics["f1_score"] = _f1(metrics["precision"], metrics["recall"])
    scored_matches = matched_count - unscored_match_count
    micro_precision = _ratio(correct_count, scored_prediction_count)
    micro_recall = _ratio(correct_count, scored_ground_truth_count)
    summary = {
        "scored_ground_truth_count": scored_ground_truth_count,
        "scored_prediction_count": scored_prediction_count,
        "matched_count": matched_count,
        "correct_count": correct_count,
        "unscored_match_count": unscored_match_count,
        "accuracy": _ratio(correct_count, scored_ground_truth_count),
        "micro_precision": micro_precision,
        "micro_recall": micro_recall,
        "micro_f1_score": _f1(micro_precision, micro_recall),
        "mean_absolute_error_seconds": _ratio(timing_error_sum, scored_matches),
        "max_absolute_error_seconds": max_absolute_error_seconds,
    }
    return {
        "success": True,
        "target": "ecg_beat_classification",
        "tolerance_seconds": tolerance_seconds,
        "summary": summary,
        "classes": [_class_row(name, classes[name]) for name in BEAT_CLASSES],
        "confusion_matrix": {"labels": list(BEAT_CLASSES), "rows": [[confusion[actual][predicted] for predicted in BEAT_CLASSES] for actual in BEAT_CLASSES]},
        "matches": matches,
        "unmatched_ground_truth": unmatched_ground_truth,
        "unmatched_predictions": unmatched_predictions,
    }


def _truth_events_for_target(target, annotations, case_summary):
    events = []
    if target == "r_peak":
        for array_index, beat in enumerate(annotations.get("beats", [])):
            if beat.get("qrs_present", False) and _finite_non_negative(beat.get("r_peak_seconds")):
                time_seconds = float(beat["r_peak_seconds"])
                index = beat.get("beat_index", array_index)
                events.append(_TruthEvent(index, time_seconds, "", _in_artifact_interval(target, time_seconds, annotations, case_summary)))
        return events
    if target == "ecg_beat_classification":
        for array_index, beat in enumerate(annotations.get("beats", [])):
            if beat.get("qrs_present", False) and _finite_non_negative(beat.get("r_peak_seconds")):
                label = beat.get("beat_class", "unscored")
                if label not in BEAT_CLASSES:
                    label = "unscored"
                events.append(_TruthEvent(beat.get("beat_index", array_index), beat["r_peak_seconds"], label, False))
        return events
    if target == "ppg_systolic_peak":
        for item in annotations.get("ppg_fiducials", []):
            if item.get("kind") == "systolic_peak" and item.get("source") == "measurement" and _finite_non_negative(item.get("time_seconds")):
                time_seconds = float(item["time_seconds"])
                pulse = _ppg_pulse_for_beat(annotations, item.get("ecg_beat_index"))
                events.append(_TruthEvent(
                    len(events), time_seconds, "", _in_artifact_interval(target, time_seconds, annotations, case_summary),
                    bool(pulse and pulse.get("low_perfusion", False)),
                    bool(pulse and _ppg_pulse_state(pulse) == "weak")))
        return events
    raise VerificationError("unsupported target: %s" % target)


def _ppg_pulse_for_beat(annotations, beat_index):
    for pulse in annotations.get("ppg_pulses", []):
        if pulse.get("ecg_beat_index") == beat_index:
            return pulse
    return None


def _ppg_pulse_state(pulse):
    if pulse.get("state"):
        return pulse["state"]
    return "missing" if pulse.get("intentionally_missing", False) else "valid"


def _ppg_pulse_at_time(annotations, time_seconds):
    best = None
    best_distance = None
    for pulse in annotations.get("ppg_pulses", []):
        onset = float(pulse.get("expected_onset_time_seconds", 0.0))
        offset = float(pulse.get("expected_offset_time_seconds", onset))
        if time_seconds < onset or time_seconds > offset:
            continue
        distance = abs(time_seconds - float(pulse.get("expected_peak_time_seconds", onset)))
        if best is None or distance < best_distance:
            best = pulse
            best_distance = distance
    return best


def _in_artifact_interval(target, time_seconds, annotations, case_summary):
    intervals = annotations.get("artifact_intervals")
    if intervals is None:
        intervals = case_summary.get("artifact_intervals", [])
    for interval in intervals:
        start = float(interval.get("start_seconds", 0.0))
        end = float(interval.get("end_seconds", 0.0))
        if time_seconds >= start and time_seconds < end and _artifact_affects_target(interval, target):
            return True
    return False


def _artifact_affects_target(interval, target):
    channels = [str(item).lower() for item in interval.get("channels", [])]
    if not channels:
        return True
    if target == "ppg_systolic_peak":
        return any(item == "ppg_green" or "ppg" in item for item in channels)
    return any("ppg" not in item for item in channels)


def _case_scoring_entries(scoring_manifest, case_id, case_summary):
    for item in scoring_manifest.get("cases", []):
        if item.get("case_id") == case_id:
            return list(item.get("scoring", []))
    return list(case_summary.get("scoring", []))


def _find_detection_file(detections_dir, entry, case_id, case_targets, target):
    candidates = []
    for item in entry.get("recommended_files", []):
        recommended = item.get("path", "")
        if recommended:
            candidates.extend(_candidate_paths_from_recommended(detections_dir, recommended))
    for name in _fallback_detection_names(case_id, case_targets, target):
        candidates.append(os.path.join(detections_dir, name))
    seen = set()
    for path in candidates:
        normalized = os.path.normpath(path)
        if normalized in seen:
            continue
        seen.add(normalized)
        if os.path.isfile(normalized):
            return normalized
    return None


def _candidate_paths_from_recommended(detections_dir, recommended):
    normalized = recommended.replace("\\", "/")
    if os.path.isabs(normalized) or ".." in normalized.split("/"):
        return []
    candidates = [os.path.join(detections_dir, normalized), os.path.join(detections_dir, os.path.basename(normalized))]
    parts = normalized.split("/")
    if parts and parts[0] == "detections":
        candidates.insert(0, os.path.join(detections_dir, "/".join(parts[1:])))
    return candidates


def _fallback_detection_names(case_id, case_targets, target):
    names = []
    stem = _detection_stem_for_target(case_id, case_targets, target)
    for suffix in (".json", ".csv"):
        names.append(stem + suffix)
        names.append(case_id + "_" + target + suffix)
        names.append(case_id + suffix)
    if target == "ecg_beat_classification":
        names.extend([case_id + "_beat_classes.json", case_id + "_beat_classes.csv", case_id + "_beat_classification.json", case_id + "_beat_classification.csv"])
    if target == "r_peak":
        names.extend([case_id + "_rpeaks.json", case_id + "_rpeaks.csv"])
    if target == "ppg_systolic_peak":
        names.extend([case_id + "_ppg_peaks.json", case_id + "_ppg_peaks.csv"])
    return names


def _detection_stem_for_target(case_id, case_targets, target):
    return case_id if len(case_targets) <= 1 else case_id + "_" + target


def _target_output_relative_path(entry, case_id, case_targets, target):
    command = entry.get("score_command", "")
    if command:
        try:
            tokens = shlex.split(command)
            for index, token in enumerate(tokens):
                if token == "--out" and index + 1 < len(tokens):
                    return _safe_relative_path(tokens[index + 1])
        except ValueError:
            pass
    return _safe_relative_path("verification/" + _detection_stem_for_target(case_id, case_targets, target))


def _safe_relative_path(path):
    normalized = path.replace("\\", "/")
    if os.path.isabs(normalized) or ".." in normalized.split("/"):
        raise VerificationError("unsafe relative output path: %s" % path)
    return normalized


def _case_result_from_report(case, case_summary, target, relative_out, detection_path, detection_identity, report_json):
    result = {
        "case_id": case.id,
        "scenario_id": case_summary.get("scenario_id", case.scenario_id),
        "target": target,
        "status": "scored",
        "success": True,
        "report_path": relative_out + "/comparison.json",
        "report_directory": relative_out,
        "detection_input": detection_identity,
        "detection_input_sha256": _sha256_file(detection_path),
    }
    if target == "ecg_beat_classification":
        result["score_type"] = "classification"
        result["exclusion_policy"] = "Ground-truth beats labelled unscored are excluded from accuracy, recall, and F1 denominators."
        result["summary"] = dict(report_json.get("summary", {}))
        result["classes"] = list(report_json.get("classes", []))
        result["confusion_matrix"] = dict(report_json.get("confusion_matrix", {}))
    elif target == "hrv":
        result["score_type"] = "hrv_metrics"
        result["exclusion_policy"] = report_json.get("exclusion_policy", "")
        result["summary"] = {
            "passed_metric_count": report_json.get("passed_metric_count", 0),
            "metric_count": len(report_json.get("metrics", [])),
            "metric_pass_fraction": report_json.get("metric_pass_fraction", 0.0),
        }
        result["metrics"] = list(report_json.get("metrics", []))
        result["rr"] = dict(report_json.get("rr", {}))
    else:
        result["score_type"] = "event_detection"
        result["exclusion_policy"] = "All events are scored in total; clean and artifact bins partition events by target-affecting half-open artifact intervals [start,end)."
        result["metrics"] = dict(report_json.get("comparison", {}).get("metrics", {}))
        errors = {"total": [], "clean": [], "artifact": []}
        for match in report_json.get("comparison", {}).get("matches", []):
            value = abs(float(match["error_seconds"]))
            errors["total"].append(value)
            errors["artifact" if match.get("in_artifact_interval", False) else "clean"].append(value)
        result["_absolute_errors"] = errors
    return result


def _unsupported_result(case, case_summary, entry, out_dir):
    return {
        "case_id": case.id,
        "scenario_id": case_summary.get("scenario_id", case.scenario_id),
        "target": entry.get("target", ""),
        "score_type": entry.get("score_type", ""),
        "status": "unsupported",
        "success": False,
        "message": entry.get("note", "target is not supported by local verifier"),
        "report_path": "",
    }


def _error_result(package, case, case_summary, entry, target_dir, status, message, detection_path=None):
    target = entry.get("target", "")
    report = {
        "schema_version": 1,
        "score_type": entry.get("score_type", ""),
        "scoring_version": SCORING_VERSION,
        "package": _package_identity(package),
        "scenario": _scenario_identity(case, case_summary, {}),
        "target": target,
        "success": False,
        "status": status,
        "message": message,
        "notes": [LIMITATION_TEXT],
    }
    if detection_path is not None:
        report["detection_input_path"] = detection_path
        report["detection_input_sha256"] = _sha256_file(detection_path)
    _write_json(os.path.join(target_dir, "comparison.json"), report)
    _write_text(os.path.join(target_dir, "comparison.csv"), "status,message\n%s,%s\n" % (status, _csv_cell(message)))
    _write_text(os.path.join(target_dir, "comparison_report.html"), _error_html(report))
    return {
        "case_id": case.id,
        "scenario_id": case_summary.get("scenario_id", case.scenario_id),
        "target": target,
        "score_type": entry.get("score_type", ""),
        "status": status,
        "success": False,
        "message": message,
        "report_path": _relative_to_parent(target_dir) + "/comparison.json",
    }


def _build_verification_summary(package, scoring_manifest, integrity, results, messages, threshold_profile):
    targets = _aggregate_targets(results)
    scoring_success = bool(results) and all(item.get("success", False) for item in results)
    policy = _evaluate_policy(targets, threshold_profile)
    success = scoring_success and policy["passed"]
    public_results = []
    for result in results:
        public_result = dict(result)
        public_result.pop("_absolute_errors", None)
        public_results.append(public_result)
    return {
        "schema_version": 1,
        "summary_type": "synsigra_local_verification",
        "scoring_version": SCORING_VERSION,
        "status": "passed" if success else "failed",
        "success": success,
        "scoring_success": scoring_success,
        "threshold_profile": threshold_profile,
        "policy": policy,
        "package": _package_identity(package, scoring_manifest),
        "integrity": integrity,
        "case_target_count": len(results),
        "scored_case_target_count": sum(1 for item in results if item.get("success", False)),
        "passed_case_target_count": sum(1 for item in results if item.get("success", False)),
        "failed_case_target_count": sum(1 for item in results if not item.get("success", False)),
        "targets": targets,
        "cases": public_results,
        "messages": messages,
        "limitation": LIMITATION_TEXT,
    }


def _aggregate_targets(results):
    aggregates = {}
    for result in results:
        target = result.get("target", "")
        if target not in aggregates:
            aggregates[target] = {"target": target, "case_count": 0, "passed_case_count": 0, "failed_case_count": 0}
        aggregate = aggregates[target]
        if result.get("exclusion_policy") and "exclusion_policy" not in aggregate:
            aggregate["exclusion_policy"] = result["exclusion_policy"]
        aggregate["case_count"] += 1
        if result.get("success", False):
            aggregate["passed_case_count"] += 1
        else:
            aggregate["failed_case_count"] += 1
        if result.get("score_type") == "event_detection" and result.get("success", False):
            _aggregate_event_result(aggregate, result)
        if result.get("score_type") == "classification" and result.get("success", False):
            _aggregate_classification_result(aggregate, result)
        if result.get("score_type") == "hrv_metrics" and result.get("success", False):
            _aggregate_hrv_result(aggregate, result)
    ordered = []
    for target in sorted(aggregates.keys()):
        aggregate = aggregates[target]
        if "event_errors_total" in aggregate:
            for name in ("total", "clean", "artifact"):
                _finalize_event_metrics(aggregate[name], aggregate.pop("event_errors_" + name))
        if "scored_ground_truth_count" in aggregate:
            aggregate["accuracy"] = _ratio(aggregate["correct_count"], aggregate["scored_ground_truth_count"])
            aggregate["micro_precision"] = _ratio(aggregate["correct_count"], aggregate["scored_prediction_count"])
            aggregate["micro_recall"] = _ratio(aggregate["correct_count"], aggregate["scored_ground_truth_count"])
            aggregate["micro_f1_score"] = _f1(aggregate["micro_precision"], aggregate["micro_recall"])
            _finalize_classification_aggregate(aggregate)
        if aggregate.get("score_type") == "hrv_metrics":
            _finalize_hrv_aggregate(aggregate)
        ordered.append(aggregate)
    return ordered


def _aggregate_event_result(aggregate, result):
    if "total" not in aggregate:
        aggregate["score_type"] = "event_detection"
        for name in ("total", "clean", "artifact"):
            aggregate[name] = _empty_event_metrics()
            aggregate["event_errors_" + name] = []
    metrics = result.get("metrics", {})
    for name in ("total", "clean", "artifact"):
        source = metrics.get(name, {})
        for key in ("ground_truth_count", "detection_count", "true_positive_count", "false_positive_count", "false_negative_count"):
            aggregate[name][key] += int(source.get(key, 0))
        aggregate["event_errors_" + name].extend(result.get("_absolute_errors", {}).get(name, []))


def _aggregate_classification_result(aggregate, result):
    if "score_type" not in aggregate:
        aggregate["score_type"] = "classification"
        aggregate["scored_ground_truth_count"] = 0
        aggregate["scored_prediction_count"] = 0
        aggregate["matched_count"] = 0
        aggregate["correct_count"] = 0
        aggregate["classes"] = dict((name, _empty_class_metrics(name != "unscored")) for name in BEAT_CLASSES)
        aggregate["confusion_matrix"] = dict((actual, dict((predicted, 0) for predicted in BEAT_CLASSES)) for actual in BEAT_CLASSES)
    summary = result.get("summary", {})
    for key in ("scored_ground_truth_count", "scored_prediction_count", "matched_count", "correct_count"):
        aggregate[key] += int(summary.get(key, 0))
    for item in result.get("classes", []):
        name = item.get("class")
        if name not in aggregate["classes"]:
            continue
        for key in ("ground_truth_count", "prediction_count", "true_positive_count", "false_positive_count", "false_negative_count"):
            aggregate["classes"][name][key] += int(item.get(key, 0))
    matrix = result.get("confusion_matrix", {})
    labels = matrix.get("labels", [])
    for row_index, actual in enumerate(labels):
        if actual not in aggregate["confusion_matrix"] or row_index >= len(matrix.get("rows", [])):
            continue
        for column_index, predicted in enumerate(labels):
            if predicted in aggregate["confusion_matrix"][actual] and column_index < len(matrix["rows"][row_index]):
                aggregate["confusion_matrix"][actual][predicted] += int(matrix["rows"][row_index][column_index])


def _finalize_classification_aggregate(aggregate):
    class_rows = []
    for name in BEAT_CLASSES:
        metrics = aggregate["classes"][name]
        if metrics["scored"]:
            metrics["precision"] = _ratio(metrics["true_positive_count"], metrics["true_positive_count"] + metrics["false_positive_count"])
            metrics["recall"] = _ratio(metrics["true_positive_count"], metrics["true_positive_count"] + metrics["false_negative_count"])
            metrics["f1_score"] = _f1(metrics["precision"], metrics["recall"])
        class_rows.append(_class_row(name, metrics))
    aggregate["classes"] = class_rows
    confusion = aggregate["confusion_matrix"]
    aggregate["confusion_matrix"] = {"labels": list(BEAT_CLASSES), "rows": [[confusion[actual][predicted] for predicted in BEAT_CLASSES] for actual in BEAT_CLASSES]}


def _aggregate_hrv_result(aggregate, result):
    if aggregate.get("score_type") != "hrv_metrics":
        aggregate["score_type"] = "hrv_metrics"
        aggregate["evaluated_metric_count"] = 0
        aggregate["passed_metric_count"] = 0
        aggregate["metrics"] = {}
        aggregate["rr"] = {"evaluated_case_count": 0, "ground_truth_count": 0, "user_count": 0, "matched_count": 0, "missing_count": 0, "extra_count": 0, "passed_count": 0}
    for item in result.get("metrics", []):
        name = item.get("name", "")
        row = aggregate["metrics"].setdefault(name, {"name": name, "unit": item.get("unit", ""), "evaluated_count": 0, "passed_count": 0, "_absolute_errors": []})
        row["evaluated_count"] += 1
        row["passed_count"] += 1 if item.get("passed", False) else 0
        row["_absolute_errors"].append(float(item.get("absolute_error", 0.0)))
        aggregate["evaluated_metric_count"] += 1
        aggregate["passed_metric_count"] += 1 if item.get("passed", False) else 0
    rr = result.get("rr", {})
    if rr.get("evaluated", False):
        aggregate["rr"]["evaluated_case_count"] += 1
        for key in ("ground_truth_count", "user_count", "matched_count", "missing_count", "extra_count", "passed_count"):
            aggregate["rr"][key] += int(rr.get(key, 0))


def _finalize_hrv_aggregate(aggregate):
    aggregate["metric_pass_fraction"] = _ratio(aggregate["passed_metric_count"], aggregate["evaluated_metric_count"])
    rows = []
    for name in sorted(aggregate["metrics"].keys()):
        row = aggregate["metrics"][name]
        errors = row.pop("_absolute_errors")
        row["pass_fraction"] = _ratio(row["passed_count"], row["evaluated_count"])
        row["mean_absolute_error"] = _mean(errors)
        row["max_absolute_error"] = max(errors) if errors else 0.0
        rows.append(row)
    aggregate["metrics"] = rows
    aggregate["rr"]["pass_fraction"] = _ratio(aggregate["rr"]["passed_count"], aggregate["rr"]["ground_truth_count"])


def _evaluate_policy(targets, profile):
    checks = []
    target_results = []
    definitions = profile.get("targets", {})
    for target in targets:
        score_type = target.get("score_type", "")
        definition_name = target["target"] if target["target"] in definitions else score_type
        if score_type == "classification":
            definition_name = "ecg_beat_classification"
        elif score_type == "hrv_metrics":
            definition_name = "hrv"
        definition = definitions.get(definition_name, {})
        target_checks = []
        if score_type == "event_detection":
            for section_name, limits in definition.items():
                metrics = target.get(section_name, {})
                applicable = int(metrics.get("ground_truth_count", 0)) > 0 or int(metrics.get("detection_count", 0)) > 0
                target_checks.extend(_threshold_checks(target["target"], section_name, metrics, limits, applicable))
        elif score_type == "classification":
            target_checks.extend(_threshold_checks(target["target"], "summary", target, definition.get("summary", {}), target.get("scored_ground_truth_count", 0) > 0))
            per_class = definition.get("per_class", {})
            for class_metrics in target.get("classes", []):
                applicable = class_metrics.get("scored", False) and class_metrics.get("ground_truth_count", 0) > 0
                target_checks.extend(_threshold_checks(target["target"], "class/%s" % class_metrics.get("class", ""), class_metrics, per_class, applicable))
        elif score_type == "hrv_metrics":
            target_checks.extend(_threshold_checks(target["target"], "summary", target, definition.get("summary", {}), target.get("evaluated_metric_count", 0) > 0))
            target_checks.extend(_threshold_checks(target["target"], "rr", target.get("rr", {}), definition.get("rr", {}), target.get("rr", {}).get("evaluated_case_count", 0) > 0))
        target_passed = bool(target_checks) and all(item["passed"] for item in target_checks if item["applicable"])
        target["policy"] = {"profile_id": profile["profile_id"], "passed": target_passed, "checks": target_checks}
        target_results.append({"target": target["target"], "passed": target_passed, "check_count": len(target_checks)})
        checks.extend(target_checks)
    applicable_checks = [item for item in checks if item["applicable"]]
    passed = bool(target_results) and all(item["passed"] for item in target_results)
    return {
        "profile_id": profile["profile_id"],
        "passed": passed,
        "target_count": len(target_results),
        "check_count": len(checks),
        "applicable_check_count": len(applicable_checks),
        "failed_check_count": sum(1 for item in applicable_checks if not item["passed"]),
        "targets": target_results,
        "checks": checks,
    }


def _threshold_checks(target, section, metrics, limits, applicable):
    checks = []
    for metric_name in sorted(limits.keys()):
        actual_present = metric_name in metrics and isinstance(metrics.get(metric_name), (int, float))
        actual = float(metrics.get(metric_name, 0.0))
        for operator in ("min", "max"):
            if operator not in limits[metric_name]:
                continue
            threshold = float(limits[metric_name][operator])
            check_applicable = applicable and actual_present
            passed = not check_applicable or (actual >= threshold if operator == "min" else actual <= threshold)
            checks.append({
                "target": target,
                "section": section,
                "metric": metric_name,
                "operator": operator,
                "threshold": threshold,
                "actual": actual if actual_present else None,
                "applicable": check_applicable,
                "passed": passed,
            })
    return checks


def _event_comparison_csv(report_json):
    comparison = report_json["comparison"]
    output = io.StringIO()
    writer = csv.writer(output)
    writer.writerow(["row_type", "bin", "ground_truth_index", "detection_index", "ground_truth_time_seconds", "detection_time_seconds", "error_seconds", "ground_truth_count", "detection_count", "true_positive_count", "false_positive_count", "false_negative_count", "sensitivity", "positive_predictive_value", "f1_score", "mean_absolute_error_seconds", "median_absolute_error_seconds", "rms_error_seconds", "max_absolute_error_seconds"])
    for name in ("total", "clean", "artifact", "low_perfusion", "weak"):
        metrics = comparison["metrics"][name]
        writer.writerow(["metrics", name, "", "", "", "", "", metrics["ground_truth_count"], metrics["detection_count"], metrics["true_positive_count"], metrics["false_positive_count"], metrics["false_negative_count"], metrics["sensitivity"], metrics["positive_predictive_value"], metrics["f1_score"], metrics["mean_absolute_error_seconds"], metrics["median_absolute_error_seconds"], metrics["rms_error_seconds"], metrics["max_absolute_error_seconds"]])
    missing = comparison["metrics"]["missing_pulse"]
    writer.writerow(["metrics", "missing_pulse", "", "", "", "", "", missing["opportunity_count"], missing["detection_count"], 0, missing["detection_count"], 0, 0, 0, 0, 0, 0, 0, 0])
    for match in comparison["matches"]:
        writer.writerow(["match", _event_bin(match), match["ground_truth_index"], match["detection_index"], match["ground_truth_time_seconds"], match["detection_time_seconds"], match["error_seconds"], "", "", "", "", "", "", "", "", "", "", "", ""])
    for item in comparison["false_positives"]:
        writer.writerow(["false_positive", _event_bin(item), "", item["detection_index"], "", item["time_seconds"], "", "", "", "", "", "", "", "", "", "", "", "", ""])
    for item in comparison["false_negatives"]:
        writer.writerow(["false_negative", _event_bin(item), item["ground_truth_index"], "", item["time_seconds"], "", "", "", "", "", "", "", "", "", "", "", "", "", ""])
    return output.getvalue()


def _event_bin(item):
    if item.get("in_artifact_interval", False):
        return "artifact"
    if item.get("missing_pulse_window", False):
        return "missing_pulse"
    if item.get("weak_pulse", False):
        return "weak"
    if item.get("low_perfusion", False):
        return "low_perfusion"
    return "clean"


def _beat_classification_csv(report_json):
    output = io.StringIO()
    writer = csv.writer(output)
    writer.writerow(["row_type", "class", "scored", "ground_truth_count", "prediction_count", "true_positive_count", "false_positive_count", "false_negative_count", "precision", "recall", "f1_score"])
    for item in report_json["classes"]:
        writer.writerow(["class", item["class"], int(item["scored"]), item["ground_truth_count"], item["prediction_count"], item["true_positive_count"], item["false_positive_count"], item["false_negative_count"], item["precision"], item["recall"], item["f1_score"]])
    writer.writerow([])
    writer.writerow(["actual_class", "predicted_class", "count"])
    labels = report_json["confusion_matrix"]["labels"]
    for actual, row in zip(labels, report_json["confusion_matrix"]["rows"]):
        for predicted, count in zip(labels, row):
            writer.writerow([actual, predicted, count])
    writer.writerow([])
    writer.writerow(["row_type", "ground_truth_index", "prediction_index", "ground_truth_time_seconds", "prediction_time_seconds", "error_seconds", "actual_class", "predicted_class", "scored", "correct"])
    for match in report_json["matches"]:
        writer.writerow(["match", match["ground_truth_index"], match["prediction_index"], match["ground_truth_time_seconds"], match["prediction_time_seconds"], match["error_seconds"], match["actual_class"], match["predicted_class"], int(match["scored"]), int(match["correct"])])
    for item in report_json["unmatched_ground_truth"]:
        writer.writerow(["unmatched_ground_truth", item["ground_truth_index"], "", item["time_seconds"], "", "", item["actual_class"], "", int(item["actual_class"] in SCORED_BEAT_CLASSES), 0])
    for item in report_json["unmatched_predictions"]:
        writer.writerow(["unmatched_prediction", "", item["prediction_index"], "", item["time_seconds"], "", "", item["predicted_class"], int(item["predicted_class"] in SCORED_BEAT_CLASSES), 0])
    return output.getvalue()


def _hrv_csv(report_json):
    output = io.StringIO()
    writer = csv.writer(output)
    writer.writerow(["row_type", "name", "unit", "ground_truth", "user", "absolute_error", "relative_error_percent", "absolute_tolerance", "relative_tolerance_percent", "passed", "count"])
    for item in report_json["metrics"]:
        writer.writerow(["metric", item["name"], item["unit"], item["ground_truth"], item["user"], item["absolute_error"], item["relative_error_percent"], item["absolute_tolerance"], item["relative_tolerance_percent"], int(item["passed"]), ""])
    rr = report_json["rr"]
    writer.writerow(["rr", "matched", "count", "", "", "", "", "", "", int(rr["evaluated"]), rr["matched_count"]])
    writer.writerow(["rr", "missing", "count", "", "", "", "", "", "", "", rr["missing_count"]])
    writer.writerow(["rr", "extra", "count", "", "", "", "", "", "", "", rr["extra_count"]])
    writer.writerow(["rr", "mean_absolute_error", "seconds", "", "", rr["mean_absolute_error_seconds"], "", "", "", "", rr["matched_count"]])
    return output.getvalue()


def _summary_csv(summary):
    output = io.StringIO()
    writer = csv.writer(output)
    writer.writerow(["case_id", "scenario_id", "target", "status", "score_type", "ground_truth_count", "detection_count", "true_positive_count", "false_positive_count", "false_negative_count", "f1_score", "accuracy", "micro_f1_score", "hrv_metric_pass_fraction", "threshold_profile", "package_policy_passed", "report_path", "message"])
    for item in summary["cases"]:
        metric_data = item.get("metrics", {})
        total = metric_data.get("total", {}) if isinstance(metric_data, dict) else {}
        classification = item.get("summary", {})
        writer.writerow([item.get("case_id", ""), item.get("scenario_id", ""), item.get("target", ""), item.get("status", ""), item.get("score_type", ""), total.get("ground_truth_count", ""), total.get("detection_count", ""), total.get("true_positive_count", ""), total.get("false_positive_count", ""), total.get("false_negative_count", ""), total.get("f1_score", ""), classification.get("accuracy", ""), classification.get("micro_f1_score", ""), classification.get("metric_pass_fraction", ""), summary["policy"]["profile_id"], int(summary["policy"]["passed"]), item.get("report_path", ""), item.get("message", "")])
    return output.getvalue()


def _event_comparison_html(report_json):
    comparison = report_json["comparison"]
    scenario = report_json["scenario"]
    rows = []
    for name in ("total", "clean", "artifact", "low_perfusion", "weak"):
        metrics = comparison["metrics"][name]
        rows.append("<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%.6g</td><td>%.6g</td><td>%.6g</td><td>%.6g s</td><td>%.6g s</td></tr>" % (
            _h(name), metrics["ground_truth_count"], metrics["detection_count"], metrics["true_positive_count"], metrics["false_positive_count"], metrics["false_negative_count"], metrics["sensitivity"], metrics["positive_predictive_value"], metrics["f1_score"], metrics["mean_absolute_error_seconds"], metrics["rms_error_seconds"]))
    return _html_page(
        "Algorithm Comparison Report",
        "<h1>Algorithm Comparison Report</h1><p class=\"notice\">%s</p><h2>Identity</h2><table><tr><th>Scenario</th><td>%s</td></tr><tr><th>Document fingerprint</th><td>%s</td></tr><tr><th>Render identity</th><td>%s</td></tr><tr><th>Target</th><td>%s</td></tr><tr><th>Tolerance</th><td>%.6g s</td></tr></table><h2>Metrics</h2><table><tr><th>Bin</th><th>GT</th><th>Detections</th><th>TP</th><th>FP</th><th>FN</th><th>Sensitivity</th><th>PPV</th><th>F1</th><th>Mean abs error</th><th>RMS error</th></tr>%s</table><p>Missing-pulse opportunities: %s; detections inside missing-pulse windows: %s.</p><h2>Artifacts</h2><p>comparison.json, comparison.csv, comparison_report.html</p>" % (
            _h(LIMITATION_TEXT), _h(scenario.get("id", "")), _h(scenario.get("document_fingerprint", "")), _h(scenario.get("render_identity", "")), _h(comparison["target"]), comparison["tolerance_seconds"], "".join(rows), comparison["metrics"]["missing_pulse"]["opportunity_count"], comparison["metrics"]["missing_pulse"]["detection_count"]))


def _beat_classification_html(report_json):
    summary = report_json["summary"]
    class_rows = []
    for item in report_json["classes"]:
        class_rows.append("<tr><td>%s%s</td><td>%s</td><td>%s</td><td>%.6g</td><td>%.6g</td><td>%.6g</td></tr>" % (
            _h(item["class"]), "" if item["scored"] else " (unscored)", item["ground_truth_count"], item["prediction_count"], item["precision"], item["recall"], item["f1_score"]))
    return _html_page(
        "ECG Beat Classification QA Report",
        "<h1>ECG Beat Classification QA Report</h1><p class=\"notice\">%s</p><table><tr><th>Accuracy</th><td>%.6g</td></tr><tr><th>Micro F1</th><td>%.6g</td></tr><tr><th>Correct / scored ground truth</th><td>%s / %s</td></tr><tr><th>Timing tolerance</th><td>%.6g s</td></tr></table><h2>Per-class metrics</h2><table><tr><th>Class</th><th>GT</th><th>Pred</th><th>Precision</th><th>Recall</th><th>F1</th></tr>%s</table>" % (
            _h(LIMITATION_TEXT), summary["accuracy"], summary["micro_f1_score"], summary["correct_count"], summary["scored_ground_truth_count"], report_json["tolerance_seconds"], "".join(class_rows)))


def _hrv_html(report_json):
    metric_rows = []
    for item in report_json["metrics"]:
        metric_rows.append("<tr><td>%s</td><td>%.8g</td><td>%.8g</td><td>%.8g</td><td>%s</td></tr>" % (
            _h(item["name"]), item["ground_truth"], item["user"], item["absolute_error"], "PASS" if item["passed"] else "FAIL"))
    rr = report_json["rr"]
    return _html_page(
        "HRV Algorithm QA Score",
        "<h1>HRV Algorithm QA Score</h1><p class=\"notice\">%s</p><table><tr><th>Scenario</th><td>%s</td></tr><tr><th>Metric pass fraction</th><td>%.6g</td></tr></table><h2>Metrics</h2><table><tr><th>Metric</th><th>GT</th><th>User</th><th>Abs error</th><th>Result</th></tr>%s</table><h2>RR intervals</h2><table><tr><th>Evaluated</th><th>GT</th><th>User</th><th>Matched</th><th>Missing</th><th>Extra</th><th>Passed</th><th>MAE s</th></tr><tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%.6g</td></tr></table>" % (
            _h(LIMITATION_TEXT), _h(report_json["scenario"].get("id", "")), report_json["metric_pass_fraction"], "".join(metric_rows),
            "yes" if rr["evaluated"] else "no", rr["ground_truth_count"], rr["user_count"], rr["matched_count"], rr["missing_count"], rr["extra_count"], rr["passed_count"], rr["mean_absolute_error_seconds"]))


def _summary_html(summary):
    rows = []
    for item in summary["cases"]:
        metric_data = item.get("metrics", {})
        event_f1 = metric_data.get("total", {}).get("f1_score", "") if isinstance(metric_data, dict) else ""
        metric = item.get("summary", {}).get("micro_f1_score", item.get("summary", {}).get("metric_pass_fraction", event_f1))
        rows.append("<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>" % (_h(item.get("case_id", "")), _h(item.get("target", "")), _h(item.get("status", "")), _h(str(metric)), _h(item.get("report_path", ""))))
    failed_checks = [item for item in summary["policy"]["checks"] if item["applicable"] and not item["passed"]]
    check_rows = ["<tr><td>%s</td><td>%s</td><td>%s</td><td>%.6g</td><td>%s %.6g</td></tr>" % (_h(item["target"]), _h(item["section"]), _h(item["metric"]), item["actual"], _h(item["operator"]), item["threshold"]) for item in failed_checks]
    return _html_page("Synsigra Local Verification Report", "<h1>Synsigra Local Verification Report</h1><p class=\"notice\">%s</p><table><tr><th>Package</th><td>%s</td></tr><tr><th>Status</th><td>%s</td></tr><tr><th>Threshold profile</th><td>%s</td></tr><tr><th>Successfully scored case-targets</th><td>%s / %s</td></tr><tr><th>Failed policy checks</th><td>%s</td></tr></table><h2>Cases</h2><table><tr><th>Case</th><th>Target</th><th>Status</th><th>Primary score</th><th>Report</th></tr>%s</table><h2>Failed policy checks</h2><table><tr><th>Target</th><th>Section</th><th>Metric</th><th>Actual</th><th>Required</th></tr>%s</table>" % (_h(summary["limitation"]), _h(summary["package"].get("package_id", "")), _h(summary["status"]), _h(summary["policy"]["profile_id"]), summary["scored_case_target_count"], summary["case_target_count"], summary["policy"]["failed_check_count"], "".join(rows), "".join(check_rows)))


def _error_html(report):
    return _html_page("Synsigra Verification Error", "<h1>Synsigra Verification Error</h1><p class=\"notice\">%s</p><table><tr><th>Target</th><td>%s</td></tr><tr><th>Status</th><td>%s</td></tr><tr><th>Message</th><td>%s</td></tr></table>" % (_h(LIMITATION_TEXT), _h(report.get("target", "")), _h(report.get("status", "")), _h(report.get("message", ""))))


def _html_page(title, body):
    return "<!doctype html><html><head><meta charset=\"utf-8\"><title>%s</title><style>body{font:14px Arial,sans-serif;color:#202124;max-width:1100px;margin:32px auto;padding:0 20px}h1,h2{color:#111827}table{border-collapse:collapse;width:100%%;margin:12px 0 24px}th,td{border:1px solid #d1d5db;padding:7px;text-align:left}th{background:#f3f4f6}.notice{border-left:4px solid #b42318;padding:10px 14px;background:#fef3f2}</style></head><body>%s</body></html>" % (_h(title), body)


def _empty_event_metrics():
    return {
        "ground_truth_count": 0,
        "detection_count": 0,
        "true_positive_count": 0,
        "false_positive_count": 0,
        "false_negative_count": 0,
        "sensitivity": 0.0,
        "positive_predictive_value": 0.0,
        "f1_score": 0.0,
        "mean_absolute_error_seconds": 0.0,
        "median_absolute_error_seconds": 0.0,
        "rms_error_seconds": 0.0,
        "max_absolute_error_seconds": 0.0,
    }


def _empty_class_metrics(scored):
    return {
        "scored": scored,
        "ground_truth_count": 0,
        "prediction_count": 0,
        "true_positive_count": 0,
        "false_positive_count": 0,
        "false_negative_count": 0,
        "precision": 0.0,
        "recall": 0.0,
        "f1_score": 0.0,
    }


def _class_row(name, metrics):
    row = dict(metrics)
    row["class"] = name
    return row


def _finalize_event_metrics(metrics, absolute_errors):
    metrics["sensitivity"] = _ratio(metrics["true_positive_count"], metrics["ground_truth_count"])
    metrics["positive_predictive_value"] = _ratio(metrics["true_positive_count"], metrics["detection_count"])
    metrics["f1_score"] = _f1(metrics["sensitivity"], metrics["positive_predictive_value"])
    if absolute_errors:
        sorted_errors = sorted(absolute_errors)
        total = sum(sorted_errors)
        metrics["mean_absolute_error_seconds"] = total / len(sorted_errors)
        metrics["rms_error_seconds"] = math.sqrt(sum(item * item for item in sorted_errors) / len(sorted_errors))
        metrics["max_absolute_error_seconds"] = sorted_errors[-1]
        middle = len(sorted_errors) // 2
        if len(sorted_errors) & 1:
            metrics["median_absolute_error_seconds"] = sorted_errors[middle]
        else:
            metrics["median_absolute_error_seconds"] = 0.5 * (sorted_errors[middle - 1] + sorted_errors[middle])


def _bin_metrics(in_artifact_interval, clean, artifact):
    return artifact if in_artifact_interval else clean


def _ratio(numerator, denominator):
    return float(numerator) / float(denominator) if denominator else 0.0


def _f1(recall_or_precision, precision_or_recall):
    return 2.0 * recall_or_precision * precision_or_recall / (recall_or_precision + precision_or_recall) if recall_or_precision + precision_or_recall > 0.0 else 0.0


def _mean(values):
    return sum(values) / float(len(values)) if values else 0.0


def _rms(values):
    return math.sqrt(sum(item * item for item in values) / len(values)) if values else 0.0


def _default_tolerance_seconds(target):
    if target == "ppg_systolic_peak":
        return 0.080
    if target == "ecg_beat_classification":
        return 0.075
    return 0.050


def _finite_non_negative(value):
    try:
        numeric = float(value)
    except (TypeError, ValueError):
        return False
    return numeric >= 0.0 and math.isfinite(numeric)


def _finite_positive(value):
    return _finite_non_negative(value) and float(value) > 0.0


def _detection_identity(detections):
    if not isinstance(detections, DetectionDocument):
        raise TypeError("detections must be a DetectionDocument")
    return {
        "path": detections.path,
        "sha256": _sha256_file(detections.path),
        "target": detections.target,
        "algorithm": {"name": detections.algorithm_name, "version": detections.algorithm_version},
        "event_count": len(detections),
    }


def _scenario_identity(case, case_summary, annotations):
    identity = {
        "id": case_summary.get("scenario_id", case.scenario_id),
        "case_id": case.id,
        "document_fingerprint": case_summary.get("document_fingerprint", case.document_fingerprint),
        "render_identity": case_summary.get("render_identity", case.render_identity),
    }
    if "generation_fingerprint" in annotations:
        identity["generation_fingerprint"] = annotations["generation_fingerprint"]
    return identity


def _package_identity(package, scoring_manifest=None):
    identity = {
        "package_id": package.package_id,
        "name": package.name,
        "version": package.version,
        "package_type": package.manifest.get("package_type", ""),
        "generator_version": package.manifest.get("generator_version", ""),
        "ground_truth_included": package.manifest.get("ground_truth_included", False),
        "usage_restrictions": package.manifest.get("usage_restrictions", ""),
        "not_for": package.manifest.get("not_for", ""),
    }
    if scoring_manifest is not None:
        identity["pack_fingerprint"] = scoring_manifest.get("pack_fingerprint", "")
        if not identity["generator_version"]:
            identity["generator_version"] = scoring_manifest.get("generator_version", "")
    return identity


def _normalize_filter(value):
    if value is None:
        return None
    if isinstance(value, str):
        return set([value])
    return set(value)


def _as_package(challenge):
    if isinstance(challenge, ChallengePackage):
        return challenge, False
    return load_challenge(challenge), True


def _prepare_output_dir(out_dir, force):
    if os.path.exists(out_dir):
        if not force:
            raise VerificationError("output directory already exists: %s" % out_dir)
        shutil.rmtree(out_dir)
    os.makedirs(out_dir)


def _ensure_dir(path):
    if not os.path.isdir(path):
        os.makedirs(path)


def _write_json(path, data):
    with open(path, "w") as handle:
        json.dump(data, handle, sort_keys=True, indent=2)
        handle.write("\n")


def _write_text(path, text):
    parent = os.path.dirname(path)
    if parent:
        _ensure_dir(parent)
    with open(path, "w") as handle:
        handle.write(text)


def _sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        while True:
            block = handle.read(1024 * 1024)
            if not block:
                break
            digest.update(block)
    return "sha256:" + digest.hexdigest()


def _csv_cell(value):
    output = io.StringIO()
    writer = csv.writer(output)
    writer.writerow([value])
    return output.getvalue().strip()


def _relative_to_parent(path):
    parts = path.replace("\\", "/").split("/")
    if len(parts) >= 2:
        return "/".join(parts[-2:])
    return parts[-1]


def _h(value):
    return html.escape(str(value), quote=True)
