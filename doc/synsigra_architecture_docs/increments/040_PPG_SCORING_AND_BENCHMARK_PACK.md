# PPG Scoring And Benchmark Pack

**Document ID:** SYN-INC-040

**Version:** 1.0

**Status:** Verified locally

**Owner role:** Engineering

**Date:** 2026-07-06

**Implementation issue:** [signal_synth#53](https://github.com/tamask1s/signal_synth/issues/53)

## 1. Decision

Extend the existing event comparison engine with `ppg_pulse_onset`; do not
create a PPG-specific matching implementation. Peak and onset detections use
the same CSV/JSON input, deterministic one-to-one matching, tolerance handling,
artifact overlap logic, and C++/Python report contract.

Every successful PPG event comparison also reports pulse timing:

- ground-truth, detected, and matched adjacent interval counts;
- interval MAE, RMS, and maximum absolute error;
- ground-truth and detected mean pulse rate and absolute rate error.

Only adjacent ground-truth events for which both endpoints are matched
contribute interval error. Mean rates use all positive intervals in their
respective event streams.

## 2. Quality Bins

`clean` and `artifact` remain mutually exclusive aggregate bins. The following
overlapping PPG stress bins are reported independently:

- `low_perfusion`;
- `weak`;
- `motion`;
- `dropout`.

Dropout means the explicit `ppg_dropout` acquisition artifact. Saturation and
ambient light remain in the all-artifact bin and retain exact typed intervals.

## 3. Interfaces

- C++ target: `ecg_compare_ppg_pulse_onset`;
- CLI: `signal-synth compare ppg-onsets ...`;
- Python wrapper: `synsigra.compare_ppg_onsets(...)`;
- detection target name: `ppg_pulse_onset`;
- authoring target support: local scoring when PPG is enabled.

The generator-free Python verifier shall reproduce C++ JSON metrics exactly.

## 4. Benchmark Pack

`ppg_benchmark_v1` covers clean delay, variable PTT, respiratory modulation,
perfusion reduction, motion with reference, dropout/saturation, weak/missing
pulses, and combined wearable stress. Every case is render-tested and scored
with exact exported detections.

The pack contains eight isolated cases. `TEST-ECG-PACK-001` renders every case
and scores both onset and peak targets, for 16 exact case-target comparisons.

## 5. Compatibility

Existing target enum values and input names remain valid. Event comparison
JSON gains additive `motion`, `dropout`, and `pulse_timing` metrics. Scenario
schema remains version 4.

## 6. DataBrowser And SVN Impact

None. This increment adds no DataBrowser API.

## 7. Verification

The full 38-test C++11 release suite passed, including generator-free Python /
C++ onset, peak, quality-bin and pulse-timing JSON parity. The affected scoring,
pack, detection-I/O, facade and Python tests passed under
AddressSanitizer/UndefinedBehaviorSanitizer with leak detection disabled due to
the execution environment's ptrace policy.
