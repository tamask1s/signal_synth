# ECG Rhythm Engine v2

**Document ID:** SYN-ARCH-INC-030

**Version:** 0.9

**Status:** Implementing

**Owner role:** Core generation / Algorithm QA

**Date:** 2026-07-04

**Traceability ID:** `TRC-ECG-RHY-001`

**Implementation issue:** [signal_synth#45](https://github.com/tamask1s/signal_synth/issues/45)

**Implementation commit:** pending

**CI verification:** pending

## 1. Decision

Extend the ECG rhythm engine with deterministic v2 behavior for AFib,
flutter, exact ectopy cadence, and SVT/PSVT transition truth while preserving
the existing offline challenge-package architecture.

The C++ generator remains the authoritative source of construction truth. The
scenario/JSON layer exposes only compact product-facing controls:

- AFib uses a deterministic irregularly irregular RR process with skewed,
  short/long, and slow components rather than independent simple jitter;
- flutter keeps fixed conduction and adds explicit variable conduction patterns
  `alternate_2_3` and `cycle_2_3_4`;
- bigeminy/trigeminy continue to use `BIGU`/`TRIGU` plus PAC/PVC origin, with
  exact beat-index cadence asserted in tests;
- PSVT/SVARR episodes export onset and offset transition windows as ground
  truth;
- signal-quality artifacts overlay waveform samples without changing
  construction beat or episode truth.

## 2. Applicable Requirements

- `REQ-ECG-RHY-001`;
- `REQ-ECG-RHY-002`;
- `REQ-ECG-RHY-003`;
- `REQ-ECG-RHY-004`;
- `REQ-ECG-RHY-005`.

## 3. Public Contracts

C++:

- `clinical_rhythm_config::flutter_conduction_pattern` selects the low-level
  flutter conduction pattern;
- `ecg_qa_scenario::set_flutter_conduction_pattern()` and
  `flutter_conduction_pattern()` expose the scenario-level control;
- `clinical_episode_annotation` includes onset/offset transition windows and
  sample indexes.

Scenario JSON:

- optional `ecg.flutter_conduction_pattern`;
- accepted values: `fixed`, `alternate_2_3`, `cycle_2_3_4`;
- omitted value defaults to `fixed` for compatibility;
- canonical schema-v2 output writes the field explicitly.

Export:

- `annotations.json` episode objects include transition start/end seconds and
  sample indexes;
- beat, episode, and artifact truth remain separate.

## 4. Compatibility and Migration

- Existing scenario JSON files that omit `flutter_conduction_pattern` remain
  valid.
- Canonical JSON and document fingerprints change because the new field is
  explicit in canonical output.
- Scenario engine version increments from 10 to 11 because generated rhythm
  semantics and run fingerprints change.
- No DataBrowser API or script is added in this increment, so no SVN sync is
  required.

## 5. Verification

Extended existing stable procedures:

- `TEST-ECG-PHANTOM-001`: AFib irregularity, clipping, flutter fixed and
  variable conduction;
- `TEST-ECG-SCENARIO-001`: scenario validation, fingerprinting, exact
  bigeminy/trigeminy beat-origin cadence;
- `TEST-ECG-EPISODE-001`: transition truth and export contract;
- `TEST-ECG-JSON-001`: flutter pattern parse/write/canonical roundtrip;
- `TEST-ECG-EXPORT-001`: artifact overlap preserves rhythm construction truth;
- `TEST-CLI-001`: updated canonical fingerprint contract.

Verified locally on 2026-07-04:

- `cmake --build build-release`: passed;
- `ctest --output-on-failure` in `build-release`: 29/29 passed;
- `cmake --build build-sanitize`: passed;
- `env ASAN_OPTIONS=detect_leaks=0 ctest -E TEST-BUILD-001 --output-on-failure`
  in `build-sanitize`: 28/28 passed.

## 6. Risks and Limitations

- AFib remains an engineering QA model, not a clinical electrophysiology
  simulator.
- Flutter variable conduction supports two deterministic canonical patterns,
  not arbitrary user-provided pattern DSL.
- Artifact overlap preserves construction truth but does not yet provide
  rhythm-specific artifact severity assertions.

## 7. Change Log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-07-04 | Accepted rhythm engine v2 architecture |
| 0.9 | 2026-07-04 | Implemented C++ model, JSON, export, and verification updates |
