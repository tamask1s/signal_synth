# Acquisition Artifact and Signal Quality Pack

**Document ID:** SYN-ARCH-INC-015

**Version:** 0.2

**Status:** Implemented and verified

**Owner role:** Biosignal Algorithms / Verification

**Date:** 2026-07-02

**Proposed traceability ID:** `TRC-SQ-001`

**Implementation issue:** [signal_synth#26](https://github.com/tamask1s/signal_synth/issues/26)

## 1. Decision

Implement the first acquisition artifact and signal-quality layer as a portable
post-render transform. It modifies exported ECG/PPG sample buffers after the
clean physiological record has been generated, and emits explicit artifact
interval ground truth.

The clean clinical ECG record, beat timeline, PQRST construction fiducials,
measured fiducials, morphology measurements, PPG pulse annotations, and
phenotype assertions remain tied to the clean generated source. The artifact
layer represents acquisition corruption, not a new cardiac condition.

## 2. Product Rationale

The product is a synthetic ECG/PPG ground-truth testbench for algorithm QA.
Clean ECG phenotype coverage is now broad enough that the next high-value
increment is controlled signal-quality stress:

- R-peak detector robustness;
- HRV behavior under corrupted intervals;
- PPG peak detection and dropout handling;
- wearable fusion and signal-quality algorithms;
- repeatable customer QA packs and engineering reports.

## 3. Scenario Contract

Add optional `artifacts` to scenario JSON schema version 2. Each artifact has:

- `type`: `ecg_baseline_wander`, `ecg_powerline`, `ecg_emg_noise`,
  `ecg_dropout`, `ecg_saturation`, or `ppg_dropout`;
- `start_seconds`;
- `duration_seconds`;
- `severity`, normalized to `[0, 1]`;
- `seed`;
- `channels`: array of ECG lead names or `all_ecg` for ECG artifacts, and `all_ppg` for PPG artifacts.

The canonical JSON includes the artifact list in input order. Artifact
parameters contribute to the document fingerprint and render identity.

Validation rejects:

- schema version 1 artifacts;
- unsupported type or channel;
- negative start, non-positive duration, or interval outside the record;
- non-finite or out-of-range severity;
- duplicate channels inside one artifact;
- PPG artifact requests when PPG is disabled.

## 4. Render Contract

The render bundle contains:

- the clean `clinical_ecg_record`;
- optional clean `ppg_record`;
- acquisition-corrupted ECG lead sample buffers;
- optional acquisition-corrupted PPG sample buffer;
- artifact interval annotations.

Exports use corrupted sample buffers in `waveform.csv` and visual previews.
Clinical ground truth remains clean by construction. Artifact ground truth is
exported as interval annotations and metrics.

## 5. Artifact Semantics

Initial artifact behaviors:

- ECG baseline wander: low-frequency deterministic sinusoidal baseline offset;
- ECG powerline: deterministic 50 Hz sinusoid;
- ECG EMG noise: deterministic high-frequency band-shaped noise using seeded
  pseudo-random samples and short smoothing;
- ECG dropout: amplitude attenuation toward zero in the interval;
- ECG saturation: hard clipping to a severity-dependent threshold;
- PPG dropout: attenuation toward baseline in the interval.

This increment does not model electrode physics, impedance, motion sensors, or
country-specific powerline configuration. Those are later extensions.

## 6. Export and DataBrowser Integration

`annotations.json` exports `artifact_intervals` with type, start/end time,
sample range, severity, seed, and affected channels.

`ground_truth_metrics.json` exports artifact count, total artifact seconds, and
per-channel affected seconds.

The report adds an artifact summary and no longer states that no artifacts are
present once artifacts exist.

The DataBrowser adapter adds API support and script
`075_ECG_Artifact_Signal_Quality.txt`. Existing annotation display mode
semantics are reused:

- `1`: show artifact intervals as markers;
- `2`: add an artifact mask channel;
- `3`: hide artifact annotations.

## 7. Verification

Create `TEST-SIGNAL-QUALITY-001` covering:

- schema validation and canonical JSON round trip;
- deterministic repeated render;
- fingerprint changes when artifact parameters change;
- interval exactness in annotations;
- selected-channel-only ECG modification;
- PPG artifact rejection when PPG is disabled;
- PPG artifact application when enabled;
- artifact metrics and report/export content;
- DataBrowser script presence.

Existing export, CLI, JSON, PPG, and package-smoke tests must continue to pass.

## 8. Exit Criteria

1. Artifacts are deterministic and explicitly annotated.
2. Clean physiological ground truth is preserved.
3. Export/report outputs contain corrupted waveforms and artifact truth.
4. Scenario JSON rejects ambiguous or unsupported artifact requests.
5. DataBrowser can visualize the pack.
6. Git and SVN working copies are synchronized and recorded.

## 9. Change Log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-07-02 | Proposed acquisition artifact and signal-quality pack v1 |
| 0.2 | 2026-07-02 | Implemented portable artifact layer, exports, DataBrowser API/script, and `TEST-SIGNAL-QUALITY-001` |
