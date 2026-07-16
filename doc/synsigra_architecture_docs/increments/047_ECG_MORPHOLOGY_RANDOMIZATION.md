# ECG Morphology Randomization

**Document ID:** SYN-INC-047

**Version:** 1.0

**Status:** Verified

**Owner role:** Engineering

**Date:** 2026-07-16

**Implementation issue:** [signal_synth#48](https://github.com/tamask1s/signal_synth/issues/48)

## 1. Decision

Extend the existing schema-v3 controlled randomization layer with ECG
morphology controls under `ecg.morphology.*`.

The feature reuses the existing randomization contract:

- envelope parameters are sorted before draw generation;
- draws are deterministic from `randomization.seed` and parameter name;
- draw values are exported in `randomization.json`;
- the resolved scenario JSON contains the exact drawn morphology values;
- generation fingerprints include active morphology overrides.

## 2. Public Contract

The C++ API adds `ecg_morphology_control` and compact helper functions:

```text
ecg_morphology_control_name(...)
ecg_morphology_control_from_name(...)
ecg_morphology_control_bounds(...)
ecg_qa_scenario::set_morphology_control(...)
ecg_qa_scenario::clear_morphology_control(...)
ecg_qa_scenario::morphology_control_enabled(...)
ecg_qa_scenario::morphology_control_value(...)
ecg_qa_scenario::has_morphology_controls()
```

Supported JSON/randomization parameter names are:

```text
ecg.morphology.p_amplitude_mv
ecg.morphology.q_amplitude_mv
ecg.morphology.r_amplitude_mv
ecg.morphology.s_amplitude_mv
ecg.morphology.t_amplitude_mv
ecg.morphology.st_j_amplitude_mv
ecg.morphology.st_slope_mv_per_second
ecg.morphology.p_axis_degrees
ecg.morphology.qrs_axis_degrees
ecg.morphology.t_axis_degrees
ecg.morphology.p_duration_ms
ecg.morphology.qrs_duration_ms
ecg.morphology.qt_interval_ms
ecg.morphology.t_duration_ms
```

Direct scenario JSON may also contain schema-v3+ `ecg.morphology` with the
same field names. The writer emits this object only when at least one
morphology override is active, preserving old canonical JSON for scenarios
that do not use the feature.

## 3. Data Flow

Morphology overrides are applied during ECG scenario compilation after base
rhythm timing setup and before condition-specific phenotype modifications.
That makes them useful as a baseline population-like variation layer while
allowing requested conditions to remain authoritative.

`qt_interval_ms` sets both QT and QTc to a fixed QT correction mode for that
render. This keeps replayed randomized QT values explicit and avoids hidden
heart-rate correction changes.

## 4. Example Pack

`examples/packs/ecg_morphology_population_v1.json` contains three stable-seed
normal-rhythm cases:

- `morph_seed_101`;
- `morph_seed_202`;
- `morph_seed_303`.

The pack is an engineering example and test fixture, not part of the curated
SaaS beta release catalog.

## 5. Verification

Implemented checks:

- schema-v3 morphology randomization envelopes validate and reject out-of-range
  bounds;
- repeated resolution with the same seed is byte-stable;
- changed seeds produce changed draws;
- resolved scenario JSON roundtrips with `ecg.morphology`;
- multiple seeds render with beat, fiducial and morphology integrity;
- the example pack validates and renders in `TEST-ECG-PACK-001`.

Local targeted verification:

```text
ctest -R "TEST-(SCENARIO-STRESS-001|ECG-PACK-001|ECG-JSON-001|AUTHORING-001|AUTHORING-CLI-001)$" --output-on-failure
```

Result: passed.

## 6. Non-goals

This is not a population-fitted clinical morphology model and does not support
claims about demographic representativeness or diagnostic validity. It is a
controlled engineering variation layer for algorithm QA.
