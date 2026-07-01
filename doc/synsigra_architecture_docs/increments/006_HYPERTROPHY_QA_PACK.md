# ECG Hypertrophy QA Pack

**Document ID:** SYN-ARCH-INC-006

**Version:** 0.2

**Status:** Verified

**Owner role:** Biosignal Algorithms / Verification

**Date:** 2026-07-01

**Proposed traceability ID:** `TRC-ECG12-003`

**Implementation issue:** [signal_synth#18](https://github.com/tamask1s/signal_synth/issues/18)

## 1. Decision

Promote six cataloged PTB-XL/SCP hypertrophy and overload statements to
deterministic parameterized engineering phenotypes:

- `LVH`: left ventricular hypertrophy;
- `RVH`: right ventricular hypertrophy;
- `SEHYP`: septal hypertrophy;
- `VCLVH`: voltage criteria for left ventricular hypertrophy;
- `LAO/LAE`: left atrial overload or enlargement;
- `RAO/RAE`: right atrial overload or enlargement.

Each condition shall compile to an explicit multi-source ECG configuration and
shall be verified against morphology measured from the final sampled 12-lead
waveform. These are synthetic stress-test phenotypes, not diagnostic
classifiers and not patient-equivalent disease models.

## 2. Requirements

- `REQ-GEN-001..003`, `REQ-GEN-005..006`;
- `REQ-ECG-001..003`, `REQ-ECG-006..007`;
- `REQ-SCN-001..004`, `REQ-SCN-006`;
- `REQ-GT-001`, `REQ-GT-004`;
- `REQ-API-001..003`;
- `REQ-NFR-001..005`, `REQ-NFR-008`;
- `REQ-VER-001`, `REQ-VER-003`, `REQ-VER-008..010`;
- `REQ-DOC-001..002`.

## 3. Public contract

No new condition enum or scenario JSON field is required. Existing condition
codes and their `severity` values become executable with
`ecg_support_parameterized`.

Severity remains in `(0, 1]` and monotonically increases the source or timing
change. The scenario schema remains version 2. The scenario engine version
increases from 3 to 4 because generation semantics and run fingerprints
change.

The existing DataBrowser `GenerateECGQAScenario` API is the single public
adapter for all six conditions. A dedicated exported wrapper would duplicate
the scenario API without adding a contract.

## 4. Phenotype compilation

| Condition | Primary source/timing changes | Measured assertion |
|---|---|---|
| `LVH` | stronger leftward ventricular source | lateral/precordial QRS voltage |
| `VCLVH` | stronger ventricular and septal voltage | maximum QRS voltage |
| `RVH` | stronger rightward ventricular source | V1 R/S balance |
| `SEHYP` | stronger anterior septal source | septal QRS voltage |
| `LAO/LAE` | longer, leftward atrial activation | P duration |
| `RAO/RAE` | stronger, right-inferior atrial activation | inferior P amplitude |

Assertions use `ecg_morphology_report`, which measures each generated beat and
lead from final samples. Configuration values are construction truth;
assertion values are measured waveform evidence. Thresholds are fixed
canonical QA rules for this generator version, not clinical diagnostic
criteria.

## 5. Condition normalization and composition

Ventricular and septal conditions imply the form statement `ABQRS`. Atrial
overload conditions do not imply `ABQRS`.

For this increment:

- only one of the six hypertrophy phenotypes may be requested at a time;
- hypertrophy phenotypes do not compose with `QWAVE`, `LVOLT`, or `HVOLT`;
- clean hypertrophy phenotypes do not compose with AF, flutter, SVT, pacing,
  PAC/PVC, or complete bundle branch block models;
- sinus rate and HRV modifiers remain available where their existing
  validation permits them.

Unsupported combinations fail validation rather than silently overwriting
source parameters.

## 6. Verification

Extend `TEST-ECG-SCENARIO-001` with:

- support-level checks for all six conditions;
- compile-time source/timing direction checks;
- monotonic severity checks;
- generated measured-phenotype assertions;
- inferred `ABQRS` checks;
- conflict and fidelity-policy checks;
- deterministic repeated-generation checks;
- representative 100 Hz and 1000 Hz assertion checks.

All existing suites, package smoke tests, ASan/UBSan tests, and Linux/Windows
CI shall continue to pass.

## 7. DataBrowser and SVN integration

Add `068_ECG_Hypertrophy_Phenotypes.txt` to the Git audit mirror and SigForge
Scripts. It shall render all six conditions through
`GenerateECGQAScenario`, use API-owned channel labels, save variables before
`DisplayData`, and use `A2` or `N`.

Synchronize changed `ecg_scenario.h/.cpp` into the SVN-managed
SignalProcessors directory. The Windows application cannot be built in this
environment, so DataBrowser execution remains manual verification evidence.

## 8. Non-goals

- Clinical sensitivity, specificity, prevalence, or diagnostic validation.
- Anatomically calibrated ventricular mass or atrial volume.
- Patient-specific torso geometry or electrode placement.
- Strain, repolarization, ischemia, infarction, or mixed hypertrophy models.
- Automatic diagnosis from an arbitrary input ECG.

## 9. Acceptance criteria

1. All six conditions validate and generate under parameterized fidelity.
2. Every generated condition passes at least one measured final-waveform
   assertion at default severity.
3. Severity changes the intended source/timing parameter monotonically.
4. Unsupported compositions are rejected before generation.
5. Repeated generation remains deterministic and fingerprints include the
   changed engine version.
6. DataBrowser can visualize all six conditions through the existing API.
7. Core files are synchronized and all automated verification passes.

## 10. Risks and limitations

| Risk | Control |
|---|---|
| Clinical wording is overinterpreted | Fixed parameterized/non-diagnostic wording |
| Assertion merely repeats input config | Measure the final sampled leads |
| Lead projection defeats intended phenotype | Test actual lead morphology at multiple rates |
| Source transforms overwrite each other | Reject unsupported compositions |
| Existing run identity hides changed semantics | Increment engine version |
| Threshold appears clinically validated | Identify it as a versioned canonical QA rule |

## 11. Implementation sequence

1. Open and link the implementation issue.
2. Add catalog support, source transforms, assertions, and validation.
3. Add automated tests and tune only against measured synthetic output.
4. Add and synchronize the DataBrowser visualization script and core files.
5. Run local normal/sanitizer verification and cross-platform CI.
6. Update traceability and mark this record verified after CI passes.

## 12. Change log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-07-01 | Initial proposed design |
| 0.2 | 2026-07-01 | Accepted implementation `f38f90d`; Linux and Windows verification passed in CI run `28513074237` |
