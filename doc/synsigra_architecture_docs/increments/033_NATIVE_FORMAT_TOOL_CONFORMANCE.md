# Native Format Tool Conformance

**Document ID:** SYN-ARCH-INC-033

**Version:** 0.9

**Status:** Implementing

**Owner role:** Export / Algorithm QA

**Date:** 2026-07-04

**Traceability IDs:** `TRC-FMT-WFDB-TOOL-001`, `TRC-FMT-EDF-TOOL-001`

**Implementation issues:** [signal_synth#55](https://github.com/tamask1s/signal_synth/issues/55), [signal_synth#56](https://github.com/tamask1s/signal_synth/issues/56)

**Implementation commit:** Pending

**CI verification:** Pending

## 1. Decision

Add external-reader conformance tests for generated standard waveform exports.
Existing unit tests still validate headers, binary scaling, labels, and native
annotation bytes directly. The new tests exercise generated artifacts through
common external readers where available.

Contracts:

- WFDB: optional `rdsamp`/`rdann` CTest harness for ECG-only and ECG+PPG
  records;
- EDF/BDF: optional `pyedflib` reader CTest harness for ECG-only and ECG+PPG
  EDF+ and BDF+ files;
- tests pass as documented skips if the external reader is absent;
- writer corrections found by native readers become mandatory unit-test
  assertions.

## 2. Scope

In scope:

- generated `synsigra.hea`, `synsigra.dat`, and `synsigra.atr` can be opened
  by WFDB tooling when `rdsamp` and `rdann` are installed;
- generated `synsigra.edf` and `synsigra.bdf` can be opened by `pyedflib`
  when available;
- EDF+/BDF+ annotation signal readability covers beat labels and PPG systolic
  peaks;
- EDF/BDF writer uses native-reader-compatible data records and EDF+ recording
  identification fields.

Out of scope:

- bundled third-party tool installation;
- DICOM export;
- hosted SaaS implementation.

## 3. Verification

New procedures:

- `TEST-WFDB-NATIVE-001`: renders ECG-only and ECG+PPG examples, then runs
  `rdsamp` and `rdann` if installed;
- `TEST-EDF-BDF-NATIVE-001`: renders ECG-only and ECG+PPG examples, then opens
  EDF+ and BDF+ files with `pyedflib` if installed.

Extended existing procedure:

- `TEST-EDF-BDF-EXPORT-001`: asserts one-second data-record layout for native
  reader compatibility.

Verified locally on 2026-07-04:

- `cmake --build build-release`: passed;
- `ctest --output-on-failure` in `build-release`: 31/31 passed;
- `TEST-EDF-BDF-NATIVE-001` opened generated EDF+ and BDF+ files with
  `pyedflib`;
- `TEST-WFDB-NATIVE-001` skipped locally because `rdsamp`/`rdann` were not
  installed;
- `cmake --build build-sanitize`: passed;
- `env ASAN_OPTIONS=detect_leaks=0 ctest -E TEST-BUILD-001 --output-on-failure`
  in `build-sanitize`: 30/30 passed.

Pending before `Verified`:

- GitHub Actions Linux and Windows CI;
- #55 remains externally tool-dependent unless a CI image or local job provides
  `rdsamp`/`rdann`.

## 4. Writer Correction

The EDF/BDF native reader found a real writer compatibility issue. The writer
now uses:

- one-second data records when sample count is divisible by sample rate;
- record-local EDF+/BDF+ timekeeping annotations;
- minimum annotation capacity compatible with `pyedflib`;
- EDF+ compatible patient and recording identification fields.

## 5. Change Log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-07-04 | Accepted native format conformance design |
| 0.9 | 2026-07-04 | Added optional native tests and fixed EDF/BDF native-reader compatibility |
