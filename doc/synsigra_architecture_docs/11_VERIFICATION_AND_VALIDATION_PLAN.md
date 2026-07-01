# Verification and Validation Plan

Version: 0.1  
Status: Draft  
Scope: Generator verification and engineering evidence. Not clinical validation.

## 1. Purpose

This plan defines how Synsigra verifies that its generator and exported ground truth match the specified scenario.

It does not establish clinical validity.

## 2. Validation terminology

Use terms carefully:

| Term | Meaning in Synsigra |
|---|---|
| Verification | We built the generator correctly against its specification |
| Scenario validation | The scenario JSON is internally consistent and supported |
| Generator qualification | Evidence that the generator is suitable for a defined engineering test purpose |
| Customer algorithm test | Customer algorithm measured against synthetic ground truth |
| Clinical validation | Out of scope unless future MDR/clinical strategy is defined |

## 3. Verification layers

### 3.1 Unit verification

Test individual functions:

- deterministic RNG;
- HRV oscillator bank;
- RR clipping;
- ECG morphology generation;
- fiducial measurement;
- PPG pulse generation;
- artifact generation;
- fingerprinting;
- export serialization.

### 3.2 Integration verification

Test full scenario rendering:

- clean ECG;
- noisy ECG;
- premature beat;
- HRV scenario;
- ECG+PPG coupling;
- PPG artifact.

### 3.3 Golden-output verification

For selected scenarios, store golden metrics and/or golden files.

Verify:

- sample count;
- channel names;
- key annotation times;
- number of beats;
- HRV metrics within tolerance;
- scenario fingerprint;
- export checksums if platform-stable.

### 3.4 Chunk-invariance verification

Render same scenario:

- one-shot;
- 1-second chunks;
- irregular chunk sizes.

Outputs should match according to defined tolerance or exactly where feasible.

### 3.5 Cross-platform verification

Later:

- Linux x86_64;
- Windows;
- macOS;
- compiler variation.

If floating-point exactness cannot be guaranteed, define tolerances and document limitations.

## 4. Scenario validation tests

Reject:

- invalid schema;
- negative duration;
- unsupported channel;
- unsupported clinical condition;
- event outside duration;
- impossible RR limits;
- incompatible conditions;
- unsupported artifact/channel combination.

Warn:

- RR clipping;
- morphology overlap;
- signal clipping;
- high artifact dominance;
- observed HRV deviation.

## 5. Ground-truth verification

### ECG

Verify:

- construction R times match beat timeline;
- R sample indices are first sample at or after event time;
- measured fiducials map to actual local extrema;
- onset/offset rules are deterministic;
- beat IDs are stable.

### PPG

Verify:

- PPG pulse is linked to ECG beat ID;
- delay matches configured parameters;
- peak annotation matches pulse maximum;
- dropout/saturation annotations match artifact intervals.

### Artifact intervals

Verify:

- artifact annotations match actual injection intervals;
- affected channels are correct;
- severity and parameter metadata are preserved.

## 6. Report verification

Verify report includes:

- scenario ID;
- fingerprint;
- generator version;
- limitation statement;
- channel list;
- metrics;
- warnings;
- export/license metadata.

## 7. Regression test matrix

Initial 10 deterministic test cases:

1. `ecg_clean_60bpm_10s`
2. `ecg_hrv_lfhf_300s`
3. `ecg_baseline_wander_60s`
4. `ecg_powerline_noise_60s`
5. `ecg_emg_noise_60s`
6. `ecg_periodic_premature_120s`
7. `ecg_missed_beat_120s`
8. `ecg_af_like_irregular_120s`
9. `ecg_ppg_fixed_delay_60s`
10. `ppg_motion_dropout_60s`

For each:

- validate scenario;
- render;
- export;
- verify annotations;
- generate report.

## 8. Tool qualification package

For enterprise customers, provide:

- generator specification;
- scenario specification;
- V&V plan;
- V&V report;
- known limitations;
- release notes;
- traceability matrix;
- test evidence summary.

This does not make Synsigra a certified medical-device validator, but it makes it easier for regulated customers to use as development evidence.

## 9. Traceability model

Maintain traceability:

```text
Requirement -> Design element -> Test case -> Test result -> Report section
```

Example:

```text
SRS-FUNC-ECG-001
  -> ecg_model::render
  -> TEST-ECG-CLEAN-001
  -> passing build 0.1.0
  -> V&V Report section 4.1
```

## 10. Release criteria

A release should not be tagged if:

- deterministic tests fail;
- scenario schema tests fail;
- core golden scenarios fail;
- reports omit disclaimers;
- export metadata omits version/fingerprint;
- unsupported conditions are silently accepted.

## 11. Current repo assessment

The current repository already has good signs:

- deterministic chunk-invariant streaming is explicitly documented;
- the model separates construction events from measured fiducials;
- scenario API rejects unsupported/incompatible conditions;
- clinical engine is documented as an engineering phantom and not clinical validation evidence.

Preserve these properties.

## 12. Future MDR note

If the product later becomes MDR-relevant, this V&V plan is only a starting point. It must be integrated with:

- intended purpose;
- risk management;
- software safety classification;
- IEC 62304 software lifecycle;
- clinical evaluation strategy if applicable;
- post-market processes if marketed as a device.
