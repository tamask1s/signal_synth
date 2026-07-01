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

Traceability demonstration:

- `17_TRACEABILITY_SOP.md` defines issue, commit, test, CI-evidence, review,
  and release-record rules.
- `18_TRACEABILITY_MATRIX.md` links implemented requirements to design,
  source, verification procedures, and GitHub records.
- These records demonstrate MDR-style navigation but do not claim MDR
  compliance or clinical validation.

Primary immediate implementation targets:

- normalize repository structure;
- keep core library UI/SaaS independent;
- implement scenario JSON validation and fingerprinting;
- add CLI validate/render/report;
- add deterministic examples and golden tests;
- make PPG a first-class next module.
