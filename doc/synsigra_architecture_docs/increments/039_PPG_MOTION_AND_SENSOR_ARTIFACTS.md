# PPG Motion And Sensor Artifacts

**Document ID:** SYN-INC-039

**Version:** 1.0

**Status:** Verified locally

**Owner role:** Engineering

**Date:** 2026-07-06

**Implementation issue:** [signal_synth#51](https://github.com/tamask1s/signal_synth/issues/51)

## 1. Decision

Extend the signal-quality layer with deterministic PPG acquisition stress:

- periodic, burst, and broadband motion;
- ambient-light interference;
- sensor saturation.

Motion artifacts produce a correlated, zero-centered `accel_motion` reference
channel in `g`. Ambient light and saturation do not create accelerometer
activity because they are optical/electronic sensor faults rather than motion.
Every artifact uses the existing exact half-open interval contract and a
raised-cosine edge taper.

## 2. Contracts

The scenario continues to target `ppg_green`; `accel_motion` is generated
automatically when at least one motion artifact is present. Its sample rate and
sample count equal the ECG/PPG timeline.

CSV uses `accel_motion_g`. Metadata, WFDB, EDF+ and BDF+ use channel name
`accel_motion`, unit `g`, and role `motion_reference`. Artifact annotations add
the reference channel only to motion intervals.

Artifact morphology and its dominant frequency are deterministic functions of
type, severity, seed, sample rate, and interval. Existing schema-v2 through
schema-v4 documents remain canonical-compatible because no new configuration
field is required.

## 3. Scoring

PPG peak scoring keeps total, clean, and all-artifact metrics and adds an
overlapping `motion` bin. C++ and the generator-free Python verifier derive the
bin from exact `ppg_motion_*` intervals. Ambient-light, saturation, and dropout
remain visible in the general artifact bin.

## 4. Verification

`TEST-PPG-MOTION-001` covers:

- schema round trip and deterministic replay;
- exact interval/sample bounds and tapered starts;
- non-zero reference for all three motion classes and zero reference outside;
- CSV, metadata, WFDB and EDF+/BDF+ channel contracts;
- perfect-detection motion scoring.

The full 37-test C++11 release suite passed, including Python scorer parity and
native format conformance. The affected export, signal-quality, PPG motion and
Python parity tests also passed under AddressSanitizer/UndefinedBehaviorSanitizer
with leak detection disabled because LeakSanitizer is unavailable under the
execution environment's ptrace policy.

## 5. Limits

The accelerometer channel is an engineering motion reference, not a calibrated
three-axis inertial sensor model. Optical wavelength interaction belongs to the
future red/infrared multi-channel issue.

## 6. DataBrowser And SVN Impact

None. No DataBrowser API is added, so these files are not copied to SVN.
