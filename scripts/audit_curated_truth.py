#!/usr/bin/env python3
"""Render and audit every curated pack's user-visible truth contract."""

import argparse
import json
import math
import os
import re
import shutil
import subprocess
import sys
import tempfile


NOTICE = "Synthetic engineering QA evidence; not diagnosis, nor clinical evidence"
TRUTH_CONTRACT = "synsigra_observable_event_truth_v1"
ALLOWED_EXCLUSIONS = {
    "near_total_all_lead_ecg_dropout",
    "record_boundary_truncated_qrs",
    "near_total_ppg_dropout",
    "pulse_not_valid_for_peak_scoring",
}


class AuditError(RuntimeError):
    pass


def read_json(path):
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def require(condition, context, message):
    if not condition:
        raise AuditError("%s: %s" % (context, message))


def finite_fraction(value):
    return isinstance(value, (int, float)) and math.isfinite(value) and 0.0 <= value <= 1.0


def audit_html(root, path, context):
    with open(path, "r", encoding="utf-8") as handle:
        document = handle.read()
    require(document.count(NOTICE) == 1, context, "HTML must contain the canonical notice exactly once")
    require("background:#fef3f2" not in document and "border-left:4px solid #b42318" not in document,
            context, "HTML contains a legacy red notice")
    for link in re.findall(r'href=["\']([^"\']+)["\']', document, flags=re.IGNORECASE):
        if link.startswith(("http://", "https://", "mailto:", "#")):
            continue
        target = os.path.normpath(os.path.join(os.path.dirname(path), link.split("#", 1)[0]))
        require(os.path.commonpath((root, target)) == root, context, "link escapes package: %s" % link)
        require(os.path.isfile(target), context, "broken internal link: %s" % link)


def measurement_rows(document):
    for target in document.get("targets", []):
        for item in target.get("measurements", []):
            yield target.get("target", ""), item


def audit_case(root, case_dir, totals):
    case_id = os.path.basename(case_dir)
    context = os.path.basename(root) + "/" + case_id
    annotations = read_json(os.path.join(case_dir, "annotations.json"))
    summary = read_json(os.path.join(case_dir, "case_summary.json"))
    require(annotations.get("schema_version") == 2, context, "annotations schema is not v2")
    policy = annotations.get("truth_policy", {})
    require(policy.get("contract") == TRUTH_CONTRACT, context, "missing observable-event truth policy")
    require(policy.get("minimum_retained_signal_fraction") == 0.05, context, "unexpected observability threshold")
    duration = float(summary["render"]["duration_seconds"])

    scoreable_by_beat = {}
    ordered_qrs = []
    for array_index, beat in enumerate(annotations.get("beats", [])):
        if not beat.get("qrs_present", False):
            continue
        beat_index = int(beat.get("beat_index", array_index))
        scoreable = beat.get("r_peak_scoreable")
        reason = beat.get("r_peak_exclusion_reason", "")
        retained = beat.get("r_peak_retained_signal_fraction")
        complete = beat.get("complete_qrs_support")
        require(isinstance(scoreable, bool), context, "beat %s lacks scoreability" % beat_index)
        require(finite_fraction(retained), context, "beat %s has invalid retained-signal fraction" % beat_index)
        require(isinstance(complete, bool), context, "beat %s lacks QRS support state" % beat_index)
        qrs_offset = float(beat["qrs_onset_seconds"]) + float(beat["qrs_seconds"])
        require(complete == (float(beat["qrs_onset_seconds"]) >= 0.0 and qrs_offset <= duration),
                context, "beat %s boundary classification is inconsistent" % beat_index)
        require((scoreable and not reason) or (not scoreable and reason in ALLOWED_EXCLUSIONS),
                context, "beat %s has inconsistent exclusion reason %r" % (beat_index, reason))
        if reason == "near_total_all_lead_ecg_dropout":
            require(retained <= 0.05 and complete, context, "dropout exclusion is not deterministic")
        if reason == "record_boundary_truncated_qrs":
            require(not complete, context, "boundary exclusion has complete QRS support")
        scoreable_by_beat[beat_index] = scoreable
        ordered_qrs.append(beat_index)
        totals["qrs"] += 1
        totals["excluded_qrs"] += int(not scoreable)

    for group in ("ppg_fiducials", "ppg_channel_fiducials"):
        for item in annotations.get(group, []):
            if item.get("source") != "measurement":
                continue
            scoreable = item.get("scoreable")
            reason = item.get("exclusion_reason", "")
            require(isinstance(scoreable, bool), context, "%s item lacks scoreability" % group)
            require(finite_fraction(item.get("retained_signal_fraction")), context,
                    "%s item has invalid retained-signal fraction" % group)
            require((scoreable and not reason) or (not scoreable and reason in ALLOWED_EXCLUSIONS),
                    context, "%s item has inconsistent exclusion reason %r" % (group, reason))
            totals["ppg"] += 1
            totals["excluded_ppg"] += int(not scoreable)

    truth_path = os.path.join(case_dir, "measurement_truth.json")
    if os.path.isfile(truth_path):
        measurement_truth = read_json(truth_path)
        require(measurement_truth.get("contract") == "synsigra_measurement_truth_v2", context,
                "measurement truth contract is not v2")
        previous_by_beat = dict((ordered_qrs[index], ordered_qrs[index - 1]) for index in range(1, len(ordered_qrs)))
        for target, item in measurement_rows(measurement_truth):
            measurement = item.get("measurement", {})
            status = measurement.get("status")
            require(status in ("valid", "undefined", "absent", "not_evaluable"), context,
                    "invalid measurement status")
            if status != "valid":
                require("value" not in measurement, context, "non-valid measurement carries a numeric value")
                require(bool(item.get("reason")), context, "non-valid measurement lacks a reason")
                totals["nonvalid_measurements"] += 1
            if target == "rr_interval" and measurement.get("name") == "rr_interval" and "beat_index" in measurement:
                current = int(measurement["beat_index"])
                previous = previous_by_beat.get(current)
                if previous is not None and (not scoreable_by_beat.get(previous, True) or not scoreable_by_beat.get(current, True)):
                    require(status == "not_evaluable", context,
                            "RR interval touching excluded event remains numerically scoreable")

    for path_root, _directories, filenames in os.walk(case_dir):
        for name in filenames:
            if name.endswith(".html"):
                audit_html(root, os.path.join(path_root, name), context + "/" + name)
    totals["cases"] += 1


def audit_pack(cli, source_root, entry, noise_assets, keep_root):
    catalog_dir = os.path.dirname(entry["_catalog_path"])
    pack_path = os.path.normpath(os.path.join(catalog_dir, entry["path"]))
    work_parent = keep_root or tempfile.mkdtemp(prefix="synsigra-truth-audit-")
    output = os.path.join(work_parent, entry["pack_id"])
    totals = {"cases": 0, "qrs": 0, "excluded_qrs": 0, "ppg": 0, "excluded_ppg": 0,
              "nonvalid_measurements": 0}
    try:
        command = [cli, "pack", "challenge", pack_path, "--out", output, "--noise-assets", noise_assets]
        process = subprocess.run(command, cwd=source_root, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                 text=True, check=False)
        require(process.returncode == 0, entry["pack_id"],
                "generation failed: %s" % (process.stderr.strip() or process.stdout.strip()))
        receipt = json.loads(process.stdout)
        require(receipt.get("package_id") == entry["pack_id"], entry["pack_id"], "receipt package ID mismatch")
        audit_html(output, os.path.join(output, "index.html"), entry["pack_id"] + "/index.html")
        cases_root = os.path.join(output, "cases")
        case_dirs = sorted(os.path.join(cases_root, name) for name in os.listdir(cases_root)
                           if os.path.isdir(os.path.join(cases_root, name)))
        require(len(case_dirs) == receipt.get("scenario_count"), entry["pack_id"], "case count mismatch")
        for case_dir in case_dirs:
            audit_case(output, case_dir, totals)
        return totals
    finally:
        if not keep_root:
            shutil.rmtree(work_parent, ignore_errors=True)


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    source_root = os.path.dirname(script_dir)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", default=os.path.join(source_root, "build", "signal-synth"))
    parser.add_argument("--catalog", default=os.path.join(source_root, "examples", "catalog", "verification_packs_v1.json"))
    parser.add_argument("--noise-assets", default=os.path.join(source_root, "examples", "assets", "noise"))
    parser.add_argument("--pack-id", action="append", default=[])
    parser.add_argument("--keep-root", help="Keep rendered packs below this existing directory.")
    args = parser.parse_args()
    catalog = read_json(args.catalog)
    selected = set(args.pack_id)
    entries = []
    for source in catalog.get("packs", []):
        if selected and source.get("pack_id") not in selected:
            continue
        entry = dict(source)
        entry["_catalog_path"] = os.path.abspath(args.catalog)
        entries.append(entry)
    missing = selected.difference(entry["pack_id"] for entry in entries)
    require(not missing, "catalog", "unknown pack IDs: %s" % ", ".join(sorted(missing)))
    require(entries, "catalog", "no packs selected")
    combined = {"packs": 0, "cases": 0, "qrs": 0, "excluded_qrs": 0, "ppg": 0,
                "excluded_ppg": 0, "nonvalid_measurements": 0}
    for entry in entries:
        totals = audit_pack(os.path.abspath(args.cli), source_root, entry, os.path.abspath(args.noise_assets), args.keep_root)
        combined["packs"] += 1
        for key, value in totals.items():
            combined[key] += value
        print("pack=%s cases=%d qrs=%d excluded_qrs=%d ppg=%d excluded_ppg=%d" %
              (entry["pack_id"], totals["cases"], totals["qrs"], totals["excluded_qrs"],
               totals["ppg"], totals["excluded_ppg"]))
    print("truth_audit=ok " + " ".join("%s=%d" % (key, combined[key]) for key in
          ("packs", "cases", "qrs", "excluded_qrs", "ppg", "excluded_ppg", "nonvalid_measurements")))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (AuditError, OSError, ValueError, KeyError) as error:
        print("truth_audit=failed error=%s" % error, file=sys.stderr)
        sys.exit(1)
