import csv
import json
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


def load_detections(path, target=None):
    with open(path, "r") as handle:
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
