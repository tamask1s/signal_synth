import csv
import json
import math
import os


class DetectionEvent(object):
    def __init__(self, time_seconds, sample_index=None, channel="", label="", confidence=None, original_index=0):
        self.time_seconds = float(time_seconds)
        self.sample_index = sample_index
        self.channel = channel
        self.label = label
        self.confidence = confidence
        self.original_index = int(original_index)


class DetectionDocument(object):
    def __init__(self, path, target, events, algorithm_name="", algorithm_version="", raw=None):
        self.path = path
        self.target = target
        self.events = list(events)
        self.algorithm_name = algorithm_name
        self.algorithm_version = algorithm_version
        self.raw = raw or {}

    def __len__(self):
        return len(self.events)


def load_detections(path, target=None, format_name=None):
    with open(path, "r") as handle:
        if format_name == "point_events_json_v1":
            return _load_point_events_json(path, handle, target)
        if format_name == "point_events_csv_v1":
            return _load_point_events_csv(path, handle, target)
        prefix = handle.read(4096)
        handle.seek(0)
        first = prefix.lstrip()[:1]
        if first == "{":
            return _load_json(path, handle, target)
        return _load_csv(path, handle, target)


def _load_json(path, handle, target):
    document = json.load(handle)
    algorithm = document.get("algorithm", {})
    actual_target = document.get("target", target or "")
    if target is not None and actual_target != target:
        raise ValueError("detection target mismatch: expected %s, got %s" % (target, actual_target))
    events = []
    for index, item in enumerate(document.get("events", [])):
        events.append(DetectionEvent(
            item["time_seconds"],
            item.get("sample_index"),
            item.get("channel", ""),
            item.get("label", ""),
            item.get("confidence"),
            index,
        ))
    return DetectionDocument(path, actual_target, events, algorithm.get("name", ""), algorithm.get("version", ""), document)


def _load_point_events_json(path, handle, target):
    document = json.load(handle, object_pairs_hook=_unique_object_pairs)
    if not isinstance(document, dict) or set(document.keys()) != set(["schema_version", "events"]):
        raise ValueError("point-event JSON must contain only schema_version and events")
    if isinstance(document.get("schema_version"), bool) or document.get("schema_version") != 1 or not isinstance(document.get("events"), list):
        raise ValueError("point-event JSON requires schema_version 1 and an events array")
    events = [_point_event(item, index) for index, item in enumerate(document["events"])]
    _require_unique_point_events(events)
    return DetectionDocument(path, target or "", events, raw=document)


def _load_point_events_csv(path, handle, target):
    reader = csv.DictReader(handle)
    fields = reader.fieldnames or []
    allowed = set(["time_seconds", "sample_index", "channel", "label", "confidence"])
    if len(fields) != len(set(fields)) or "time_seconds" not in fields or not set(fields).issubset(allowed):
        raise ValueError("point-event CSV has missing, duplicate, or unknown columns")
    events = []
    for index, row in enumerate(reader):
        if None in row:
            raise ValueError("point-event CSV row has more cells than the header")
        events.append(_point_event(row, index, csv_input=True))
    _require_unique_point_events(events)
    return DetectionDocument(path, target or "", events)


def _point_event(item, index, csv_input=False):
    if not isinstance(item, dict):
        raise ValueError("point event %s must be an object" % index)
    allowed = set(["time_seconds", "sample_index", "channel", "label", "confidence"])
    if "time_seconds" not in item or set(item.keys()) - allowed:
        raise ValueError("point event %s has missing or unknown fields" % index)
    time_seconds = item["time_seconds"]
    sample_index = item.get("sample_index")
    channel = item.get("channel", "")
    label = item.get("label", "")
    confidence = item.get("confidence")
    if csv_input:
        if time_seconds == "":
            raise ValueError("point event %s requires time_seconds" % index)
        sample_index = sample_index or None
        confidence = confidence or None
    elif isinstance(time_seconds, bool) or not isinstance(time_seconds, (int, float)):
        raise ValueError("point event %s time_seconds must be a number" % index)
    if not isinstance(channel, str) or not isinstance(label, str):
        raise ValueError("point event %s channel and label must be strings" % index)
    try:
        time_seconds = float(time_seconds)
    except (TypeError, ValueError):
        raise ValueError("point event %s time_seconds must be a number" % index)
    if not math.isfinite(time_seconds) or time_seconds < 0.0:
        raise ValueError("point event %s time_seconds must be finite and non-negative" % index)
    if sample_index is not None:
        if isinstance(sample_index, bool) or (not csv_input and not isinstance(sample_index, int)):
            raise ValueError("point event %s sample_index must be a non-negative integer" % index)
        try:
            parsed_sample_index = int(sample_index)
        except (TypeError, ValueError):
            raise ValueError("point event %s sample_index must be a non-negative integer" % index)
        if parsed_sample_index < 0 or parsed_sample_index > 9007199254740991 or (csv_input and str(parsed_sample_index) != sample_index):
            raise ValueError("point event %s sample_index must be an exact non-negative JSON integer" % index)
        sample_index = parsed_sample_index
    if confidence is not None:
        if isinstance(confidence, bool) or (not csv_input and not isinstance(confidence, (int, float))):
            raise ValueError("point event %s confidence must be a number" % index)
        try:
            confidence = float(confidence)
        except (TypeError, ValueError):
            raise ValueError("point event %s confidence must be a number" % index)
        if not math.isfinite(confidence) or confidence < 0.0 or confidence > 1.0:
            raise ValueError("point event %s confidence must be in [0,1]" % index)
    return DetectionEvent(
        time_seconds,
        sample_index,
        channel,
        label,
        confidence,
        index,
    )


def _require_unique_point_events(events):
    identities = set()
    for item in events:
        identity = (item.time_seconds, item.channel, item.label)
        if identity in identities:
            raise ValueError("duplicate point event with identical time, channel, and label")
        identities.add(identity)


def _unique_object_pairs(pairs):
    output = {}
    for key, value in pairs:
        if key in output:
            raise ValueError("duplicate JSON object key: %s" % key)
        output[key] = value
    return output


def _load_csv(path, handle, target):
    reader = csv.DictReader(handle)
    if "time_seconds" not in (reader.fieldnames or []):
        raise ValueError("detection CSV must contain time_seconds")
    events = []
    for index, row in enumerate(reader):
        sample_index = row.get("sample_index") or None
        confidence = row.get("confidence") or None
        events.append(DetectionEvent(
            row["time_seconds"],
            int(sample_index) if sample_index is not None else None,
            row.get("channel", ""),
            row.get("label", ""),
            float(confidence) if confidence is not None else None,
            index,
        ))
    actual_target = target or _target_from_filename(path)
    return DetectionDocument(path, actual_target, events, "csv_detection_input", "v2", {})


def _target_from_filename(path):
    name = os.path.basename(path).lower()
    if "ppg" in name:
        return "ppg_systolic_peak"
    return "r_peak"
