import csv
import json
import math


DELINEATION_KINDS = ["p_onset", "p_peak", "p_offset", "qrs_onset", "j_point", "qrs_offset", "t_onset", "t_peak", "t_offset"]
ECG_LEADS = ["I", "II", "III", "aVR", "aVL", "aVF", "V1", "V2", "V3", "V4", "V5", "V6"]


class DelineationEvent(object):
    def __init__(self, beat_index, lead, kind, time_seconds, confidence=None, original_index=0):
        self.beat_index = int(beat_index)
        self.lead = str(lead)
        self.kind = str(kind)
        self.time_seconds = float(time_seconds)
        self.confidence = confidence
        self.original_index = int(original_index)


class DelineationDocument(object):
    def __init__(self, path, scope_mode, leads, events, beat_indices=None, algorithm_name="", algorithm_version="", raw=None):
        self.path = path
        self.target = "ecg_delineation"
        self.scope_mode = str(scope_mode)
        self.beat_indices = list(beat_indices or [])
        self.leads = list(leads)
        self.events = list(events)
        self.algorithm_name = str(algorithm_name)
        self.algorithm_version = str(algorithm_version)
        self.raw = raw or {}

    def __len__(self):
        return len(self.events)


def load_delineations(path):
    with open(path, "r") as handle:
        prefix = handle.read(4096)
        handle.seek(0)
        document = _load_json(path, handle) if prefix.lstrip()[:1] == "{" else _load_csv(path, handle)
    _validate_document(document)
    document.beat_indices.sort()
    document.leads.sort(key=ECG_LEADS.index)
    document.events.sort(key=lambda item: (item.beat_index, ECG_LEADS.index(item.lead), DELINEATION_KINDS.index(item.kind), item.original_index))
    return document


def delineation_truth_from_annotations(annotations, document):
    if annotations.get("fiducial_detail") != "full":
        raise ValueError("delineation scoring requires full fiducials; compact challenge output is not supported")
    beats = [int(item["beat_index"]) for item in annotations.get("beats", [])]
    selected = list(beats) if document.scope_mode == "all_beats" else list(document.beat_indices)
    missing = [beat for beat in selected if beat not in set(beats)]
    if missing:
        raise ValueError("selected delineation beat does not exist in challenge annotations: %s" % missing[0])
    construction = {}
    measured = {}
    for item in annotations.get("fiducials", []):
        if not item.get("present", False):
            continue
        beat = int(item.get("beat_index", -1))
        kind = str(item.get("kind", ""))
        source = str(item.get("source", ""))
        lead_index = int(item.get("lead_index", -1))
        if source == "construction" and lead_index == -1:
            construction.setdefault((beat, kind), float(item["time_seconds"]))
        elif source == "lead_measurement" and 0 <= lead_index < len(ECG_LEADS):
            measured.setdefault((beat, ECG_LEADS[lead_index], kind), float(item["time_seconds"]))
    truth = []

    def add(beat, lead, kind, time_seconds):
        if time_seconds is not None:
            truth.append(DelineationEvent(beat, lead, kind, time_seconds, original_index=len(truth)))

    for lead in document.leads:
        for beat in selected:
            p_peak = measured.get((beat, lead, "p_peak"))
            if p_peak is not None:
                add(beat, lead, "p_onset", construction.get((beat, "p_onset")))
                add(beat, lead, "p_peak", p_peak)
                add(beat, lead, "p_offset", construction.get((beat, "p_offset")))
            qrs_visible = any((beat, lead, kind) in measured for kind in ("q_peak", "r_peak", "s_peak"))
            if qrs_visible:
                add(beat, lead, "qrs_onset", construction.get((beat, "qrs_onset")))
                add(beat, lead, "j_point", construction.get((beat, "j_point")))
                add(beat, lead, "qrs_offset", construction.get((beat, "qrs_offset")))
            t_peak = measured.get((beat, lead, "t_peak"))
            if t_peak is not None:
                add(beat, lead, "t_onset", construction.get((beat, "t_onset")))
                add(beat, lead, "t_peak", t_peak)
                add(beat, lead, "t_offset", construction.get((beat, "t_offset")))
    truth.sort(key=lambda item: (item.beat_index, ECG_LEADS.index(item.lead), DELINEATION_KINDS.index(item.kind)))
    for index, item in enumerate(truth):
        item.original_index = index
    return truth


def score_delineation_events(ground_truth, predictions, record_duration_seconds, tolerance_seconds=0.040, scoped_leads=None):
    duration = float(record_duration_seconds)
    tolerance = float(tolerance_seconds)
    if not math.isfinite(duration) or duration <= 0.0:
        raise ValueError("record duration must be finite and positive")
    if not math.isfinite(tolerance) or tolerance <= 0.0:
        raise ValueError("delineation tolerance must be finite and positive")
    _validate_events(ground_truth, duration, "ground truth")
    _validate_events(predictions, duration, "prediction")
    prediction_by_identity = dict((_identity(item), index) for index, item in enumerate(predictions))
    used_predictions = set()
    matches = []
    missing = []
    for truth in ground_truth:
        prediction_index = prediction_by_identity.get(_identity(truth))
        if prediction_index is None:
            missing.append(truth)
            continue
        used_predictions.add(prediction_index)
        prediction = predictions[prediction_index]
        error = prediction.time_seconds - truth.time_seconds
        matches.append({
            "beat_index": str(truth.beat_index), "lead": truth.lead, "kind": truth.kind,
            "ground_truth_time_seconds": truth.time_seconds, "prediction_time_seconds": prediction.time_seconds,
            "error_seconds": error, "within_tolerance": abs(error) <= tolerance + 1e-15,
        })
    unexpected = [item for index, item in enumerate(predictions) if index not in used_predictions]
    leads = list(scoped_leads or [])
    for item in list(ground_truth) + list(predictions):
        if item.lead not in leads:
            leads.append(item.lead)
    leads.sort(key=ECG_LEADS.index)
    overall = _group_metrics(ground_truth, predictions, matches, missing, unexpected)
    by_kind = [{"kind": kind, "metrics": _group_metrics(ground_truth, predictions, matches, missing, unexpected, kind=kind)} for kind in DELINEATION_KINDS]
    by_lead = [{"lead": lead, "metrics": _group_metrics(ground_truth, predictions, matches, missing, unexpected, lead=lead)} for lead in leads]
    by_kind_lead = [{"kind": kind, "lead": lead, "metrics": _group_metrics(ground_truth, predictions, matches, missing, unexpected, kind, lead)} for lead in leads for kind in DELINEATION_KINDS]
    return {
        "overall": overall,
        "by_kind": by_kind,
        "by_lead": by_lead,
        "by_kind_lead": by_kind_lead,
        "matches": matches,
        "missing_events": [_event_dict(item) for item in missing],
        "unexpected_events": [_event_dict(item) for item in unexpected],
    }


def _unique_object_pairs(pairs):
    output = {}
    for key, value in pairs:
        if key in output:
            raise ValueError("duplicate JSON object key: %s" % key)
        output[key] = value
    return output


def _parse_uint64_string(value, name):
    if not isinstance(value, str) or not value or (len(value) > 1 and value[0] == "0") or not value.isdigit():
        raise ValueError("%s must be a canonical unsigned decimal string" % name)
    parsed = int(value)
    if parsed < 0 or parsed > 18446744073709551615:
        raise ValueError("%s is outside unsigned 64-bit range" % name)
    return parsed


def _load_json(path, handle):
    raw = json.load(handle, object_pairs_hook=_unique_object_pairs)
    if not isinstance(raw, dict) or set(raw.keys()) != set(["schema_version", "algorithm", "target", "scope", "events"]):
        raise ValueError("delineation JSON has missing or unknown top-level fields")
    if isinstance(raw.get("schema_version"), bool) or raw.get("schema_version") != 1 or raw.get("target") != "ecg_delineation":
        raise ValueError("delineation JSON requires schema_version 1 and target ecg_delineation")
    algorithm = raw.get("algorithm")
    if not isinstance(algorithm, dict) or set(algorithm.keys()) != set(["name", "version"]) or not all(isinstance(algorithm.get(name), str) for name in ("name", "version")):
        raise ValueError("delineation algorithm must contain only string name and version")
    scope = raw.get("scope")
    if not isinstance(scope, dict) or not set(["mode", "leads"]).issubset(set(scope.keys())) or not set(scope.keys()).issubset(set(["mode", "beat_indices", "leads"])):
        raise ValueError("delineation scope has missing or unknown fields")
    if not isinstance(scope.get("leads"), list) or any(not isinstance(item, str) for item in scope["leads"]):
        raise ValueError("delineation scope leads must be an array of strings")
    raw_beats = scope.get("beat_indices", [])
    if not isinstance(raw_beats, list):
        raise ValueError("delineation scope beat_indices must be an array")
    beats = [_parse_uint64_string(item, "scope beat index") for item in raw_beats]
    raw_events = raw.get("events")
    if not isinstance(raw_events, list):
        raise ValueError("delineation events must be an array")
    events = []
    allowed = set(["beat_index", "lead", "kind", "time_seconds", "confidence"])
    required = set(["beat_index", "lead", "kind", "time_seconds"])
    for index, item in enumerate(raw_events):
        if not isinstance(item, dict) or not required.issubset(set(item.keys())) or not set(item.keys()).issubset(allowed):
            raise ValueError("delineation event %s has missing or unknown fields" % index)
        if not isinstance(item["lead"], str) or not isinstance(item["kind"], str) or isinstance(item["time_seconds"], bool) or not isinstance(item["time_seconds"], (int, float)):
            raise ValueError("delineation event %s has invalid field types" % index)
        if "confidence" in item and (isinstance(item["confidence"], bool) or not isinstance(item["confidence"], (int, float))):
            raise ValueError("delineation event %s confidence must be a number" % index)
        events.append(DelineationEvent(_parse_uint64_string(item["beat_index"], "event beat index"), item["lead"], item["kind"], item["time_seconds"], item.get("confidence"), index))
    return DelineationDocument(path, scope.get("mode", ""), scope["leads"], events, beats, algorithm["name"], algorithm["version"], raw)


def _load_csv(path, handle):
    reader = csv.DictReader(handle)
    fields = reader.fieldnames or []
    required = set(["row_type", "scope_mode", "evaluated_beat_index", "beat_index", "lead", "kind", "time_seconds"])
    allowed = set(required) | set(["confidence"])
    if len(fields) != len(set(fields)) or not required.issubset(set(fields)) or not set(fields).issubset(allowed):
        raise ValueError("delineation CSV has missing, duplicate, or unknown columns")
    scope_mode = None
    beats = []
    leads = []
    events = []
    scope_rows = set()
    for row_index, row in enumerate(reader):
        if None in row:
            raise ValueError("delineation CSV row has more cells than the header")
        if row["row_type"] == "scope":
            mode = row["scope_mode"]
            if mode not in ("all_beats", "selected_beats") or (scope_mode is not None and mode != scope_mode):
                raise ValueError("delineation CSV scope modes are invalid or mixed")
            scope_mode = mode
            if not row["lead"]:
                raise ValueError("delineation CSV scope row requires lead")
            if row["lead"] not in leads:
                leads.append(row["lead"])
            evaluated = row["evaluated_beat_index"]
            if mode == "all_beats" and evaluated:
                raise ValueError("all_beats CSV scope must not contain evaluated beat indices")
            if mode == "selected_beats":
                beat = _parse_uint64_string(evaluated, "evaluated beat index")
                if beat not in beats:
                    beats.append(beat)
            if any(row.get(name, "") for name in ("beat_index", "kind", "time_seconds", "confidence")):
                raise ValueError("delineation CSV scope row contains event-only values")
            key = (mode, evaluated, row["lead"])
            if key in scope_rows:
                raise ValueError("duplicate delineation CSV scope row")
            scope_rows.add(key)
        elif row["row_type"] == "event":
            if row["scope_mode"] or row["evaluated_beat_index"]:
                raise ValueError("delineation CSV event row contains scope-only values")
            confidence = row.get("confidence") or None
            events.append(DelineationEvent(_parse_uint64_string(row["beat_index"], "event beat index"), row["lead"], row["kind"], float(row["time_seconds"]), float(confidence) if confidence is not None else None, len(events)))
        else:
            raise ValueError("delineation CSV row_type must be scope or event")
    if scope_mode is None:
        raise ValueError("delineation CSV requires at least one scope row")
    expected_scope_rows = len(leads) if scope_mode == "all_beats" else len(leads) * len(beats)
    if len(scope_rows) != expected_scope_rows:
        raise ValueError("delineation CSV scope must enumerate every evaluated beat and lead combination exactly once")
    return DelineationDocument(path, scope_mode, leads, events, beats, "csv_delineation_input", "v1")


def _validate_document(document):
    if document.scope_mode not in ("all_beats", "selected_beats"):
        raise ValueError("delineation scope mode must be all_beats or selected_beats")
    if document.scope_mode == "all_beats" and document.beat_indices:
        raise ValueError("all_beats scope must not contain beat_indices")
    if document.scope_mode == "selected_beats" and not document.beat_indices:
        raise ValueError("selected_beats scope requires beat_indices")
    if not document.leads or any(item not in ECG_LEADS for item in document.leads):
        raise ValueError("delineation scope requires standard ECG leads")
    if len(document.leads) != len(set(document.leads)) or len(document.beat_indices) != len(set(document.beat_indices)):
        raise ValueError("delineation scope contains duplicate leads or beats")
    if len(document.algorithm_name) > 128 or len(document.algorithm_version) > 128:
        raise ValueError("delineation algorithm metadata is too long")
    _validate_events(document.events, None, "prediction")
    beat_scope = set(document.beat_indices)
    lead_scope = set(document.leads)
    for item in document.events:
        if item.lead not in lead_scope or (document.scope_mode == "selected_beats" and item.beat_index not in beat_scope):
            raise ValueError("delineation event lies outside declared scope")


def _validate_events(events, duration, role):
    identities = set()
    for item in events:
        if item.lead not in ECG_LEADS or item.kind not in DELINEATION_KINDS or not math.isfinite(item.time_seconds) or item.time_seconds < 0.0:
            raise ValueError("%s delineation event is malformed" % role)
        if duration is not None and item.time_seconds > duration + 1e-9:
            raise ValueError("%s delineation event lies outside the record" % role)
        if item.confidence is not None and (not math.isfinite(float(item.confidence)) or not 0.0 <= float(item.confidence) <= 1.0):
            raise ValueError("%s delineation confidence must be in [0,1]" % role)
        key = _identity(item)
        if key in identities:
            raise ValueError("%s contains a duplicate delineation identity" % role)
        identities.add(key)


def _identity(item):
    return item.beat_index, item.lead, item.kind


def _event_dict(item):
    return {"beat_index": str(item.beat_index), "lead": item.lead, "kind": item.kind, "time_seconds": item.time_seconds}


def _group_metrics(truth, predictions, matches, missing, unexpected, kind=None, lead=None):
    def selected(item):
        item_kind = item.kind if hasattr(item, "kind") else item["kind"]
        item_lead = item.lead if hasattr(item, "lead") else item["lead"]
        return (kind is None or item_kind == kind) and (lead is None or item_lead == lead)

    selected_matches = [item for item in matches if selected(item)]
    errors = [item["error_seconds"] for item in selected_matches]
    within = sum(1 for item in selected_matches if item["within_tolerance"])
    truth_count = sum(1 for item in truth if selected(item))
    prediction_count = sum(1 for item in predictions if selected(item))
    missing_count = sum(1 for item in missing if selected(item))
    unexpected_count = sum(1 for item in unexpected if selected(item))
    out_count = len(selected_matches) - within
    denominator = truth_count + prediction_count
    absolute = sorted(abs(value) for value in errors)
    middle = len(absolute) // 2
    median = absolute[middle] if len(absolute) % 2 else (0.5 * (absolute[middle - 1] + absolute[middle]) if absolute else None)
    rank95 = int(math.ceil(0.95 * len(absolute))) - 1 if absolute else None
    return {
        "ground_truth_count": truth_count, "prediction_count": prediction_count, "paired_count": len(selected_matches),
        "within_tolerance_count": within, "missing_prediction_count": missing_count, "unexpected_prediction_count": unexpected_count,
        "out_of_tolerance_count": out_count, "false_negative_count": missing_count + out_count, "false_positive_count": unexpected_count + out_count,
        "sensitivity": float(within) / truth_count if truth_count else None,
        "positive_predictive_value": float(within) / prediction_count if prediction_count else None,
        "f1_score": 2.0 * within / denominator if denominator else None,
        "within_tolerance_fraction": float(within) / len(selected_matches) if selected_matches else None,
        "timing_error_seconds": {
            "mean": sum(errors) / len(errors) if errors else None,
            "mean_absolute": sum(absolute) / len(absolute) if absolute else None,
            "median_absolute": median,
            "rms": math.sqrt(sum(value * value for value in errors) / len(errors)) if errors else None,
            "p95_absolute": absolute[rank95] if absolute else None,
            "max_absolute": max(absolute) if absolute else None,
        },
    }
