# PPG Physiology V2

**Document ID:** SYN-INC-037

**Version:** 1.0

**Status:** Verified locally

**Owner role:** Engineering

**Date:** 2026-07-06

**Implementation issue:** [signal_synth#50](https://github.com/tamask1s/signal_synth/issues/50)

## 1. Decision

Extend the single-channel green PPG generator with a deterministic per-pulse
parameter model. Scenario schema version 4 adds:

- independent beat-to-beat pulse-delay jitter in addition to slow PTT
  variation and clock drift;
- low-frequency amplitude modulation;
- deterministic rise-time and decay-time variation;
- per-pulse effective timing, amplitude, state, and scoring-validity ground
  truth.

Respiratory amplitude modulation remains in the shared physiology layer so the
same respiratory source can affect RR, ECG baseline, and PPG amplitude.

## 2. Ground-Truth Contract

Each `ppg_pulse_annotation` records:

- ECG beat and R time;
- actual pulse delay;
- expected onset, peak, and offset;
- effective amplitude, rise time, and decay time;
- pulse state and `valid_for_peak_scoring`.

Construction annotations preserve continuous model times. Measurement
annotations expose sample-quantized onset and offset plus a peak remeasured on
the final waveform after physiology and acquisition effects. This avoids stale
ground truth after post-generation modulation.

## 3. Determinism

Every per-beat random variable is a pure function of PPG seed, ECG beat index,
and a stable stream identifier. Configuration order and unrelated channels do
not affect draws.

Schema versions 1 through 3 reject non-default v2 physiology controls. Existing
canonical documents remain unchanged.

## 4. Verification

- deterministic generation and seed sensitivity;
- non-zero beat-to-beat PTT and morphology spread inside configured bounds;
- final-waveform onset/peak/offset measurement consistency;
- schema-v4 canonical round trip and older-schema rejection;
- existing ECG/PPG/export/native-format regression suites.

Local release verification passed all 36 tests on 2026-07-06.

## 5. Limits

This is an engineering waveform model, not validated arterial mechanics.
Single-channel green PPG remains the stable base; red/IR are tracked by #54.

## 6. DataBrowser And SVN Impact

None. No DataBrowser API is added.
