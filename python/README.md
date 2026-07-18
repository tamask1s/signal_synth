# Synsigra Python SDK

`synsigra` is the customer-facing, generator-free SDK for scoring algorithm
outputs against a downloaded Synsigra challenge. It contains no C++ generator
and does not execute customer detector code.

## Install

```bash
python -m pip install synsigra-0.8.0-py3-none-any.whl
synsigra-verify --help
```

## Verify A Challenge

Every challenge contains `user-output-template/`. Copy that directory, replace
the algorithm placeholders in `submission.json`, and write each declared
output file. The manifest is authoritative: no filename discovery or
target-specific directory convention is used.

```bash
synsigra-verify challenge.synsigra submission/ verification-results/ --profile regression
```

Useful filters:

```bash
synsigra-verify challenge.synsigra submission/ out/ --case clean_70 --target r_peak
synsigra-verify challenge.synsigra submission/ out/ --profile path/to/profile.json
```

Use `--force` to replace an existing result directory.

## Submission Contract

`submission.json` uses `synsigra_submission_v1`. It identifies the challenge
and algorithm once, then maps every `(case_id,target)` to an explicit format
and relative path.

R-peak, PPG event, beat-classification and ECG-delineation outputs use the same
point-event payload:

```json
{"schema_version":1,"events":[{"time_seconds":1.234,"sample_index":617,"channel":"II","label":"qrs_onset","confidence":0.98}]}
```

Its CSV columns are
`time_seconds,sample_index,channel,label,confidence`. Optional cells may be
empty. For delineation, `channel` is a standard ECG lead and `label` is one of
P onset/peak/offset, QRS onset/offset, J point, or T onset/peak/offset.
Predictions contain no generator beat identity and no negative rows.

Rhythm and signal-quality outputs use `interval_events_json_v1` or
`interval_events_csv_v1` with half-open `[start_seconds,end_seconds)` bounds,
label and channel. HRV uses `hrv_metrics_json_v1`. The generated manifest lists
the accepted formats for every target.

Morphology and ECG/PPG alignment use one measurement payload family. JSON uses
`measurement_values_json_v1`; CSV uses the exact columns
`name,value,unit,status,scope,time_seconds,beat_index,channel,formula,confidence`.
Status is one of `valid`, `undefined`, `absent`, or `not_evaluable`. Beat-level
outputs may use a decimal-string beat index or a time anchor; record, lead, and
paired-signal outputs use the same contract.

## Reports

The verifier writes:

- `verification_summary.json`, `.csv`, and `verification_report.html`;
- `verification/<case-target>/comparison.json`, `.csv`, and HTML evidence.

ECG delineation reports expose truth-side atrial or ventricular anchors and
`present`, `absent`, or `not_evaluable` status. Predictions for absent waves
are false positives; predictions inside a not-evaluable wave window are
reported as excluded.

Exit code `0` means integrity, scoring and threshold policy passed. Exit code
`1` means one of those checks failed; `2` is invalid command-line usage.

## Python API

```python
import synsigra

report = synsigra.verify_package("challenge.synsigra", "submission", "results")
assert report.summary["success"]
```

Schema-v5 wearable challenges expose independent device streams and their
auditable clock mapping without shipping generator code:

```python
with synsigra.load_challenge("wearable.synsigra") as challenge:
    case = challenge.cases[0]
    ecg = case.wearable_samples("ecg")
    ppg = case.wearable_samples("ppg")
    timestamps = case.wearable_timestamp_truth()
    timebase = case.wearable_timebase_truth()
    alignment = case.wearable_alignment_truth()
```

The pure-Python wheel supports CPython 3.8 through 3.11 on Linux, macOS and
Windows. Development-only generator-backed helpers require a separately
installed `signal-synth` executable; the customer verification workflow does
not.

## Scope Boundary

This package produces synthetic engineering QA evidence. It is not a
diagnostic device, patient monitor, clinical-validation system, or standalone
conformity-assessment tool.
