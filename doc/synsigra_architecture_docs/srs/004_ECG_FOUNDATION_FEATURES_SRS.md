# ECG Foundation Feature SRS

**Document ID:** SYN-SRS-ECG-FOUND-001

**Version:** 0.1

**Status:** Draft implementation input

**Date:** 2026-07-04

**Parent issue:** [signal_synth#32](https://github.com/tamask1s/signal_synth/issues/32)

## 1. Purpose

Define ECG generator and scoring features still needed before serious SaaS
buildout.

Current ECG strengths:

- deterministic 12-lead compact phantom;
- condition catalog with native/parameterized support;
- rhythm/conduction phenotypes;
- infarction/injury, ischemia/ST-T, hypertrophy, advanced conduction;
- explicit fiducials, episodes, artifacts, and event scoring for R peaks.

Remaining work should focus on algorithm QA value, not clinical realism claims.

## 2. Beat Classification Requirements

| ID | Requirement |
|---|---|
| `REQ-ECG-BC-001` | Ground truth shall include beat classification labels suitable for detector/classifier QA. |
| `REQ-ECG-BC-002` | Labels shall distinguish normal, supraventricular ectopic, ventricular ectopic, paced, escape, and unknown/unscored categories. |
| `REQ-ECG-BC-003` | Beat classification scoring shall compute confusion matrix, per-class precision/recall/F1, and timing-aware classification correctness. |
| `REQ-ECG-BC-004` | Beat labels shall be exported in JSON and standard annotation formats where supported. |

## 3. Rhythm Scenario Requirements

| ID | Requirement |
|---|---|
| `REQ-ECG-RHY-001` | AFib scenarios shall model irregularly irregular RR behavior beyond simple random jitter. |
| `REQ-ECG-RHY-002` | Atrial flutter scenarios shall model atrial activity and variable conduction patterns. |
| `REQ-ECG-RHY-003` | Bigeminy and trigeminy scenarios shall expose exact cadence ground truth. |
| `REQ-ECG-RHY-004` | SVT/PSVT episode scenarios shall support onset, duration, rate, and transition truth. |
| `REQ-ECG-RHY-005` | Rhythm scenarios shall support artifact overlap without changing construction truth. |

## 4. Paced Rhythm Requirements

| ID | Requirement |
|---|---|
| `REQ-ECG-PACE-001` | Paced rhythm scenarios shall support visible pacing spikes. |
| `REQ-ECG-PACE-002` | Paced scenarios shall distinguish atrial, ventricular, and dual-chamber pacing where modeled. |
| `REQ-ECG-PACE-003` | Capture and non-capture events shall be explicit if supported. |
| `REQ-ECG-PACE-004` | Paced beat ground truth shall be exported and scoreable. |

## 5. Acquisition and Lead-Fault Requirements

| ID | Requirement |
|---|---|
| `REQ-ECG-AQ-001` | Lead reversal scenarios shall support selected limb/precordial swaps with exact truth labels. |
| `REQ-ECG-AQ-002` | Electrode misplacement scenarios shall perturb lead projection reproducibly. |
| `REQ-ECG-AQ-003` | Per-lead gain mismatch and offset drift shall be supported. |
| `REQ-ECG-AQ-004` | Sampling clock drift and dropped-sample scenarios shall be supported for algorithm robustness testing. |
| `REQ-ECG-AQ-005` | Quantization and ADC clipping shall be supported with exact masks. |

## 6. Morphology and Population Variation Requirements

| ID | Requirement |
|---|---|
| `REQ-ECG-MVAR-001` | Morphology randomization shall produce deterministic population-like variation within controlled bounds. |
| `REQ-ECG-MVAR-002` | Randomization parameters shall be fingerprinted and replayable. |
| `REQ-ECG-MVAR-003` | Scenario packs shall include stable seeds, not hidden random behavior. |
| `REQ-ECG-MVAR-004` | Randomized morphology shall keep fiducial and phenotype assertions valid or explicitly warning-labeled. |

## 7. Dynamic Repolarization Requirements

| ID | Requirement |
|---|---|
| `REQ-ECG-DYN-001` | QT adaptation to heart-rate changes shall be modelable and annotated. |
| `REQ-ECG-DYN-002` | ST/T episodes shall support time-varying severity with smooth transitions. |
| `REQ-ECG-DYN-003` | Dynamic repolarization scenarios shall expose episode boundaries and severity trace ground truth. |

## 8. Acceptance Criteria

1. Beat classification ground truth and scoring are available for core rhythm
   cases.
2. AFib, flutter, ectopy cadence, and paced rhythm scenarios are deterministic
   and reportable.
3. ECG acquisition fault scenarios corrupt waveform data while preserving
   construction truth.
4. Morphology variation can produce multiple replayable cases from one scenario
   template.
5. Dynamic QT/ST/T behavior has smooth morphology and exact annotations.

## 9. Planned Implementation Issues

- beat classification ground truth and scoring;
- AFib/flutter rhythm engine v2;
- ectopy cadence and rhythm pack expansion;
- paced rhythm pack;
- ECG lead/acquisition fault pack;
- morphology population variation (implemented by #48);
- dynamic QT/ST/T episodes.
