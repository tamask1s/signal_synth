# Synsigra Documentation Index

Recommended read order:

1. `01_SRS.md`
2. `02_DOCUMENTATION_PLAN.md`
3. `03_SYSTEM_ARCHITECTURE.md`
4. `04_REPOSITORY_STRUCTURE.md`
5. `05_CORE_LIBRARY_ARCHITECTURE.md`
6. `06_ALGORITHM_DESIGN.md`
7. `07_SCENARIO_AND_DATA_MODEL.md`
8. `08_API_DESIGN.md`
9. `09_SAAS_ARCHITECTURE.md`
10. `10_EXPORT_AND_REPORTING.md`
11. `11_VERIFICATION_AND_VALIDATION_PLAN.md`
12. `12_SECURITY_PRIVACY_AND_LICENSE.md`
13. `13_HARDWARE_ROADMAP.md`
14. `14_MDR_READINESS_AND_QUALITY.md`
15. `15_IMPLEMENTATION_ROADMAP.md`
16. `16_REPO_REVIEW.md`
17. `17_TRACEABILITY_SOP.md`
18. `18_TRACEABILITY_MATRIX.md`

Focused SRS documents:

- `srs/README.md` explains the focused SRS set and the offline-first B2B
  challenge model.
- `srs/001_OFFLINE_CHALLENGE_AND_PYTHON_SCORING_SRS.md` defines challenge
  packages, the local Python scoring workflow, and report boundaries.
- `srs/002_FORMATS_AND_IO_CONTRACTS_SRS.md` defines WFDB, EDF+/BDF+,
  ground-truth, and CSV/JSON detection contracts.
- `srs/003_HRV_FOUNDATION_AND_SCORING_SRS.md` defines HRV scenario, metric,
  ground-truth, and scoring requirements.
- `srs/004_ECG_FOUNDATION_FEATURES_SRS.md` defines ECG foundation feature gaps
  required before SaaS buildout.
- `srs/005_PPG_FOUNDATION_FEATURES_SRS.md` defines PPG foundation feature gaps
  required before wearable QA/SaaS buildout.
- `srs/006_IMPLEMENTATION_ISSUE_MAP.md` maps focused SRS requirement groups
  to GitHub implementation issues and recommended order.

Implementation increment designs:

- `increments/README.md` defines the required architecture-record content and
  lifecycle for each implementation increment.
- `increments/001_PORTABLE_BUILD_BASELINE.md` proposes the next increment:
  one root, installable C++ library build exercised by all verification suites.
- `increments/002_VERSIONED_SCENARIO_JSON.md` defines the strict portable
  scenario document, canonicalization, and fingerprint contract.
- `increments/003_SCENARIO_CLI_VALIDATE_FINGERPRINT.md` defines the first
  product-facing validate/fingerprint executable.
- `increments/004_RENDER_EXPORT_REPORT.md` defines deterministic render,
  export artifacts, and the self-contained HTML evidence report.
- `increments/005_SHARED_TIMELINE_PPG.md` defines schema-v2 PPG generated from
  the exact ECG ventricular timeline.
- `increments/006_HYPERTROPHY_QA_PACK.md` defines six measured multi-source
  hypertrophy and atrial-overload engineering phenotypes.
- `increments/007_TERRITORIAL_INFARCTION_INJURY_QA_PACK.md` defines the
  territorial Q-wave, posterior reciprocal, and injury-ST phenotype family.
- `increments/008_ISCHEMIA_STT_REPOLARIZATION_QA_PACK.md` defines the
  complete ST-T/repolarization condition family and broad-statement proxies.
- `increments/009_CONTINUOUS_STT_RENDERING.md` corrects discontinuous
  terminal-QRS, ST, and T source transitions.
- `increments/010_DATABROWSER_FIXED_LABEL_SAFETY.md` corrects fixed-label
  buffer overflow in the Windows DataBrowser adapter.
- `increments/011_CATALOG_MORPHOLOGY_QUALITY.md` records the catalog-wide ECG
  morphology quality correction.
- `increments/012_ADVANCED_CONDUCTION_PHENOTYPES.md` defines advanced
  conduction phenotype coverage.
- `increments/013_EPISODE_TIMELINE_SVARR_PSVT.md` defines reproducible
  PSVT/SVARR episode timelines.
- `increments/014_DATABROWSER_ZAX_GCC49_COMPATIBILITY.md` records the
  DataBrowser ZAX/GCC 4.9 adapter compatibility correction.
- `increments/015_ACQUISITION_ARTIFACT_SIGNAL_QUALITY_PACK.md` defines the
  acquisition artifact and signal-quality pack.
- `increments/016_SCENARIO_PACK_BATCH_QA.md` defines scenario-pack manifests,
  batch CLI rendering, and curated QA packs.
- `increments/017_ALGORITHM_COMPARISON_SCORING.md` defines event-detector
  comparison against synthetic ECG/PPG ground truth.
- `increments/018_STABLE_CPP_FACADE.md` defines the stable C++ facade for
  validation, render/export, and comparison.
- `increments/019_CHALLENGE_PACKAGE_MANIFEST.md` defines the offline challenge
  package manifest contract.
- `increments/020_DETECTION_OUTPUT_CONTRACTS.md` defines versioned CSV/JSON
  user detection output contracts.

Traceability demonstration:

- `17_TRACEABILITY_SOP.md` defines issue, commit, test, CI-evidence, review,
  and release-record rules.
- `18_TRACEABILITY_MATRIX.md` links implemented requirements to design,
  source, verification procedures, and GitHub records.
- These records demonstrate MDR-style navigation but do not claim MDR
  compliance or clinical validation.

Primary immediate implementation targets:

- keep core library UI/SaaS independent;
- stabilize challenge package, standard export, and local Python scoring
  contracts before hosted SaaS service work;
- implement WFDB and EDF+/BDF+ exports;
- add CSV/JSON user detection contracts;
- implement HRV foundation and scoring;
- close ECG and PPG foundation feature gaps required for robust algorithm QA.
