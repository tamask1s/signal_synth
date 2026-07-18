# PRV and respiratory-rate verification

Issue: https://github.com/tamask1s/signal_synth/issues/85

## Purpose

This increment adds a product-facing cardiorespiratory verification workflow without introducing another customer submission format. PRV and respiratory-rate outputs use the existing `measurement_values_json_v1` or `measurement_values_csv_v1` contract and the existing local Python verifier.

The feature is an engineering test reference. It does not establish clinical autonomic, respiratory, pulse-transit-time, blood-pressure, or patient-population validity.

## Layering

1. `physiology_coupling_config` declares one deterministic respiratory frequency, seed and independent coupling amplitudes for RR, ECG baseline, PPG amplitude, PPG delay and accelerometer.
2. `scenario_stress` resolves one phase from that seed. The same phase drives every enabled coupling. Direct PPG delay variation and respiratory PPG-delay coupling are mutually exclusive in source scenarios.
3. `ecg_render` applies the resolved controls to the ECG, PPG and accelerometer waveforms, then calls `analyze_cardiorespiratory` after final PPG fiducial measurement.
4. `cardiorespiratory` derives PRV from measured green-PPG systolic peaks and samples the respiratory reference at 10 Hz.
5. `ecg_export` publishes versioned portable truth artifacts. `measurement_scoring` projects the same result into the uniform measurement contract.
6. Challenge assembly, Python `ChallengeCase`, local verification, curated metadata and DataBrowser consume these contracts without accessing generator internals.

## Respiratory reference

`synsigra_cardiorespiratory_truth_v1` uses a constant respiratory frequency per scenario and an exact seeded phase. The sampled reference contains time, phase, normalized waveform and respiratory rate. Coupling amplitudes are independent, so a pack can enable any useful subset while retaining a shared phase reference.

The v1 reference is intentionally not a variable-rate breathing model. Future variable-rate trajectories must define an integrated phase contract before they can replace this reference.

## PRV definition

PRV intervals are differences between final measured PPG systolic-peak times. An interval is excluded when either bounding pulse is missing or unsuitable, is in low perfusion, is linked to arrhythmia, overlaps a PPG artifact, or yields a nonpositive interval. This policy is exported verbatim and is separate from the ECG HRV exclusion policy.

The generic variability engine computes mean interval/rate, SDNN, RMSSD, pNN50, SD1, SD2, LF, HF and total power. Spectral measurement truth is valid only when at least eight accepted intervals span at least 60 seconds. The curated cases use five-minute windows.

`cardiorespiratory_truth.json` also reports signed `PRV - HRV` metric differences. This is agreement truth for algorithm QA, not a claim that PRV and HRV are physiologically interchangeable.

## Portable contracts

- `cardiorespiratory_truth.json`: `synsigra_cardiorespiratory_truth_v1`
- `prv_tachogram.csv`: interval value and explicit exclusion flags
- `respiration_reference.csv`: 10 Hz deterministic respiratory trajectory
- `measurement_truth.json`: `prv` and `respiratory_rate` targets through `synsigra_measurement_truth_v1`
- customer output: `measurement_values_json_v1` or `measurement_values_csv_v1`

The curated `cardiorespiratory_v1` pack covers clean coupling, respiratory PTT variation, and pulse-loss/low-perfusion/motion stress. Package roles are explicit in the challenge manifest and curated SaaS metadata.

## DataBrowser

`GenerateCardiorespiratoryScenarioJSON(signals, truth, scenario_json, annotation_output)` creates:

- final ECG, all generated PPG channels and accelerometer signal channels;
- normalized respiratory reference and rate;
- ECG RR and PPG pulse-interval tachograms;
- accepted/excluded masks for both interval series.

`084_Cardiorespiratory_PRV_Respiration.txt` is the visual acceptance script. The adapter remains C++11/GCC 4.9 compatible and does not depend on challenge, scoring, WFDB or EDF implementation files.

## Verification

- `TEST-CARDIORESPIRATORY-001`: shared phase, PRV exclusions, metrics, artifacts, roles and C++ scoring
- `TEST-CARDIORESPIRATORY-PYTHON-001`: challenge integrity, Python artifact access and perfect local scoring
- `TEST-VERIFICATION-CATALOG-001` and `TEST-PACK-METADATA-EXPORT-001`: curated pack/SaaS metadata
- `TEST-DATABROWSER-GCC49-001`: script render and generation-only adapter dependency boundary
