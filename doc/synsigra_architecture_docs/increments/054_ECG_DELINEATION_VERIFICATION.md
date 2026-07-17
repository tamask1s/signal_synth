# ECG Delineation Verification

**Document ID:** SYN-INC-054

**Version:** 1.0

**Status:** Verified

**Owner role:** Engineering

**Date:** 2026-07-17

**Implementation issue:** [signal_synth#75](https://github.com/tamask1s/signal_synth/issues/75)

## 1. Purpose

Provide a reproducible verification workflow for ECG delineation algorithms.
The workflow evaluates lead-specific P, QRS, J-point, and T fiducials against
the exact timing used to construct a synthetic record. It supports complete
records and deliberately selected beat subsets without treating absent waves
as required detections.

This is an engineering verification feature. It does not establish clinical
diagnostic performance.

## 2. Output Contract

`delineation_json_v1` contains an explicit evaluation scope and zero or more
predicted events:

```json
{
  "schema_version": 1,
  "algorithm": {"name": "example", "version": "1.0"},
  "target": "ecg_delineation",
  "scope": {"mode": "all_beats", "leads": ["II", "V2"]},
  "events": [
    {
      "beat_index": "0",
      "lead": "II",
      "kind": "qrs_onset",
      "time_seconds": 0.48,
      "confidence": 0.97
    }
  ]
}
```

`mode` is `all_beats` or `selected_beats`. Selected scope requires a unique,
non-empty `beat_indices` array. All unsigned 64-bit beat identities are
canonical decimal strings. `leads` is required and contains unique standard
12-lead ECG names.

The nine scoreable kinds are `p_onset`, `p_peak`, `p_offset`, `qrs_onset`,
`j_point`, `qrs_offset`, `t_onset`, `t_peak`, and `t_offset`. An event identity
is `(beat_index,lead,kind)` and may occur at most once. Empty event arrays are
valid.

`delineation_csv_v1` uses
`row_type,scope_mode,evaluated_beat_index,beat_index,lead,kind,time_seconds,confidence`.
Scope rows preserve empty results and the evaluated lead/beat set. Event rows
carry predictions. JSON is the recommended interchange format; CSV exists for
simple algorithm adapters.

## 3. Exact Ground Truth

Construction annotations define P onset/offset, QRS onset/offset, J point,
and T onset/offset. Lead-measurement annotations define P and T peak time and
whether those waves are visible above the configured presence threshold.
QRS visibility is true when any measured Q, R, or S peak is present in that
lead.

Construction boundaries are projected to a lead only when the corresponding
wave is visible there. Therefore:

- absent P or T waves are omitted from truth and do not become misses;
- a prediction for an omitted wave is an unexpected event;
- QRS onset, J point, and QRS offset remain one coherent group;
- truth describes the clean generated morphology even when acquisition
  artifacts are subsequently mixed into the waveform.

Selected beat indices must exist in the rendered record. Every requested lead
must be present. Truth generation fails rather than silently reducing scope.

## 4. Matching And Metrics

Events are paired by exact identity, not by nearest time. This preserves the
known synthetic beat association and prevents a late boundary from being
matched to the next beat. A paired event outside the tolerance remains
reported with its signed timing error and contributes one false negative and
one false positive.

The default absolute boundary tolerance is 40 ms and is configurable per
scoring invocation. Overall, per-kind, per-lead, and kind-by-lead groups
report:

- truth, prediction, paired, within-tolerance, missing, unexpected, and
  out-of-tolerance counts;
- sensitivity, positive predictive value, and F1;
- signed bias, mean/median/RMS/95th-percentile/maximum absolute error for
  paired events.

Metrics with a zero denominator are `null` in JSON and `NA` in CSV/HTML.
Reports retain paired, missing, and unexpected event identities for diagnosis.

## 5. Interfaces

- C++: `delineation_io.h` and `delineation_scoring.h`;
- CLI: `signal-synth delineation score ...`;
- Python: generator-free parsing and scoring from challenge annotations;
- challenge metadata: `ecg_delineation`, accepted JSON/CSV formats, default
  tolerance, and dedicated `delineations/` output paths;
- curated pack: clean, rate, conduction, repolarization, absent-P, and noisy
  morphology cases;
- DataBrowser: an example script using the existing scenario marker API.

The measurement-output contract in issue #76 remains separate. It will derive
interval, amplitude, axis, and alignment measurements from fiducials instead
of changing this event contract.

## 6. QT Database Interoperability

The PhysioNet QT Database evaluation workflow is the interoperability
reference. This repository does not redistribute QTDB records or annotations.
Optional downloaded fixtures must retain their upstream provenance and
license terms. Native Synsigra challenge cases remain exportable as WFDB so
external delineators can use a familiar record workflow without receiving
generator source code.

## 7. Failure And Resource Policy

- Unknown JSON members and CSV columns are errors.
- Duplicate keys, scope members, event identities, and CSV scope rows are
  errors.
- Non-finite, negative, or out-of-record times are errors.
- Events outside declared scope are errors.
- Parsing and scoring remain C++11 and GCC 4.9 compatible.
- Reports are written only after parsing, rendering, and scoring succeed.

## 8. Verification Evidence

Tests cover JSON/CSV round trips, malformed identities, duplicate and
out-of-scope events, selected/all-beat truth, absent waves, exact and shifted
predictions, missing/unexpected boundaries, group metrics, deterministic
ordering, C++/Python parity, challenge metadata, CLI reports, and the existing
GCC 4.9 DataBrowser generation smoke.

The curated seven-case pack was rendered as a challenge and scored through the
generator-free Python verifier with exact synthetic outputs. All seven cases
passed the benchmark profile. The C++/Python parity test compares complete
overall, per-kind, and per-lead metric objects. The DataBrowser dry-run
SHA-256 synchronization check reports all mapped files identical. A fresh
install-enabled build passed all 46 registered CTest procedures; JSON and CSV
score reports were also parsed independently. The Python distribution version
is 0.3.0 and the pack declares that version as its minimum local verifier.

## 9. DataBrowser And SVN Impact

No core DataBrowser source changes are required. The existing scenario API
already renders the relevant P/QRS/J/T markers. One example script is added
and synchronized to the SVN application tree; verification/scoring sources
remain Git-only.

## 10. Reference

- PhysioNet QT Database evaluation material:
  https://physionet.org/content/qtdb/1.0.0/eval/
