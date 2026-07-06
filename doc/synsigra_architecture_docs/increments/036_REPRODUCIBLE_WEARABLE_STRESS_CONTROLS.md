# Reproducible Wearable Stress Controls

**Document ID:** SYN-INC-036

**Version:** 1.0

**Status:** Verified locally

**Owner role:** Engineering

**Date:** 2026-07-06

**Implementation issue:** [signal_synth#64](https://github.com/tamask1s/signal_synth/issues/64)

## 1. Decision

Introduce scenario schema version 3 for reproducible population draws,
cardiorespiratory and activity coupling, ECG/PPG synchronization stress, and
compact long-duration output. Preserve schema versions 1 and 2 unchanged.

The resolved scenario and every random draw are exported alongside the input
scenario. Ground truth distinguishes expected, generated, and intentionally
missing PPG pulses.

## 2. Public Contracts

`ecg_scenario_document` adds:

- `randomization`: seed and bounded named parameter envelopes;
- `physiology`: respiration, respiratory RR modulation, ECG baseline,
  PPG amplitude modulation, and tapered activity interval;
- `output`: compact mode and explicit artifact-retention controls;
- PPG timing stress: variable pulse delay, missing-pulse cadence, sensor clock
  drift, and seed.

`scenario_stress.h` provides deterministic control resolution, coupling, and
the machine-readable randomization audit record. `scenario_authoring.h`
publishes v3 fields, ranges, UI hints, cross-field rules, package-size
estimates, and peak-memory estimates.

## 3. Processing Order

1. Validate and canonicalize the input scenario.
2. Resolve named envelopes from `randomization.seed`.
3. Export the resolved scenario identity.
4. Generate the ECG timeline, including respiratory RR and activity effects.
5. Generate PPG from ECG R events with per-pulse timing ground truth.
6. Apply acquisition artifacts.
7. Apply coupled respiratory baseline/amplitude and activity waveform effects.
8. Remeasure PPG systolic peaks on the final exported waveform.
9. Compute metrics and build exports.

This ordering keeps rhythm and pulse ground truth independent from additive
sensor effects while retaining exact auditability.

## 4. Compact Output

Compact mode is an explicit consistent preset:

- internal cardiac source and VCG channels are not retained;
- morphology remeasurement and verbose lead-level fiducials are omitted;
- waveform CSV, EDF+ and BDF+ are omitted;
- WFDB signal and annotation files remain;
- beat, PPG pulse, HRV, artifact, resolved-scenario, and randomization ground
  truth remain available.

Generation memory remains linear in sample count; this is not a streaming
generator. Preflight analysis reports estimated peak memory and package size
so SaaS workers can enforce deployment-specific limits before rendering.

## 5. Determinism And Compatibility

Draws depend only on the randomization seed and stable parameter name, not
envelope order. The original and resolved document fingerprints plus ECG run
fingerprint form the render identity. Waveform semantics increment the ECG
engine identity to version 13 and the generator development version to
`0.2.0-dev`.

Schema-v1 and schema-v2 canonical documents and export selection remain
unchanged. V3 controls are rejected in older schemas.

## 6. Verification

- `TEST-SCENARIO-STRESS-001`: deterministic draws, bounds, v3 round trip,
  PPG timing variation, missing-pulse truth, and compact export contract.
- `TEST-AUTHORING-001`: v3 discovery and compact resource estimates.
- `TEST-AUTHORING-CLI-001`: validates every template and catalog pack.
- Existing ECG, PPG, JSON, export, package, and native-format suites provide
  regression coverage.

Local release verification passed all 35 tests on 2026-07-06.

Examples:

- `examples/scenarios/packs/wearable_cardiorespiratory_stress_v3.json`
- `examples/scenarios/packs/wearable_long_duration_compact_v3.json`
- `examples/packs/wearable_stress_v1.json`

## 7. DataBrowser And SVN Impact

None. No DataBrowser API or visualization script is added, so these files do
not require SVN synchronization.

## 8. Residual Limits

- Activity is a controlled engineering stress proxy, not a biomechanical
  model and has no accelerometer channel.
- Missing PPG pulses use deterministic cadence, not perfusion physics.
- PPG remains single-channel green; multi-wavelength modeling is tracked
  separately.
- Compact generation is lower-memory but not chunked or streaming.

## 9. Change Log

- 2026-07-06: Initial implementation record for issue #64.
