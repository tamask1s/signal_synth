# HRV Robustness v2

**Issue:** [signal_synth#89](https://github.com/tamask1s/signal_synth/issues/89)

**Status:** Implemented

**Date:** 2026-07-18

## Objective

Provide an ECG-to-HRV engineering verification pack that separates waveform
detection, NN/RR reconstruction, and metric calculation. The package remains
synthetic algorithm QA and does not make autonomic, diagnostic, or clinical
validation claims.

## Contract changes

- Scenario schema 9 adds `vlf_power_fraction`, `vlf_center_hz`, and
  `vlf_bandwidth_hz` to enabled HRV configuration.
- `vlf_power_fraction` allocates part of the non-respiratory target RR
  variance to VLF. The remaining variance is divided by `lf_hf_ratio`.
- HRV metric definition v2 uses deterministic linear interpolation at 4 Hz,
  mean removal, a Hann window, and a direct periodogram.
- Frequency bands are VLF `[0.0033,0.04)`, LF `[0.04,0.15)`, and HF
  `[0.15,0.40]` Hz. Total power includes all three bands.
- Frequency output adds VLF power, LF normalized units, and HF normalized
  units. LF/HF remains the ratio of absolute LF and HF power.
- The shared measurement contract accepts `nu`, so ECG HRV and PPG PRV use
  the same normalized-power unit in C++ and in the generator-free verifier.
- The integration contract and authoring metadata advance because SaaS
  consumers must accept schema 9 and the expanded metric set.

## Verification layers

The replacement `hrv_robustness_v2` pack uses existing uniform targets:

1. `r_peak` scores ECG waveform detection.
2. The `rr_intervals` member of `hrv_metrics_json_v1` scores NN/RR timing and
   interval values after the declared exclusion policy.
3. The `metrics` member scores time, Poincare, and frequency calculations.
4. `signal_quality` scores the declared acquisition disturbance intervals.

The local verifier summarizes these three HRV pipeline stages independently.
It does not infer a clinical cause or hide one stage behind a combined score.

Scenario schema 9 also makes the duplicated mean-rate, variability, and seed
aliases explicit consistency constraints. A document with conflicting ECG and
HRV values is rejected instead of being silently normalized during parsing.

## Stress matrix

The curated pack contains deterministic clean VLF/LF/HF/respiratory cases,
high variability, ectopy, and analytic baseline-wander, powerline, EMG,
offset-drift, and dropout cases. No third-party noise asset is required.
The exact physiological annotations remain unchanged by acquisition noise.

SDNN and SDSD use sample standard deviations. RMSSD, pNN50 and Poincare
successive differences are formed only from adjacent accepted NN intervals;
excluded intervals are never bridged.

## Compatibility

Backward compatibility with the beta `hrv_v1` catalog entry is not required.
The old pack is replaced instead of aliased. Existing non-HRV scenario schema
versions remain readable. GCC 4.9 and C++11 compatibility remains mandatory.

## Limitations

- VLF estimates require longer windows and are estimator-dependent. The
  VLF-dominant curated case uses a ten-minute record.
- The reference method is intentionally explicit; it is not declared
  equivalent to Welch, Lomb-Scargle, or proprietary preprocessing methods.
- Patient-derived normal ranges and clinical autonomic interpretation are out
  of scope.

## Verification

- The complete CTest suite covers schema, generation, rendering, scoring,
  package creation, local verification, and integration-contract behavior.
- The HRV pack is rendered end to end as a ten-case challenge package.
- The DataBrowser GCC 4.9/C++11 smoke and SHA-256 synchronization check cover
  the Windows adapter subset.
