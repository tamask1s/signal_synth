import csv
import json
import math


INTERVAL_TARGETS = set(["rhythm_episode", "signal_quality"])


class IntervalEvent(object):
    def __init__(self, start_seconds, end_seconds, label, channel="global", confidence=None, original_index=0):
        self.start_seconds = float(start_seconds)
        self.end_seconds = float(end_seconds)
        self.label = str(label)
        self.channel = str(channel or "global")
        self.confidence = confidence
        self.original_index = int(original_index)


class IntervalDocument(object):
    def __init__(self, path, target, intervals, algorithm_name="", algorithm_version="", raw=None):
        self.path = path
        self.target = target
        self.intervals = list(intervals)
        self.algorithm_name = algorithm_name
        self.algorithm_version = algorithm_version
        self.raw = raw or {}

    def __len__(self):
        return len(self.intervals)


def load_intervals(path, target=None):
    with open(path, "r") as handle:
        prefix = handle.read(4096)
        handle.seek(0)
        if prefix.lstrip()[:1] == "{":
            document = _load_json(path, handle, target)
        else:
            document = _load_csv(path, handle, target)
    _validate_document(document)
    document.intervals.sort(key=lambda item: (item.start_seconds, item.end_seconds, item.channel, item.label, item.original_index))
    return document


def score_interval_events(ground_truth, predictions, record_duration_seconds, minimum_iou=0.1):
    duration = float(record_duration_seconds)
    minimum_iou = float(minimum_iou)
    if not math.isfinite(duration) or duration <= 0.0:
        raise ValueError("record duration must be finite and positive")
    if not math.isfinite(minimum_iou) or minimum_iou <= 0.0 or minimum_iou > 1.0:
        raise ValueError("minimum IoU must be in the interval (0,1]")
    _validate_interval_collection(ground_truth, duration, "ground truth")
    _validate_interval_collection(predictions, duration, "prediction")
    labels = sorted(set([item.label for item in ground_truth] + [item.label for item in predictions]))
    matches, truth_matched, prediction_matched = _select_matches(ground_truth, predictions, minimum_iou, True)
    match_rows = []
    onset_errors = []
    offset_errors = []
    for truth_index, prediction_index, iou, overlap in matches:
        truth = ground_truth[truth_index]
        prediction = predictions[prediction_index]
        onset_error = prediction.start_seconds - truth.start_seconds
        offset_error = prediction.end_seconds - truth.end_seconds
        onset_errors.append(onset_error)
        offset_errors.append(offset_error)
        match_rows.append({
            "ground_truth_index": truth.original_index,
            "prediction_index": prediction.original_index,
            "label": truth.label,
            "channel": truth.channel,
            "intersection_over_union": iou,
            "overlap_seconds": overlap,
            "onset_error_seconds": onset_error,
            "offset_error_seconds": offset_error,
        })
    class_rows = []
    overall = _empty_metrics()
    overall["ground_truth_count"] = len(ground_truth)
    overall["prediction_count"] = len(predictions)
    overall["matched_count"] = len(matches)
    for label in labels:
        label_truth = [item for item in ground_truth if item.label == label]
        label_predictions = [item for item in predictions if item.label == label]
        label_matches = [row for row in match_rows if row["label"] == label]
        metrics = _empty_metrics()
        metrics["ground_truth_count"] = len(label_truth)
        metrics["prediction_count"] = len(label_predictions)
        metrics["matched_count"] = len(label_matches)
        truth_duration, prediction_duration, overlap_duration = _duration_metrics(label, ground_truth, predictions)
        metrics["ground_truth_duration_seconds"] = truth_duration
        metrics["prediction_duration_seconds"] = prediction_duration
        metrics["overlap_duration_seconds"] = overlap_duration
        _finalize_metrics(metrics, duration, [row["onset_error_seconds"] for row in label_matches], [row["offset_error_seconds"] for row in label_matches])
        for key in ("ground_truth_duration_seconds", "prediction_duration_seconds", "overlap_duration_seconds"):
            overall[key] += metrics[key]
        class_rows.append({"label": label, "metrics": metrics})
    _finalize_metrics(overall, duration, onset_errors, offset_errors)

    confusion_matches, confusion_truth_matched, confusion_prediction_matched = _select_matches(ground_truth, predictions, minimum_iou, False)
    confusion = {}
    for truth_index, prediction_index, _iou, _overlap in confusion_matches:
        key = (ground_truth[truth_index].label, predictions[prediction_index].label)
        confusion[key] = confusion.get(key, 0) + 1
    for index, item in enumerate(ground_truth):
        if not confusion_truth_matched[index]:
            key = (item.label, "__missed__")
            confusion[key] = confusion.get(key, 0) + 1
    for index, item in enumerate(predictions):
        if not confusion_prediction_matched[index]:
            key = ("__false_alarm__", item.label)
            confusion[key] = confusion.get(key, 0) + 1
    confusion_rows = [{"ground_truth_label": key[0], "prediction_label": key[1], "count": confusion[key]} for key in sorted(confusion.keys())]
    return {
        "overall": overall,
        "classes": class_rows,
        "confusion_matrix": confusion_rows,
        "matches": match_rows,
        "false_positive_indices": [item.original_index for index, item in enumerate(predictions) if not prediction_matched[index]],
        "false_negative_indices": [item.original_index for index, item in enumerate(ground_truth) if not truth_matched[index]],
    }


def _unique_object_pairs(pairs):
    output = {}
    for key, value in pairs:
        if key in output:
            raise ValueError("duplicate JSON object key: %s" % key)
        output[key] = value
    return output


def _load_json(path, handle, expected_target):
    raw = json.load(handle, object_pairs_hook=_unique_object_pairs)
    if not isinstance(raw, dict) or set(raw.keys()) != set(["schema_version", "algorithm", "target", "intervals"]):
        raise ValueError("interval JSON must contain only schema_version, algorithm, target, and intervals")
    if isinstance(raw.get("schema_version"), bool) or raw.get("schema_version") != 1:
        raise ValueError("interval JSON schema_version must be 1")
    algorithm = raw.get("algorithm")
    if not isinstance(algorithm, dict) or set(algorithm.keys()) != set(["name", "version"]) or not isinstance(algorithm.get("name"), str) or not isinstance(algorithm.get("version"), str):
        raise ValueError("interval algorithm must contain only name and version")
    target = raw.get("target", "")
    if not isinstance(target, str):
        raise ValueError("interval target must be a string")
    if expected_target is not None and target != expected_target:
        raise ValueError("interval target mismatch: expected %s, got %s" % (expected_target, target))
    raw_intervals = raw.get("intervals")
    if not isinstance(raw_intervals, list):
        raise ValueError("intervals must be an array")
    intervals = []
    allowed = set(["start_seconds", "end_seconds", "label", "channel", "confidence"])
    required = set(["start_seconds", "end_seconds", "label"])
    for index, item in enumerate(raw_intervals):
        if not isinstance(item, dict) or not required.issubset(set(item.keys())) or not set(item.keys()).issubset(allowed):
            raise ValueError("interval %s has missing or unknown fields" % index)
        if any(isinstance(item[name], bool) or not isinstance(item[name], (int, float)) for name in ("start_seconds", "end_seconds")) or not isinstance(item["label"], str):
            raise ValueError("interval %s has invalid field types" % index)
        if "channel" in item and not isinstance(item["channel"], str):
            raise ValueError("interval %s channel must be a string" % index)
        if "confidence" in item and (isinstance(item["confidence"], bool) or not isinstance(item["confidence"], (int, float))):
            raise ValueError("interval %s confidence must be a number" % index)
        intervals.append(IntervalEvent(item["start_seconds"], item["end_seconds"], item["label"], item.get("channel", "global"), item.get("confidence"), index))
    return IntervalDocument(path, target, intervals, algorithm["name"], algorithm["version"], raw)


def _load_csv(path, handle, target):
    if target is None:
        raise ValueError("target is required when loading interval CSV")
    reader = csv.DictReader(handle)
    fields = reader.fieldnames or []
    allowed = set(["start_seconds", "end_seconds", "label", "channel", "confidence"])
    if len(fields) != len(set(fields)) or not set(["start_seconds", "end_seconds", "label"]).issubset(set(fields)) or not set(fields).issubset(allowed):
        raise ValueError("interval CSV has missing, duplicate, or unknown columns")
    intervals = []
    for index, row in enumerate(reader):
        if None in row:
            raise ValueError("interval CSV row has more cells than the header")
        confidence = row.get("confidence") or None
        intervals.append(IntervalEvent(row["start_seconds"], row["end_seconds"], row["label"], row.get("channel", "global"), float(confidence) if confidence is not None else None, index))
    return IntervalDocument(path, target, intervals, "csv_interval_input", "v1", {})


def _validate_document(document):
    if document.target not in INTERVAL_TARGETS:
        raise ValueError("interval target must be rhythm_episode or signal_quality")
    if len(document.algorithm_name) > 128 or len(document.algorithm_version) > 128:
        raise ValueError("interval algorithm name and version must contain at most 128 characters")
    duplicates = set()
    for item in document.intervals:
        _validate_interval(item)
        if document.target == "rhythm_episode" and item.channel != "global":
            raise ValueError("rhythm_episode intervals must use channel global")
        key = (item.start_seconds, item.end_seconds, item.label, item.channel)
        if key in duplicates:
            raise ValueError("duplicate interval with identical bounds, label, and channel")
        duplicates.add(key)


def _validate_interval(item, record_duration_seconds=None):
    if not math.isfinite(item.start_seconds) or not math.isfinite(item.end_seconds) or item.start_seconds < 0.0 or item.end_seconds <= item.start_seconds:
        raise ValueError("interval bounds must be finite, non-negative, and increasing")
    if record_duration_seconds is not None and item.end_seconds > record_duration_seconds + 1e-9:
        raise ValueError("interval lies outside the record")
    if not item.label or len(item.label) > 64 or not item.channel or len(item.channel) > 64:
        raise ValueError("interval label/channel length is invalid")
    if item.confidence is not None and (not math.isfinite(float(item.confidence)) or float(item.confidence) < 0.0 or float(item.confidence) > 1.0):
        raise ValueError("interval confidence must be in [0,1]")


def _validate_interval_collection(intervals, record_duration_seconds, role):
    identities = set()
    for item in intervals:
        _validate_interval(item, record_duration_seconds)
        identity = (item.start_seconds, item.end_seconds, item.label, item.channel)
        if identity in identities:
            raise ValueError("%s contains a duplicate interval" % role)
        identities.add(identity)


def _select_matches(ground_truth, predictions, minimum_iou, require_same_label):
    candidates = []
    for truth_index, truth in enumerate(ground_truth):
        for prediction_index, prediction in enumerate(predictions):
            if truth.channel != prediction.channel or (require_same_label and truth.label != prediction.label):
                continue
            overlap = max(0.0, min(truth.end_seconds, prediction.end_seconds) - max(truth.start_seconds, prediction.start_seconds))
            if overlap <= 0.0:
                continue
            union = truth.end_seconds - truth.start_seconds + prediction.end_seconds - prediction.start_seconds - overlap
            iou = overlap / union if union > 0.0 else 0.0
            if iou + 1e-15 < minimum_iou:
                continue
            boundary_error = abs(prediction.start_seconds - truth.start_seconds) + abs(prediction.end_seconds - truth.end_seconds)
            candidates.append((-iou, boundary_error, truth_index, prediction_index, overlap))
    candidates.sort()
    truth_matched = [False] * len(ground_truth)
    prediction_matched = [False] * len(predictions)
    matches = []
    for negative_iou, _boundary_error, truth_index, prediction_index, overlap in candidates:
        if truth_matched[truth_index] or prediction_matched[prediction_index]:
            continue
        truth_matched[truth_index] = True
        prediction_matched[prediction_index] = True
        matches.append((truth_index, prediction_index, -negative_iou, overlap))
    return matches, truth_matched, prediction_matched


def _duration_metrics(label, ground_truth, predictions):
    channels = sorted(set([item.channel for item in ground_truth if item.label == label] + [item.channel for item in predictions if item.label == label]))
    truth_duration = 0.0
    prediction_duration = 0.0
    overlap_duration = 0.0
    for channel in channels:
        truth_segments = _merged_segments(ground_truth, label, channel)
        prediction_segments = _merged_segments(predictions, label, channel)
        truth_duration += sum(end - start for start, end in truth_segments)
        prediction_duration += sum(end - start for start, end in prediction_segments)
        overlap_duration += _segment_overlap(truth_segments, prediction_segments)
    return truth_duration, prediction_duration, overlap_duration


def _merged_segments(intervals, label, channel):
    segments = sorted((item.start_seconds, item.end_seconds) for item in intervals if item.label == label and item.channel == channel)
    merged = []
    for start, end in segments:
        if not merged or start > merged[-1][1]:
            merged.append([start, end])
        elif end > merged[-1][1]:
            merged[-1][1] = end
    return merged


def _segment_overlap(left, right):
    output = 0.0
    left_index = 0
    right_index = 0
    while left_index < len(left) and right_index < len(right):
        start = max(left[left_index][0], right[right_index][0])
        end = min(left[left_index][1], right[right_index][1])
        if end > start:
            output += end - start
        if left[left_index][1] < right[right_index][1]:
            left_index += 1
        else:
            right_index += 1
    return output


def _empty_metrics():
    return {
        "ground_truth_count": 0, "prediction_count": 0, "matched_count": 0, "false_alarm_count": 0, "missed_count": 0,
        "ground_truth_duration_seconds": 0.0, "prediction_duration_seconds": 0.0, "overlap_duration_seconds": 0.0,
        "time_sensitivity": None, "time_precision": None, "time_f1_score": None, "temporal_iou": None,
        "event_sensitivity": None, "event_precision": None, "false_alarms_per_hour": 0.0,
        "onset_error_seconds": {"mean": None, "mean_absolute": None, "median_absolute": None, "max_absolute": None},
        "offset_error_seconds": {"mean": None, "mean_absolute": None, "median_absolute": None, "max_absolute": None},
    }


def _finalize_metrics(metrics, record_duration_seconds, onset_errors, offset_errors):
    metrics["false_alarm_count"] = metrics["prediction_count"] - metrics["matched_count"]
    metrics["missed_count"] = metrics["ground_truth_count"] - metrics["matched_count"]
    truth_duration = metrics["ground_truth_duration_seconds"]
    prediction_duration = metrics["prediction_duration_seconds"]
    overlap = metrics["overlap_duration_seconds"]
    metrics["time_sensitivity"] = overlap / truth_duration if truth_duration > 0.0 else None
    metrics["time_precision"] = overlap / prediction_duration if prediction_duration > 0.0 else None
    metrics["time_f1_score"] = 2.0 * overlap / (truth_duration + prediction_duration) if truth_duration + prediction_duration > 0.0 else None
    union = truth_duration + prediction_duration - overlap
    metrics["temporal_iou"] = overlap / union if union > 0.0 else None
    metrics["event_sensitivity"] = float(metrics["matched_count"]) / metrics["ground_truth_count"] if metrics["ground_truth_count"] else None
    metrics["event_precision"] = float(metrics["matched_count"]) / metrics["prediction_count"] if metrics["prediction_count"] else None
    metrics["false_alarms_per_hour"] = metrics["false_alarm_count"] * 3600.0 / record_duration_seconds
    metrics["onset_error_seconds"] = _error_metrics(onset_errors)
    metrics["offset_error_seconds"] = _error_metrics(offset_errors)


def _error_metrics(values):
    if not values:
        return {"mean": None, "mean_absolute": None, "median_absolute": None, "max_absolute": None}
    absolute = sorted(abs(value) for value in values)
    middle = len(absolute) // 2
    median_absolute = absolute[middle] if len(absolute) % 2 else 0.5 * (absolute[middle - 1] + absolute[middle])
    return {
        "mean": sum(values) / len(values),
        "mean_absolute": sum(absolute) / len(absolute),
        "median_absolute": median_absolute,
        "max_absolute": max(absolute),
    }
