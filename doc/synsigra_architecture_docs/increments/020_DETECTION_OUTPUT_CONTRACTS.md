# Detection Output Contracts

**Document ID:** SYN-ARCH-INC-020

**Version:** 0.1

**Status:** Implementing

**Owner role:** Platform / SDK

**Date:** 2026-07-04

**Proposed traceability ID:** `TRC-DET-001`

**Implementation issue:** [signal_synth#38](https://github.com/tamask1s/signal_synth/issues/38)

## 1. Decision

Promote external detector output import from the previous CLI-only minimal CSV
parser to a versioned C++ input contract. This increment adds one shared parser
and writer module for user algorithm detections:

- CSV v2;
- JSON v1.

The contract is intentionally local/offline. It accepts algorithm output files
without requiring the customer to disclose algorithm source code.

## 2. Applicable Requirements

- `REQ-DET-001..006`;
- supporting `REQ-SCORE-004`;
- supporting `REQ-CHAL-006`.

## 3. Public Contract

Add:

- `src/detection_io.h`;
- `src/detection_io.cpp`.

The parser supports:

- `time_seconds`;
- optional `sample_index`;
- optional `channel`;
- optional `label`;
- optional `confidence`;
- original input event index preservation.

Detection JSON v1 also includes:

- `schema_version`;
- `algorithm.name`;
- `algorithm.version`;
- `target`;
- `events`.

Supported scoring targets in this increment:

- `r_peak`;
- `ppg_systolic_peak`.

Future target names such as QRS onset and HRV metrics remain planned work.

## 4. Validation Policy

The importer rejects:

- malformed CSV or JSON;
- duplicate JSON object keys;
- unknown JSON fields;
- duplicate or unknown CSV headers;
- missing `time_seconds`;
- non-finite or negative event times;
- confidence values outside `[0,1]`;
- duplicate-identical events;
- target mismatch between CLI compare command and detection JSON.

CSV and JSON imports both canonicalize to JSON for audit/debugging. The scoring
layer receives `ecg_detected_event` values with `has_original_index=true`, so
comparison reports can refer back to the user's original detection event index.

## 5. CLI Impact

`signal-synth compare` now accepts either CSV v2 or JSON v1 detection inputs.
The command-line target remains explicit:

```text
signal-synth compare <rpeaks|ppg-peaks> <scenario.json|-> <detections.csv|detections.json> --out <new-directory>
```

For JSON input, the declared JSON target must match the command target.

## 6. DataBrowser/SVN Impact

No DataBrowser API is added in this increment and no visualization script is
required. Per project policy, the new standalone C++ parser module is not copied
to the DataBrowser/SVN working copy.

## 7. Verification

Add `TEST-DETECTION-IO-001`:

- CSV v2 parse;
- JSON v1 parse/write;
- CSV quoted-cell parse;
- CSV and JSON score identity against the same render;
- original detection index preservation through comparison;
- duplicate header rejection;
- unknown header rejection;
- invalid confidence rejection;
- duplicate-identical event rejection;
- invalid JSON number rejection;
- unknown JSON field rejection;
- target name parsing.

Extend `TEST-CLI-001` with JSON detection input scoring.
Extend `TEST-BUILD-001` package smoke to include the installed public header.

## 8. Non-Goals

- No Python package in this increment.
- No pack-level scoring aggregation.
- No WFDB/EDF/BDF waveform export.
- No DataBrowser script/API.

## 9. Risks and Limitations

- CSV is still a developer/interchange detection format, not the primary
  waveform distribution format.
- Only event-detector targets already supported by `ecg_compare` can be scored.
- Duplicate-identical event rejection is strict; near-duplicate events remain
  valid and are handled by the scoring layer.

## 10. Change Log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-07-04 | Added CSV v2 and JSON v1 detection output contract |
