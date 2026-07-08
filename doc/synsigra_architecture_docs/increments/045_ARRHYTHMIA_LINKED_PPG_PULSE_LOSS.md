# Arrhythmia-linked PPG Pulse Loss

**Document ID:** SYN-INC-045

**Version:** 1.0

**Status:** Verified

**Owner role:** Engineering

**Date:** 2026-07-08

**Implementation issue:** [signal_synth#67](https://github.com/tamask1s/signal_synth/issues/67)

## 1. Decision

Add deterministic PPG pulse-response controls linked to ECG beat origin. The
PPG generator remains driven by the shared ECG ventricular timeline; selected
non-sinus beat origins can attenuate or omit the corresponding expected PPG
pulse without creating a new rhythm engine.

## 2. Public Contract

`ppg_config` adds three amplitude-scale controls:

- `pac_pulse_amplitude_scale`;
- `pvc_pulse_amplitude_scale`;
- `paced_pulse_amplitude_scale`.

Each scale is in `[0, 1]`. The default is `1`, preserving existing waveform
behavior. A value of `0` creates an intentionally missing pulse. A value
between `0` and `1` creates a generated weak pulse.

Scenario JSON schema-v4 accepts the same fields in the `ppg` object. Existing
schema-v4 documents that omit these fields remain valid and default to `1`.

`ppg_pulse_annotation` and `annotations.json` add:

- `arrhythmia_linked`;
- `arrhythmia_amplitude_scale`.

Ground-truth metrics add arrhythmia-linked pulse counts and
arrhythmia-linked missing-pulse counts.

## 3. Data Flow

The PPG generator reads `clinical_beat_annotation.origin`:

- PAC uses `pac_pulse_amplitude_scale`;
- PVC, ventricular escape and VT beats use `pvc_pulse_amplitude_scale`;
- paced and atrial-paced beats use `paced_pulse_amplitude_scale`;
- conducted and junctional escape beats retain scale `1`.

The resulting scale multiplies the existing effective PPG amplitude after
low-frequency, perfusion and weak-pulse scaling. The existing pulse-state
contract remains authoritative: missing pulses receive no fabricated fiducials;
weak pulses remain generated and scoreable.

## 4. Pack Impact

`ppg_benchmark_v1` now includes `arrhythmia_pulse_loss`, backed by
`examples/scenarios/ppg_arrhythmia_pulse_loss_v4.json`.

The pack remains local-scoring for `ppg_systolic_peak` and `ppg_pulse_onset`.
The ECG PVC timeline explains the expected pulse-loss opportunities but does
not convert this pack into a clinical physiology validation claim.

## 5. Compatibility

The C++ and JSON additions are additive. Default scales of `1` preserve legacy
generated waveforms. Consumers that ignore the new annotation and metrics
fields remain compatible.

Changing the curated PPG benchmark pack changes its pack fingerprint. SaaS
deployments should re-import the curated metadata artifact before exposing the
new case.

## 6. Verification

Implemented verification:

- `TEST-PPG-001` checks PVC-linked missing and weak pulse states;
- `TEST-PPG-PHYS-001` checks schema roundtrip, exported annotations and
  exported metrics;
- `TEST-ECG-PACK-001` renders the expanded PPG benchmark pack and scores both
  PPG targets.

## 7. Non-goals

This increment does not model patient-specific hemodynamics, blood pressure,
vascular disease, SpO2, or clinically validated ECG-to-PPG coupling. It is a
deterministic engineering stress feature for algorithm QA.
