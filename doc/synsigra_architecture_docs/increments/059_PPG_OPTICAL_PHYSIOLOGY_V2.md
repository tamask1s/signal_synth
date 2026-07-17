# PPG Optical Physiology V2

**Document ID:** SYN-INC-059

**Version:** 1.0

**Status:** Implemented

**Owner role:** Engineering

**Date:** 2026-07-17

**Implementation issue:** [signal_synth#81](https://github.com/tamask1s/signal_synth/issues/81)

## 1. Decision

Schema version 6 introduces one coherent `ppg.optical` object. When enabled it
always creates paired red and infrared channels and exports explicit optical
state. The old independent top-level red/infrared channel switches are removed
from the current authoring contract; partial optical pairs are not meaningful
for ratio-of-ratios verification.

The model is an engineering pulse-oximetry stress source, not a clinical SpO2
simulator or oximeter calibration claim.

## 2. Signal Layers

The processing order is fixed:

```text
ECG-linked pulse timing
  -> clean green/red/infrared optical latent waveforms
  -> respiratory and activity coupling
  -> channel-dependent acquisition artifacts
  -> sensor noise, red/IR crosstalk, sensor gain and ambient offset
  -> clipping and quantization
  -> final measured fiducials and wearable resampling
```

`ppg_record` owns clean latent channels, pulse annotations, optical state, and
sensor metadata. `signal_quality_waveforms` owns every final observable PPG
channel. Export, wearable rendering, and remeasurement always use the final
observable channels.

## 3. Optical Equations

For each generated pulse:

```text
PI_ir = AC_ir / DC_ir
R = (AC_red / DC_red) / (AC_ir / DC_ir)
SpO2_target = calibration_intercept + calibration_slope * R
```

The configured SpO2 trajectory therefore determines `R`; infrared perfusion
index determines `AC_ir`; and `AC_red` follows exactly. Calibration identity,
coefficients, target validity range, units, and per-pulse values are ground
truth. Smooth cosine transitions avoid discontinuities between trajectory
segments.

## 4. Public And Package Contracts

The C++ API exposes optical configuration/state through `ppg_model.h` and adds
the generic measurement target `ppg_optical`. Case exports add:

```text
ppg_optical_latent.csv
ppg_optical_truth.json
```

Truth contains calibration provenance, equations, clean AC/DC/PI/R/SpO2 per
pulse, channel electronics, and final clipping counts. Challenge manifests and
the generator-free Python package expose dedicated artifact roles/accessors.

## 5. Artifacts And Electronics

Motion and ambient-light artifacts are derived once and applied with explicit
per-channel sensitivities. Crosstalk uses the other channel's pre-electronics
AC component, avoiding recursive channel order dependence. Gain, clipping, and
uniform quantization are deterministic. Sensor noise is added only in the
electronics layer, so `ppg_optical_latent.csv` remains clean. Physiological
annotations retain clean timing; measured fiducials refer to final observable
samples. Red and infrared pulses are emitted atomically at record boundaries.

## 6. Scoring

`ppg_optical` measurement truth includes SpO2 target, ratio-of-ratios, and red
and infrared perfusion indices. It uses the existing uniform measurement CSV/
JSON submission and local scoring contracts rather than a new output format.

## 7. Verification And Visualization

Stable tests shall cover equations, smooth trajectories, channel separation,
artifact sensitivity, crosstalk, clipping, quantization, exact replay, schema
round trip, measurement scoring, challenge/Python access, and native exports.
A curated pack and DataBrowser script shall show normoxia, desaturation,
low-perfusion, and wavelength-dependent interference cases.

Implemented verification includes `TEST-PPG-OPTICAL-V2-001`,
`TEST-PPG-OPTICAL-PYTHON-001`, pack-metadata validation, the complete native
regression suite, and `TEST-DATABROWSER-GCC49-001`. The curated
`ppg_optical_v2` pack contains normoxia, desaturation, low-perfusion, and
interference cases. DataBrowser script 082 displays final signal channels and
interpolated optical ground truth in separate windows.

## 8. Compatibility And Limits

Repository scenarios and adapters migrate to schema 6 `ppg.optical`; no alias
for the old top-level `ppg.red`/`ppg.infrared` layout is added. The calibration
is user-declared engineering metadata. Generated values are not evidence of
clinical accuracy, patient equivalence, or hardware conformity.

Schema 6 does not require wearable streams. A `wearable` object is serialized
only when at least one stream is configured; this keeps ordinary optical
scenarios independent from the multi-rate device layer.
