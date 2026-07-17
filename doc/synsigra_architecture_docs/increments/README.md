# Architecture Increment Records

Purpose: keep one reviewable design record for every implementation increment
without turning the system-level architecture documents into a chronological
change log.

Each increment record shall be created before implementation and contain:

- document ID, version, status, owner role, and date;
- decision and rationale;
- scope, assumptions, and explicit non-goals;
- applicable SRS requirements and architecture inputs;
- proposed traceability ID and implementation issue;
- public contracts, module ownership, and data flow;
- compatibility and migration rules;
- verification procedures, stable test IDs, and acceptance criteria;
- DataBrowser/SVN integration impact;
- risks, residual limitations, and open issues;
- implementation sequence and change log.

Lifecycle:

```text
Proposed -> Accepted -> Implementing -> Verified -> Superseded
```

An increment may enter `Implementing` only after its GitHub issue exists and
contains the records required by `../17_TRACEABILITY_SOP.md`. It may enter
`Verified` only after the exact implementation commit has passed the required
CI jobs and the traceability matrix has been updated.

These records describe engineering design and verification intent. They do
not establish clinical validity, MDR compliance, or a qualified test
environment.

Current records:

- `001_PORTABLE_BUILD_BASELINE.md`
- `002_VERSIONED_SCENARIO_JSON.md`
- `003_SCENARIO_CLI_VALIDATE_FINGERPRINT.md`
- `004_RENDER_EXPORT_REPORT.md`
- `005_SHARED_TIMELINE_PPG.md`
- `006_HYPERTROPHY_QA_PACK.md`
- `007_TERRITORIAL_INFARCTION_INJURY_QA_PACK.md`
- `008_ISCHEMIA_STT_REPOLARIZATION_QA_PACK.md`
- `009_CONTINUOUS_STT_RENDERING.md`
- `010_DATABROWSER_FIXED_LABEL_SAFETY.md`
- `011_CATALOG_MORPHOLOGY_QUALITY.md`
- `012_ADVANCED_CONDUCTION_PHENOTYPES.md`
- `013_EPISODE_TIMELINE_SVARR_PSVT.md`
- `014_DATABROWSER_ZAX_GCC49_COMPATIBILITY.md`
- `015_ACQUISITION_ARTIFACT_SIGNAL_QUALITY_PACK.md`
- `016_SCENARIO_PACK_BATCH_QA.md`
- `017_ALGORITHM_COMPARISON_SCORING.md`
- `018_STABLE_CPP_FACADE.md`
- `019_CHALLENGE_PACKAGE_MANIFEST.md`
- `020_DETECTION_OUTPUT_CONTRACTS.md`
- `021_PACK_LEVEL_SCORING_AGGREGATION.md`
- `022_LOCAL_PYTHON_SCORING_PACKAGE.md`
- `023_WFDB_WAVEFORM_EXPORT.md`
- `024_EDF_BDF_WAVEFORM_EXPORT.md`
- `025_HRV_SCENARIO_SCHEMA.md`
- `026_HRV_METRICS_AND_TACHOGRAM_EXPORT.md`
- `027_HRV_USER_OUTPUT_SCORING.md`
- `028_HRV_BENCHMARK_PACK.md`
- `029_ECG_BEAT_CLASSIFICATION_SCORING.md`
- `030_ECG_RHYTHM_ENGINE_V2.md`
- `031_ECG_PACED_RHYTHM_SCENARIOS.md`
- `032_ECG_ACQUISITION_LEAD_FAULT_PACK.md`
- `033_NATIVE_FORMAT_TOOL_CONFORMANCE.md`
- `034_VERIFICATION_CI_RUNTIME_POLICY.md`
- `035_SAAS_CHALLENGE_PACKAGE_ASSEMBLY.md`
- `036_REPRODUCIBLE_WEARABLE_STRESS_CONTROLS.md`
- `037_PPG_PHYSIOLOGY_V2.md`
- `038_PPG_PERFUSION_AND_PULSE_STATE.md`
- `039_PPG_MOTION_AND_SENSOR_ARTIFACTS.md`
- `040_PPG_SCORING_AND_BENCHMARK_PACK.md`
- `041_PYTHON_SDK_DISTRIBUTION.md`
- `042_GOLDEN_CHALLENGE_COMPATIBILITY.md`
- `043_SAAS_CURATED_PACK_METADATA.md`
- `044_SAAS_RELEASE_SET_EXPORT.md`
- `045_ARRHYTHMIA_LINKED_PPG_PULSE_LOSS.md`
- `046_RELEASE_PROVENANCE_BUNDLE.md`
- `047_ECG_MORPHOLOGY_RANDOMIZATION.md`
- `048_DYNAMIC_REPOLARIZATION_EPISODES.md`
- `049_PPG_OPTICAL_MULTICHANNEL.md`
- `050_DATABROWSER_GENERATION_SYNC.md`
- `051_UINT64_JSON_IDENTITY.md`
- `052_SAAS_INTEGRATION_CLOSURE.md`
- `055_UNIFIED_SUBMISSION_AND_DELINEATION_V2.md`
- `056_GENERIC_MEASUREMENT_SCORING.md`
- `057_WEARABLE_TIMEBASE_V2.md`
