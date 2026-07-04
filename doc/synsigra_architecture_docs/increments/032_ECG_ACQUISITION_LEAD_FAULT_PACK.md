# ECG Acquisition and Lead-Fault Scenario Pack

**Document ID:** SYN-ARCH-INC-032

**Version:** 1.0

**Status:** Verified

**Owner role:** Core generation / Algorithm QA

**Date:** 2026-07-04

**Traceability ID:** `TRC-ECG-AQ-001`

**Implementation issue:** [signal_synth#47](https://github.com/tamask1s/signal_synth/issues/47)

**Implementation commit:** `d20d9eb4ba640f6aeeb4da70fcffd8d25c0c4076`

**CI verification:** [GitHub Actions run 28718686207](https://github.com/tamask1s/signal_synth/actions/runs/28718686207)

## 1. Decision

Extend the existing post-render `signal_quality` layer with deterministic ECG
acquisition and lead-fault artifacts. The clinical generator remains clean and
truth-preserving; acquisition faults corrupt exported waveform samples only.

The scenario JSON shape stays compact and compatible with the existing
artifact contract:

- `type`;
- `start_seconds`;
- `duration_seconds`;
- `severity`;
- `seed`;
- `channels`.

The artifact interval and affected-channel truth already exported in
`annotations.json` is the primary mask contract for these faults.

## 2. Applicable Requirements

- `REQ-ECG-AQ-001`;
- `REQ-ECG-AQ-002`;
- `REQ-ECG-AQ-003`;
- `REQ-ECG-AQ-004`;
- `REQ-ECG-AQ-005`.

## 3. Public Contracts

New ECG artifact types:

- `ecg_lead_reversal`: polarity inversion on selected channels;
- `ecg_lead_swap`: exact sample-buffer swap between exactly two selected ECG
  channels;
- `ecg_electrode_misplacement`: deterministic blend of selected leads toward
  adjacent lead vectors;
- `ecg_gain_mismatch`: deterministic per-lead gain error;
- `ecg_offset_drift`: deterministic slow offset drift;
- `ecg_clock_drift`: deterministic per-lead resampling drift inside the
  interval;
- `ecg_dropped_samples`: deterministic sample hold/drop pattern;
- `ecg_quantization`: deterministic ADC quantization;
- `ecg_adc_clipping`: severity-dependent clipping.

Compatibility:

- existing artifact types, JSON fields, and export contracts remain valid;
- canonical JSON writes the new type names through the same artifact array;
- lead swap rejects anything other than exactly two ECG leads;
- all new types affect ECG channels only and preserve construction truth.

## 4. Verification Plan

Extended `TEST-SIGNAL-QUALITY-001` to cover:

- JSON parse/write roundtrip for new artifact types;
- lead swap exactness and validation;
- lead reversal polarity;
- misplacement/gain/offset/clock/drop/quantization/clipping corruption;
- unchanged clean construction truth;
- `annotations.json` artifact interval/channel masks and metrics.

Extended curated examples/packs so the new faults are renderable through the
CLI and included in package-level coverage.

Verified locally on 2026-07-04:

- `cmake --build build-release`: passed;
- `ctest --output-on-failure` in `build-release`: 29/29 passed;
- `cmake --build build-sanitize`: passed;
- `env ASAN_OPTIONS=detect_leaks=0 ctest -E TEST-BUILD-001 --output-on-failure`
  in `build-sanitize`: 28/28 passed.

Verified in CI on 2026-07-04:

- Ubuntu C++11 configure/build/test: passed;
- Windows C++11 configure/build/test: passed.

## 5. DataBrowser/SVN Impact

No new DataBrowser API is required. Existing `GenerateECGScenarioJSON` can
render scenarios containing the new artifact types once the core JSON parser
and artifact layer support them. Therefore no SVN sync is required unless a
DataBrowser script is added or changed.

## 6. Risks and Limitations

- These are deterministic acquisition-fault QA transforms, not a hardware
  frontend simulator.
- Electrode misplacement is represented as lead-space perturbation rather
  than anatomically exact electrode relocation.
- Sampling-clock drift and dropped samples are interval-local waveform
  corruptions; sample count and construction truth timelines remain unchanged.

## 7. Change Log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-07-04 | Accepted acquisition and lead-fault extension design |
| 0.9 | 2026-07-04 | Implemented C++ artifact types, JSON support, curated pack, and local verification |
| 1.0 | 2026-07-04 | Completed Linux and Windows CI verification |
