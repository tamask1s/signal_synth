# HRV Foundation and Scoring SRS

**Document ID:** SYN-SRS-HRV-001

**Version:** 0.1

**Status:** Draft implementation input

**Date:** 2026-07-04

**Parent issue:** [signal_synth#32](https://github.com/tamask1s/signal_synth/issues/32)

## 1. Purpose

Define the missing HRV foundation required for ECG/PPG algorithm QA and future
SaaS readiness.

Current state:

- the legacy ECG model has an LF/HF-like oscillator-bank RR generator;
- the product-facing scenario layer currently exposes only simpler RR
  variability controls;
- export currently reports mean RR, mean HR, SDNN, RMSSD, and pNN50;
- LF, HF, LF/HF measured power, SD1, SD2, and HRV scoring are not yet
  complete product features.

## 2. Scenario Requirements

| ID | Requirement |
|---|---|
| `REQ-HRV-SCN-001` | Product-facing scenario JSON shall include an explicit HRV block. |
| `REQ-HRV-SCN-002` | The HRV block shall support target mean HR, SDNN-like variability, LF/HF ratio, LF center/bandwidth, HF center/bandwidth, and seed. |
| `REQ-HRV-SCN-003` | The HRV block shall support respiratory sinus arrhythmia style HF modulation parameters. |
| `REQ-HRV-SCN-004` | The HRV block shall support minimum and maximum RR bounds and shall report clipping. |
| `REQ-HRV-SCN-005` | HRV scenarios shall require durations appropriate for the requested metrics, with warnings or errors for unsupported short windows. |
| `REQ-HRV-SCN-006` | HRV parameters shall be deterministic and fingerprinted. |

## 3. Ground Truth Requirements

| ID | Requirement |
|---|---|
| `REQ-HRV-GT-001` | Ground truth shall include the RR interval sequence used to generate ECG and PPG. |
| `REQ-HRV-GT-002` | Ground truth shall include beat times, RR intervals, clipping flags, ectopic/excluded flags, and artifact-overlap flags. |
| `REQ-HRV-GT-003` | Ground truth shall define which RR intervals are included in HRV metrics. |
| `REQ-HRV-GT-004` | HRV metric definitions shall be explicit and versioned. |

## 4. Metric Requirements

Required time-domain metrics:

- mean RR;
- mean HR;
- SDNN;
- RMSSD;
- pNN50;
- SD1;
- SD2;
- SD1/SD2.

Required frequency-domain metrics:

- LF power;
- HF power;
- LF/HF ratio;
- total power over the configured band;
- analysis window metadata;
- interpolation/resampling policy;
- spectral method policy.

| ID | Requirement |
|---|---|
| `REQ-HRV-MET-001` | HRV metric computation shall be implemented in a reusable core module. |
| `REQ-HRV-MET-002` | The spectral method shall be deterministic and documented. |
| `REQ-HRV-MET-003` | Poincare metrics SD1 and SD2 shall be computed from the same accepted RR sequence as time-domain metrics. |
| `REQ-HRV-MET-004` | Metrics shall identify whether ectopic, clipped, or artifact-overlapped intervals were excluded. |
| `REQ-HRV-MET-005` | Metric output shall include units and numeric tolerances for scoring. |

## 5. HRV Scoring Requirements

| ID | Requirement |
|---|---|
| `REQ-HRV-SCORE-001` | The local scoring package shall compare user HRV metrics against ground-truth HRV metrics. |
| `REQ-HRV-SCORE-002` | The scoring layer shall support absolute error, relative error, pass/fail tolerance, and aggregate score. |
| `REQ-HRV-SCORE-003` | The scoring layer shall optionally score user-provided RR intervals against ground-truth RR intervals before metric comparison. |
| `REQ-HRV-SCORE-004` | HRV reports shall show metric definitions, analysis window, and exclusion policy. |

## 6. Scenario Pack Requirements

At minimum, create HRV packs for:

- clean fixed HR baseline;
- mild variability;
- high variability;
- LF-dominant modulation;
- HF-dominant modulation;
- balanced LF/HF;
- respiratory frequency variation;
- ectopic contamination with exclusion policy;
- artifact contamination with mask-aware scoring;
- short-window rejection/warning examples.

## 7. Acceptance Criteria

1. A 5-minute HRV scenario can target LF/HF-like modulation reproducibly.
2. Exported HRV metrics include SD1, SD2, LF, HF, and LF/HF.
3. User-provided HRV JSON can be scored locally.
4. HRV pack render and scoring reports are deterministic.
5. Short or unsupported HRV windows are rejected or warning-labeled.

## 8. Planned Implementation Issues

- product-facing HRV scenario schema;
- reusable HRV metrics engine;
- RR tachogram and HRV ground-truth export;
- HRV user-output JSON/CSV import;
- HRV scoring and report;
- HRV benchmark scenario pack.
