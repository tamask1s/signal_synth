# Advanced Conduction Phenotype Pack

**Document ID:** SYN-ARCH-INC-012

**Version:** 0.3

**Status:** Verified

**Owner role:** Biosignal Algorithms / Verification

**Date:** 2026-07-02

**Proposed traceability ID:** `TRC-ECG12-008`

**Implementation issue:** [signal_synth#24](https://github.com/tamask1s/signal_synth/issues/24)

## 1. Decision

Implement the remaining specific clean conduction conditions before adding
population randomization, acquisition artifacts, or learned realism:

- `LAFB`;
- `LPFB`;
- `IRBBB`;
- `ILBBB`;
- `IVCD`;
- `WPW`.

This closes the largest coherent gap in the 71-statement catalog and builds
directly on the verified complete-BBB lead morphology. `ABQRS` remains a
derived broad form statement, while `SVARR` and `PSVT` belong to the following
episode-timeline increment.

## 2. Product rationale

The product pipeline requires a stable clean latent/reference layer before
correlated variation and acquisition corruption are introduced. Randomizing
or adding noise to incomplete conduction proxies would make later validation
packs more varied without making their ground truth more trustworthy.

Completing this pack leaves only broad/derived `ABQRS` and episode-level
`SVARR`/`PSVT` unsupported. The next dependency order is therefore:

1. advanced conduction phenotypes;
2. episode transitions and `SVARR`/`PSVT`;
3. correlated subject/record/beat variation;
4. acquisition noise and artifacts with exact masks;
5. curated detector stress packs and comparison reports.

## 3. Model boundaries

### 3.1 Intraventricular conduction

Extend the internal conduction mode so incomplete BBB, fascicular block, and
nonspecific IVCD are explicit states rather than arbitrary axis offsets.
Complete and incomplete bundle blocks may share source-orientation logic but
must have independent timing ranges.

### 3.2 Pre-excitation

WPW shall not be encoded as a bundle branch block. Add a separate
pre-excitation configuration and smooth delta-wave activation path with:

- shortened PR;
- early ventricular onset;
- slow initial QRS upstroke;
- widened total QRS;
- secondary repolarization change;
- explicit construction and measured delta-wave evidence.

Pre-excitation is rendered as a separately asserted smooth component of the
existing septal source in this increment. The public source count remains
seven so existing DataBrowser source-channel contracts do not change.

## 4. Lead-domain phenotype contracts

| Condition | Timing | Required lead evidence |
|---|---|---|
| `IRBBB` | QRS 100-119 ms | terminal R/R-prime in V1/V2; terminal S in I/V6; right-precordial secondary T discordance |
| `ILBBB` | QRS 110-119 ms | LBBB-like negative V1 and positive lateral terminal activation without complete-LBBB duration |
| `LAFB` | QRS below 120 ms | left frontal axis; qR in I/aVL; rS in II/III/aVF |
| `LPFB` | QRS below 120 ms | right frontal axis; rS in I/aVL; qR in III/aVF |
| `IVCD` | widened QRS | does not satisfy the complete RBBB or LBBB terminal lead contracts |
| `WPW` | PR below 120 ms and widened QRS | sampled delta upstroke plus secondary ST-T behavior |

Assertions shall use final sampled lead morphology. Construction timing or a
hidden source direction is supporting evidence, not a passing phenotype by
itself.

## 5. Scenario and compatibility rules

- Preserve existing condition codes and schema-v2 representation.
- Severity controls timing delay and morphology strength monotonically within
  condition-specific bounded ranges.
- Reject simultaneous mutually exclusive conduction modes.
- Initially reject composition with infarction, hypertrophy, ischemia/ST-T,
  ventricular ectopy, pacing, and other complex rhythms unless a dedicated
  composition contract exists.
- Increment scenario engine identity when waveform semantics change.
- Do not claim clinical diagnostic fidelity; these remain measurable
  engineering-testbench phenotypes.

## 6. Verification

Create `TEST-ECG-CONDUCTION-001` covering:

- default and mild severity for every condition;
- 100, 500, and 1000 Hz;
- construction timing and sampled lead-domain assertions;
- monotonic severity;
- mutual-exclusion and unsupported-composition errors;
- deterministic repeat generation and fingerprint change;
- exact lead identities, source summation, finite output, amplitude bounds,
  fiducial ordering, and non-QRS continuity;
- negative controls proving IVCD does not accidentally satisfy complete RBBB
  or LBBB contracts;
- extension of `TEST-ECG-MORPH-QUALITY-001` to every newly executable catalog
  condition.

Extend the catalog-wide morphology gate and add one DataBrowser script showing
all six cases in separate displays through the existing `GenerateECGQAScenario`
API. Annotation mode remains selectable through that API's existing
`annotation_output` parameter.

## 7. DataBrowser integration

- Add condition support to the portable core first.
- Keep DataBrowser adapter logic limited to ZAX parsing, channel metadata, and
  display/export integration.
- Synchronize changed portable files byte-for-byte to the SVN working copy.
- Add the visual script to both the Git example directory and DataBrowser
  `SigForge/Scripts`.
- The Windows application remains a manual integration gate in this
  environment.

## 8. Risks and controls

| Risk | Control |
|---|---|
| Axis rotation alone creates mislabeled fascicular blocks | Assert qR/rS lead patterns as well as frontal axis |
| Incomplete BBB is indistinguishable from complete BBB | Independent measured QRS-duration ranges |
| IVCD becomes a generic wide copy of BBB | Explicit negative RBBB/LBBB assertions |
| WPW is squeezed into an unrelated conduction mode | Separate pre-excitation state and delta evidence |
| New source breaks DataBrowser channel contracts | Resolve source-count decision before API acceptance |
| Visual plausibility hides assertion failure | Final sampled lead metrics are release gates |

## 9. Non-goals

- Accessory-path localization variants.
- Bifascicular or alternating bundle branch block.
- Mechanistic Purkinje-network simulation.
- Population prevalence or clinical sensitivity/specificity claims.
- Noise, lead faults, artifacts, or random patient generation.

## 10. Exit criteria

1. All six conditions are executable with explicit support level and strict
   conflicts.
2. Every condition passes its sampled lead-domain phenotype assertions at all
   required rates and severities.
3. The catalog-wide morphology quality gate includes the new cases.
4. DataBrowser APIs and the visual script expose all six conditions.
5. Portable release, sanitizer, package-smoke, Ubuntu, and Windows C++11
   verification pass.
6. Git and SVN working copies are synchronized and recorded.

## 10.1 Verification evidence

Implementation commit:
`79ef201033da70f4c1c85e9b6fba21750a8f62d5`.

Automated local evidence:

- Release CTest: 14/14 passed, including `TEST-ECG-CONDUCTION-001`.
- ASan/UBSan CTest: 13/13 passed with `ASAN_OPTIONS=detect_leaks=0` and
  `TEST-BUILD-001` excluded.

CI evidence:

- GitHub Actions `CI-VER-001`, run
  <https://github.com/tamask1s/signal_synth/actions/runs/28590301188>,
  passed on Ubuntu and Windows C++11.

DataBrowser/SVN sync evidence:

- `src/clinical_ecg.h`, `src/clinical_ecg.cpp`, `src/ecg_scenario.h`, and
  `src/ecg_scenario.cpp` were copied byte-for-byte to the SVN DataBrowser
  `SignalProcessors` working copy and verified with `cmp`.
- `examples/databrowser/073_ECG_Advanced_Conduction_Phenotypes.txt` was
  copied byte-for-byte to the SVN DataBrowser `SigForge/Scripts` working copy
  and verified with `cmp`.
- SVN command-line status is not available in this Linux environment because
  `svn` is not installed.

## 11. Source basis to review before acceptance

- AHA/ACCF/HRS recommendations, Part III, for intraventricular conduction
  terminology and criteria:
  <https://www.jacc.org/doi/10.1016/j.jacc.2008.12.013>.
- Primary literature and consensus criteria for contemporary LBBB and
  pre-excitation morphology shall be added during issue refinement.

No external waveform data or implementation code will be copied.

## 12. Proposed implementation sequence

1. Resolve public source-count and WPW annotation design.
2. Add failing condition-specific lead-domain tests.
3. Implement incomplete BBB using the verified complete-BBB primitives.
4. Implement fascicular modes and nonspecific IVCD with negative controls.
5. Implement the independent pre-excitation path.
6. Add scenario assertions, conflicts, API/script integration, and engine
   versioning.
7. Run full verification, synchronize DataBrowser, and record CI evidence.

## 13. Change log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-07-02 | Proposed dependency-ordered advanced conduction pack |
| 0.2 | 2026-07-02 | Accepted implementation issue, fixed WPW source-count decision, and moved to implementation |
| 0.3 | 2026-07-02 | Recorded implementation, verification, CI, and DataBrowser sync evidence |
