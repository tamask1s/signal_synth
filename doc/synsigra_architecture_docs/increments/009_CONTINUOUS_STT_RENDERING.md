# Continuous ST-T Rendering Correction

**Document ID:** SYN-ARCH-INC-009

**Version:** 0.2

**Status:** Implemented; verification pending

**Owner role:** Biosignal Algorithms / Verification

**Date:** 2026-07-01

**Traceability ID:** `TRC-ECG12-006`

**Implementation issue:** [signal_synth#21](https://github.com/tamask1s/signal_synth/issues/21)

## 1. Decision

Replace the discontinuous injury-source renderer with a piecewise cubic
Hermite construction that is continuous in value and first derivative from
the terminal QRS through J, ST, T onset, and T offset.

This is a defect correction to the clean synthetic waveform model. It does
not change the non-diagnostic engineering-testbench claim posture.

## 2. Reproduction and root cause

Direct inspection of the DataBrowser 071/072 binary outputs reproduced
block-like level changes in `NST_`, `DIG`, `STD_`, `STE_`, `ANEUR`, and all
territorial ischemia conditions. The same renderer is used by the injury pack.

The previous `render_injury_wave` behavior was:

1. zero injury-source value before QRS offset;
2. nonzero configured ST-J value at the first sample at or after QRS offset;
3. interpolation from that value to the end-ST value;
4. independent decay under the T wave.

Because the terminal QRS source reaches zero at QRS offset, step 2 introduces
a true level discontinuity. Nearest-sample J measurement can consequently
select either side of the step depending on beat/sample phase. Existing
phenotype assertions average amplitudes and lead counts, so they did not
detect local continuity defects.

## 3. Requirements

- `REQ-GEN-001..003`;
- `REQ-ECG-001..003`, `REQ-ECG-006..007`;
- `REQ-GT-001`;
- `REQ-NFR-001..005`, `REQ-NFR-008`;
- `REQ-VER-001`, `REQ-VER-003`, `REQ-VER-008..010`;
- `REQ-DOC-001..002`.

## 4. Rendering contract

The injury source shall use four intervals:

| Interval | Value contract | Derivative contract |
|---|---|---|
| before S peak | zero | zero |
| S peak to J | Hermite zero to configured J value | zero to configured ST slope |
| J to T onset | configured J value plus elapsed-time ST slope | configured ST slope |
| T onset to T offset | Hermite end-ST value to zero | configured ST slope to zero |
| after T offset | zero | zero |

The independent repolarization source remains a compact smooth T wave. Its
zero derivative at T onset and offset composes with the injury baseline
without introducing a level or slope discontinuity.

All interpolation is performed on the three-dimensional source vector before
lead projection. Therefore continuity is inherited by VCG and every lead.

## 5. Compatibility and versioning

No public type, condition code, JSON field, or DataBrowser API signature
changes.

Waveform samples and measured assertions change. The scenario schema remains
version 2 and the scenario engine version increases from 6 to 7 so run
fingerprints do not identify the corrected output as engine-v6 output.

## 6. Verification

Extend `TEST-ECG-PHANTOM-001` and `TEST-ECG-REPOLARIZATION-001` to verify:

- injury-source value continuity and bounded derivative change around J;
- continuity around T onset and T offset;
- all ST-T and injury conditions at 100, 500, and 1000 Hz;
- final-waveform phenotype assertions after the renderer correction;
- deterministic repeated generation;
- unchanged source summation and lead identities.

The continuity test must inspect sampled source/lead data around construction
fiducials. Configuration-only assertions are insufficient.

## 7. DataBrowser and SVN integration

Synchronize changed portable core files into
`DataBrowser_psaa/src/SignalProcessors/`. Scripts 071 and 072 retain their API
and content; rerunning them shall produce corrected waveforms. Byte-compare
all synchronized source files.

The Windows DataBrowser application cannot be compiled in this environment.
Portable Linux verification and GitHub Windows C++11 CI remain automated;
interactive DataBrowser inspection remains manual evidence.

## 8. Risks and controls

| Risk | Control |
|---|---|
| Smoothing suppresses requested J amplitude | Assert final sampled ST-J evidence |
| ST slope loses its physical unit | Use derivative vectors in mV/s and an exact linear ST interval |
| QRS morphology changes unexpectedly | Begin the correction only at S peak and retain all QRS source primitives |
| T waveform gains extra discontinuities | Match value and derivative at both T boundaries |
| Old and corrected outputs share identity | Increment scenario engine version |
| Tests again miss visible defects | Add local sampled continuity metrics |

## 9. Non-goals

- Mechanistic ischemia electrophysiology or clinical diagnostic thresholds.
- Population-calibrated morphology.
- Biphasic/notched T, U waves, or arbitrary ST curvature.
- Redesign of QRS, lead projection, or scenario condition taxonomy.

## 10. Acceptance criteria

1. No nonzero injury-source onset step occurs at J.
2. The analytic injury construction is C1 at all piece boundaries.
3. Sampled continuity checks pass for every affected condition and supported
   test sampling rate.
4. All phenotype assertions and existing regressions pass.
5. Engine version 7 and fingerprints expose the semantic correction.
6. DataBrowser source copies are synchronized.
7. Ubuntu and Windows `CI-VER-001` pass.

## 11. Source basis

- AHA/ACCF/HRS Part IV describes the ST segment as the interval connecting
  ventricular depolarization and repolarization and separates J, ST, T, and
  QT measurements.
- Continuous wave-based synthetic ECG models motivate smooth component
  functions rather than block insertion at fiducials.

No external source code or waveform data is copied.

## 12. Implementation sequence

1. Record the defect and design.
2. Add failing sampled continuity regression.
3. Implement the C1 injury renderer and engine-version change.
4. Re-evaluate all final-waveform phenotype assertions.
5. Run release, sanitizer, and cross-platform verification.
6. Synchronize DataBrowser/SVN files and close the issue with evidence.

## 13. Change log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-07-01 | Initial defect analysis and accepted correction design |
| 0.2 | 2026-07-01 | Implemented C1 rendering, engine v7, and multi-rate sampled continuity regression |
