# Generic Interval Output And Scoring

**Document ID:** SYN-INC-053

**Version:** 1.0

**Status:** Verified

**Owner role:** Engineering

**Date:** 2026-07-17

**Implementation issue:** [signal_synth#74](https://github.com/tamask1s/signal_synth/issues/74)

## 1. Purpose

Add one reusable interval contract and scoring engine before adding ECG
delineation, measurement scoring, or more rhythm names. The first consumers
are rhythm-episode and signal-quality algorithms. They must not grow separate
parsers, matching rules, or report semantics.

This is an engineering verification feature. It does not establish clinical
diagnostic performance.

## 2. Public Contract

`interval_json_v1` contains:

```json
{
  "schema_version": 1,
  "algorithm": {"name": "example", "version": "1.0"},
  "target": "rhythm_episode",
  "intervals": [
    {
      "start_seconds": 2.0,
      "end_seconds": 6.0,
      "label": "psvt",
      "channel": "global",
      "confidence": 0.98
    }
  ]
}
```

`interval_csv_v1` has the columns `start_seconds,end_seconds,label,channel`
and optional `confidence`. CSV receives its target from the scoring command.

Intervals are half-open `[start_seconds,end_seconds)`. Start is finite and
non-negative; end is finite and strictly greater than start. Empty prediction
sets are valid. Labels are required. Missing channels normalize to `global`.
Exact duplicates and mixed global/physical-channel signal-quality documents
are rejected.

## 3. Ground Truth

`rhythm_episode` uses global PSVT and SVARR construction intervals. Dynamic
repolarization episodes are not rhythm truth.

`signal_quality` supports two document-wide modes:

- `global`: each injected artifact contributes one typed interval;
- `per_channel`: each artifact is expanded to the ECG, PPG, and motion
  reference channels it affects.

An empty signal-quality prediction document selects global mode. This keeps a
valid no-detection result unambiguous.

## 4. Metrics

For each class and for the micro aggregate, the engine reports:

- truth, prediction, matched, false-alarm, and missed interval counts;
- truth duration, prediction duration, correctly overlapped duration;
- time sensitivity, time precision, time F1, and temporal IoU;
- event sensitivity and event precision;
- signed and absolute onset/offset errors for matched intervals;
- false alarms per hour.

Duration metrics merge overlapping intervals independently for each
label/channel before measuring overlap. Channel-specific aggregate durations
therefore represent channel-seconds. Classes may overlap and remain
independently scoreable.

Event matching is deterministic. Eligible pairs share channel and, for class
metrics, label; their IoU must meet `minimum_iou`. Candidates are ordered by
descending IoU, then boundary error and source indices, and accepted one to
one. A second label-agnostic matching pass produces the confusion matrix with
explicit `__missed__` and `__false_alarm__` margins.

Metrics with a zero denominator are `null` in JSON and `NA` in CSV/HTML. They
are never silently reported as perfect or zero performance.

## 5. Interfaces

- C++: `interval_io.h` and `interval_scoring.h`;
- CLI: `signal-synth interval score <target> ...`;
- Python: interval loading plus `score_rhythm_episodes` and
  `score_signal_quality`;
- challenge metadata: `interval_json_v1`, `interval_csv_v1`, interval scoring
  commands, and `time_f1_score` as the primary metric;
- generator-free verifier: the same metric definitions using exported
  `annotations.json` and `case_summary.json` only.

## 6. Failure And Resource Policy

- Unknown JSON members and CSV columns are errors.
- Inputs outside the rendered record are errors.
- Target and channel-mode mismatches are errors.
- Input remains subject to the CLI's 16 MiB bound.
- Reports are written only after parsing, rendering, and scoring succeed, and
  the output directory must not already exist.
- The implementation remains C++11 and GCC 4.9 compatible.

## 7. Verification

Tests cover exact matches, partial overlap, boundary shifts,
split/merge behavior, wrong class, false alarms, misses, overlapping classes,
channel-specific truth, empty predictions/truth, malformed input, output
round-trip, deterministic ordering, C++/Python parity, and CLI reports.

Verification completed on 2026-07-17:

- `TEST-INTERVAL-IO-001` and `TEST-INTERVAL-SCORING-001` pass;
- `TEST-PYTHON-SCORING-001` verifies generator-free scoring and C++/Python
  metric parity;
- `TEST-CLI-001` verifies challenge manifests and report generation;
- `TEST-DATABROWSER-GCC49-001` preserves the portable generation subset;
- the complete CTest suite passes: 43/43 tests.

## 8. DataBrowser And SVN Impact

None. This increment consumes generated ground truth and produces verification
reports; it adds no visualization or generation API.

## 9. References

- PhysioNet QT Database evaluation material:
  https://physionet.org/content/qtdb/1.0.0/eval/
- MIT-BIH Noise Stress Test Database:
  https://physionet.org/content/nstdb/1.0.0/
