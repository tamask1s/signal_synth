# Ischemia, ST-T, and Repolarization QA Pack

**Document ID:** SYN-ARCH-INC-008

**Version:** 0.3

**Status:** Verified

**Owner role:** Biosignal Algorithms / Verification

**Date:** 2026-07-01

**Proposed traceability ID:** `TRC-ECG12-005`

**Implementation issue:** [signal_synth#20](https://github.com/tamask1s/signal_synth/issues/20)

## 1. Decision

Complete the 19-statement PTB-XL/SCP ischemia and repolarization family.
`LNGQT` remains the existing parameterized QT phenotype and regression case.
The following 18 catalog-only conditions become deterministic parameterized
engineering phenotypes:

- territorial ischemia: `ISC_`, `ISCAL`, `ISCIN`, `ISCIL`, `ISCAS`, `ISCLA`,
  and `ISCAN`;
- ST forms: `NST_`, `STD_`, and `STE_`;
- T forms: `NDT`, `LOWT`, `NT_`, `INVT`, and `TAB_`;
- canonical broad-statement proxies: `DIG`, `ANEUR`, and `EL`.

Assertions shall be measured from the final sampled 12-lead waveform at the J
point, J+60 ms, and T-wave interval. The pack supplies controlled waveform
stress cases, not diagnoses, etiological coverage, or clinical validation.

## 2. Rationale and dependency order

`PRODUCT_DIRECTION.md` places ischemia/ST-T after rhythm, conduction,
morphology, hypertrophy, and infarction/injury. Those families are now
implemented.

The current engine has the required prerequisites:

- separate injury and repolarization sources;
- configurable ST-J amplitude and ST slope;
- configurable T amplitude, duration, axis, and source orientation;
- explicit territorial lead masks from the infarction/injury pack;
- measured ST-J, ST-J60, T amplitude, and T duration per beat and lead;
- deterministic scenario validation, export, report, and DataBrowser adapter.

No acquisition noise or population randomization is mixed into this clean
condition increment.

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
codes and `(0, 1]` severity values become executable with
`ecg_support_parameterized`.

The scenario schema remains version 2. The scenario engine version increases
from 5 to 6 because waveform semantics and run fingerprints change.

The existing DataBrowser `GenerateECGQAScenario` function remains the public
adapter. No forwarding wrapper is added.

## 5. Territorial contract

| Condition | Required evidence mask |
|---|---|
| `ISC_` | widespread: I, II, aVL, aVF, V3-V6 |
| `ISCAS` | anteroseptal: V1-V4 |
| `ISCAN` | anterior: V2-V4 |
| `ISCLA` | lateral: I, aVL, V5, V6 |
| `ISCAL` | anterior and lateral components |
| `ISCIN` | inferior: II, III, aVF |
| `ISCIL` | inferior and lateral components |

Territorial ischemia v1 uses ST depression plus T-wave polarity change in
the target mask. Combined territories require evidence in both components.
The assertion guarantees target evidence, not absence of changes outside the
mask.

## 6. Waveform contracts

### 6.1 Territorial ischemia

For `ISC_` and the six named territories:

- negative ST-J displacement grows monotonically with severity;
- the injury source is oriented to the target mask;
- T polarity is changed through the separate repolarization source;
- QRS construction remains unchanged.

### 6.2 Explicit ST forms

- `STD_`: widespread horizontal ST-depression stress form;
- `STE_`: widespread ST-elevation stress form;
- `NST_`: milder non-specific ST-depression/slope stress form.

### 6.3 Explicit T forms

- `LOWT`: globally reduced T amplitude;
- `INVT`: inverted-T stress form;
- `NDT`: mild T-amplitude and T-axis perturbation;
- `NT_`: stronger non-specific T-axis perturbation;
- `TAB_`: mixed-polarity T-wave stress form.

The simple dipole model cannot produce arbitrary biphasic or notched T
morphology. The pack asserts amplitude and polarity behavior only.

### 6.4 Broad-statement proxies

- `DIG`: downsloping ST-depression proxy;
- `ANEUR`: persistent anterior ST elevation plus anterior Q-wave proxy;
- `EL`: tall, shortened-T electrolyte/drug-effect proxy.

These are intentionally one canonical form per broad statement. `EL` does not
represent every electrolyte or drug effect, `DIG` is not a pharmacological
model, and `ANEUR` does not establish ventricular aneurysm.

## 7. Measured assertion contract

ST evidence uses beat-mean lead values:

- separate J-point extreme;
- lead count at the configured sign and QA magnitude;
- J+60 minus J slope evidence for `DIG` and `NST_`.

T evidence uses:

- target-lead T amplitude extreme;
- positive and negative T lead counts;
- maximum absolute T amplitude;
- maximum measured T duration for `EL`.

Territorial ischemia requires at least two ST and two T leads in each required
component; the widespread `ISC_` form requires three. Explicit global forms
use a widespread mask and require multiple matching leads. Lead-count
magnitude gates scale with requested severity while sign, component, and
extreme-value requirements remain explicit.

Thresholds are generator-version QA gates. They are not copied clinical
diagnostic cutoffs.

## 8. Normalization and composition

- territorial ischemia implies `STD_` and `INVT`;
- `DIG` implies `STD_`;
- `ANEUR` implies `STE_`, `QWAVE`, and `ABQRS`;
- `EL` implies `TAB_`;
- one ST-T/repolarization statement, including `LNGQT`, is accepted per v1
  scenario;
- the pack does not compose with Q/voltage morphology, hypertrophy,
  infarction/injury, PAC/PVC, AF/flutter/SVT, pacing, or complete bundle
  branch block;
- sinus rate and HRV modifiers remain available where current validation
  permits them.

Unsupported combinations fail before generation.

## 9. Verification

Add `TEST-ECG-REPOLARIZATION-001`:

- support-level and fidelity checks for all 19 family conditions;
- default and mild measured phenotype assertions for all 18 new forms;
- `LNGQT` regression;
- compile-time source/timing direction and severity monotonicity;
- distinct source configurations for adjacent descriptive statements;
- combined-territory component coverage;
- normalization and conflict checks;
- JSON roundtrip;
- deterministic repeated generation;
- transactional output preservation;
- representative 100 Hz and 1000 Hz cases.

All existing CTest, package smoke, CLI, ASan/UBSan, Linux, and Windows
verification shall remain green.

## 10. DataBrowser and SVN integration

Add:

- `071_ECG_STT_Phenotypes.txt`;
- `072_ECG_Ischemia_Phenotypes.txt`.

Both scripts shall call `GenerateECGQAScenario`, use API-owned labels, save
variables before `DisplayData`, and use `A2` or `N`. Core files and scripts
shall be synchronized into the SVN-managed DataBrowser working copy and
byte-compared.

The Windows application cannot be built in this environment. That limitation
remains separate from portable core verification.

## 11. Non-goals

- Clinical diagnosis, etiological inference, or acute coronary syndrome
  classification.
- Complete digitalis, electrolyte, drug, or ventricular-aneurysm physiology.
- U-wave generation, biphasic/notched T-wave primitives, or QT dispersion.
- Population-calibrated amplitudes, patient-specific torso models, or learned
  realism.
- Localization exclusivity outside the target lead mask.
- Composition of multiple ST-T family statements in one v1 scenario.

## 12. Acceptance criteria

1. All 19 family conditions are executable under parameterized fidelity.
2. Every new default and mild phenotype passes final-waveform assertions.
3. `LNGQT` behavior remains covered and passing.
4. Severity changes the intended ST, T, source, or timing parameter
   monotonically.
5. Combined territory evidence is present in both components.
6. Broad proxies are explicitly labeled and documented.
7. Unsupported compositions fail before generation.
8. Repeated generation is deterministic at engine version 6.
9. Both DataBrowser scripts use the existing generic API.
10. Git/SVN copies and all automated verification are green.

## 13. Risks and controls

| Risk | Control |
|---|---|
| Ischemia label is read as diagnosis | Fixed parameterized/non-diagnostic warning |
| Broad etiologic statement is overgeneralized | Name one canonical proxy and document exclusions |
| Assertion repeats input config | Measure final per-beat, per-lead samples |
| Combined territory narrows silently | Separate component lead-count assertions |
| Dipole creates off-target changes | Claim target presence, not exclusivity |
| ST baseline or slope is ambiguous | Report J, J+60, sign, units, and label |
| Changed semantics retain old run identity | Increment engine version |

## 14. Source basis

- PTB-XL 1.0.3 `scp_statements.csv`, CC BY 4.0, supplies condition names,
  classes, and statement taxonomy.
- AHA/ACCF/HRS ECG standardization Part IV motivates separate ST-J, ST slope,
  T-wave, and QT measurement.
- AHA/ACCF/HRS Part VI motivates contiguous territorial lead evidence and
  separating ST depression/elevation, T polarity, and QRS evidence.

No source code, waveform, patient data, or clinical threshold table is copied
into this implementation.

## 15. Implementation sequence

1. Open and link the implementation issue.
2. Add condition support, transforms, assertions, and strict validation.
3. Add the focused test suite and tune against measured synthetic output.
4. Add JSON regression and DataBrowser scripts.
5. Synchronize core/scripts and run release plus sanitizer verification.
6. Commit, push, verify Linux/Windows CI, update traceability, and close issue.

## 16. Verification record

- implementation commit:
  [`cd6ab95`](https://github.com/tamask1s/signal_synth/commit/cd6ab959f9c53c3c80b116a3ab5fe04c6dc7a28e);
- `CI-VER-001`:
  [GitHub Actions run 28518794062](https://github.com/tamask1s/signal_synth/actions/runs/28518794062),
  successful on Ubuntu and Windows C++11;
- local release verification: 12/12 CTest suites passed;
- local ASan/UBSan verification: 11/11 applicable suites passed with package
  smoke excluded;
- `ecg_scenario.h`, `ecg_scenario.cpp`, and DataBrowser scripts 071/072 were
  synchronized to the SVN-managed working copy and verified byte-identical.

The Windows DataBrowser application was not compiled in this environment.
That manual integration evidence remains outside `CI-VER-001`.

## 17. Change log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-07-01 | Initial proposed design |
| 0.2 | 2026-07-01 | Implemented engine v6, measured assertions, tests, and DataBrowser packs |
| 0.3 | 2026-07-01 | Recorded release, sanitizer, synchronization, and cross-platform CI evidence |
