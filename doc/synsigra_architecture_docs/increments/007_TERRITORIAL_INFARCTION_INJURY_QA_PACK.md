# Territorial Infarction and Injury QA Pack

**Document ID:** SYN-ARCH-INC-007

**Version:** 0.2

**Status:** Verified

**Owner role:** Biosignal Algorithms / Verification

**Date:** 2026-07-01

**Proposed traceability ID:** `TRC-ECG12-004`

**Implementation issue:** [signal_synth#19](https://github.com/tamask1s/signal_synth/issues/19)

## 1. Decision

Promote the complete PTB-XL/SCP myocardial infarction and subendocardial
injury condition family to deterministic parameterized engineering
phenotypes:

- infarction: `IMI`, `ASMI`, `ILMI`, `AMI`, `ALMI`, `LMI`, `IPLMI`, `IPMI`,
  and `PMI`;
- subendocardial injury: `INJAS`, `INJAL`, `INJIN`, `INJLA`, and `INJIL`.

Non-posterior infarction conditions use territorial pathological-Q evidence.
Posterior conditions use a reciprocal anterior R-wave proxy because the
standard 12-lead output has no V7-V9 posterior leads. Injury conditions use
territorial ST-depression evidence.

Every assertion shall be measured from the final sampled 12-lead waveform.
These conditions are synthetic stress-test labels, not diagnoses, disease
models, or evidence that an ECG alone establishes myocardial infarction.

## 2. Rationale and dependency order

`PRODUCT_DIRECTION.md` defines the clean-condition order as rhythm,
conduction, morphology, hypertrophy, infarction/injury, then ischemia/ST-T.
The first four families are implemented.

The current engine already provides the prerequisites:

- a separate septal/Q source and territorial Q-wave phenotype;
- a separate injury/ST source;
- deterministic standard 12-lead projection;
- measured Q amplitude, Q duration, R/S amplitude, and ST-J morphology;
- condition severity, strict composition checks, reports, export, and
  DataBrowser visualization.

This increment therefore extends the existing clean cardiac phantom before
population randomization or acquisition artifacts are introduced.

## 3. Requirements

- `REQ-GEN-001..003`, `REQ-GEN-005..006`;
- `REQ-ECG-001..003`, `REQ-ECG-006..007`;
- `REQ-SCN-001..004`, `REQ-SCN-006`;
- `REQ-GT-001`, `REQ-GT-004`;
- `REQ-API-001..003`;
- `REQ-NFR-001..005`, `REQ-NFR-008`;
- `REQ-VER-001`, `REQ-VER-003`, `REQ-VER-008..010`;
- `REQ-DOC-001..002`.

## 4. Public and versioning contract

No new condition enum or scenario JSON field is required. Existing condition
codes and their existing `(0, 1]` severity become executable with
`ecg_support_parameterized`.

The scenario schema remains version 2. The scenario engine version increases
from 4 to 5 because condition semantics and generated run fingerprints
change. Existing condition-free scenario fingerprints remain unchanged;
run fingerprints include the engine version.

The existing DataBrowser `GenerateECGQAScenario` export remains the public
adapter. No condition-specific forwarding API is added.

## 5. Territorial lead contract

The pack uses explicit internal lead masks:

| Territory | Evidence leads |
|---|---|
| inferior | II, III, aVF |
| septal | V1, V2 |
| anterior | V2, V3, V4 |
| anteroseptal | V1, V2, V3, V4 |
| lateral | I, aVL, V5, V6 |
| anterolateral | I, aVL, V3, V4, V5, V6 |
| inferolateral | II, III, aVF, I, aVL, V5, V6 |
| posterior reciprocal | V1, V2, V3 |

Condition mapping:

| Condition | Primary evidence |
|---|---|
| `IMI` | inferior Q |
| `ASMI` | anteroseptal Q |
| `AMI` | anterior Q |
| `LMI` | lateral Q |
| `ALMI` | anterolateral Q |
| `ILMI` | inferolateral Q |
| `PMI` | posterior reciprocal R |
| `IPMI` | inferior Q plus posterior reciprocal R |
| `IPLMI` | inferolateral Q plus posterior reciprocal R |
| `INJAS` | anteroseptal ST depression |
| `INJAL` | anterolateral ST depression |
| `INJIN` | inferior ST depression |
| `INJLA` | lateral ST depression |
| `INJIL` | inferolateral ST depression |

Combined territories are one explicit mask, not a textual union interpreted
at runtime.

## 6. Waveform compilation

### 6.1 Infarction-Q phenotype

The compiler shall:

- increase negative Q amplitude and Q duration monotonically with severity;
- orient and scale the septal source for the requested territory;
- retain the normal ventricular timeline and QRS duration;
- avoid directly painting individual leads.

### 6.2 Posterior reciprocal phenotype

The compiler shall:

- strengthen and orient the ventricular source toward the right precordial
  projection;
- retain a bounded terminal component;
- combine this source transform with inferior/inferolateral Q evidence for
  `IPMI` and `IPLMI`.

The report and assertion labels shall contain the word `proxy` so the output
cannot be mistaken for direct posterior-lead evidence.

### 6.3 Subendocardial-injury phenotype

The compiler shall:

- set a negative ST-J displacement that grows monotonically with severity;
- orient only the injury source toward the requested territory;
- leave QRS sources and the repolarization source unchanged.

This is a canonical ST-depression stress phenotype, not an acute coronary
syndrome model.

## 7. Measured assertions

Non-posterior infarction evidence:

- minimum target-lead Q amplitude at or below -0.10 mV;
- maximum target-lead Q duration at or above 0.030 s;
- at least two leads per required component whose beat-mean Q amplitude is at
  or below -0.10 mV and duration is at or above 0.020 s.

Posterior reciprocal evidence:

- maximum mean R amplitude of at least 1.0 mV in V1-V3;
- at least two V1-V3 leads whose mean R/S ratio is at least 1.0.

Subendocardial-injury evidence:

- minimum target-lead ST-J displacement at or below -0.05 mV;
- at least two leads per required component whose beat-mean ST-J displacement
  is at or below -0.03 mV.

Thresholds are generator-version QA rules. They shall not be documented or
reported as clinically validated diagnostic cutoffs.

## 8. Normalization and composition

- All infarction conditions imply `ABQRS`.
- Q-wave infarction conditions additionally imply `QWAVE`.
- Injury conditions do not imply `QWAVE`.
- One infarction/injury phenotype is accepted per scenario in v1.
- The pack does not compose with explicit `QWAVE`, `LVOLT`, `HVOLT`,
  hypertrophy phenotypes, PAC/PVC, AF/flutter/SVT, pacing, or complete bundle
  branch block.
- Existing sinus rate and HRV modifiers remain valid where the current
  timeline permits them.

Unsupported combinations fail validation before waveform generation.

## 9. Verification

Add `TEST-ECG-INFARCTION-001` as a focused suite:

- support level and fidelity-policy checks for all 14 conditions;
- source/timing direction and monotonic-severity checks;
- default and mild-severity measured assertion checks;
- exact assertion-code ownership and non-empty units/labels;
- effective-condition implications;
- pairwise family and clean-morphology conflict checks;
- deterministic repeated generation;
- representative 100 Hz and 1000 Hz cases;
- transactional rejection and output preservation.

All existing CTest, package smoke, CLI, ASan/UBSan, Linux, and Windows
verification shall remain green.

## 10. DataBrowser and SVN integration

Add:

- `069_ECG_Infarction_Phenotypes.txt`;
- `070_ECG_Injury_Phenotypes.txt`.

Both scripts shall call `GenerateECGQAScenario`, use API-owned labels, save
variables before `DisplayData`, and use `A2` or `N`. Core files and scripts
shall be synchronized into the SVN-managed DataBrowser working copy and
byte-compared.

The Windows application cannot be built in this environment. That limitation
remains separate from portable core verification.

## 11. Non-goals

- Clinical diagnosis, diagnostic accuracy, infarct age/stage, or acute-event
  classification.
- Biomarkers, symptoms, imaging, coronary anatomy, or patient physiology.
- Direct posterior V7-V9, right-sided V3R/V4R, or vectorcardiographic
  localization claims.
- Population-calibrated amplitudes or learned patient realism.
- Ischemia/ST-T family conditions outside the five injury statements.
- Composition of multiple infarction/injury statements in one v1 scenario.
- Absence of Q/ST evidence outside the target mask; v1 asserts required target
  evidence, not clinically exclusive localization.

## 12. Acceptance criteria

1. All 14 conditions validate and generate under parameterized fidelity.
2. Every default and mild phenotype passes measured final-waveform
   assertions.
3. Severity changes the intended Q, ventricular, or injury-source parameter
   monotonically.
4. Posterior conditions expose an explicitly labeled reciprocal proxy.
5. Unsupported compositions fail before generation.
6. Repeated generation is deterministic at engine version 5.
7. Both DataBrowser scripts use the existing generic API.
8. Git and SVN-managed copies are byte-identical.
9. Normal, sanitizer, Linux, and Windows verification pass.

## 13. Risks and controls

| Risk | Control |
|---|---|
| Condition name is read as a diagnosis | Fixed parameterized/non-diagnostic wording |
| Posterior output is mistaken for direct evidence | Explicit reciprocal-proxy assertion and documentation |
| Assertion repeats construction config | Measure final per-beat, per-lead samples |
| Combined territory silently narrows | Explicit lead masks and lead-count assertions |
| Dipole projection creates off-target evidence | Claim target presence, not localization exclusivity |
| Source transforms overwrite each other | Reject unsupported composition |
| Changed semantics retain old run identity | Increment engine version |

## 14. Source basis

- PTB-XL 1.0.3 `scp_statements.csv`, CC BY 4.0, supplies the condition names,
  classes, and statement taxonomy.
- AHA/ACCF/HRS ECG standardization Part VI and the Universal Definition of
  Myocardial Infarction motivate separating Q-wave, ST-T, and contiguous-lead
  evidence. No source code, waveform, patient data, or threshold table is
  copied into this implementation.

## 15. Implementation sequence

1. Open and link the implementation issue.
2. Add territorial masks, transforms, assertions, and validation.
3. Add the focused test suite and tune only against measured synthetic output.
4. Add and synchronize DataBrowser scripts and core files.
5. Run local release and sanitizer verification.
6. Commit, push, verify Linux/Windows CI, update traceability, and close issue.

## 16. Change log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-07-01 | Initial proposed design |
| 0.2 | 2026-07-01 | Accepted implementation `c4c3204`; Linux and Windows verification passed in CI run `28516223985` |
