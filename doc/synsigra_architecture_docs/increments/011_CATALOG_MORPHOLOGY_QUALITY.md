# Catalog-wide ECG Morphology Quality Correction

**Document ID:** SYN-ARCH-INC-011

**Version:** 0.2

**Status:** Implementing

**Owner role:** Biosignal Algorithms / Verification

**Date:** 2026-07-02

**Traceability ID:** `TRC-ECG12-007`

**Implementation issue:** [signal_synth#23](https://github.com/tamask1s/signal_synth/issues/23)

## 1. Decision

Establish a catalog-wide waveform quality gate for every executable ECG
condition and correct the lead-domain morphology defects found in complete
right bundle branch block, complete left bundle branch block, and typical
atrial flutter.

The gate verifies final sampled output rather than accepting construction
parameters or hidden source values as sufficient evidence.

## 2. Audit result and rationale

A deterministic 500 Hz, 12-second record was generated for every executable
condition, both second-degree AV-block variants, all Q-wave territory variants,
and PAC/PVC bigeminy and trigeminy compositions. All 66 cases generated,
remained finite, preserved exact limb-lead identities, had ordered fiducials,
and passed their existing phenotype assertions.

Visual 12-lead beat atlases and ten-second lead-II rhythm strips then found
three lead-domain defects that those checks did not detect:

| Model | Observed defect | Required correction |
|---|---|---|
| CRBBB | dominant negative V1 QRS; positive terminal I/V6 force | terminal R/R-prime in V1 and terminal S in I/V6 |
| CLBBB | residual lateral q and terminal S; concordant T | broad positive lateral terminal activation, no material lateral q, secondary T discordance |
| AFLT | short positive P-like waves separated by an isoelectric interval | continuous asymmetric F-wave train, predominantly negative in inferior leads |

The normal/timing, AV conduction, AF, SVT, ectopy, pacing, voltage,
hypertrophy, atrial-overload, infarction/injury, ischemia/ST-T, and
repolarization cases had no level jumps, clipped half-waves, non-finite
samples, invalid lead identities, or unsupported marker ordering in this
audit. Several remain explicit engineering proxies and are not diagnostic
representations.

## 3. Requirements

- `REQ-GEN-001..003`;
- `REQ-ECG-001..003`, `REQ-ECG-006..007`;
- `REQ-GT-001`;
- `REQ-NFR-001..005`, `REQ-NFR-008`;
- `REQ-VER-001`, `REQ-VER-003`, `REQ-VER-008..010`;
- `REQ-DOC-001..002`.

## 4. Rendering contracts

### 4.1 Complete RBBB

- construction QRS duration is at least 120 ms;
- initial activation retains the ordinary left-ventricular component;
- delayed terminal activation projects positively in V1 and negatively in I
  and V6;
- V1 therefore has an rS followed by terminal R/R-prime pattern;
- the existing right-precordial negative T and lateral positive T become
  secondary repolarization discordance relative to the corrected terminal
  QRS force.

### 4.2 Complete LBBB

- construction QRS duration is at least 120 ms;
- septal activation does not create a material lateral q wave;
- delayed leftward activation remains negative in V1 and positive through
  I/V5/V6, including the terminal QRS component;
- repolarization polarity is reversed relative to the ordinary T vector so
  representative right-precordial and lateral leads are discordant to their
  dominant QRS direction.

The compact source primitives remain continuous and zero-valued with zero
first derivative at their boundaries. No step-like QRS correction is allowed.

### 4.3 Typical atrial flutter

Each atrial cycle is one continuous asymmetric compact F wave:

- adjacent F-wave offset and onset times coincide;
- the slow deflection occupies 73% of the atrial cycle and the return 27%;
- the source returns smoothly to zero only at the cycle boundary, leaving no
  finite isoelectric interval;
- the scenario default projects the dominant deflection negatively in II,
  III, and aVF;
- conducted beats retain the configured fixed AV ratio and their reciprocal
  atrial/ventricular indexes.

## 5. Verification design

Add `TEST-ECG-MORPH-QUALITY-001` as a separate catalog-wide executable. It
shall cover:

- all executable catalog conditions;
- Mobitz I and II;
- inferior, anterior, and lateral generic Q-wave variants;
- PAC/PVC bigeminy and trigeminy;
- successful generation and all phenotype assertions;
- finite samples, plausible global amplitude, exact Einthoven/Goldberger
  identities, and construction-fiducial ordering;
- bounded non-QRS adjacent-sample changes, with pacing impulses explicitly
  excluded;
- sampled CRBBB and CLBBB lead-polarity contracts;
- continuous, asymmetric, inferior-negative AFLT F waves.

Existing focused tests remain responsible for exact ST-T boundary continuity,
territory evidence, rhythm timing, source summation, and morphology
measurement details.

## 6. Compatibility and versioning

No public type, condition code, JSON field, or DataBrowser API signature
changes.

Generated CRBBB, CLBBB, and AFLT samples and flutter atrial fiducial times
change. The scenario schema remains version 2. The scenario engine version
shall increase from 7 to 8 so fingerprints expose the semantic waveform
correction.

## 7. DataBrowser and SVN integration

Synchronize every changed portable core file into
`DataBrowser_psaa/src/SignalProcessors/` and verify byte identity. Existing
DataBrowser phenotype APIs and scripts require no signature change; their
rerun shall visualize the corrected records.

The Windows application cannot be compiled in this environment. Portable
Linux verification and GitHub Windows C++11 CI are automated; interactive
DataBrowser inspection remains manual integration evidence.

## 8. Risks and controls

| Risk | Control |
|---|---|
| Source direction passes while final leads remain wrong | Assert sampled V1/I/V5/V6 values |
| LBBB T reversal conflicts with explicit ST-T scenarios | Existing validation rejects those condition-family compositions |
| Flutter timing breaks AV linkage | Retain event indexes and assert fixed conduction ratio |
| Flutter creates a cycle-boundary step | Use adjacent compact waves with coincident zero/zero-slope boundaries |
| Generic continuity threshold rejects valid QRS/pacing | Exclude annotated QRS intervals and explicit pacing impulses |
| Corrected output keeps an old fingerprint | Increment the scenario engine version |

## 9. Non-goals and residual limitations

- Incomplete bundle blocks, fascicular blocks, WPW, and nonspecific IVCD.
- Mechanistic Purkinje or torso-volume-conductor simulation.
- Population-calibrated variability or clinical diagnostic validation.
- Atypical flutter circuits or variable flutter conduction.
- Automatic image-based approval of morphology atlases.

## 10. Acceptance criteria

1. All catalog audit cases pass the permanent quality gate.
2. CRBBB and CLBBB satisfy their sampled lead-domain contracts.
3. Typical AFLT has no finite isoelectric interval between F waves and retains
   its fixed conduction ratio.
4. Existing focused regressions remain successful.
5. Engine version 8 and fingerprints expose the correction.
6. DataBrowser source copies are synchronized byte-for-byte.
7. Release, sanitizer, package-smoke, Ubuntu, and Windows C++11 verification
   pass.

## 11. Source basis

- AHA/ACCF/HRS recommendations, Part III, define complete bundle-branch-block
  criteria and secondary ST-T changes:
  <https://www.jacc.org/doi/10.1016/j.jacc.2008.12.013>.
- The 2023 ACC/AHA/ACCP/HRS guideline identifies the classic inferior-lead
  sawtooth pattern of typical atrial flutter:
  <https://www.ahajournals.org/doi/10.1161/CIR.0000000000001193>.
- Published atrial-flutter surface-ECG analysis reports a slow downslope over
  approximately 73% of the cycle and a faster upslope:
  <https://pubmed.ncbi.nlm.nih.gov/28833757/>.

No external source code, patient waveform, or learned parameter set is copied.

## 12. Implementation sequence

1. Record the audit and accepted design.
2. Add failing sampled lead-domain and flutter regressions.
3. Correct the source orientation, polarity, and flutter event shape.
4. Add the complete catalog-wide quality gate and increment engine identity.
5. Run release, sanitizer, and package verification.
6. Synchronize DataBrowser files, push, and record cross-platform CI evidence.

## 13. Change log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-07-02 | Accepted design from the complete supported-phenotype morphology audit |
| 0.2 | 2026-07-02 | Implemented catalog quality gate and CRBBB, CLBBB, and AFLT waveform corrections |
