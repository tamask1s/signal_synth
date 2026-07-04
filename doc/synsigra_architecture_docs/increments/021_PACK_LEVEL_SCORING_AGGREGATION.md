# Pack-Level Scoring Aggregation

**Document ID:** SYN-ARCH-INC-021

**Version:** 0.1

**Status:** Implementing

**Owner role:** Platform / SDK

**Date:** 2026-07-04

**Proposed traceability ID:** `TRC-PACK-SCORE-001`

**Implementation issue:** [signal_synth#39](https://github.com/tamask1s/signal_synth/issues/39)

## 1. Decision

Add the first pack-level local Algorithm QA scoring layer. The existing
per-scenario `ecg_compare` result remains the source of truth for matching and
event-detector metrics. This increment aggregates those per-case comparison
results across a scenario pack and writes deterministic summary artifacts.

The resulting report is for synthetic engineering QA evidence. It is not a
clinical validation certificate and does not make diagnostic claims.

## 2. Applicable Requirements

- `REQ-SCORE-001..006`;
- supporting `REQ-CHAL-006`;
- supporting `REQ-CHAL-008`.

## 3. Public Contract

Add:

- `src/ecg_pack_score.h`;
- `src/ecg_pack_score.cpp`.

The pack score model includes:

- pack identity and fingerprint;
- scoring version;
- case ID;
- scenario ID and path;
- scenario document fingerprint;
- render identity;
- detection input identity;
- detection algorithm name/version;
- per-case comparison metrics;
- target-level aggregate metrics for total, clean, and artifact bins.

Aggregate TP, FP, FN, sensitivity, PPV, F1, and timing error metrics are
recomputed from per-case comparison results. Counts are summed; timing error
statistics are recomputed over all matched events in the relevant aggregate
bin.

## 4. CLI Contract

Add:

```text
signal-synth pack score <pack.json> <detections-directory> --out <new-directory>
```

Detection files are discovered by scenario case ID:

- `<case_id>.json`;
- `<case_id>.csv`.

JSON detection target must match the pack scenario target. CSV detection target
is supplied by the pack/scenario target. Unsupported pack targets are rejected.

Output artifacts:

- `pack_score_summary.json`;
- `pack_score_summary.csv`;
- `pack_score_report.html`.

## 5. DataBrowser/SVN Impact

No DataBrowser API is added in this increment and no visualization script is
required. Per project policy, the new standalone C++ pack-scoring module is not
copied to the DataBrowser/SVN working copy.

## 6. Verification

Add `TEST-PACK-SCORE-001`:

- aggregates multiple case comparison results;
- sums TP/FP/FN and ground-truth/detection counts;
- recomputes sensitivity and PPV from aggregate counts;
- preserves case, scenario, render, and detection input identity;
- writes JSON, CSV, and HTML report contracts;
- rejects empty case lists.

Extend `TEST-CLI-001` with an end-to-end `pack score` run using JSON detections
derived from rendered pack annotations.

Extend `TEST-BUILD-001` package smoke to include the installed public header.

## 7. Non-Goals

- No hosted SaaS dashboard.
- No Python wrapper in this increment.
- No challenge archive writer.
- No WFDB/EDF/BDF waveform export.
- No DataBrowser script/API.

## 8. Risks and Limitations

- Initial CLI discovery uses case-id based detection filenames.
- The first implementation scores only event-detector targets already supported
  by `ecg_compare`.
- Report persistence is file-based; long-term immutable evidence archiving is
  still future product work.

## 9. Change Log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-07-04 | Added pack-level scoring aggregation design |
