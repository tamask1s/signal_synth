# Unified Submission And ECG Delineation V2

**Document ID:** SYN-INC-055

**Version:** 1.0

**Status:** Implemented, local verification complete

**Owner role:** Engineering

**Date:** 2026-07-17

**Implementation issues:** [signal_synth#86](https://github.com/tamask1s/signal_synth/issues/86), [signal_synth#87](https://github.com/tamask1s/signal_synth/issues/87)

## 1. Decision

Customer verification uses one generator-free submission contract and one
command:

```text
synsigra-verify <challenge> <submission-directory> <result-directory>
```

Every challenge contains a ready-to-fill `user-output-template/` directory.
Its `submission.json` records challenge identity, algorithm identity and an
explicit `(case,target,format,path)` entry for every output. Algorithm identity
is supplied once. Typed payload readers do not invent provenance for CSV.
The adjacent `formats.json` is the machine-readable field/column contract for
all accepted customer payload adapters.

Point-like customer outputs use one table contract:

- `point_events_json_v1`: `schema_version` and `events`;
- `point_events_csv_v1`: `time_seconds,sample_index,channel,label,confidence`.

Target and algorithm semantics come from `submission.json`. ECG delineation
uses `channel` as lead and `label` as fiducial kind. Interval, HRV and future
measurement payloads remain separate typed records because forcing unlike
data into one union would make validation weaker and the interface harder to
understand.

Direct C++ scoring remains an engineering interface. Customer documentation
uses only the local Python package and does not require generator source or a
C++ build.

## 2. Delineation Truth Model

Predictions contain only detected event properties. They do not contain
generator beat or atrial identifiers and do not contain negative rows.

Ground truth uses:

- anchor type: `atrial_event` or `ventricular_beat`;
- canonical decimal-string anchor index;
- lead and fiducial kind;
- status: `present`, `absent`, or `not_evaluable`;
- stable reason code and reference/evaluation times.

P fiducials for existing atrial activity use atrial anchors, including
non-conducted activity. QRS, J and T fiducials use ventricular anchors. When no
atrial event exists for a ventricular opportunity, absent P slots use the
ventricular anchor so denominator accounting remains explicit.

Global construction boundaries become lead truth only when that wave is
measurable in the lead. A globally present but sub-threshold lead waveform is
`not_evaluable`, not absent. Predictions near a not-evaluable wave window are
excluded and reported; predictions for absent waves remain false positives.

## 3. Matching

For each `(lead,kind)` group, present truth and predictions are paired
one-to-one by smallest absolute time error with deterministic tie-breaking.
Pairs farther than the configured pairing window remain unmatched. Paired
events outside the scoring tolerance count as one false negative and one false
positive while preserving signed timing error.

Challenge-defined scope is authoritative. The first released template uses
all-record scope and explicit leads. User payloads cannot reduce the evaluated
scope. Selected time windows remain an internal scope capability for future
curated packs.

## 4. Public Contracts

`synsigra_submission_v1` contains only:

- `schema_version` and contract name;
- challenge package id, version and pack fingerprint;
- non-empty algorithm name and version;
- unique output entries with safe relative paths and declared formats.

Challenge scoring metadata exposes a uniform `accepted_formats` array,
`recommended_format`, `recommended_path`, and target-specific options. Legacy
target-specific accepted-format keys and path discovery are removed from the
customer verifier. Private-beta compatibility wrappers are not retained.

## 5. Reports

Every report receives algorithm provenance from `submission.json`. Delineation
reports add truth status/reason counts overall, per kind, per lead and per
kind-by-lead. Matches and missing truth retain anchor identity. Excluded
predictions are reported separately.

## 6. Verification

Stable procedures:

- `TEST-SUBMISSION-001`: strict submission loading, identity, format and path
  validation;
- `TEST-PYTHON-SCORING-001`: generated template, perfect output and one-command
  verification for all currently scoreable target families;
- `TEST-DELINEATION-IO-001`: point-event delineation adapter validation;
- `TEST-DELINEATION-SCORING-001`: atrial anchors, all truth states, temporal
  matching and grouped metrics;
- `TEST-DELINEATION-PYTHON-001`: Python/C++ parity and malformed input;
- `TEST-INTEGRATION-CONTRACT-001`: exported contract and generated challenge
  metadata.

The release build and full CTest suite must pass. GCC 4.9/C++11 compatibility
is retained. No DataBrowser generation source changes are required because
this increment consumes existing exported annotations.

## 7. Non-goals And Limitations

- no hosted SaaS implementation;
- no QTDB download or clinical-data interoperability harness (#88);
- no clinical-performance claim;
- no compatibility layer for the private-beta delineation v1 submission
  format;
- no untyped universal record for interval, HRV or measurement values.

## 8. Implementation Sequence

1. Add submission model, generated templates and strict Python loader.
2. Route the verifier exclusively through submission entries and provenance.
3. Replace delineation prediction identity and truth construction.
4. Update C++/Python scoring, reports, pack/catalog contracts and tests.
5. Update customer documentation, traceability and release metadata.

## 9. Released Contract Set

- generator: `0.6.0-dev`;
- integration contract: `synsigra_core_integration_v2`;
- challenge package: `synsigra_challenge_package_v2`;
- scoring manifest: `synsigra_scoring_manifest_v2`;
- submission: `synsigra_submission_v1`;
- local Python verifier: `0.4.0`, scoring engine
  `synsigra-python-local-v3`;
- curated catalog: `1.7`, including `ecg_delineation_v2`.

The v2 delineation pack adds Mobitz II non-conducted atrial activity. Automated
coverage also includes atrial flutter identity separation, AF absent P truth,
sub-threshold lead P truth, record-edge windows, malformed/duplicate payloads,
and perfect verification of all eight curated cases.
