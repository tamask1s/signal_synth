# Product-Facing HRV Scenario Schema

**Document ID:** SYN-ARCH-INC-025

**Version:** 0.1

**Status:** Implemented, verification pending

**Owner role:** Platform / SDK

**Date:** 2026-07-04

**Traceability ID:** `TRC-HRV-SCN-001`

**Implementation issue:** [signal_synth#40](https://github.com/tamask1s/signal_synth/issues/40)

**Implementation commit:** Pending

**CI verification:** Pending

## 1. Decision

Add a product-facing HRV block to schema-version-2 scenario JSON.

The first HRV increment makes HRV scenarios explicit and reproducible without
claiming that the full HRV metric and scoring engine is complete. The HRV block
captures:

- target mean heart rate;
- target SDNN-like RR variability;
- LF/HF target ratio and LF/HF oscillator band metadata;
- respiratory sinus arrhythmia style HF modulation metadata;
- minimum and maximum RR bounds;
- deterministic HRV seed.

When HRV is enabled, the parser translates the supported first-layer HRV
controls to the existing ECG beat-timeline generator:

- `target_mean_hr_bpm` maps to ECG heart rate;
- `target_sdnn_seconds` maps to ECG RR variability;
- `minimum_rr_seconds` and `maximum_rr_seconds` override ECG rhythm clamps;
- `seed` maps to the ECG rhythm seed.

LF/HF center, bandwidth, ratio, and respiratory fields are validated,
canonicalized, and fingerprinted in this increment. The reusable spectral HRV
metric engine, tachogram export, and HRV scoring are handled by later HRV
issues.

## 2. Applicable Requirements

- `REQ-HRV-SCN-001`;
- `REQ-HRV-SCN-002`;
- `REQ-HRV-SCN-003`;
- `REQ-HRV-SCN-004`;
- `REQ-HRV-SCN-005`;
- `REQ-HRV-SCN-006`.

## 3. Scenario Contract

The `hrv` object is optional and is only valid in schema version 2.

Required fields when present:

- `enabled`;
- `target_mean_hr_bpm`;
- `target_sdnn_seconds`;
- `lf_hf_ratio`;
- `lf_center_hz`;
- `lf_bandwidth_hz`;
- `hf_center_hz`;
- `hf_bandwidth_hz`;
- `respiratory_frequency_hz`;
- `respiratory_amplitude_seconds`;
- `minimum_rr_seconds`;
- `maximum_rr_seconds`;
- `seed`.

Validation rules:

- enabled HRV scenarios require at least 300 seconds of duration;
- target mean HR must be within the supported ECG rhythm range;
- target mean RR must lie inside the configured minimum/maximum RR bounds;
- SDNN-like variability, respiratory amplitude, LF/HF ratio, centers, and
  bandwidths must be finite and in supported engineering ranges;
- schema version 1 rejects HRV configuration.

## 4. API and Layering

The public scenario JSON layer owns the product-facing HRV object:

```cpp
struct hrv_scenario_config;
```

`ecg_qa_scenario` gains optional minimum and maximum RR override accessors. The
defaults preserve existing scenario fingerprints when no HRV block or explicit
RR clamp is used.

No DataBrowser/SVN API is added in this increment.

## 5. Verification

Extend `TEST-ECG-JSON-001`:

- parse valid schema-version-2 HRV object;
- verify ECG timeline mapping for HR, SDNN, RR bounds, and seed;
- verify canonical JSON contains the HRV block;
- verify HRV parameter changes alter the document fingerprint;
- reject invalid LF/HF values transactionally;
- reject HRV windows shorter than 300 seconds;
- reject HRV configuration in schema version 1.

Extend existing scenario and CLI coverage by preserving installed build and
render behavior through `TEST-ECG-SCENARIO-001`, `TEST-CLI-001`, and
`TEST-BUILD-001`.

## 6. Non-Goals

- No spectral HRV metric computation.
- No SD1/SD2, LF/HF measured output, or HRV scoring report.
- No HRV scenario pack.
- No DataBrowser visualization API.
- No hosted SaaS service.

## 7. Risks and Limitations

- The LF/HF and respiratory fields are schema-stable inputs before the full
  HRV metric/scoring engine exists.
- The current mapping uses the existing RR generator and does not yet guarantee
  measured spectral LF/HF targets.
- Short-window rejection is intentionally conservative for product-facing HRV
  scenarios.

## 8. Change Log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-07-04 | Added product-facing HRV scenario schema design |
