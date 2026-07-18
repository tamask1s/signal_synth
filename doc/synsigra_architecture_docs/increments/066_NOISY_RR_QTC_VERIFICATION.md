# Noisy R-Peak/RR and QTc Verification

**Document ID:** SYN-ARCH-INC-066

**Issue:** [signal_synth#90](https://github.com/tamask1s/signal_synth/issues/90)

**Version:** 1.0

**Status:** Verified

**Owner role:** Engineering / Quality

**Date:** 2026-07-18

**Implementation:** [`d09babb6801171f20d67169a83a9d0914d229a4b`](https://github.com/tamask1s/signal_synth/commit/d09babb6801171f20d67169a83a9d0914d229a4b)

**CI verification:** [GitHub Actions run 29642031806](https://github.com/tamask1s/signal_synth/actions/runs/29642031806) and [Python package run 29642031808](https://github.com/tamask1s/signal_synth/actions/runs/29642031808)

## 1. Decision

Add two deterministic challenge packs and two scoreable measurement targets:

- `r_peak_rr_noise_v1` exercises R-peak detection, observable R-R interval
  reconstruction, and signal-quality interval detection under clean, analytic,
  and calibrated project-owned external noise;
- `ecg_qtc_verification_v1` exercises R-peak detection, ECG delineation, QT
  measurement, and five explicit QT-correction formulas across rate and
  morphology boundaries;
- `rr_interval` and `qtc` use the existing uniform
  `measurement_values_json_v1` / `measurement_values_csv_v1` customer contract;
- target-specific threshold sections extend the existing smoke, regression,
  stress, and benchmark profiles without adding another scoring interface.

This increment produces engineering verification evidence. It does not qualify
the generator as an FDA Medical Device Development Tool, establish clinical
validity, certify a customer algorithm, or replace a sponsor-specific protocol,
risk analysis, representative data, independent review, and controlled evidence
archive.

## 2. Context Of Use

The intended use is deterministic synthetic algorithm QA for:

1. ECG R-peak detection in known acquisition disturbances;
2. reconstruction of intervals between observable, present-QRS R peaks;
3. T-offset/QT measurement and formula-explicit QTc calculation;
4. repeatable regression, stress, and negative-control testing through the
   generator-free local Python verifier.

The generated records are not patient data. Model-defined fiducials do not
model human-reader uncertainty, and pack pass/fail does not imply diagnostic or
clinical performance.

## 3. Requirements

- `RRQTC-REQ-001`: publish the context of use and claim boundary with each
  protocol.
- `RRQTC-REQ-002`: provide deterministic clean, rate-boundary, analytic-noise,
  clipping/dropout, and calibrated external-noise R-peak/RR cases.
- `RRQTC-REQ-003`: define RR as the difference between consecutive present-QRS
  construction R peaks in the record; exclude the first beat and retain
  artifact-overlapping intervals.
- `RRQTC-REQ-004`: score R peaks, RR values, and signal-quality intervals as
  separate targets so detector, reconstruction, and quality-gating failures
  remain distinguishable.
- `RRQTC-REQ-005`: provide fixed, Bazett, Fridericia, Framingham, and Hodges QTc
  cases at normal and applicable bradycardic/tachycardic rates.
- `RRQTC-REQ-006`: include difficult T/U morphology, dynamic long-QT, and
  baseline-wander stress while preserving exact construction truth.
- `RRQTC-REQ-007`: make formula, unit, status, beat identity, and timing anchor
  part of measurement identity and report QT, RR, and QTc separately.
- `RRQTC-REQ-008`: pre-specify objective profile thresholds and preserve raw
  target reports, policy checks, package fingerprints, generator identity, and
  scenario documents.
- `RRQTC-REQ-009`: prove perfect-output acceptance and deterministic rejection
  of material RR/QTc bias through an end-to-end challenge test.
- `RRQTC-REQ-010`: provide DataBrowser visualization while retaining the
  generation-only GCC 4.9/C++11 adapter boundary.

## 4. Public Contracts

### 4.1 RR target

For every present-QRS beat after the first present-QRS beat in the record:

```text
RR[n] = construction_R_time[n] - construction_R_time[n-1]
```

The measurement is named `rr_interval`, uses seconds, has beat scope, and is
anchored to the later R peak. Its default numeric tolerance is the greater of
10 ms and 2 percent. Acquisition artifacts do not delete physiological truth;
the independent `signal_quality` target lets a customer evaluate quality gates.

### 4.2 QTc target

The `qtc` target emits per-beat `qt_interval`, followed by `rr_interval` and
`qtc_interval` once an in-record preceding R peak exists. QT is construction
QRS onset to construction T offset. QT/QTc numeric tolerance is the greater of
25 ms and 5 percent; RR retains its 10 ms / 2 percent tolerance.

The scenario-selected formula is part of `qtc_interval` identity:

```text
fixed:       QTc = QT
bazett:      QTc = QT / sqrt(RR)
fridericia:  QTc = QT / cbrt(RR)
framingham:  QTc = QT + 0.154 * (1 - RR)
hodges:      QTc = QT + 0.00175 * (60 / RR - 60)
```

All interval values are seconds. An absent T wave is represented by explicit
measurement status rather than a fabricated numeric endpoint.

### 4.3 Customer workflow

Both targets use the existing submission manifest and measurement schema:

```text
signal-synth pack challenge <pack.json> --out <directory>
synsigra-verify <challenge> <submission-directory> <result-directory>
```

The generator remains server/core-side. The customer verifier wheel contains
no C++ generator implementation.

## 5. Verification Matrix

`r_peak_rr_noise_v1` contains clean 70 bpm, 45 bpm, 120 bpm, moderate analytic
noise, dropout/saturation, severe analytic noise, calibrated external noise,
and 12-lead external-noise extremes. The project-owned CC0 fixture supplies
baseline-wander, muscle, and electrode-motion channels at declared SNR values
from +12 dB through -12 dB.

`ecg_qtc_verification_v1` contains fixed QT at 60 bpm; Bazett, Fridericia,
Framingham, and Hodges at 45 and 120 bpm; dynamic long QT; biphasic/notched T
with U wave; and low-amplitude T with baseline wander.

Machine-readable pre-specified protocols live beside each pack in
`*_expectations.json`. The noise pack selects the `stress` profile and the QTc
pack selects `regression`. Named measurement sections constrain coverage,
tolerance pass fraction, status agreement, mean absolute error, and p95 error.

## 6. Data And Evidence Flow

```text
scenario JSON -> C++ render -> waveform + construction annotations
              -> measurement_truth.json -> customer submission template
              -> generator-free Python scoring -> raw target reports
              -> threshold profile evaluation -> package summary
```

The challenge manifest hashes package files. Scenario fingerprints, generator
version/build identity, source scenarios, scoring manifest, algorithm
provenance supplied by the customer, raw reports, and final policy checks form
the reproducible engineering record. A regulated use would additionally need a
frozen released toolchain, environment qualification, approvals/signatures,
deviation handling, immutable long-term retention, and sponsor-controlled
acceptance criteria.

## 7. Verification

- `TEST-RR-QTC-PACK-001` renders and integrity-checks both complete challenge
  packs, reconstructs every RR value, independently recomputes every QTc
  formula, verifies 12-lead achieved external SNR, accepts perfect submissions,
  rejects missing artifact-bin R peaks, and rejects a systematic +40 ms RR or
  QTc bias.
- Existing measurement C++/Python parity tests cover parsing, identity,
  matching, status, and numeric report behavior.
- Integration, authoring, catalog, metadata-export, CLI, facade, and Python
  tests cover publication through every supported interface.
- `TEST-DATABROWSER-GCC49-001` parses and renders scripts 089 and 090 under the
  strict generation-only C++11 target.

## 8. DataBrowser And SaaS Impact

- Script 089 visualizes severe analytic and calibrated all-lead external-noise
  records with truth channels.
- Script 090 visualizes rate/formula boundaries and low-amplitude T stress with
  QT/QTc annotation channels.
- The SaaS worker must require `synsigra_core_integration_v5`, ingest curated
  catalog 2.6, distribute verifier 0.8.0, preserve measurement truth unchanged,
  and expose named policy checks. No v4 compatibility layer is required.

## 9. Regulatory Alignment And Limitations

The structure deliberately supports software-evidence practices described by
FDA software submission and software validation guidance: intended use,
pre-specified requirements and acceptance criteria, normal/boundary/stress and
error cases, objective expected results, reproducibility, configuration
identity, and retained test results. The authoritative guidance must be
interpreted for the actual device, risk, and submission by the sponsor.

References:

- FDA, *Content of Premarket Submissions for Device Software Functions*, 2023.
- FDA, *General Principles of Software Validation*, 2002.
- FDA, *Medical Device Development Tools (MDDT)* program.

Residual limitations include synthetic-model bias, no representative clinical
population, no annotation-observer distribution, no customer-specific failure
hazard analysis, and no claim that the built-in thresholds are clinically
meaningful.

## 10. Change Log

- 2026-07-18: version 1.0, implementation and verification design recorded.
- 2026-07-18: implementation commit and successful required CI recorded;
  status advanced to Verified.
