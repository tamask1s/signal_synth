# ECG Paced Rhythm Scenarios

**Document ID:** SYN-ARCH-INC-031

**Version:** 1.0

**Status:** Verified

**Owner role:** Core generation / Algorithm QA

**Date:** 2026-07-04

**Traceability ID:** `TRC-ECG-PACE-001`

**Implementation issue:** [signal_synth#46](https://github.com/tamask1s/signal_synth/issues/46)

**Implementation commit:** `068c7a1684de2ff44980cd8dd2996d62a9264674`

**CI verification:** [GitHub Actions run 28718245367](https://github.com/tamask1s/signal_synth/actions/runs/28718245367)

## 1. Decision

Add deterministic paced-rhythm scenarios to the ECG construction engine and
export explicit pacing-event truth. The paced model is intended for Algorithm
QA challenge packages, where users need visible pacing artifacts and exact
ground truth for capture/non-capture behavior.

The C++ generator remains authoritative for waveform construction and truth.
The scenario layer exposes compact controls:

- pacing mode: ventricular, atrial, or dual-chamber;
- deterministic non-capture cadence, disabled by default;
- paced event truth independent from beat truth;
- canonical scenario JSON fields so paced packs are reproducible and
  fingerprinted.

## 2. Applicable Requirements

- `REQ-ECG-PACE-001`;
- `REQ-ECG-PACE-002`;
- `REQ-ECG-PACE-003`;
- `REQ-ECG-PACE-004`.

## 3. Scope and Non-Goals

In scope:

- visible pacing spikes rendered through the pacing source;
- atrial-only, ventricular-only, and dual-chamber construction paths;
- explicit `clinical_pacing_event` truth with kind, timing, sample index,
  capture flag, and linked atrial/ventricular indexes;
- paced beat class export and standard annotation export;
- scenario JSON parse/write/canonicalization/fingerprinting.

Non-goals:

- certified pacemaker simulator behavior;
- device-specific sensing, refractory periods, hysteresis, or rate-response
  logic;
- arbitrary pacing programs or adaptive pacemaker state machines.

## 4. Public Contracts

C++ low-level generator:

- `clinical_pacing_mode` selects ventricular, atrial, or dual-chamber pacing;
- `clinical_pacing_event` records atrial/ventricular pacing artifacts and
  links each event to generated atrial/ventricular truth where applicable;
- `clinical_scenario_config::pacing_non_capture_every_n_beats` models a
  deterministic missed-capture cadence, where `0` disables missed capture and
  `1` is rejected.

Scenario API:

- `ecg_qa_scenario::set_pacing_mode()` and `pacing_mode()`;
- `ecg_qa_scenario::set_pacing_non_capture_every_n_beats()` and
  `pacing_non_capture_every_n_beats()`;
- pacing parameters require the `PACE` condition.

Scenario JSON:

- `ecg.pacing_mode`: `ventricular`, `atrial`, or `dual_chamber`;
- `ecg.pacing_non_capture_every_n_beats`: unsigned cadence, `0` for disabled;
- canonical JSON writes both fields explicitly.

Export:

- `annotations.json` contains a top-level `pacing_events` array;
- each event exports pacing index, kind, time, sample index, capture flag, and
  linked atrial/ventricular indexes;
- atrial-paced and ventricular-paced beats map to the canonical paced beat
  class for scoring/export.

## 5. Data Flow

1. Scenario JSON or C++ API sets the `PACE` condition and optional pacing
   controls.
2. The scenario compiler maps product-facing controls to
   `clinical_ecg_config`.
3. The clinical generator builds a paced timeline and records pacing events
   separately from beats.
4. Rendering draws spikes from pacing-event truth.
5. Construction fiducials and export artifacts are derived from the same event
   truth, preventing marker/waveform divergence.

## 6. Compatibility and Migration

- Existing scenarios that omit pacing controls remain valid and default to
  ventricular pacing with non-capture disabled.
- Canonical JSON and document fingerprints change because pacing controls are
  now explicit.
- Scenario engine version increments from 11 to 12 because generated paced
  semantics and fingerprints change.
- No DataBrowser API or script is added in this increment, so no SVN sync is
  required.

## 7. Verification

Extended stable procedures:

- `TEST-ECG-PHANTOM-001`: ventricular, atrial, dual-chamber, and non-capture
  event truth;
- `TEST-ECG-SCENARIO-001`: scenario validation, compiler mapping, and
  fingerprinting;
- `TEST-ECG-JSON-001`: JSON canonicalization and pacing-control roundtrip;
- `TEST-ECG-EXPORT-001`: `annotations.json` pacing-event contract;
- `TEST-ECG-BEAT-CLASS-001`: atrial-paced/ventricular-paced beat class;
- `TEST-WFDB-EXPORT-001`: paced beat annotation mapping through existing
  export coverage;
- `TEST-CLI-001`: updated canonical fingerprint contract.

Verified locally on 2026-07-04:

- `cmake --build build-release`: passed;
- `ctest --output-on-failure` in `build-release`: 29/29 passed.
- `cmake --build build-sanitize`: passed;
- `env ASAN_OPTIONS=detect_leaks=0 ctest -E TEST-BUILD-001 --output-on-failure`
  in `build-sanitize`: 28/28 passed.

Verified in CI on 2026-07-04:

- Ubuntu C++11 configure/build/test: passed;
- Windows C++11 configure/build/test: passed.

## 8. Risks and Limitations

- Capture/non-capture is deterministic construction truth, not inferred from a
  pacemaker device model.
- Dual-chamber non-capture currently models atrial capture with ventricular
  non-capture for the missed ventricular cycle.
- Pacing spike amplitude and vector are engineering defaults intended for
  detector stress testing; future packs may need configurable spike amplitude,
  width, and polarity.

## 9. Change Log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-07-04 | Accepted paced rhythm scenario architecture |
| 0.9 | 2026-07-04 | Implemented C++ model, JSON, export, and verification updates |
| 1.0 | 2026-07-04 | Completed local sanitizer and Linux/Windows CI verification |
