# Standard Formats and I/O Contracts SRS

**Document ID:** SYN-SRS-FMT-001

**Version:** 0.1

**Status:** Draft implementation input

**Date:** 2026-07-04

**Parent issue:** [signal_synth#32](https://github.com/tamask1s/signal_synth/issues/32)

## 1. Purpose

Define the minimum user-facing file formats needed for the offline challenge
workflow and future SaaS readiness.

Supported standard waveform formats shall be limited initially to:

- WFDB;
- EDF+;
- BDF+.

CSV and JSON shall remain supported for detection output and developer
debugging, but CSV shall not be the primary user-facing waveform format.

## 2. Waveform Export Requirements

| ID | Requirement |
|---|---|
| `REQ-FMT-001` | Synsigra shall export WFDB records for ECG/PPG algorithm testing workflows. |
| `REQ-FMT-002` | WFDB export shall include signal files, header metadata, channel names, units, sampling frequency, and annotations where technically appropriate. |
| `REQ-FMT-003` | Synsigra shall export EDF+ for general biosignal exchange. |
| `REQ-FMT-004` | Synsigra shall export BDF+ for higher-resolution biosignal exchange where 16-bit EDF scaling is limiting. |
| `REQ-FMT-005` | Exported standard-format files shall preserve scenario identity through sidecar metadata if the standard format cannot encode all Synsigra fields natively. |
| `REQ-FMT-006` | Standard waveform exports shall be deterministic for the same scenario, generator version, and export settings. |
| `REQ-FMT-007` | Exported channel names shall be stable and user-friendly. |
| `REQ-FMT-008` | Units and scaling shall be explicit and test-covered. |
| `REQ-FMT-009` | The export layer shall avoid wall-clock timestamps in deterministic artifacts unless an explicit non-deterministic export mode is selected later. |

## 3. Ground-Truth Export Requirements

| ID | Requirement |
|---|---|
| `REQ-GTFMT-001` | Ground truth shall be exported as JSON with a documented schema. |
| `REQ-GTFMT-002` | Ground truth shall include event type, time seconds, sample index, channel/lead when applicable, source level, and present/valid flags. |
| `REQ-GTFMT-003` | Ground truth shall include artifact intervals and affected channels. |
| `REQ-GTFMT-004` | HRV ground truth shall include RR intervals and computed metric definitions once HRV scoring is implemented. |
| `REQ-GTFMT-005` | Ground truth JSON shall include scenario and render identity fields. |

## 4. Detection Output Requirements

Supported user detection outputs:

- CSV;
- JSON.

| ID | Requirement |
|---|---|
| `REQ-DET-001` | Detection CSV shall support at minimum a `time_seconds` column. |
| `REQ-DET-002` | Detection CSV shall support optional `sample_index`, `channel`, `label`, and `confidence` columns. |
| `REQ-DET-003` | Detection JSON shall support event arrays with time, optional sample index, channel, label, confidence, and algorithm metadata. |
| `REQ-DET-004` | Detection JSON shall support declared target type such as `r_peak`, `ppg_systolic_peak`, `qrs_onset`, or future HRV metrics. |
| `REQ-DET-005` | Detection import shall reject malformed, ambiguous, out-of-range, or duplicate-identical events according to documented policy. |
| `REQ-DET-006` | Detection import shall preserve the original detection index for report traceability. |

Example detection JSON:

```json
{
  "schema_version": 1,
  "algorithm": {
    "name": "customer_rpeak_detector",
    "version": "1.2.3"
  },
  "target": "r_peak",
  "events": [
    {"time_seconds": 0.842, "label": "r", "confidence": 0.98}
  ]
}
```

## 5. Package Manifest Requirements

| ID | Requirement |
|---|---|
| `REQ-PKG-001` | A challenge package manifest shall enumerate all files and their roles. |
| `REQ-PKG-002` | The manifest shall include checksums for payload files. |
| `REQ-PKG-003` | The manifest shall state which waveform formats are included. |
| `REQ-PKG-004` | The manifest shall identify whether ground truth is included. |
| `REQ-PKG-005` | The manifest shall include usage restrictions and non-clinical limitation text. |

## 6. Acceptance Criteria

1. WFDB export can be read by common WFDB tooling.
2. EDF+/BDF+ export preserves all generated waveform channels with correct
   scaling.
3. Detection CSV and JSON import produce identical scoring for equivalent
   event lists.
4. Challenge package manifest can be validated without rendering signals.
5. Standard-format exports remain linked to Synsigra ground truth by stable
   metadata.

## 7. Planned Implementation Issues

- WFDB export and annotation support;
- EDF+/BDF+ export support;
- detection JSON schema and importer;
- detection CSV v2 importer;
- challenge manifest checksums and validation;
- format conformance tests and golden examples.
