# Offline Challenge and Python Scoring SRS

**Document ID:** SYN-SRS-CHAL-001

**Version:** 0.1

**Status:** Draft implementation input

**Date:** 2026-07-04

**Parent issue:** [signal_synth#32](https://github.com/tamask1s/signal_synth/issues/32)

## 1. Purpose

Define the first B2B workflow that can be sold before a hosted SaaS service is
implemented.

The workflow is offline-first:

1. Synsigra generates a deterministic scenario or scenario pack.
2. The user receives a challenge package containing waveform data, metadata,
   ground truth, and usage/reporting instructions.
3. The user runs their own algorithm locally.
4. The user scores local algorithm output against ground truth using the
   Synsigra Python package.
5. The user obtains a reproducible algorithm QA report.

The hosted SaaS implementation is explicitly out of scope for this SRS. The
requirements here should make a later SaaS service straightforward because the
same package, scoring, and report contracts can be executed by a server.

## 2. Product Boundary

Use the following terms consistently:

| Term | Meaning |
|---|---|
| Scenario | One deterministic signal-generation definition |
| Scenario pack | A curated set of scenarios with a manifest and pack fingerprint |
| Challenge package | Deliverable containing waveforms, ground truth, metadata, scenario identity, and instructions |
| User algorithm output | Detection or metric output produced by the customer's algorithm |
| Local scoring package | Python package that compares user output to challenge ground truth |
| Algorithm QA report | Engineering report of synthetic ground-truth performance |

Do not call the workflow a clinical validation package. Acceptable external
names are:

- Algorithm QA package;
- Ground-truth verification package;
- Synthetic testbench package;
- Detector verification package.

## 3. Layering Requirements

| ID | Requirement |
|---|---|
| `REQ-CHAL-001` | The C++ core shall remain the deterministic source of generation, export, and scoring behavior. |
| `REQ-CHAL-002` | The Python package shall wrap stable C++ or CLI/facade behavior and shall not reimplement signal generation or scoring algorithms independently. |
| `REQ-CHAL-003` | The challenge package shall be reproducible from scenario JSON, generator version, and seed. |
| `REQ-CHAL-004` | The challenge package shall include enough metadata to identify scenario ID, pack ID if any, generator version, scenario fingerprint, render identity, sample rate, channels, units, and duration. |
| `REQ-CHAL-005` | Ground truth shall be delivered with the challenge package for the offline-first B2B model. |
| `REQ-CHAL-006` | The scoring workflow shall accept user detection output without requiring the user to disclose algorithm source code. |
| `REQ-CHAL-007` | The scoring workflow shall support local execution without network access after package/license activation. |
| `REQ-CHAL-008` | Every report shall state that results are synthetic engineering QA evidence, not diagnosis or clinical validation certification. |
| `REQ-CHAL-009` | The package format shall be stable enough to be used by future hosted SaaS jobs without changing the user-facing scoring contract. |

## 4. Challenge Package Contents

The challenge package shall contain:

- `manifest.json`;
- one or more scenario JSON files;
- waveform files in supported standard formats;
- ground-truth annotation files;
- ground-truth metrics files;
- optional rendered HTML scenario report;
- checksums or fingerprints;
- `README.md` or `README.html` with usage instructions;
- optional example Python scoring script.

For scenario packs, the package shall also contain:

- pack manifest;
- case list;
- per-case scenario identity;
- per-case waveform and ground-truth artifacts;
- pack-level summary.

## 5. Python Package Requirements

| ID | Requirement |
|---|---|
| `REQ-PY-001` | The Python package shall load a challenge package from a directory or archive. |
| `REQ-PY-002` | The Python package shall expose waveform arrays and metadata through Python-native objects. |
| `REQ-PY-003` | The Python package shall load user detections from CSV and JSON. |
| `REQ-PY-004` | The Python package shall run the same scoring logic as the C++ `ecg_compare` layer. |
| `REQ-PY-005` | The Python package shall emit JSON, tabular, and HTML report outputs. |
| `REQ-PY-006` | The Python API shall be simple for notebook and CI use. |
| `REQ-PY-007` | The Python package shall preserve deterministic fingerprints in every result. |
| `REQ-PY-008` | Python-only convenience code may transform data formats but shall not change ground-truth semantics. |

Candidate API:

```python
import synsigra as ss

challenge = ss.load_challenge("r_peak_stress_v1.synsigra")
detections = ss.load_detections("my_rpeaks.json")
report = ss.compare_rpeaks(challenge.case("clean_70"), detections)
report.write("out/")
```

## 6. Scoring Workflow Requirements

| ID | Requirement |
|---|---|
| `REQ-SCORE-001` | R-peak event scoring shall support TP, FP, FN, sensitivity, PPV, F1, and timing-error metrics. |
| `REQ-SCORE-002` | PPG peak event scoring shall support the same event-detector metrics. |
| `REQ-SCORE-003` | Metrics shall be reported for total, clean, and artifact intervals where artifact truth exists. |
| `REQ-SCORE-004` | The scoring layer shall reject ambiguous or malformed user outputs with structured errors. |
| `REQ-SCORE-005` | Future HRV scoring shall compare user-provided HRV metrics and/or RR intervals against ground truth. |
| `REQ-SCORE-006` | Reports shall include scenario/pack identity, engine version, scoring version, input file identity, and limitation statements. |

## 7. Acceptance Criteria

1. A generated challenge package can be loaded by the Python package.
2. A user can score R-peak and PPG peak detections locally without writing C++.
3. The same input detections produce the same score through CLI and Python.
4. The report includes fingerprints and controlled non-clinical wording.
5. No hosted SaaS service is required to complete the workflow.

## 8. Planned Implementation Issues

Issue links are maintained in the GitHub issue tracker. Planned implementation
areas:

- challenge package manifest and archive contract;
- stable C++ facade for render/export/compare;
- Python package wrapper and local scoring API;
- local Algorithm QA report writer;
- pack-level local scoring aggregation;
- example challenge package and tutorial.

## 9. Open Design Questions

- Whether the first archive extension should be `.zip`, `.synsigra`, or both.
- Whether the Python package should bind C++ directly or initially call the CLI
  for faster delivery.
- How license activation is handled for fully offline customer workflows.
