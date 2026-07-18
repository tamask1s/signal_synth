# Wearable Device And Anatomical-Site Profiles

**Document ID:** SYN-INC-060

**Version:** 1.0

**Status:** Implemented

**Owner role:** Engineering

**Date:** 2026-07-17

**Implementation issue:** [signal_synth#84](https://github.com/tamask1s/signal_synth/issues/84)

## 1. Decision

Device behavior belongs to the acquisition layer. Clinical 12-lead ECG and
clean green/red/infrared PPG remain latent reference signals. A wearable ECG
profile transforms the ECG stream consumed by an algorithm; a PPG site profile
resolves the physiological and optical source parameters before wearable
resampling.

The existing wearable sample role remains the user-facing device input. No
second waveform format or profile-specific scoring API is introduced.

## 2. Public Configuration

`wearable.ecg_profile_id` selects one registered ECG profile:

- `clinical_12lead_reference_v1`
- `holter_lead_ii_v1`
- `patch_left_chest_vector_v1`
- `watch_lead_i_v1`

`ppg.optical.profile_id` selects `custom`, `finger_transmissive_v1`,
`wrist_reflectance_v1`, or `ear_reflectance_v1`. The site-profile C++ API
resolves base PPG timing, morphology, perfusion, optical coupling, and sensor
susceptibility. Explicit scenario fields remain the authoritative resolved
values, allowing controlled overrides while retaining profile provenance.

## 3. ECG Device Pipeline

```text
final observable 12-lead ECG
  -> declared lead or lead-vector projection
  -> wearable clock resampling
  -> profile bandwidth response
  -> gain
  -> clipping and quantization
  -> packet availability
```

The profile registry exports channel names, lead-vector weights, placement,
band limits, gain, ADC range, and quantization. Filtering is a deterministic
engineering approximation, not a hardware conformity model. Profile identity
and resolved parameters participate in stream/package fingerprints.

## 4. PPG Site Pipeline

Site profiles resolve pulse delay, rise/decay and dicrotic morphology,
perfusion, red/infrared DC levels, wavelength-dependent motion and ambient
sensitivity, crosstalk, noise, range, and quantization. Optical calibration and
scenario oxygenation trajectories remain explicit engineering inputs.

## 5. Export And SaaS Contract

`wearable_timebase_truth.json` records profile identity and resolved ECG device
parameters. `ppg_optical_truth.json` records site identity and resolved PPG and
optical parameters. The authoring metadata exposes finite profile option lists.
The generator-free Python package continues to read profile-rendered streams
through `ChallengeCase.wearable_samples()`.

The profile-aware timebase truth contract is
`synsigra_wearable_timebase_v3`. `wearable_timebase_v2` pack version 2.1 adds
Holter/finger, patch/wrist, and watch/ear cross-profile cases without adding a
second user-facing waveform or scoring format.

## 6. Verification

Stable tests cover profile catalog identity, deterministic rendering, channel
selection/vector projection, bandwidth/gain/ADC behavior, distinct PPG site
resolution, schema round trip, challenge provenance, a curated cross-profile
pack, and GCC 4.9 DataBrowser visualization. The implementation passed all 57
portable tests and a 43-file SHA-256 DataBrowser synchronization check on
2026-07-17.

## 7. Claim Boundary

Profiles are reproducible engineering stress approximations. They do not claim
equivalence to a named commercial device, anatomical measurement accuracy,
diagnostic suitability, or regulatory conformity.
