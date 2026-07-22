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
    "micro_f1_score": "Micro-averaged F1 score",
    "micro_precision": "Micro-averaged precision",
    "micro_recall": "Micro-averaged recall",
    "p95_absolute_error": "95th-percentile absolute error",
    "p95_absolute_error_seconds": "95th-percentile timing error",
    "positive_predictive_value": "Positive predictive value",
    "prediction_match_fraction": "Predictions matched to truth",
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
    "truth_match_fraction": "Truth measurements matched",
    "within_tolerance_fraction": "Events within timing tolerance",
}


METRIC_DESCRIPTIONS = {
    "accuracy": "Share of scored labels classified correctly.",
    "assertion_agreement_fraction": "Share of comparable assertions for which truth and algorithm agree.",
    "f1_score": "Harmonic mean of sensitivity and positive predictive value.",
    "mean_absolute_error": "Average absolute difference between matched measured and truth values.",
    "mean_absolute_error_seconds": "Average absolute timing difference for matched events.",
    "mean_absolute_offset_error_seconds": "Average absolute end-boundary timing difference for matched intervals.",
    "mean_absolute_onset_error_seconds": "Average absolute start-boundary timing difference for matched intervals.",
    "micro_f1_score": "F1 score after pooling all scored classes.",
    "p95_absolute_error": "Absolute error below which 95% of matched values fall.",
    "positive_predictive_value": "Share of algorithm detections that match truth.",
    "prediction_match_fraction": "Share of submitted measurements paired with a truth measurement.",
    "sensitivity": "Share of truth events detected by the algorithm.",
    "status_match_fraction": "Share of matched measurements with identical validity status.",
    "temporal_iou": "Duration overlap divided by the union of truth and predicted intervals.",
    "time_f1_score": "F1 score computed from time-weighted interval overlap.",
    "tolerance_pass_fraction": "Share of numeric pairs whose absolute error is within the packaged tolerance.",
    "truth_match_fraction": "Share of truth measurements paired with a submitted measurement.",
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
    "maximum_absolute_error", "mean_absolute_error", "p95_absolute_error",
    "root_mean_square_error",
])


STYLE = """
:root{color-scheme:light;--ink:#172033;--muted:#5f6b7a;--line:#d8dee8;--soft:#f6f8fb;--pass:#176b45;--pass-bg:#eaf7f0;--fail:#a12828;--fail-bg:#fff0f0;--accent:#3157b7}
*{box-sizing:border-box}body{font:14px/1.5 Inter,ui-sans-serif,system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;color:var(--ink);max-width:1240px;margin:0 auto;padding:28px 32px 64px;background:#fff}a{color:var(--accent);text-underline-offset:2px}nav{margin-bottom:20px}.eyebrow{font-size:12px;letter-spacing:.08em;text-transform:uppercase;color:var(--muted);font-weight:700;margin:0 0 4px}h1{font-size:30px;line-height:1.15;margin:0 0 8px}h2{font-size:20px;margin:34px 0 10px}h3{font-size:16px;margin:22px 0 8px}.subtitle,.muted{color:var(--muted)}.notice{border-left:4px solid #6b7280;background:#f3f4f6;color:#374151;padding:10px 14px;margin:20px 0 24px}.verdict{border:1px solid var(--line);border-left-width:7px;border-radius:8px;padding:18px 20px;margin:18px 0}.verdict.pass{border-left-color:var(--pass);background:var(--pass-bg)}.verdict.fail{border-left-color:var(--fail);background:var(--fail-bg)}.verdict h2{margin:0 0 4px}.cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(165px,1fr));gap:10px;margin:14px 0 24px}.card{border:1px solid var(--line);border-radius:7px;padding:12px;background:var(--soft)}.card strong{display:block;font-size:20px}.table-wrap{overflow-x:auto;border:1px solid var(--line);border-radius:7px;margin:10px 0 22px}table{border-collapse:collapse;width:100%;min-width:660px}th,td{border-bottom:1px solid var(--line);padding:9px 10px;text-align:left;vertical-align:top}th{background:#eef2f7;font-size:12px;letter-spacing:.02em}tr:last-child td{border-bottom:0}.kv th{width:220px}.badge{display:inline-block;border-radius:999px;padding:2px 8px;font-size:11px;font-weight:800;letter-spacing:.04em}.badge.pass{color:var(--pass);background:var(--pass-bg)}.badge.fail{color:var(--fail);background:var(--fail-bg)}.badge.neutral{color:#4b5563;background:#eceff3}.raw{display:block;color:var(--muted);font-size:11px}.mono{font:12px/1.45 ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;overflow-wrap:anywhere}.criterion{min-width:210px}.section-note{border:1px solid var(--line);background:var(--soft);padding:10px 12px;border-radius:6px}.footer{border-top:1px solid var(--line);margin-top:36px;padding-top:16px;color:var(--muted)}details{border:1px solid var(--line);border-radius:7px;padding:10px 12px;margin:12px 0}summary{font-weight:700;cursor:pointer}.nowrap{white-space:nowrap}
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

    if overall_success:
        verdict_title = "PASS — all acceptance criteria met"
        verdict_text = "%d of %d applicable criteria passed and every selected case-target was scored." % (passed, len(applicable))
    elif not scoring_success:
        verdict_title = "INCOMPLETE — scoring could not be completed"
        verdict_text = "%d case-target result(s) contain missing, unsupported, or invalid algorithm output." % summary.get("incomplete_case_target_count", 0)
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
        ("Acceptance profile", policy.get("profile_id", "")),
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
        )

    case_rows = []
    for item in summary.get("results", []):
        scored = bool(item.get("success", False))
        detail_path = item.get("report_path", "")
        detail = "<a href=\"%s\">Open detail report</a>" % _h(detail_path) if detail_path else "—"
        case_rows.append(
            "<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>" % (
                _h(item.get("case_id", "")), _h(target_name(item.get("target", ""))),
                _badge("SCORED" if scored else "ERROR", "pass" if scored else "fail"),
                _h(primary_score(item)), detail,
            )
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
            _card("Criteria passed", "%s / %s" % (passed, len(applicable))),
            _card("Package integrity", "PASS" if summary.get("integrity", {}).get("ok", False) else "FAIL"),
            _card("Evidence eligible", _yes_no(verification.get("evidence_eligible", False)).upper()),
        ])
        + _section("Run identity and provenance", _table(["Field", "Value"], ["<tr><th>%s</th><td class=\"mono\">%s</td></tr>" % (_h(key), _h(value)) for key, value in identity_rows], "kv"))
        + _section("Acceptance criteria", "<p class=\"section-note\">%s %s Every applicable row is evaluated against the immutable profile shown above; raw numeric values remain in <a href=\"evidence.json\">evidence.json</a>.</p>" % (_h(mode_note), _h(summary.get("threshold_profile", {}).get("description", ""))) + _table(["ID", "Metric", "Required", "Actual", "Margin", "Verdict"], criteria_rows))
        + _section("Case-target traceability", "<p class=\"subtitle\">SCORED means that the case-target produced a valid comparison. Acceptance is decided by the aggregate criteria above.</p>" + _table(["Case", "Target", "Scoring", "Primary result", "Detail"], case_rows))
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
        criteria_rows.append("<tr><td><a href=\"../index.html#criterion-%s\">%s</a></td><td>%s</td><td>%s</td><td>%s</td></tr>" % (
            _h(check.get("criterion_id", "")), _h(check.get("criterion_id", "")),
            _h(check.get("display_name", "")), _required_value(check),
            _badge(verdict, "neutral" if not applicable else "pass" if check.get("passed", False) else "fail"),
        ))
    criteria_html = _table(["ID", "Aggregate criterion", "Required", "Verdict"], criteria_rows) if criteria_rows else "<p>No acceptance criteria are defined for this target.</p>"
    body = (
        "<nav><a href=\"../index.html\">← Back to verification overview</a></nav>"
        "<header><p class=\"eyebrow\">Case-target detail</p><h1>%s</h1>"
        "<p class=\"subtitle\">%s · %s</p></header>" % (
            _h(target_name(target)), _h(result.get("case_id", "")), _h(result.get("scenario_id", "")),
        )
        + _notice()
        + _section("Identity and traceability", _table(["Field", "Value"], ["<tr><th>%s</th><td class=\"mono\">%s</td></tr>" % (_h(key), _h(value)) for key, value in identity_rows], "kv"))
        + render_target_metrics(result, report)
        + _section("Acceptance context", "<p class=\"section-note\">Package-level criteria aggregate the complete pack. Named acceptance-stratum criteria aggregate only their listed cases; this case is shown only the criteria to which it contributes.</p>" + criteria_html)
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
        ("Pairing tolerance", format_seconds(comparison.get("tolerance_seconds"))),
        ("Matched-event mean absolute timing error", format_seconds(comparison.get("metrics", {}).get("total", {}).get("mean_absolute_error_seconds"))),
        ("Matched-event RMS timing error", format_seconds(comparison.get("metrics", {}).get("total", {}).get("rms_error_seconds"))),
    ]
    return _section("Detection metrics", _table(["Signal stratum", "Truth", "Predicted", "TP", "FP", "FN", "Sensitivity", "PPV", "F1"], rows) + _table(["Timing measure", "Value"], ["<tr><th>%s</th><td>%s</td></tr>" % (_h(key), _h(value)) for key, value in timing_rows], "kv"))


def _classification_metrics(report):
    summary = report.get("summary", {})
    summary_rows = [
        ("Accuracy", format_ratio(summary.get("accuracy"))),
        ("Micro F1", format_ratio(summary.get("micro_f1_score"))),
        ("Correct / scored truth", "%s / %s" % (summary.get("correct_count", 0), summary.get("scored_ground_truth_count", 0))),
        ("Timing tolerance", format_seconds(report.get("tolerance_seconds"))),
    ]
    rows = []
    for item in report.get("classes", []):
        rows.append("<tr><td>%s%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>" % (
            _h(item.get("class", "")), "" if item.get("scored", False) else " (not scored)",
            item.get("ground_truth_count", 0), item.get("prediction_count", 0),
            _h(format_ratio(item.get("precision"))), _h(format_ratio(item.get("recall"))), _h(format_ratio(item.get("f1_score"))),
        ))
    return _section("Classification summary", _table(["Measure", "Value"], ["<tr><th>%s</th><td>%s</td></tr>" % (_h(key), _h(value)) for key, value in summary_rows], "kv") + "<h3>Per-class metrics</h3>" + _table(["Class", "Truth", "Predicted", "Precision", "Recall", "F1"], rows))


def _interval_metrics(report):
    overall = report.get("overall", {})
    summary_rows = [
        ("Truth intervals", overall.get("ground_truth_count", 0)),
        ("Predicted intervals", overall.get("prediction_count", 0)),
        ("Matched intervals", overall.get("matched_count", 0)),
        ("Time sensitivity", format_ratio(overall.get("time_sensitivity"))),
        ("Time precision", format_ratio(overall.get("time_precision"))),
        ("Time F1", format_ratio(overall.get("time_f1_score"))),
        ("Temporal IoU", format_ratio(overall.get("temporal_iou"))),
        ("Mean onset error", format_seconds(overall.get("mean_absolute_onset_error_seconds"))),
        ("Mean offset error", format_seconds(overall.get("mean_absolute_offset_error_seconds"))),
    ]
    rows = []
    for item in report.get("classes", []):
        metrics = item.get("metrics", {})
        rows.append("<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>" % (
            _h(item.get("label", "")), metrics.get("ground_truth_count", 0), metrics.get("prediction_count", 0), metrics.get("matched_count", 0),
            _h(format_ratio(metrics.get("time_f1_score"))), _h(format_ratio(metrics.get("temporal_iou"))),
        ))
    extra = "<h3>Per-label metrics</h3>" + _table(["Label", "Truth", "Predicted", "Matched", "Time F1", "IoU"], rows) if rows else ""
    return _section("Interval-detection metrics", _table(["Measure", "Value"], ["<tr><th>%s</th><td>%s</td></tr>" % (_h(key), _h(value)) for key, value in summary_rows], "kv") + extra)


def _delineation_metrics(report):
    overall = report.get("overall", {})
    summary_rows = [
        ("Truth events", overall.get("ground_truth_count", 0)),
        ("Predicted events", overall.get("prediction_count", 0)),
        ("Sensitivity", format_ratio(overall.get("sensitivity"))),
        ("Positive predictive value", format_ratio(overall.get("positive_predictive_value"))),
        ("F1", format_ratio(overall.get("f1_score"))),
        ("Within tolerance", format_ratio(overall.get("within_tolerance_fraction"))),
        ("Mean absolute timing error", format_seconds(overall.get("mean_absolute_error_seconds"))),
        ("P95 absolute timing error", format_seconds(overall.get("p95_absolute_error_seconds"))),
    ]
    rows = []
    for item in report.get("by_kind", []):
        metrics = item.get("metrics", item)
        rows.append("<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>" % (
            _h(item.get("kind", item.get("name", ""))), metrics.get("ground_truth_count", 0), metrics.get("prediction_count", 0),
            _h(format_ratio(metrics.get("f1_score"))), _h(format_seconds(metrics.get("mean_absolute_error_seconds"))),
        ))
    extra = "<h3>Per-wave metrics</h3>" + _table(["Wave", "Truth", "Predicted", "F1", "Mean error"], rows) if rows else ""
    return _section("Delineation metrics", _table(["Measure", "Value"], ["<tr><th>%s</th><td>%s</td></tr>" % (_h(key), _h(value)) for key, value in summary_rows], "kv") + extra)


def _measurement_metrics(report):
    overall = report.get("overall", {})
    summary_rows = [
        ("Truth values", overall.get("ground_truth_count", 0)),
        ("Submitted values", overall.get("prediction_count", 0)),
        ("Matched values", overall.get("matched_count", 0)),
        ("Truth matched", format_ratio(overall.get("truth_match_fraction"))),
        ("Predictions matched", format_ratio(overall.get("prediction_match_fraction"))),
        ("Within tolerance", format_ratio(overall.get("tolerance_pass_fraction"))),
        ("Status agreement", format_ratio(overall.get("status_match_fraction"))),
        ("Mean absolute error", format_plain(overall.get("error", {}).get("mean_absolute"))),
        ("P95 absolute error", format_plain(overall.get("error", {}).get("p95_absolute"))),
    ]
    context_rows = []
    for item in report.get("by_measurement_context", []):
        metrics = item.get("metrics", {})
        context_rows.append("<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>" % (
            _h(MEASUREMENT_NAMES.get(item.get("name", ""), item.get("name", ""))), _h(item.get("scope", "")),
            metrics.get("ground_truth_count", 0), metrics.get("prediction_count", 0), metrics.get("matched_count", 0),
            _h(format_ratio(metrics.get("tolerance_pass_fraction"))), _h(format_plain(metrics.get("error", {}).get("mean_absolute"))),
        ))
    match_rows = []
    for item in report.get("matches", []):
        numeric = item.get("numeric_pair", False)
        passed = item.get("within_tolerance", False) if numeric else item.get("status_matches", False)
        truth = format_measurement_value(item.get("ground_truth_value"), item.get("unit", "")) if numeric else item.get("ground_truth_status", "")
        predicted = format_measurement_value(item.get("prediction_value"), item.get("unit", "")) if numeric else item.get("prediction_status", "")
        error = format_measurement_value(item.get("absolute_error"), item.get("unit", "")) if numeric else "—"
        tolerance = format_measurement_value(item.get("effective_tolerance"), item.get("unit", "")) if numeric else "status equality"
        window = ""
        if "window_start_seconds" in item:
            window = "[%s, %s) s" % (format_plain(item.get("window_start_seconds")), format_plain(item.get("window_end_seconds")))
        match_rows.append("<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>" % (
            _h(MEASUREMENT_NAMES.get(item.get("name", ""), item.get("name", ""))), _h(item.get("scope", "")), _h(window),
            _h(truth), _h(predicted), _h(error), _h(tolerance), _h(item.get("reason", "")), _badge("PASS" if passed else "FAIL", "pass" if passed else "fail"),
        ))
    match_html = ""
    if match_rows:
        match_html = "<details><summary>Matched truth vs algorithm values (%d)</summary>%s</details>" % (
            len(match_rows), _table(["Metric", "Scope", "Window", "Truth", "Algorithm", "Absolute error", "Tolerance", "Tolerance rationale", "Verdict"], match_rows),
        )
    return _section("Measurement metrics", _table(["Measure", "Value"], ["<tr><th>%s</th><td>%s</td></tr>" % (_h(key), _h(value)) for key, value in summary_rows], "kv") + "<h3>Measurement contexts</h3>" + _table(["Metric", "Scope", "Truth", "Submitted", "Matched", "Within tolerance", "MAE"], context_rows) + match_html)


def _page(title, body):
    return "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>%s</title><style>%s</style></head><body>%s</body></html>" % (_h(title), STYLE, body)


def _notice():
    return "<p class=\"notice\">%s</p>" % _h(NOTICE_TEXT)


def _section(title, content):
    return "<section><h2>%s</h2>%s</section>" % (_h(title), content)


def _table(headers, rows, class_name=""):
    return "<div class=\"table-wrap\"><table class=\"%s\"><thead><tr>%s</tr></thead><tbody>%s</tbody></table></div>" % (
        _h(class_name), "".join("<th>%s</th>" % _h(item) for item in headers), "".join(rows),
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


def primary_score(item):
    report = item.get("comparison", {})
    score_type = item.get("score_type", "")
    if score_type == "event_detection":
        value = report.get("comparison", {}).get("metrics", {}).get("total", {}).get("f1_score")
    elif score_type == "classification":
        value = report.get("summary", {}).get("micro_f1_score")
    elif score_type == "interval_detection":
        value = report.get("overall", {}).get("time_f1_score")
    elif score_type == "ecg_delineation":
        value = report.get("overall", {}).get("f1_score")
    elif score_type == "measurement":
        value = report.get("overall", {}).get("tolerance_pass_fraction")
    else:
        value = None
    if value is not None:
        return format_ratio(value)
    return item.get("message", "—") or "—"


def _with_unit(value, unit):
    return "" if value is None else "%s %s" % (format_plain(value), unit)


def _yes_no(value):
    return "yes" if value else "no"


def _h(value):
    return html.escape(str(value), quote=True)
