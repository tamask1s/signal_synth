import html
import math


NOTICE_TEXT = "Synthetic engineering QA evidence; not diagnosis, nor clinical evidence"


TARGET_NAMES = {
    "r_peak": "ECG R-peak detection",
    "ecg_beat_classification": "ECG beat classification",
    "ecg_delineation": "ECG delineation",
    "rr_interval": "RR interval measurement",
    "hrv": "Heart-rate variability",
    "qtc": "QT and QTc measurement",
    "rhythm_episode": "Rhythm episode detection",
    "signal_quality": "Signal-quality interval detection",
    "ppg_systolic_peak": "PPG systolic-peak detection",
    "ppg_pulse_onset": "PPG pulse-onset detection",
    "morphology_assertions": "ECG morphology assertions",
    "ecg_ppg_alignment": "ECG–PPG alignment",
    "ppg_optical": "PPG optical measurements",
    "prv": "Pulse-rate variability",
    "respiratory_rate": "Respiratory-rate measurement",
    "rhythm_burden": "Rhythm-burden measurement",
}


METRIC_NAMES = {
    "accuracy": "Accuracy",
    "assertion_agreement_fraction": "Assertion agreement",
    "event_precision": "Event precision",
    "event_sensitivity": "Event sensitivity",
    "f1_score": "F1 score",
    "false_alarms_per_hour": "False alarms per hour",
    "maximum_absolute_error": "Maximum absolute error",
    "max_absolute_error_seconds": "Maximum timing error",
    "mean_absolute_error": "Mean absolute error",
    "mean_absolute_error_seconds": "Mean absolute timing error",
    "mean_absolute_offset_error_seconds": "Mean offset timing error",
    "mean_absolute_onset_error_seconds": "Mean onset timing error",
    "median_absolute_error_seconds": "Median absolute timing error",
    "median_absolute_error": "Median absolute error",
    "micro_f1_score": "Micro-averaged F1 score",
    "micro_precision": "Micro-averaged precision",
    "micro_recall": "Micro-averaged recall",
    "p95_absolute_error": "95th-percentile absolute error",
    "p95_absolute_error_seconds": "95th-percentile timing error",
    "positive_predictive_value": "Positive predictive value",
    "prediction_match_fraction": "Submitted measurements associated with reference",
    "precision": "Precision",
    "recall": "Recall",
    "rms_error_seconds": "Root-mean-square timing error",
    "root_mean_square_error": "Root-mean-square error",
    "sensitivity": "Sensitivity",
    "status_match_fraction": "Status agreement",
    "temporal_iou": "Temporal intersection over union",
    "time_f1_score": "Time-weighted F1 score",
    "time_precision": "Time-weighted precision",
    "time_sensitivity": "Time-weighted sensitivity",
    "tolerance_pass_fraction": "Measurements within tolerance",
    "truth_match_fraction": "Reference values covered",
    "within_tolerance_fraction": "Events within timing tolerance",
}


METRIC_DESCRIPTIONS = {
    "accuracy": "Share of scored labels classified correctly.",
    "assertion_agreement_fraction": "Share of comparable assertions for which truth and algorithm agree.",
    "event_precision": "Share of submitted intervals that pair with a reference interval.",
    "event_sensitivity": "Share of reference intervals paired with a submitted interval.",
    "f1_score": "Harmonic mean of sensitivity and positive predictive value.",
    "false_alarms_per_hour": "Unmatched submitted intervals normalized to one hour of signal.",
    "maximum_absolute_error": "Largest absolute difference among matched numeric values.",
    "max_absolute_error_seconds": "Largest absolute timing difference among matched events.",
    "mean_absolute_error": "Average absolute difference between matched submitted and reference values.",
    "mean_absolute_error_seconds": "Average absolute timing difference for matched events.",
    "mean_absolute_offset_error_seconds": "Average absolute end-boundary timing difference for matched intervals.",
    "mean_absolute_onset_error_seconds": "Average absolute start-boundary timing difference for matched intervals.",
    "median_absolute_error_seconds": "Median absolute timing difference for matched events.",
    "median_absolute_error": "Median absolute difference between associated submitted and reference values; it shows typical error and is less dominated by split/merge outliers than MAE or P95.",
    "micro_f1_score": "F1 score after pooling all scored classes.",
    "micro_precision": "Share of pooled scored class predictions that are correct.",
    "micro_recall": "Share of pooled scored reference classes recovered correctly.",
    "p95_absolute_error": "95th percentile of |submitted − reference| across matched numeric values.",
    "p95_absolute_error_seconds": "Absolute timing error below which 95% of matched events fall.",
    "positive_predictive_value": "Share of algorithm detections that match truth.",
    "prediction_match_fraction": "Share of unique submitted measurements associated with at least one packaged reference value. An RR interval split or merge may create multiple comparison pairs but is counted once here.",
    "precision": "Share of submitted labels or events that match the reference.",
    "recall": "Share of reference labels or events recovered by the submission.",
    "rms_error_seconds": "Square root of the mean squared timing error; larger errors receive more weight.",
    "root_mean_square_error": "Square root of the mean squared numeric error; larger errors receive more weight.",
    "sensitivity": "Share of truth events detected by the algorithm.",
    "status_match_fraction": "Share of matched measurements with the same valid, undefined, absent, or not-evaluable status.",
    "temporal_iou": "Duration overlap divided by the union of truth and predicted intervals.",
    "time_f1_score": "F1 score computed from time-weighted interval overlap.",
    "time_precision": "Share of submitted interval duration overlapping the reference.",
    "time_sensitivity": "Share of reference interval duration covered by the submission.",
    "tolerance_pass_fraction": "Share of matched numeric pairs within the packaged absolute-or-relative tolerance.",
    "truth_match_fraction": "Share of unique packaged reference values associated with at least one submitted measurement. An RR interval split or merge may create multiple comparison pairs but is counted once here.",
    "within_tolerance_fraction": "Share of paired events whose timing error is within the packaged tolerance.",
}


MEASUREMENT_NAMES = {
    "hf_normalized_units": "HF normalized power",
    "hf_power_seconds2": "HF power",
    "lf_hf_ratio": "LF/HF ratio",
    "lf_normalized_units": "LF normalized power",
    "lf_power_seconds2": "LF power",
    "mean_heart_rate_bpm": "Mean heart rate",
    "mean_rr_seconds": "Mean RR interval",
    "pnn50_percent": "pNN50",
    "qtc_interval": "QTc interval",
    "qt_interval": "QT interval",
    "rmssd_seconds": "RMSSD",
    "rr_interval": "RR interval",
    "sd1_sd2_ratio": "SD1/SD2 ratio",
    "sd1_seconds": "Poincaré SD1",
    "sd2_seconds": "Poincaré SD2",
    "sdnn_seconds": "SDNN",
    "total_power_seconds2": "Total spectral power",
    "vlf_power_seconds2": "VLF power",
}


RATIO_METRICS = frozenset([
    "accuracy", "assertion_agreement_fraction", "event_precision",
    "event_sensitivity", "f1_score", "micro_f1_score", "micro_precision",
    "micro_recall", "positive_predictive_value", "prediction_match_fraction",
    "precision", "recall", "sensitivity", "status_match_fraction",
    "temporal_iou", "time_f1_score", "time_precision", "time_sensitivity",
    "tolerance_pass_fraction", "truth_match_fraction",
    "within_tolerance_fraction",
])


ERROR_METRICS = frozenset([
    "maximum_absolute_error", "mean_absolute_error", "median_absolute_error", "p95_absolute_error",
    "root_mean_square_error",
])


STYLE = """
:root{color-scheme:light;--ink:#172033;--muted:#5f6b7a;--line:#d8dee8;--soft:#f6f8fb;--pass:#176b45;--pass-bg:#eaf7f0;--warn:#8a5a00;--warn-bg:#fff7df;--fail:#a12828;--fail-bg:#fff0f0;--accent:#3157b7}
*{box-sizing:border-box}body{font:14px/1.5 Inter,ui-sans-serif,system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;color:var(--ink);max-width:1420px;margin:0 auto;padding:28px 32px 64px;background:#fff}a{color:var(--accent);text-underline-offset:2px}nav{margin-bottom:20px}.eyebrow{font-size:12px;letter-spacing:.08em;text-transform:uppercase;color:var(--muted);font-weight:700;margin:0 0 4px}h1{font-size:30px;line-height:1.15;margin:0 0 8px}h2{font-size:20px;margin:34px 0 10px}h3{font-size:16px;margin:22px 0 8px}.subtitle,.muted{color:var(--muted)}.notice{border-left:4px solid #6b7280;background:#f3f4f6;color:#374151;padding:10px 14px;margin:20px 0 24px}.verdict{border:1px solid var(--line);border-left-width:7px;border-radius:8px;padding:18px 20px;margin:18px 0}.verdict.pass{border-left-color:var(--pass);background:var(--pass-bg)}.verdict.fail{border-left-color:var(--fail);background:var(--fail-bg)}.verdict h2{margin:0 0 4px}.cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(165px,1fr));gap:10px;margin:14px 0 24px}.card{border:1px solid var(--line);border-radius:7px;padding:12px;background:var(--soft)}.card strong{display:block;font-size:20px}.table-wrap{overflow-x:auto;border:1px solid var(--line);border-radius:7px;margin:10px 0 22px}table{border-collapse:collapse;width:100%;min-width:660px}th,td{border-bottom:1px solid var(--line);padding:9px 10px;text-align:left;vertical-align:top}th{background:#eef2f7;font-size:12px;letter-spacing:.02em}tr:last-child td{border-bottom:0}.per-case-wide{min-width:1640px;font-size:13px}.per-case-wide th,.per-case-wide td{border:1px solid var(--line);text-align:center;vertical-align:middle;padding:7px 9px}.per-case-wide th{font-size:11px;white-space:nowrap}.per-case-wide th:first-child,.per-case-wide td:first-child{text-align:left}.per-case-wide tr:hover td{background:#f6f8fb}.gate-value{display:block}.gate-value strong{display:block;white-space:nowrap}.gate-value small{display:block;color:inherit;opacity:.82;white-space:nowrap}.gate-value.pass{color:var(--pass)}.gate-value.warning{color:var(--warn)}.gate-value.fail{color:var(--fail)}.gate-value.neutral{color:var(--muted)}.kv th{width:260px}.badge{display:inline-block;border-radius:999px;padding:2px 8px;font-size:11px;font-weight:800;letter-spacing:.04em}.badge.pass{color:var(--pass);background:var(--pass-bg)}.badge.warning{color:var(--warn);background:var(--warn-bg)}.badge.fail{color:var(--fail);background:var(--fail-bg)}.badge.neutral{color:#4b5563;background:#eceff3}.raw{display:block;color:var(--muted);font-size:11px}.mono{font:12px/1.45 ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;overflow-wrap:anywhere}.criterion{min-width:210px}.section-note{border:1px solid var(--line);background:var(--soft);padding:10px 12px;border-radius:6px}.criterion-breakdown td{background:#fbfcfe;padding:0 12px 8px}.criterion-breakdown details{margin:0}.compact{font-size:12px}.compact th,.compact td{padding:6px 8px}.metric-stack>div+div{border-top:1px solid var(--line);margin-top:5px;padding-top:5px}.info{position:relative;display:inline-flex;align-items:center;justify-content:center;width:16px;height:16px;margin-left:4px;border:1px solid #8792a2;border-radius:50%;color:#526071;font-size:10px;font-weight:800;cursor:help;vertical-align:middle}.info:after{content:attr(data-tip);display:none;position:absolute;z-index:20;left:50%;top:22px;transform:translateX(-50%);width:340px;padding:9px 11px;border-radius:6px;background:#172033;color:#fff;font-size:12px;font-weight:400;line-height:1.45;letter-spacing:0;white-space:normal;box-shadow:0 6px 20px rgba(23,32,51,.2)}.info:hover:after,.info:focus:after{display:block}.footer{border-top:1px solid var(--line);margin-top:36px;padding-top:16px;color:var(--muted)}details{border:1px solid var(--line);border-radius:7px;padding:10px 12px;margin:12px 0}summary{font-weight:700;cursor:pointer}.nowrap{white-space:nowrap}
@media(max-width:720px){body{padding:20px 14px}h1{font-size:25px}.kv th{width:auto}.cards{grid-template-columns:1fr 1fr}}
@media print{body{max-width:none;padding:0;font-size:10pt}nav,.no-print{display:none}.table-wrap{overflow:visible;border:0}table{min-width:0}tr{break-inside:avoid}.notice,.verdict,.card{print-color-adjust:exact;-webkit-print-color-adjust:exact}a{color:inherit;text-decoration:none}}
"""


def annotate_policy(policy):
    """Add stable human-facing metadata without changing numeric decisions."""
    for index, check in enumerate(policy.get("checks", []), 1):
        target = check.get("target", "")
        section = check.get("section", "")
        metric = check.get("metric", "")
        scope = check.get("stratum_id", "")
        section_label = section_name(section)
        if scope:
            section_label = "%s / %s" % (scope.replace("_", " ").title(), section_label)
        check.update({
            "criterion_id": "AC-%03d" % index,
            "display_name": "%s — %s — %s" % (
                target_name(target), section_label, metric_name(metric),
            ),
            "description": metric_description(metric),
            "unit": metric_unit(section, metric),
        })
    return policy


def target_name(value):
    return TARGET_NAMES.get(value, value.replace("_", " ").strip().title())


def section_name(value):
    if value.startswith("class/"):
        return "%s class" % value.split("/", 1)[1].replace("_", " ").title()
    if value in ("overall", "total", "summary"):
        return "Overall"
    if value in ("clean", "artifact", "motion", "dropout", "low_perfusion"):
        return value.replace("_", " ").title()
    return MEASUREMENT_NAMES.get(value, value.replace("_", " ").strip().title())


def metric_name(value):
    return METRIC_NAMES.get(value, value.replace("_", " ").strip().title())


def metric_description(value):
    return METRIC_DESCRIPTIONS.get(value, "Pre-specified aggregate acceptance metric.")


def metric_unit(section, metric):
    if metric.endswith("_seconds"):
        return "seconds"
    if metric in RATIO_METRICS:
        return "ratio"
    if metric in ERROR_METRICS:
        if section.endswith("_seconds") or section in ("rr_interval", "qt_interval", "qtc_interval"):
            return "seconds"
        if section.endswith("_seconds2"):
            return "seconds_squared"
        if section.endswith("_percent"):
            return "percentage_points"
        if section.endswith("_bpm"):
            return "bpm"
    return "value"


def _case_gate(policy, stratum_id, target, section, metric):
    return next((
        check for check in policy.get("checks", [])
        if check.get("stratum_id") == stratum_id
        and check.get("target") == target
        and check.get("section") == section
        and check.get("metric") == metric
    ), None)


def _compact_check_value(check, value):
    if value is None:
        return "—"
    numeric = float(value)
    unit = check.get("unit", "value")
    if unit == "ratio":
        return "%.1f%%" % (100.0 * numeric)
    if unit == "seconds":
        return "%.1f ms" % (1000.0 * numeric) if abs(numeric) < 1.0 else "%.3g s" % numeric
    if unit == "seconds_squared":
        return "%.4g s²" % numeric
    if unit == "percentage_points":
        return "%.4g pp" % numeric
    if unit == "bpm":
        return "%.4g bpm" % numeric
    return "%.4g" % numeric


def _gate_cell(check, label=""):
    if check is None:
        return "<span class=\"gate-value neutral\"><strong>—</strong><small>Not gated</small></span>"
    if not check.get("applicable", False):
        return "<span class=\"gate-value neutral\"><strong>Not evaluated</strong><small>Not applicable</small></span>"
    actual = _compact_check_value(check, check.get("actual"))
    required = "%s %s" % (
        "≥" if check.get("operator") == "min" else "≤",
        _compact_check_value(check, check.get("threshold")),
    )
    caption = ("%s · " % _h(label)) if label else ""
    passed = bool(check.get("passed", False))
    return "<span class=\"gate-value %s\"><strong>%s</strong><small>%s%s %s</small></span>" % (
        "pass" if passed else "fail",
        _h(actual),
        caption,
        _h(required),
        "✅" if passed else "❌",
    )


def _gate_group(items):
    return "<div class=\"metric-stack\">%s</div>" % "".join(
        "<div>%s</div>" % _gate_cell(check, label)
        for label, check in items
    )


def _case_display_name(case_id):
    marker = "snr_m"
    if marker in case_id:
        level = case_id.split(marker, 1)[1].split("_", 1)[0]
        numeric = level.replace("p", ".", 1)
        try:
            float(numeric)
            return "−%s dB" % numeric
        except ValueError:
            pass
    if "clean" in case_id:
        return "Clean"
    return case_id.replace("_", " ").title()


def _result_metric(case_results, target, section, metric):
    result = next(
        (item for item in case_results if item.get("target") == target),
        None,
    )
    if not result or not result.get("success", False):
        return None
    report = result.get("comparison", {})
    if result.get("score_type") == "event_detection":
        metrics = result.get("metrics")
        if metrics is None:
            metrics = report.get("comparison", {}).get("metrics", {})
        return metrics.get(section, {}).get(metric)
    if result.get("score_type") == "measurement":
        if section == "overall":
            metrics = result.get("summary")
            if metrics is None:
                metrics = report.get("overall", {})
        else:
            by_measurement = result.get("by_measurement")
            if by_measurement is None:
                by_measurement = report.get("by_measurement", [])
            row = next(
                (
                    item for item in by_measurement
                    if item.get("name") == section
                ),
                {},
            )
            metrics = row.get("metrics", {})
        if metric.endswith("_error"):
            return metrics.get("error", {}).get(metric[:-6])
        return metrics.get(metric)
    return None


def _case_metric_cell(policy, stratum_id, case_results, target, section, metric):
    check = _case_gate(policy, stratum_id, target, section, metric)
    if check is not None:
        return _gate_cell(check)
    actual = _result_metric(case_results, target, section, metric)
    if actual is None:
        return "<span class=\"gate-value neutral\"><strong>—</strong><small>No result</small></span>"
    pseudo_check = {
        "target": target,
        "section": section,
        "metric": metric,
        "unit": metric_unit(section, metric),
    }
    return "<span class=\"gate-value neutral\"><strong>%s</strong><small>Information</small></span>" % _h(
        _compact_check_value(pseudo_check, actual)
    )


def _case_detail_links(case_results):
    return " · ".join(
        "<a href=\"%s\">%s</a>" % (
            _h(item.get("report_path", "")),
            _h(target_name(item.get("target", ""))),
        )
        for item in case_results if item.get("report_path")
    ) or "—"


def _case_verdict_cell(passed, completed=True):
    if not completed:
        return "<span class=\"gate-value fail\"><strong>❌ INCOMPLETE</strong></span>"
    return "<span class=\"gate-value %s\"><strong>%s</strong></span>" % (
        "pass" if passed else "fail",
        "✅ PASS" if passed else "❌ FAIL",
    )


def _rpeak_rr_per_case_verdicts(summary):
    policy = summary.get("policy", {})
    strata = policy.get("acceptance_strata", [])
    results = summary.get("results", [])
    rows = []
    for stratum in strata:
        case_ids = stratum.get("case_ids", [])
        case_id = case_ids[0] if case_ids else ""
        case_results = [item for item in results if item.get("case_id") == case_id]
        completed = bool(case_results) and all(item.get("success", False) for item in case_results)
        passed = completed and bool(stratum.get("passed", False))
        stratum_id = stratum.get("stratum_id")
        metric_columns = [
            ("r_peak", "total", "f1_score"),
            ("r_peak", "total", "sensitivity"),
            ("r_peak", "total", "positive_predictive_value"),
            ("r_peak", "total", "mean_absolute_error_seconds"),
            ("rr_interval", "overall", "truth_match_fraction"),
            ("rr_interval", "overall", "prediction_match_fraction"),
            ("rr_interval", "overall", "status_match_fraction"),
            ("rr_interval", "overall", "tolerance_pass_fraction"),
            ("rr_interval", "rr_interval", "mean_absolute_error"),
            ("rr_interval", "rr_interval", "median_absolute_error"),
        ]
        cells = [
            _case_metric_cell(
                policy, stratum_id, case_results, target, section, metric,
            )
            for target, section, metric in metric_columns
        ]
        rows.append(
            "<tr><td><strong>%s</strong><span class=\"raw\">%s</span></td>%s<td>%s</td><td>%s</td></tr>" % (
                _h(_case_display_name(case_id)), _h(case_id),
                "".join("<td>%s</td>" % cell for cell in cells),
                _case_verdict_cell(passed, completed),
                _case_detail_links(case_results),
            )
        )
    return _table([
        ("Case", "One complete, independently generated signal. The machine-readable case ID is shown below the human label."),
        ("Rpk F1", METRIC_DESCRIPTIONS["f1_score"]),
        ("Rpk Se", METRIC_DESCRIPTIONS["sensitivity"]),
        ("Rpk PPV", METRIC_DESCRIPTIONS["positive_predictive_value"]),
        ("Rpk tMAE", METRIC_DESCRIPTIONS["mean_absolute_error_seconds"]),
        ("RR cover", METRIC_DESCRIPTIONS["truth_match_fraction"]),
        ("RR pred", METRIC_DESCRIPTIONS["prediction_match_fraction"]),
        ("RR stat", METRIC_DESCRIPTIONS["status_match_fraction"]),
        ("RR tol%", "Share of peak-anchored RR associations within the packaged numeric tolerance. FP splits and FN merges create local comparison pairs rather than shifting all later intervals."),
        ("RR MAE", "Mean absolute RR error over local split/merge associations. This is intentionally shown even when it is not an acceptance gate; a few fragment or merged-interval outliers can raise it."),
        ("RR median", METRIC_DESCRIPTIONS["median_absolute_error"]),
        ("Case verdict", "Official verdict for this complete case. Every applicable gate in this row must pass; another case cannot compensate."),
        ("Details", "Open the R-peak and RR case-target evidence pages. Each page links back to this overview."),
    ], rows, "per-case-wide")


def _generic_per_case_table(summary, official=False):
    policy = summary.get("policy", {})
    results = summary.get("results", [])
    case_ids = []
    for item in results:
        if item.get("case_id") not in case_ids:
            case_ids.append(item.get("case_id"))
    columns = []
    for check in policy.get("checks", []):
        key = (check.get("target"), check.get("section"), check.get("metric"))
        if key not in columns:
            columns.append(key)
    rows = []
    strata = dict(
        (item.get("case_ids", [""])[0], item)
        for item in policy.get("acceptance_strata", [])
        if len(item.get("case_ids", [])) == 1
    )
    for case_id in case_ids:
        case_results = [item for item in results if item.get("case_id") == case_id]
        completed = bool(case_results) and all(item.get("success", False) for item in case_results)
        diagnostics = []
        cells = []
        for target, section, metric in columns:
            checks = [
                check for check in policy.get("checks", [])
                if (
                    check.get("target"), check.get("section"),
                    check.get("metric"),
                ) == (target, section, metric)
            ]
            pairs = [
                (check, _case_contribution(check, case_id))
                for check in checks
            ]
            pairs = [
                (check, contribution)
                for check, contribution in pairs
                if contribution is not None and contribution.get("contributes", False)
            ]
            diagnostics.extend(
                contribution.get("diagnostic_passed")
                for _check, contribution in pairs
                if contribution.get("diagnostic_passed") is not None
            )
            cells.append(
                _gate_group([
                    (
                        check.get("stratum_id", ""),
                        dict(check, actual=contribution.get("actual"),
                             applicable=True,
                             passed=contribution.get("diagnostic_passed")),
                    )
                    for check, contribution in pairs
                ]) if pairs else
                "<span class=\"gate-value neutral\"><strong>—</strong><small>Out of scope</small></span>"
            )
        if official:
            stratum = strata.get(case_id, {})
            passed = completed and bool(stratum.get("passed", False))
            verdict = _badge(
                "PASS" if passed else "INCOMPLETE" if not completed else "FAIL",
                "pass" if passed else "fail",
            )
        elif not completed:
            verdict = _badge("ERROR", "fail")
        elif not diagnostics:
            verdict = _badge("CONTEXT ONLY", "neutral")
        elif all(diagnostics):
            verdict = _badge("MEETS ALL GATES", "pass")
        else:
            verdict = _badge("BELOW A GATE", "warning")
        rows.append(
            "<tr><td><strong>%s</strong></td>%s<td>%s</td><td>%s</td></tr>" % (
                _h(case_id),
                "".join("<td>%s</td>" % cell for cell in cells),
                verdict,
                _case_detail_links(case_results),
            )
        )
    headers = [
        ("Case", "One generated signal and its case-target outputs."),
    ] + [
        (
            "%s · %s · %s" % (
                target_name(target), section_name(section), metric_name(metric),
            ),
            metric_description(metric),
        )
        for target, section, metric in columns
    ] + [
        (
            "Case verdict" if official else "Case view",
            "Official independent verdict." if official else
            "Diagnostic comparison with the package's aggregate gates. It does not replace the official pooled or stratum verdict.",
        ),
        ("Details", "Open every case-target evidence page for this signal."),
    ]
    return _table(headers, rows, "per-case-wide")


def _per_case_verdicts(summary):
    targets = set(
        item.get("target") for item in summary.get("results", [])
    )
    if set(["r_peak", "rr_interval"]).issubset(targets):
        return _rpeak_rr_per_case_verdicts(summary)
    return _generic_per_case_table(summary, official=True)


def render_index(summary):
    policy = summary.get("policy", {})
    applicable = [item for item in policy.get("checks", []) if item.get("applicable", False)]
    passed = sum(1 for item in applicable if item.get("passed", False))
    failed = len(applicable) - passed
    scoring_success = bool(summary.get("scoring_success", False))
    overall_success = bool(summary.get("success", False))
    verification = summary.get("verification", {})
    package = summary.get("package", {})
    submission = summary.get("submission", {})
    algorithm = submission.get("algorithm", {})
    protocol = verification.get("protocol") or {}
    per_case = protocol.get("verdict_scope") == "per_case"
    truth_policy = protocol.get("truth_policy", {})
    case_verdicts = policy.get("acceptance_strata", []) if per_case else []
    passed_cases = sum(1 for item in case_verdicts if item.get("passed", False))

    if overall_success and per_case:
        verdict_title = "PASS — every case met its acceptance criteria"
        verdict_text = "%d of %d independent cases passed. No case pooling or cross-case averaging was used." % (passed_cases, len(case_verdicts))
    elif overall_success:
        verdict_title = "PASS — all acceptance criteria met"
        verdict_text = "%d of %d applicable criteria passed and every selected case-target was scored." % (passed, len(applicable))
    elif not scoring_success:
        verdict_title = "INCOMPLETE — scoring could not be completed"
        verdict_text = "%d case-target result(s) contain missing, unsupported, or invalid algorithm output." % summary.get("incomplete_case_target_count", 0)
    elif per_case:
        verdict_title = "FAIL — %d of %d cases did not pass" % (len(case_verdicts) - passed_cases, len(case_verdicts))
        verdict_text = "Each case was judged independently; a stronger case cannot hide a failed case."
    else:
        verdict_title = "FAIL — %d acceptance %s not met" % (failed, "criterion" if failed == 1 else "criteria")
        verdict_text = "%d of %d applicable criteria passed. Failed rows remain part of the evidence record." % (passed, len(applicable))

    identity_rows = [
        ("Package", "%s (%s)" % (package.get("name", package.get("package_id", "")), package.get("package_id", ""))),
        ("Pack version", package.get("version", "")),
        ("Pack fingerprint", package.get("pack_fingerprint", "")),
        ("Generator version", package.get("generator_version", "")),
        ("Generator commit", package.get("generator_git_commit", "")),
        ("Algorithm", "%s %s" % (algorithm.get("name", ""), algorithm.get("version", ""))),
        ("Verifier", summary.get("scoring_version", "")),
        ("Verification mode", verification.get("mode", "")),
        ("Evidence eligible", _yes_no(verification.get("evidence_eligible", False))),
        ("Acceptance profile", "Independent per-case profiles (no pooling)" if per_case else policy.get("profile_id", "")),
        ("Protocol", protocol.get("protocol_id", "")),
        ("Protocol SHA-256", protocol.get("sha256", "")),
        ("Generated at (UTC)", summary.get("generated_at_utc", "")),
    ]
    identity_rows = [item for item in identity_rows if item[1] not in ("", " ")]

    criteria_rows = []
    for check in policy.get("checks", []):
        criterion = check.get("criterion_id", "")
        applicable_check = check.get("applicable", False)
        verdict = "PASS" if check.get("passed", False) else "FAIL"
        if not applicable_check:
            verdict = "NOT APPLICABLE"
        criteria_rows.append(
            "<tr id=\"criterion-%s\"><td class=\"nowrap\"><strong>%s</strong></td>"
            "<td class=\"criterion\"><strong>%s</strong><span class=\"raw\">%s</span></td>"
            "<td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>" % (
                _h(criterion), _h(criterion), _h(check.get("display_name", "")),
                _h(check.get("description", "")), _required_value(check),
                _actual_value(check), _margin_value(check),
                _badge(verdict, "neutral" if not applicable_check else "pass" if check.get("passed", False) else "fail"),
            )
            + _criterion_breakdown_row(check)
        )

    pipeline = summary.get("hrv_pipeline", {})
    pipeline_html = ""
    if pipeline.get("available", False):
        pipeline_rows = []
        for stage in pipeline.get("stages", []):
            pipeline_rows.append("<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>" % (
                _h(stage.get("stage", "").replace("_", " ").title()),
                _h(target_name(stage.get("target", ""))),
                _badge("AVAILABLE" if stage.get("available", False) else "NOT AVAILABLE", "pass" if stage.get("available", False) else "neutral"),
                _h(format_plain(stage.get("score"))),
            ))
        pipeline_html = _section("Pipeline trace", "<p class=\"subtitle\">Availability and aggregate score at each HRV processing stage.</p>" + _table(["Stage", "Target", "Availability", "Score"], pipeline_rows))

    mode_note = "This run used the complete package-authoritative evidence matrix and its embedded acceptance profile." if verification.get("mode") == "evidence" else "This is a diagnostic run. Filters or a caller-selected profile may apply, so it is not package-protocol evidence."
    if per_case:
        case_overview_html = _section(
            "Per-case verdicts",
            "<p class=\"section-note\">Each row is an official, independent verdict over one complete signal. "
            "Cases are not split into acceptance bins, pooled, averaged, or allowed to compensate for one another. "
            "Hover or focus any “i” marker for the full metric definition.</p>"
            + _per_case_verdicts(summary),
        )
        acceptance_html = _section(
            "Audit detail",
            "<details><summary>Show all %d metric-level criteria</summary>"
            "<p class=\"section-note\">These rows are the individual gates behind the case verdicts. "
            "Raw numeric values remain in <a href=\"evidence.json\">evidence.json</a>.</p>%s</details>" % (
                len(criteria_rows),
                _table(["ID", "Metric", "Required", "Actual", "Margin", "Official verdict"], criteria_rows),
            ),
        )
    else:
        case_overview_html = _section(
            "Per-case results",
            "<p class=\"section-note\">This wide table makes every case visible before the aggregate decision. "
            "Its case badges are diagnostic comparisons with the applicable package gates; bins, strata and pooled "
            "acceptance remain authoritative below. A case badge never silently changes the official pack verdict.</p>"
            + _generic_per_case_table(summary),
        )
        acceptance_html = _section("Acceptance criteria", "<p class=\"section-note\">%s %s Aggregate values are calculated by pooling the contributing case evidence, not by averaging the displayed case percentages. Expand a row to inspect its cases, counts and diagnostic comparison with the same gate. Raw numeric values remain in <a href=\"evidence.json\">evidence.json</a>.</p>" % (_h(mode_note), _h(summary.get("threshold_profile", {}).get("description", ""))) + _table(["ID", "Metric", "Required", "Actual", "Margin", "Official verdict"], criteria_rows))
    body = (
        "<nav>Verification evidence / overview</nav>"
        "<header><p class=\"eyebrow\">Synsigra verification evidence</p><h1>%s</h1>"
        "<p class=\"subtitle\">Algorithm: %s %s · Package: %s</p></header>" % (
            _h(package.get("name", package.get("package_id", "Verification report"))),
            _h(algorithm.get("name", "")), _h(algorithm.get("version", "")),
            _h(package.get("package_id", "")),
        )
        + _notice()
        + "<section class=\"verdict %s\"><h2>%s</h2><p>%s</p></section>" % (
            "pass" if overall_success else "fail", _h(verdict_title), _h(verdict_text),
        )
        + "<div class=\"cards\">%s</div>" % "".join([
            _card("Case-targets completed", "%s / %s" % (summary.get("completed_case_target_count", 0), summary.get("case_target_count", 0))),
            _card("Cases passed" if per_case else "Criteria passed", "%s / %s" % ((passed_cases, len(case_verdicts)) if per_case else (passed, len(applicable)))),
            _card("Package integrity", "PASS" if summary.get("integrity", {}).get("ok", False) else "FAIL"),
            _card("Evidence eligible", _yes_no(verification.get("evidence_eligible", False)).upper()),
        ])
        + case_overview_html
        + _section("Run identity and provenance", _table(["Field", "Value"], ["<tr><th>%s</th><td class=\"mono\">%s</td></tr>" % (_h(key), _h(value)) for key, value in identity_rows], "kv"))
        + (_section("Truth and exclusion policy", _policy_table(truth_policy)) if truth_policy else "")
        + acceptance_html
        + pipeline_html
        + _section("Evidence files", "<p><a href=\"evidence.json\">evidence.json</a> is the single canonical machine-readable record. The detail pages contain human-readable views of the same case-target evidence.</p>")
        + "<footer class=\"footer\">Pack fingerprint: <span class=\"mono\">%s</span></footer>" % _h(package.get("pack_fingerprint", ""))
    )
    return _page("Synsigra verification evidence", body)


def render_detail(summary, result, report):
    target = result.get("target", "")
    scenario = report.get("scenario", {}) if isinstance(report, dict) else {}
    algorithm = report.get("algorithm", result.get("submission_output", {}).get("algorithm", {})) if isinstance(report, dict) else {}
    submission_output = result.get("submission_output", {})
    per_case = (summary.get("verification", {}).get("protocol") or {}).get("verdict_scope") == "per_case"
    criteria = [
        item for item in summary.get("policy", {}).get("checks", [])
        if item.get("target") == target and (
            not item.get("case_ids") or result.get("case_id") in item["case_ids"]
        )
    ]
    identity_rows = [
        ("Case", result.get("case_id", "")),
        ("Scenario", result.get("scenario_id", scenario.get("id", ""))),
        ("Target", target_name(target)),
        ("Scoring status", result.get("status", "")),
        ("Algorithm", "%s %s" % (algorithm.get("name", ""), algorithm.get("version", ""))),
        ("Submitted format", submission_output.get("format", "")),
        ("Submitted file", submission_output.get("path", "")),
        ("Submitted SHA-256", submission_output.get("sha256", result.get("submission_output_sha256", ""))),
        ("Scenario fingerprint", scenario.get("document_fingerprint", "")),
        ("Render identity", scenario.get("render_identity", "")),
        ("Duration", _with_unit(scenario.get("duration_seconds"), "s")),
        ("Sampling rate", _with_unit(scenario.get("sample_rate_hz"), "Hz")),
    ]
    identity_rows = [item for item in identity_rows if item[1] not in ("", " ")]
    criteria_rows = []
    for check in criteria:
        applicable = check.get("applicable", False)
        verdict = "NOT APPLICABLE" if not applicable else "PASS" if check.get("passed", False) else "FAIL"
        if per_case:
            criteria_rows.append("<tr><td><a href=\"../index.html#criterion-%s\">%s</a></td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>" % (
                _h(check.get("criterion_id", "")), _h(check.get("criterion_id", "")),
                _h(check.get("display_name", "")), _required_value(check),
                _actual_value(check), _margin_value(check),
                _badge(verdict, "neutral" if not applicable else "pass" if check.get("passed", False) else "fail"),
            ))
        else:
            contribution = _case_contribution(check, result.get("case_id", ""))
            case_actual = _contribution_actual(check, contribution)
            case_diagnostic = _contribution_badge(contribution)
            criteria_rows.append("<tr><td><a href=\"../index.html#criterion-%s\">%s</a></td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>" % (
                _h(check.get("criterion_id", "")), _h(check.get("criterion_id", "")),
                _h(check.get("display_name", "")), _required_value(check),
                case_actual, case_diagnostic, _actual_value(check),
                _badge(verdict, "neutral" if not applicable else "pass" if check.get("passed", False) else "fail"),
            ))
    if per_case:
        criteria_html = _table([
            "ID", "Official case criterion", "Required", "Actual", "Margin", "Verdict",
        ], criteria_rows) if criteria_rows else "<p>No acceptance criteria are defined for this target.</p>"
        acceptance_note = "These are the official criteria for this complete case. No other case contributes to these values or this verdict."
    else:
        criteria_html = _table([
            "ID",
            "Aggregate criterion",
            "Required",
            ("This case", "The case-level value feeding the pooled aggregate, when this case has data for the criterion."),
            ("Case diagnostic", "Informative comparison of this case with the aggregate threshold."),
            "Aggregate",
            "Official verdict",
        ], criteria_rows) if criteria_rows else "<p>No acceptance criteria are defined for this target.</p>"
        acceptance_note = "Package-level criteria pool the complete pack. Named acceptance-stratum criteria pool only their listed cases. “This case” is diagnostic context; the aggregate value and official verdict remain authoritative. Criterion IDs link back to the overview and its full case breakdown."
    body = (
        "<nav><a href=\"../index.html\">← Back to verification overview</a></nav>"
        "<header><p class=\"eyebrow\">Case-target detail</p><h1>%s</h1>"
        "<p class=\"subtitle\">%s · %s</p></header>" % (
            _h(target_name(target)), _h(result.get("case_id", "")), _h(result.get("scenario_id", "")),
        )
        + _notice()
        + _section("Identity and traceability", _table(["Field", "Value"], ["<tr><th>%s</th><td class=\"mono\">%s</td></tr>" % (_h(key), _h(value)) for key, value in identity_rows], "kv"))
        + render_target_metrics(result, report)
        + (_section("Truth and exclusion policy", "<p class=\"section-note\">%s</p>" % _h(result.get("exclusion_policy", ""))) if result.get("exclusion_policy") else "")
        + _section("Acceptance context", "<p class=\"section-note\">%s</p>%s" % (_h(acceptance_note), criteria_html))
        + _section("Machine-readable evidence", "<p>The complete raw comparison, submission identity, package identity and policy decisions are in <a href=\"../evidence.json\">evidence.json</a>.</p>")
        + "<footer class=\"footer\"><a href=\"../index.html\">← Back to verification overview</a></footer>"
    )
    return _page("%s — %s" % (result.get("case_id", ""), target_name(target)), body)


def render_target_metrics(result, report):
    if not result.get("success", False):
        return _section("Scoring error", "<section class=\"verdict fail\"><h2>Comparison not completed</h2><p>%s</p></section>" % _h(result.get("message", report.get("message", "Unknown scoring error."))))
    score_type = result.get("score_type", "")
    if score_type == "event_detection":
        return _event_metrics(report)
    if score_type == "classification":
        return _classification_metrics(report)
    if score_type == "interval_detection":
        return _interval_metrics(report)
    if score_type == "ecg_delineation":
        return _delineation_metrics(report)
    if score_type == "measurement":
        return _measurement_metrics(report)
    return _section("Comparison result", "<p>Scoring completed. See <a href=\"../evidence.json\">evidence.json</a> for the canonical result.</p>")


def _event_metrics(report):
    comparison = report.get("comparison", {})
    rows = []
    for name, metrics in comparison.get("metrics", {}).items():
        if not isinstance(metrics, dict) or "f1_score" not in metrics:
            continue
        rows.append("<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>" % (
            _h(section_name(name)), metrics.get("ground_truth_count", 0), metrics.get("detection_count", 0),
            metrics.get("true_positive_count", 0), metrics.get("false_positive_count", 0), metrics.get("false_negative_count", 0),
            _h(format_ratio(metrics.get("sensitivity"))), _h(format_ratio(metrics.get("positive_predictive_value"))), _h(format_ratio(metrics.get("f1_score"))),
        ))
    timing_rows = [
        ("Pairing tolerance", format_seconds(comparison.get("tolerance_seconds")), "Maximum time separation used to pair a submitted detection with one observable reference event."),
        ("Matched-event mean absolute timing error", format_seconds(comparison.get("metrics", {}).get("total", {}).get("mean_absolute_error_seconds")), METRIC_DESCRIPTIONS["mean_absolute_error_seconds"]),
        ("Matched-event RMS timing error", format_seconds(comparison.get("metrics", {}).get("total", {}).get("rms_error_seconds")), METRIC_DESCRIPTIONS["rms_error_seconds"]),
        ("Excluded reference events", comparison.get("metrics", {}).get("total", {}).get("excluded_ground_truth_count", 0), "Explicitly reasoned, physically unobservable reference events omitted from FN counts."),
        ("Excluded nearby submitted detections", comparison.get("metrics", {}).get("total", {}).get("excluded_detection_count", 0), "Otherwise-unmatched detections near excluded reference events, reported but omitted from FP counts."),
    ]
    excluded_rows = [
        "<tr><td>%s</td><td>%s</td><td>%s</td></tr>" % (
            item.get("ground_truth_index", ""), _h(format_seconds(item.get("time_seconds"))), _h(item.get("reason", "")))
        for item in comparison.get("excluded_ground_truth", [])
    ]
    excluded_html = ""
    if excluded_rows:
        excluded_html = "<details><summary>Excluded truth events (%d)</summary>%s</details>" % (
            len(excluded_rows), _table(["Truth index", "Time", "Reason"], excluded_rows))
    return _section(
        "Detection metrics",
        "<p class=\"section-note\">TP is a paired reference/detection, FP is an unmatched submitted detection, and FN is an unmatched observable reference event. Strata such as Artifact are pooled independently for the aggregate acceptance checks.</p>"
        + _table([
            "Signal stratum",
            ("Reference", "Observable packaged reference events in this stratum."),
            ("Submitted", "Algorithm detections counted in this stratum."),
            ("TP", "Submitted detections paired with reference events."),
            ("FP", "Unmatched submitted detections."),
            ("FN", "Unmatched observable reference events."),
            ("Sensitivity", METRIC_DESCRIPTIONS["sensitivity"]),
            ("PPV", METRIC_DESCRIPTIONS["positive_predictive_value"]),
            ("F1", METRIC_DESCRIPTIONS["f1_score"]),
        ], rows)
        + _metric_summary_table(timing_rows)
        + excluded_html,
    )


def _classification_metrics(report):
    summary = report.get("summary", {})
    summary_rows = [
        ("Accuracy", format_ratio(summary.get("accuracy")), METRIC_DESCRIPTIONS["accuracy"]),
        ("Micro F1", format_ratio(summary.get("micro_f1_score")), METRIC_DESCRIPTIONS["micro_f1_score"]),
        ("Correct / scored reference", "%s / %s" % (summary.get("correct_count", 0), summary.get("scored_ground_truth_count", 0)), "Correct class labels divided by all scoreable packaged class labels."),
        ("Explicitly excluded reference", summary.get("excluded_ground_truth_count", 0), "Reference beats marked unscoreable by the packaged truth policy."),
        ("Timing tolerance", format_seconds(report.get("tolerance_seconds")), "Maximum time difference used to pair submitted and reference beat labels."),
    ]
    rows = []
    for item in report.get("classes", []):
        rows.append("<tr><td>%s%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>" % (
            _h(item.get("class", "")), "" if item.get("scored", False) else " (not scored)",
            item.get("ground_truth_count", 0), item.get("prediction_count", 0),
            _h(format_ratio(item.get("precision"))), _h(format_ratio(item.get("recall"))), _h(format_ratio(item.get("f1_score"))),
        ))
    return _section("Classification summary", _metric_summary_table(summary_rows) + "<h3>Per-class metrics</h3>" + _table([
        "Class",
        "Reference",
        "Submitted",
        ("Precision", METRIC_DESCRIPTIONS["precision"]),
        ("Recall", METRIC_DESCRIPTIONS["recall"]),
        ("F1", METRIC_DESCRIPTIONS["f1_score"]),
    ], rows))


def _interval_metrics(report):
    overall = report.get("overall", {})
    summary_rows = [
        ("Reference intervals", overall.get("ground_truth_count", 0), "Packaged intervals for the requested label/channel mode."),
        ("Submitted intervals", overall.get("prediction_count", 0), "Intervals supplied by the algorithm."),
        ("Matched intervals", overall.get("matched_count", 0), "Reference/submitted intervals paired under the packaged overlap rule."),
        ("Time sensitivity", format_ratio(overall.get("time_sensitivity")), METRIC_DESCRIPTIONS["time_sensitivity"]),
        ("Time precision", format_ratio(overall.get("time_precision")), METRIC_DESCRIPTIONS["time_precision"]),
        ("Time F1", format_ratio(overall.get("time_f1_score")), METRIC_DESCRIPTIONS["time_f1_score"]),
        ("Temporal IoU", format_ratio(overall.get("temporal_iou")), METRIC_DESCRIPTIONS["temporal_iou"]),
        ("Mean onset error", format_seconds(overall.get("mean_absolute_onset_error_seconds")), METRIC_DESCRIPTIONS["mean_absolute_onset_error_seconds"]),
        ("Mean offset error", format_seconds(overall.get("mean_absolute_offset_error_seconds")), METRIC_DESCRIPTIONS["mean_absolute_offset_error_seconds"]),
    ]
    rows = []
    for item in report.get("classes", []):
        metrics = item.get("metrics", {})
        rows.append("<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>" % (
            _h(item.get("label", "")), metrics.get("ground_truth_count", 0), metrics.get("prediction_count", 0), metrics.get("matched_count", 0),
            _h(format_ratio(metrics.get("time_f1_score"))), _h(format_ratio(metrics.get("temporal_iou"))),
        ))
    extra = "<h3>Per-label metrics</h3>" + _table(["Label", "Reference", "Submitted", "Matched", "Time F1", "IoU"], rows) if rows else ""
    return _section("Interval-detection metrics", _metric_summary_table(summary_rows) + extra)


def _delineation_metrics(report):
    overall = report.get("overall", {})
    summary_rows = [
        ("Reference events", overall.get("ground_truth_count", 0), "Packaged ECG wave-boundary reference events selected by the delineation scope."),
        ("Submitted events", overall.get("prediction_count", 0), "Wave-boundary events supplied by the algorithm."),
        ("Sensitivity", format_ratio(overall.get("sensitivity")), METRIC_DESCRIPTIONS["sensitivity"]),
        ("Positive predictive value", format_ratio(overall.get("positive_predictive_value")), METRIC_DESCRIPTIONS["positive_predictive_value"]),
        ("F1", format_ratio(overall.get("f1_score")), METRIC_DESCRIPTIONS["f1_score"]),
        ("Within tolerance", format_ratio(overall.get("within_tolerance_fraction")), METRIC_DESCRIPTIONS["within_tolerance_fraction"]),
        ("Mean absolute timing error", format_seconds(overall.get("mean_absolute_error_seconds")), METRIC_DESCRIPTIONS["mean_absolute_error_seconds"]),
        ("P95 absolute timing error", format_seconds(overall.get("p95_absolute_error_seconds")), METRIC_DESCRIPTIONS["p95_absolute_error_seconds"]),
    ]
    rows = []
    for item in report.get("by_kind", []):
        metrics = item.get("metrics", item)
        rows.append("<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>" % (
            _h(item.get("kind", item.get("name", ""))), metrics.get("ground_truth_count", 0), metrics.get("prediction_count", 0),
            _h(format_ratio(metrics.get("f1_score"))), _h(format_seconds(metrics.get("mean_absolute_error_seconds"))),
        ))
    extra = "<h3>Per-wave metrics</h3>" + _table(["Wave", "Reference", "Submitted", "F1", "Mean error"], rows) if rows else ""
    return _section("Delineation metrics", _metric_summary_table(summary_rows) + extra)


def _measurement_metrics(report):
    overall = report.get("overall", {})
    tolerance_rules = report.get("tolerance_rules", [])
    numeric_units = sorted(set(
        item.get("unit", "")
        for item in report.get("matches", [])
        if item.get("numeric_pair", False)
    ))
    pooled_mae = _measurement_error_value(overall.get("error", {}).get("mean_absolute"), numeric_units)
    pooled_median = _measurement_error_value(overall.get("error", {}).get("median_absolute"), numeric_units)
    pooled_p95 = _measurement_error_value(overall.get("error", {}).get("p95_absolute"), numeric_units)
    rr_pairing = report.get("options", {}).get("rr_pairing_method") == "peak_anchored_interval_overlap"
    summary_rows = [
        ("Reference values", overall.get("ground_truth_count", 0), "All packaged reference measurements, including explicit non-valid states."),
        ("Submitted measurements", overall.get("prediction_count", 0), "Measurements supplied by the algorithm for this case and target."),
        ("Comparison associations", overall.get("matched_count", 0), "Reference-to-submitted comparison pairs. For RR, one split or merged interval can participate in more than one local association."),
        ("Unique reference values covered", overall.get("covered_truth_count", 0), "Unique packaged reference measurements with at least one association."),
        ("Unique submitted values associated", overall.get("matched_prediction_count", 0), "Unique submitted measurements with at least one reference association."),
        ("Reference values covered", format_ratio(overall.get("truth_match_fraction")), METRIC_DESCRIPTIONS["truth_match_fraction"]),
        ("Submitted measurements matched to reference", format_ratio(overall.get("prediction_match_fraction")), METRIC_DESCRIPTIONS["prediction_match_fraction"]),
        ("Measurements within tolerance", format_ratio(overall.get("tolerance_pass_fraction")), METRIC_DESCRIPTIONS["tolerance_pass_fraction"]),
        ("Status agreement", format_ratio(overall.get("status_match_fraction")), METRIC_DESCRIPTIONS["status_match_fraction"]),
        ("Mean absolute error", pooled_mae, METRIC_DESCRIPTIONS["mean_absolute_error"] + " A pooled value is omitted when contexts use different units."),
        ("Median absolute error", pooled_median, METRIC_DESCRIPTIONS["median_absolute_error"] + " A pooled value is omitted when contexts use different units."),
        ("P95 absolute error", pooled_p95, METRIC_DESCRIPTIONS["p95_absolute_error"] + " A pooled value is omitted when contexts use different units."),
    ]
    context_rows = []
    for item in report.get("by_measurement_context", []):
        metrics = item.get("metrics", {})
        unit = item.get("unit", "")
        context_rows.append("<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>" % (
            _h(MEASUREMENT_NAMES.get(item.get("name", ""), item.get("name", ""))), _h(item.get("scope", "")),
            _h(unit or "unitless"),
            metrics.get("ground_truth_count", 0), metrics.get("prediction_count", 0), metrics.get("matched_count", 0),
            _h(format_ratio(metrics.get("tolerance_pass_fraction"))),
            _h(format_measurement_value(metrics.get("error", {}).get("mean_absolute"), unit)),
            _h(format_measurement_value(metrics.get("error", {}).get("median_absolute"), unit)),
            _h(format_measurement_value(metrics.get("error", {}).get("p95_absolute"), unit)),
            _h(_context_tolerance_text(item, tolerance_rules)),
        ))
    match_rows = []
    for item in report.get("matches", []):
        numeric = item.get("numeric_pair", False)
        passed = item.get("within_tolerance", False) if numeric else item.get("status_matches", False)
        reference = format_measurement_value(item.get("ground_truth_value"), item.get("unit", "")) if numeric else item.get("ground_truth_status", "")
        submitted = format_measurement_value(item.get("prediction_value"), item.get("unit", "")) if numeric else item.get("prediction_status", "")
        error = format_measurement_value(item.get("absolute_error"), item.get("unit", "")) if numeric else "—"
        tolerance = format_measurement_value(item.get("effective_tolerance"), item.get("unit", "")) if numeric else "status equality"
        window = ""
        if "window_start_seconds" in item:
            window = "[%s, %s) s" % (format_plain(item.get("window_start_seconds")), format_plain(item.get("window_end_seconds")))
        match_rows.append("<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>" % (
            _h(MEASUREMENT_NAMES.get(item.get("name", ""), item.get("name", ""))), _h(item.get("scope", "")), _h(window),
            _h(item.get("pairing_method", "").replace("_", " ")),
            _h(reference), _h(submitted), _h(error), _h(tolerance), _h(item.get("reason", "")), _badge("PASS" if passed else "FAIL", "pass" if passed else "fail"),
        ))
    match_html = ""
    if match_rows:
        match_html = "<details><summary>Matched reference vs submitted measurements (%d)</summary>%s</details>" % (
            len(match_rows), _table([
                "Metric", "Scope", "Window",
                ("Association", "RR uses peak-anchored interval overlap; other records use one-to-one identity and temporal-anchor matching."),
                "Reference", "Submitted",
                ("Absolute error", "Absolute value of submitted minus reference."),
                ("Effective tolerance", "Larger of the packaged absolute and relative tolerances for this reference value."),
                "Tolerance rationale", "Verdict",
            ], match_rows),
        )
    return (
        _measurement_tolerance_section(report)
        + (
            _section(
                "RR split/merge policy",
                "<p class=\"section-note\">Each valid RR is the interval ending at its submitted or reference R peak. "
                "A pair is compared when its overlap covers more than %.0f%% of the shorter interval. One extra R peak "
                "therefore creates two local fragment errors against one reference RR; one missed R peak creates one "
                "local merged-interval error against each covered reference RR. Later RR intervals do not shift. "
                "Coverage counts unique intervals, while MAE, median and tolerance percentage use the explicit local "
                "association pairs.</p>" % (
                    100.0 * report.get("options", {}).get(
                        "rr_minimum_shorter_interval_overlap_fraction", 0.5,
                    )
                ),
            ) if rr_pairing else ""
        )
        + _section(
            "Measurement metrics",
            _metric_summary_table(summary_rows)
            + "<h3>Measurement contexts</h3>"
            + _table([
                "Metric", "Scope", "Unit", "Reference", "Submitted", "Matched",
                ("Within tolerance", METRIC_DESCRIPTIONS["tolerance_pass_fraction"]),
                ("MAE", METRIC_DESCRIPTIONS["mean_absolute_error"]),
                ("Median absolute error", METRIC_DESCRIPTIONS["median_absolute_error"]),
                ("P95 absolute error", METRIC_DESCRIPTIONS["p95_absolute_error"]),
                ("Tolerance rule", "Packaged absolute-or-relative rule used for each numeric pair."),
            ], context_rows)
            + match_html,
        )
    )


def _page(title, body):
    return "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>%s</title><style>%s</style></head><body>%s</body></html>" % (_h(title), STYLE, body)


def _notice():
    return "<p class=\"notice\">%s</p>" % _h(NOTICE_TEXT)


def _section(title, content):
    return "<section><h2>%s</h2>%s</section>" % (_h(title), content)


def _policy_table(policy):
    rows = [
        "<tr><th>%s</th><td>%s</td></tr>" % (_h(str(key).replace("_", " ").title()), _h(value))
        for key, value in sorted(policy.items())
    ]
    return _table(["Rule", "Policy"], rows, "kv")


def _metric_summary_table(rows):
    return _table(
        ["Measure", "Value"],
        [
            "<tr><th>%s%s</th><td>%s</td></tr>" % (
                _h(label),
                "<span class=\"raw\">%s</span>" % _h(description) if description else "",
                _h(value),
            )
            for label, value, description in rows
        ],
        "kv",
    )


def _criterion_breakdown_row(check):
    if check.get("scope") == "acceptance_stratum" and len(check.get("case_ids", [])) == 1:
        return ""
    contributions = list(check.get("case_contributions", []))
    contributions.sort(key=lambda item: (
        0 if item.get("contributes") and item.get("diagnostic_passed") is False
        else 1 if item.get("contributes") else 2,
        item.get("case_id", ""),
    ))
    rows = []
    for item in contributions:
        path = item.get("report_path", "")
        case = (
            "<a href=\"%s\">%s</a>" % (_h(path), _h(item.get("case_id", "")))
            if path else _h(item.get("case_id", ""))
        )
        rows.append(
            "<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>" % (
                case,
                _contribution_actual(check, item),
                _h(_count_summary(item.get("counts", {}))),
                _contribution_badge(item),
            )
        )
    body = (
        "<p class=\"muted\">The aggregate above is recomputed from pooled counts or errors across contributing cases; it is not the arithmetic mean of case percentages. Case badges are diagnostic only.</p>"
        + (_table([
            "Case",
            ("Case value", "The metric calculated from this case alone."),
            ("Evidence counts", "The raw counts from this case that feed the aggregate."),
            ("Diagnostic", "Comparison of this case value with the same aggregate threshold."),
        ], rows, "compact") if rows else "<p>No case-target result is in scope.</p>")
    )
    return (
        "<tr class=\"criterion-breakdown\"><td colspan=\"6\"><details><summary>"
        "Case contribution breakdown (%d contributing of %d in scope)</summary>%s"
        "</details></td></tr>"
    ) % (check.get("contributing_case_count", 0), len(contributions), body)


def _case_contribution(check, case_id):
    return next(
        (
            item for item in check.get("case_contributions", [])
            if item.get("case_id") == case_id
        ),
        None,
    )


def _contribution_actual(check, contribution):
    if not contribution or not contribution.get("contributes", False):
        return "<span class=\"muted\">No data for this bin</span>"
    return _h(format_check_value(check, contribution.get("actual")))


def _contribution_badge(contribution):
    if not contribution:
        return _badge("OUT OF SCOPE", "neutral")
    if contribution.get("scoring_status") != "scored":
        return _badge("ERROR", "fail")
    if not contribution.get("contributes", False):
        return _badge("NO DATA", "neutral")
    if contribution.get("diagnostic_passed", False):
        return _badge("MEETS GATE", "pass")
    return _badge("BELOW GATE", "warning")


def _count_summary(counts):
    labels = {
        "ground_truth_count": "Reference",
        "detection_count": "Submitted",
        "prediction_count": "Submitted",
        "scored_ground_truth_count": "Scored reference",
        "scored_prediction_count": "Scored submitted",
        "matched_count": "Matched",
        "covered_truth_count": "Unique reference covered",
        "matched_prediction_count": "Unique submitted associated",
        "paired_count": "Paired",
        "correct_count": "Correct",
        "true_positive_count": "TP",
        "false_positive_count": "FP",
        "false_negative_count": "FN",
        "within_tolerance_count": "Within tolerance",
        "tolerance_pass_count": "Within tolerance",
        "numeric_pair_count": "Numeric pairs",
        "status_match_count": "Status matches",
        "status_mismatch_count": "Status mismatches",
        "false_alarm_count": "False alarms",
        "missed_count": "Missed",
        "missing_count": "Missing",
        "extra_count": "Extra",
        "excluded_ground_truth_count": "Excluded reference",
        "excluded_detection_count": "Excluded submitted",
    }
    return " · ".join(
        "%s %s" % (labels.get(name, name.replace("_", " ").title()), value)
        for name, value in counts.items()
    ) or "No count data"


def _measurement_error_value(value, units):
    if value is None:
        return "—"
    if len(units) == 1:
        return format_measurement_value(value, units[0])
    if len(units) > 1:
        return "Not pooled (mixed units)"
    return format_plain(value)


def _measurement_tolerance_section(report):
    rules = report.get("tolerance_rules", [])
    pairing = format_seconds(report.get("options", {}).get("pairing_window_seconds"))
    if report.get("options", {}).get("rr_pairing_method"):
        pairing_text = (
            "Valid beat-level RR rows use peak-anchored interval overlap; the %s "
            "pairing window applies only to fallback/non-numeric records."
        ) % _h(pairing)
    else:
        pairing_text = (
            "The %s pairing window identifies corresponding reference and "
            "submitted rows."
        ) % _h(pairing)
    intro = (
        "<p class=\"section-note\">%s This association rule is separate from the "
        "numeric pass tolerance below. For each numeric pair, the effective "
        "tolerance is the larger of the packaged absolute tolerance and the "
        "relative percentage of the absolute reference value.</p>"
    ) % pairing_text
    if not rules:
        return _section(
            "Measurement tolerance rules",
            intro + "<p class=\"muted\">No numeric tolerance rule is available for this case-target.</p>",
        )
    rows = []
    for rule in rules:
        rows.append(
            "<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>" % (
                _h(MEASUREMENT_NAMES.get(rule.get("name", ""), rule.get("name", ""))),
                _h(rule.get("scope", "")),
                _h(rule.get("unit", "") or "unitless"),
                _h(_tolerance_rule_text(rule)),
                _h(rule.get("error_model", "").replace("_", " ")),
                rule.get("reference_value_count", 0),
            )
        )
    return _section(
        "Measurement tolerance rules",
        intro + _table([
            "Metric", "Scope", "Unit",
            ("Pass tolerance", "A numeric pair passes when its absolute error is no larger than this effective rule."),
            ("Error model", "Linear difference, or shortest circular difference for angular measurements."),
            "Reference values",
        ], rows),
    )


def _tolerance_rule_text(rule):
    absolute = float(rule.get("absolute_tolerance", 0.0))
    relative = float(rule.get("relative_tolerance_percent", 0.0))
    absolute_text = "±%s absolute" % format_measurement_value(absolute, rule.get("unit", ""))
    relative_text = "±%.6g%% of |reference|" % relative
    if absolute > 0.0 and relative > 0.0:
        return "larger of %s or %s" % (absolute_text, relative_text)
    if relative > 0.0:
        return relative_text
    return absolute_text


def _context_tolerance_text(context, rules):
    names = (
        "name", "unit", "scope", "channel", "formula", "method_id",
        "preprocessing_policy_id",
    )
    matches = [
        rule for rule in rules
        if all((rule.get(name, "") or "") == (context.get(name, "") or "") for name in names)
    ]
    values = []
    for rule in matches:
        text = _tolerance_rule_text(rule)
        if text not in values:
            values.append(text)
    return "; ".join(values) if values else "See packaged reference rule"


def _table_header(value):
    if isinstance(value, (tuple, list)) and len(value) == 2:
        return "<th>%s%s</th>" % (_h(value[0]), _info(value[1]))
    return "<th>%s</th>" % _h(value)


def _info(text):
    escaped = _h(text)
    return "<span class=\"info\" tabindex=\"0\" aria-label=\"Information: %s\" data-tip=\"%s\" title=\"%s\">i</span>" % (
        escaped, escaped, escaped,
    )


def _table(headers, rows, class_name=""):
    return "<div class=\"table-wrap\"><table class=\"%s\"><thead><tr>%s</tr></thead><tbody>%s</tbody></table></div>" % (
        _h(class_name), "".join(_table_header(item) for item in headers), "".join(rows),
    )


def _card(label, value):
    return "<div class=\"card\"><span>%s</span><strong>%s</strong></div>" % (_h(label), _h(value))


def _badge(label, kind):
    return "<span class=\"badge %s\">%s</span>" % (_h(kind), _h(label))


def _required_value(check):
    if not check.get("applicable", False):
        return "<span class=\"muted\">Not applicable</span>"
    symbol = "≥" if check.get("operator") == "min" else "≤"
    return "%s %s" % (_h(symbol), _h(format_check_value(check, check.get("threshold"))))


def _actual_value(check):
    if not check.get("applicable", False) or check.get("actual") is None:
        return "<span class=\"muted\">Not evaluated</span>"
    return _h(format_check_value(check, check.get("actual")))


def _margin_value(check):
    if not check.get("applicable", False) or check.get("actual") is None:
        return "—"
    margin = float(check["actual"]) - float(check["threshold"]) if check.get("operator") == "min" else float(check["threshold"]) - float(check["actual"])
    prefix = "+" if margin >= 0 else ""
    return _h(prefix + format_check_value(check, margin))


def format_check_value(check, value):
    unit = check.get("unit", "value")
    if value is None:
        return "—"
    value = float(value)
    if unit == "ratio":
        return "%.2f%% (%.6g)" % (100.0 * value, value)
    if unit == "seconds":
        return format_seconds(value)
    if unit == "seconds_squared":
        return "%.6g s²" % value
    if unit == "percentage_points":
        return "%.6g percentage points" % value
    if unit == "bpm":
        return "%.6g bpm" % value
    return "%.6g" % value


def format_seconds(value):
    if value is None:
        return "—"
    numeric = float(value)
    if abs(numeric) < 1.0:
        return "%.3f ms (%.6g s)" % (1000.0 * numeric, numeric)
    return "%.6g s" % numeric


def format_ratio(value):
    if value is None:
        return "—"
    return "%.2f%%" % (100.0 * float(value))


def format_plain(value):
    if value is None:
        return "—"
    if isinstance(value, bool):
        return _yes_no(value)
    if isinstance(value, (int, float)) and math.isfinite(float(value)):
        return "%.6g" % float(value)
    return str(value)


def format_measurement_value(value, unit):
    if value is None:
        return "—"
    numeric = float(value)
    if unit == "s":
        return format_seconds(numeric)
    if unit == "s2":
        return "%.6g s²" % numeric
    if unit == "percent":
        return "%.6g %%" % numeric
    if unit:
        return "%.6g %s" % (numeric, unit)
    return "%.6g" % numeric


def primary_score_details(item):
    report = item.get("comparison", {})
    score_type = item.get("score_type", "")
    if score_type == "event_detection":
        value = report.get("comparison", {}).get("metrics", {}).get("total", {}).get("f1_score")
        section, metric = "total", "f1_score"
    elif score_type == "classification":
        value = report.get("summary", {}).get("micro_f1_score")
        section, metric = "summary", "micro_f1_score"
    elif score_type == "interval_detection":
        value = report.get("overall", {}).get("time_f1_score")
        section, metric = "overall", "time_f1_score"
    elif score_type == "ecg_delineation":
        value = report.get("overall", {}).get("f1_score")
        section, metric = "overall", "f1_score"
    elif score_type == "measurement":
        value = report.get("overall", {}).get("tolerance_pass_fraction")
        section, metric = "overall", "tolerance_pass_fraction"
    else:
        value = None
        section, metric = "", ""
    if value is not None:
        display_value = format_ratio(value)
    else:
        display_value = item.get("message", "—") or "—"
    return {
        "section": section,
        "metric": metric,
        "label": metric_name(metric) if metric else "No primary metric",
        "value": display_value,
    }


def primary_score(item):
    return primary_score_details(item)["value"]


def _with_unit(value, unit):
    return "" if value is None else "%s %s" % (format_plain(value), unit)


def _yes_no(value):
    return "yes" if value else "no"


def _h(value):
    return html.escape(str(value), quote=True)
