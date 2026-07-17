import math

from .detections import load_detections


DELINEATION_KINDS = ["p_onset", "p_peak", "p_offset", "qrs_onset", "j_point", "qrs_offset", "t_onset", "t_peak", "t_offset"]
ECG_LEADS = ["I", "II", "III", "aVR", "aVL", "aVF", "V1", "V2", "V3", "V4", "V5", "V6"]


class DelineationEvent(object):
    def __init__(self, lead, kind, time_seconds, confidence=None, original_index=0):
        self.lead = str(lead)
        self.kind = str(kind)
        self.time_seconds = float(time_seconds)
        self.confidence = confidence
        self.original_index = int(original_index)


class DelineationDocument(object):
    def __init__(self, path, events, raw=None):
        self.path = path
        self.target = "ecg_delineation"
        self.events = list(events)
        self.raw = raw or {}

    def __len__(self):
        return len(self.events)


class DelineationScope(object):
    def __init__(self, leads, windows=None):
        self.leads = list(leads)
        self.windows = [tuple(item) for item in (windows or [])]


class DelineationTruthPoint(object):
    def __init__(self, anchor_type, anchor_index, lead, kind, status, reason, time_seconds, evaluation_start_seconds, evaluation_end_seconds, original_index=0):
        self.anchor_type = str(anchor_type)
        self.anchor_index = int(anchor_index)
        self.lead = str(lead)
        self.kind = str(kind)
        self.status = str(status)
        self.reason = str(reason)
        self.time_seconds = float(time_seconds)
        self.evaluation_start_seconds = float(evaluation_start_seconds)
        self.evaluation_end_seconds = float(evaluation_end_seconds)
        self.original_index = int(original_index)


def load_delineations(path, format_name=None):
    format_name = format_name or ("point_events_json_v1" if path.lower().endswith(".json") else "point_events_csv_v1")
    detections = load_detections(path, target="ecg_delineation", format_name=format_name)
    events = [DelineationEvent(item.channel, item.label, item.time_seconds, item.confidence, item.original_index) for item in detections.events]
    for index, item in enumerate(events):
        if item.lead not in ECG_LEADS or item.kind not in DELINEATION_KINDS or not _finite_non_negative(item.time_seconds):
            raise ValueError("delineation point event %s has invalid channel, label, or time" % index)
        if item.confidence is not None and (not _finite(item.confidence) or not 0.0 <= float(item.confidence) <= 1.0):
            raise ValueError("delineation point event %s confidence must be in [0,1]" % index)
    events.sort(key=lambda item: (item.time_seconds, ECG_LEADS.index(item.lead), DELINEATION_KINDS.index(item.kind), item.original_index))
    return DelineationDocument(path, events, detections.raw)


def delineation_scope_from_entry(entry):
    raw = entry.get("evaluation_scope", {})
    if not isinstance(raw, dict) or raw.get("mode") not in ("all_record", "selected_windows"):
        raise ValueError("delineation scoring entry requires all_record or selected_windows evaluation scope")
    leads = raw.get("leads")
    if not isinstance(leads, list) or not leads or any(item not in ECG_LEADS for item in leads) or len(leads) != len(set(leads)):
        raise ValueError("delineation evaluation scope requires unique standard ECG leads")
    windows = []
    if raw["mode"] == "selected_windows":
        for index, item in enumerate(raw.get("windows", [])):
            if not isinstance(item, dict) or set(item.keys()) != set(["start_seconds", "end_seconds"]):
                raise ValueError("delineation evaluation window %s is malformed" % index)
            windows.append((float(item["start_seconds"]), float(item["end_seconds"])))
        if not windows:
            raise ValueError("selected_windows delineation scope requires at least one window")
    elif "windows" in raw and raw["windows"]:
        raise ValueError("all_record delineation scope must not contain windows")
    return DelineationScope(leads, windows)


def delineation_truth_from_annotations(annotations, scope, record_duration_seconds):
    if annotations.get("fiducial_detail") != "full":
        raise ValueError("delineation scoring requires full fiducials; compact challenge output is not supported")
    duration = float(record_duration_seconds)
    _validate_scope(scope, duration)
    construction_by_beat = {}
    measured_by_beat = {}
    measured_by_atrial = {}
    for item in annotations.get("fiducials", []):
        kind = str(item.get("kind", ""))
        source = str(item.get("source", ""))
        lead_index = int(item.get("lead_index", -1))
        if source == "construction" and lead_index == -1:
            construction_by_beat[(int(item.get("beat_index", -1)), kind)] = item
        elif source == "lead_measurement" and 0 <= lead_index < len(ECG_LEADS):
            lead = ECG_LEADS[lead_index]
            measured_by_beat[(int(item.get("beat_index", -1)), lead, kind)] = item
            measured_by_atrial[(int(item.get("atrial_index", -1)), lead, kind)] = item
    truth = []

    def add(anchor_type, anchor_index, lead, kind, status, reason, reference, start, end):
        reference = float(reference)
        scoped_time = min(max(reference, 0.0), max(0.0, duration - 1e-12))
        if not _in_scope_time(scoped_time, scope):
            return
        start = min(max(float(start), 0.0), duration)
        end = min(max(float(end), 0.0), duration)
        if end <= start:
            start = min(max(scoped_time - 0.04, 0.0), duration)
            end = min(max(scoped_time + 0.04, 0.0), duration)
        truth.append(DelineationTruthPoint(anchor_type, anchor_index, lead, kind, status, reason, reference, start, end, len(truth)))

    for lead in scope.leads:
        for atrial in annotations.get("atrial_events", []):
            atrial_index = int(atrial["atrial_index"])
            onset = float(atrial["onset_seconds"])
            peak = float(atrial["peak_seconds"])
            offset = float(atrial["offset_seconds"])
            measured = measured_by_atrial.get((atrial_index, lead, "p_peak"))
            if not atrial.get("visible", False):
                status, reason = "absent", "wave_absent"
            elif not all(_inside(item, duration) for item in (onset, peak, offset)):
                status, reason = "not_evaluable", "record_boundary"
            elif measured is None or not measured.get("present", False):
                status, reason = "not_evaluable", "below_lead_threshold"
            else:
                status, reason = "present", ""
            measured_peak = float(measured["time_seconds"]) if status == "present" else peak
            add("atrial_event", atrial_index, lead, "p_onset", status, reason, onset, onset, offset)
            add("atrial_event", atrial_index, lead, "p_peak", status, reason, measured_peak, onset, offset)
            add("atrial_event", atrial_index, lead, "p_offset", status, reason, offset, onset, offset)

        for beat in annotations.get("beats", []):
            beat_index = int(beat["beat_index"])
            qrs_onset = _construction_time(construction_by_beat, beat_index, "qrs_onset", beat.get("qrs_onset_seconds", beat.get("r_peak_seconds", 0.0)))
            if int(beat.get("linked_atrial_index", -1)) < 0:
                reference = qrs_onset - 0.12
                start, end = qrs_onset - 0.28, qrs_onset - 0.02
                add("ventricular_beat", beat_index, lead, "p_onset", "absent", "no_atrial_event", reference - 0.04, start, end)
                add("ventricular_beat", beat_index, lead, "p_peak", "absent", "no_atrial_event", reference, start, end)
                add("ventricular_beat", beat_index, lead, "p_offset", "absent", "no_atrial_event", reference + 0.04, start, end)

            j_point = _construction_time(construction_by_beat, beat_index, "j_point", qrs_onset)
            qrs_offset = _construction_time(construction_by_beat, beat_index, "qrs_offset", qrs_onset)
            qrs_visible = any(measured_by_beat.get((beat_index, lead, kind), {}).get("present", False) for kind in ("q_peak", "r_peak", "s_peak"))
            if not beat.get("qrs_present", False):
                qrs_status, qrs_reason = "absent", "wave_absent"
            elif not all(_inside(item, duration) for item in (qrs_onset, j_point, qrs_offset)):
                qrs_status, qrs_reason = "not_evaluable", "record_boundary"
            elif not qrs_visible:
                qrs_status, qrs_reason = "not_evaluable", "below_lead_threshold"
            else:
                qrs_status, qrs_reason = "present", ""
            add("ventricular_beat", beat_index, lead, "qrs_onset", qrs_status, qrs_reason, qrs_onset, qrs_onset, qrs_offset)
            add("ventricular_beat", beat_index, lead, "j_point", qrs_status, qrs_reason, j_point, qrs_onset, qrs_offset)
            add("ventricular_beat", beat_index, lead, "qrs_offset", qrs_status, qrs_reason, qrs_offset, qrs_onset, qrs_offset)

            t_onset = _construction_time(construction_by_beat, beat_index, "t_onset", qrs_offset)
            t_peak = _construction_time(construction_by_beat, beat_index, "t_peak", t_onset)
            t_offset = _construction_time(construction_by_beat, beat_index, "t_offset", t_peak)
            measured_t = measured_by_beat.get((beat_index, lead, "t_peak"))
            if not beat.get("t_present", False):
                t_status, t_reason = "absent", "wave_absent"
            elif not all(_inside(item, duration) for item in (t_onset, t_peak, t_offset)):
                t_status, t_reason = "not_evaluable", "record_boundary"
            elif measured_t is None or not measured_t.get("present", False):
                t_status, t_reason = "not_evaluable", "below_lead_threshold"
            else:
                t_status, t_reason = "present", ""
            measured_t_peak = float(measured_t["time_seconds"]) if t_status == "present" else t_peak
            add("ventricular_beat", beat_index, lead, "t_onset", t_status, t_reason, t_onset, t_onset, t_offset)
            add("ventricular_beat", beat_index, lead, "t_peak", t_status, t_reason, measured_t_peak, t_onset, t_offset)
            add("ventricular_beat", beat_index, lead, "t_offset", t_status, t_reason, t_offset, t_onset, t_offset)

    truth.sort(key=lambda item: (item.time_seconds, ECG_LEADS.index(item.lead), DELINEATION_KINDS.index(item.kind), 0 if item.anchor_type == "atrial_event" else 1, item.anchor_index))
    for index, item in enumerate(truth):
        item.original_index = index
    return truth


def score_delineation_events(ground_truth, predictions, record_duration_seconds, scope, tolerance_seconds=0.040, pairing_window_seconds=0.200):
    duration = float(record_duration_seconds)
    tolerance = float(tolerance_seconds)
    pairing_window = float(pairing_window_seconds)
    _validate_scope(scope, duration)
    if not _finite_positive(duration) or not _finite_positive(tolerance) or not _finite(pairing_window) or pairing_window < tolerance:
        raise ValueError("delineation duration/tolerance/pairing window is invalid")
    for index, item in enumerate(predictions):
        if item.lead not in ECG_LEADS or item.kind not in DELINEATION_KINDS or not _finite_non_negative(item.time_seconds) or item.time_seconds > duration + 1e-9:
            raise ValueError("prediction event %s lies outside the record or is malformed" % index)
    truth_used = [False] * len(ground_truth)
    prediction_used = [False] * len(predictions)
    prediction_excluded = [False] * len(predictions)
    candidates = []
    for truth_index, truth in enumerate(ground_truth):
        if truth.status != "present":
            continue
        for prediction_index, prediction in enumerate(predictions):
            if not _in_scope_event(prediction, scope) or truth.lead != prediction.lead or truth.kind != prediction.kind:
                continue
            error = abs(prediction.time_seconds - truth.time_seconds)
            if error <= pairing_window + 1e-15:
                candidates.append((error, truth_index, prediction_index))
    candidates.sort()
    matches = []
    for _error, truth_index, prediction_index in candidates:
        if truth_used[truth_index] or prediction_used[prediction_index]:
            continue
        truth_used[truth_index] = True
        prediction_used[prediction_index] = True
        truth = ground_truth[truth_index]
        prediction = predictions[prediction_index]
        error = prediction.time_seconds - truth.time_seconds
        matches.append({
            "ground_truth_index": truth.original_index,
            "prediction_index": prediction.original_index,
            "anchor_type": truth.anchor_type,
            "anchor_index": str(truth.anchor_index),
            "lead": truth.lead,
            "kind": truth.kind,
            "ground_truth_time_seconds": truth.time_seconds,
            "prediction_time_seconds": prediction.time_seconds,
            "error_seconds": error,
            "within_tolerance": abs(error) <= tolerance + 1e-15,
        })
    missing = [item for index, item in enumerate(ground_truth) if item.status == "present" and not truth_used[index]]
    unexpected = []
    excluded = []
    for prediction_index, prediction in enumerate(predictions):
        if prediction_used[prediction_index]:
            continue
        reason = "outside_scope" if not _in_scope_event(prediction, scope) else ""
        if not reason:
            for truth in ground_truth:
                if truth.status == "not_evaluable" and truth.lead == prediction.lead and truth.kind == prediction.kind and truth.evaluation_start_seconds <= prediction.time_seconds <= truth.evaluation_end_seconds:
                    reason = "truth_not_evaluable"
                    break
        if reason:
            excluded.append({"reason": reason, "event": _event_dict(prediction)})
            prediction_excluded[prediction_index] = True
        else:
            unexpected.append(prediction)
    scored_predictions = [item for index, item in enumerate(predictions) if not prediction_excluded[index]]

    def metrics(kind=None, lead=None):
        selected_truth = [item for item in ground_truth if (kind is None or item.kind == kind) and (lead is None or item.lead == lead)]
        selected_predictions = [item for item in scored_predictions if (kind is None or item.kind == kind) and (lead is None or item.lead == lead)]
        selected_matches = [item for item in matches if (kind is None or item["kind"] == kind) and (lead is None or item["lead"] == lead)]
        selected_missing = [item for item in missing if (kind is None or item.kind == kind) and (lead is None or item.lead == lead)]
        selected_unexpected = [item for item in unexpected if (kind is None or item.kind == kind) and (lead is None or item.lead == lead)]
        selected_excluded = [item for item in excluded if (kind is None or item["event"]["kind"] == kind) and (lead is None or item["event"]["lead"] == lead)]
        errors = [item["error_seconds"] for item in selected_matches]
        absolute = sorted(abs(item) for item in errors)
        within = sum(1 for item in selected_matches if item["within_tolerance"])
        present = sum(1 for item in selected_truth if item.status == "present")
        out = len(selected_matches) - within
        return {
            "ground_truth_count": present,
            "absent_truth_count": sum(1 for item in selected_truth if item.status == "absent"),
            "not_evaluable_truth_count": sum(1 for item in selected_truth if item.status == "not_evaluable"),
            "prediction_count": len(selected_predictions),
            "excluded_prediction_count": len(selected_excluded),
            "paired_count": len(selected_matches),
            "within_tolerance_count": within,
            "missing_prediction_count": len(selected_missing),
            "unexpected_prediction_count": len(selected_unexpected),
            "out_of_tolerance_count": out,
            "false_negative_count": len(selected_missing) + out,
            "false_positive_count": len(selected_unexpected) + out,
            "sensitivity": float(within) / present if present else None,
            "positive_predictive_value": float(within) / len(selected_predictions) if selected_predictions else None,
            "f1_score": 2.0 * within / (present + len(selected_predictions)) if present + len(selected_predictions) else None,
            "within_tolerance_fraction": float(within) / len(selected_matches) if selected_matches else None,
            "timing_error_seconds": {
                "mean": sum(errors) / len(errors) if errors else None,
                "mean_absolute": sum(absolute) / len(absolute) if absolute else None,
                "median_absolute": _nearest_rank(absolute, 0.5),
                "rms": math.sqrt(sum(item * item for item in errors) / len(errors)) if errors else None,
                "p95_absolute": _nearest_rank(absolute, 0.95),
                "max_absolute": max(absolute) if absolute else None,
            },
        }

    return {
        "overall": metrics(),
        "by_kind": [{"kind": kind, "metrics": metrics(kind=kind)} for kind in DELINEATION_KINDS],
        "by_lead": [{"lead": lead, "metrics": metrics(lead=lead)} for lead in sorted(scope.leads, key=ECG_LEADS.index)],
        "by_kind_lead": [{"kind": kind, "lead": lead, "metrics": metrics(kind, lead)} for lead in sorted(scope.leads, key=ECG_LEADS.index) for kind in DELINEATION_KINDS],
        "truth": [_truth_dict(item) for item in ground_truth],
        "matches": matches,
        "missing_events": [_truth_dict(item) for item in missing],
        "unexpected_events": [_event_dict(item) for item in unexpected],
        "excluded_predictions": excluded,
    }


def _construction_time(construction, beat_index, kind, fallback):
    item = construction.get((beat_index, kind))
    return float(item["time_seconds"]) if item is not None else float(fallback)


def _validate_scope(scope, duration):
    if not isinstance(scope, DelineationScope) or not scope.leads or any(item not in ECG_LEADS for item in scope.leads) or len(scope.leads) != len(set(scope.leads)):
        raise ValueError("delineation evaluation scope is invalid")
    for start, end in scope.windows:
        if not _finite_non_negative(start) or not _finite_positive(end) or end <= start or end > duration + 1e-9:
            raise ValueError("delineation evaluation window is invalid")


def _in_scope_time(time_seconds, scope):
    return not scope.windows or any(start <= time_seconds < end for start, end in scope.windows)


def _in_scope_event(event, scope):
    return event.lead in scope.leads and _in_scope_time(event.time_seconds, scope)


def _inside(value, duration):
    return _finite(value) and 0.0 <= float(value) < duration


def _finite(value):
    try:
        return math.isfinite(float(value))
    except (TypeError, ValueError):
        return False


def _finite_non_negative(value):
    return _finite(value) and float(value) >= 0.0


def _finite_positive(value):
    return _finite(value) and float(value) > 0.0


def _nearest_rank(values, fraction):
    return values[int(math.ceil(fraction * len(values))) - 1] if values else None


def _truth_dict(item):
    return {
        "anchor_type": item.anchor_type,
        "anchor_index": str(item.anchor_index),
        "lead": item.lead,
        "kind": item.kind,
        "status": item.status,
        "reason": item.reason,
        "time_seconds": item.time_seconds,
        "evaluation_start_seconds": item.evaluation_start_seconds,
        "evaluation_end_seconds": item.evaluation_end_seconds,
    }


def _event_dict(item):
    return {"lead": item.lead, "kind": item.kind, "time_seconds": item.time_seconds}
