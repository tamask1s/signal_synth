# Focused SRS Implementation Issue Map

**Document ID:** SYN-SRS-ISSUE-MAP-001

**Version:** 0.1

**Status:** Draft implementation roadmap

**Date:** 2026-07-04

**Parent issue:** [signal_synth#32](https://github.com/tamask1s/signal_synth/issues/32)

## 1. Purpose

Map focused SRS requirement groups to GitHub implementation issues.

The issues below are implementation-ready inputs. They intentionally exclude
hosted SaaS service implementation. They prepare the C++ core, package
contracts, local Python scoring, standard formats, and ECG/PPG/HRV foundation
features so hosted SaaS can be built later on stable contracts.

## 2. SaaS-Readiness and Offline Challenge Workflow

| Area | Trace ID | Issue |
|---|---|---|
| Stable C++ facade | `TRC-FACADE-001` | [#33](https://github.com/tamask1s/signal_synth/issues/33) |
| Challenge package contract | `TRC-CHAL-001` | [#34](https://github.com/tamask1s/signal_synth/issues/34) |
| Local Python scoring package | `TRC-PY-001` | [#35](https://github.com/tamask1s/signal_synth/issues/35) |
| Pack-level local scoring aggregation | `TRC-PACK-SCORE-001` | [#39](https://github.com/tamask1s/signal_synth/issues/39) |

Recommended implementation order:

1. `#33` stable C++ facade.
2. `#34` challenge package manifest.
3. `#38` CSV/JSON detection contracts.
4. `#39` pack-level scoring aggregation.
5. `#35` Python package once the facade and package contracts stabilize.

## 3. Standard Formats and Detection I/O

| Area | Trace ID | Issue |
|---|---|---|
| WFDB export | `TRC-FMT-WFDB-001` | [#36](https://github.com/tamask1s/signal_synth/issues/36) |
| EDF+/BDF+ export | `TRC-FMT-EDF-001` | [#37](https://github.com/tamask1s/signal_synth/issues/37) |
| CSV/JSON detection outputs | `TRC-DET-001` | [#38](https://github.com/tamask1s/signal_synth/issues/38) |

Recommended implementation order:

1. `#38` detection contracts, because scoring already exists.
2. `#36` WFDB, because ECG algorithm users commonly expect WFDB-like records.
3. `#37` EDF+/BDF+ for broader biosignal exchange.

## 4. HRV Foundation

| Area | Trace ID | Issue |
|---|---|---|
| Product-facing HRV scenario schema | `TRC-HRV-SCN-001` | [#40](https://github.com/tamask1s/signal_synth/issues/40) |
| HRV metrics and tachogram export | `TRC-HRV-MET-001` | [#41](https://github.com/tamask1s/signal_synth/issues/41) |
| HRV scoring and report | `TRC-HRV-SCORE-001` | [#42](https://github.com/tamask1s/signal_synth/issues/42) |
| HRV benchmark scenario pack | `TRC-HRV-PACK-001` | [#43](https://github.com/tamask1s/signal_synth/issues/43) |

Recommended implementation order:

1. `#40` scenario schema.
2. `#41` metrics and tachogram export.
3. `#42` user-output scoring.
4. `#43` curated benchmark pack.

## 5. ECG Foundation Features

| Area | Trace ID | Issue |
|---|---|---|
| Beat classification ground truth and scoring | `TRC-ECG-BC-001` | [#44](https://github.com/tamask1s/signal_synth/issues/44) |
| Rhythm engine v2: AFib, flutter, ectopy cadence | `TRC-ECG-RHY-001` | [#45](https://github.com/tamask1s/signal_synth/issues/45) |
| Paced rhythm scenarios | `TRC-ECG-PACE-001` | [#46](https://github.com/tamask1s/signal_synth/issues/46) |
| ECG acquisition and lead-fault pack | `TRC-ECG-AQ-001` | [#47](https://github.com/tamask1s/signal_synth/issues/47) |
| Morphology population variation | `TRC-ECG-MVAR-001` | [#48](https://github.com/tamask1s/signal_synth/issues/48) |
| Dynamic QT and ST-T episodes | `TRC-ECG-DYN-001` | [#49](https://github.com/tamask1s/signal_synth/issues/49) |

Recommended implementation order:

1. `#44` beat classification, because it expands scoring value directly.
2. `#45` rhythm engine v2, because it feeds detector/classifier packs.
3. `#47` acquisition and lead faults, because they are high-value robustness
   tests.
4. `#46`, `#48`, `#49` in the order required by customer priorities.

## 6. PPG Foundation Features

| Area | Trace ID | Issue |
|---|---|---|
| PPG physiology v2 | `TRC-PPG-PHYS-001` | [#50](https://github.com/tamask1s/signal_synth/issues/50) |
| PPG motion artifact with accelerometer | `TRC-PPG-MOT-001` | [#51](https://github.com/tamask1s/signal_synth/issues/51) |
| PPG perfusion, weak pulse, missed pulse scenarios | `TRC-PPG-PERF-001` | [#52](https://github.com/tamask1s/signal_synth/issues/52) |
| PPG scoring and benchmark pack | `TRC-PPG-SCORE-001` | [#53](https://github.com/tamask1s/signal_synth/issues/53) |
| Optional red/IR multi-channel PPG | `TRC-PPG-MCH-001` | [#54](https://github.com/tamask1s/signal_synth/issues/54) |

Recommended implementation order:

1. `#50` PPG physiology v2.
2. `#51` motion artifact and accelerometer reference.
3. `#52` perfusion and weak/missed pulse semantics.
4. `#53` scoring and benchmark pack.
5. `#54` only after single-channel PPG is stable.

## 7. Recommended Overall Next Step

The best next implementation is `#33` stable C++ facade.

Reason:

- it reduces coupling between CLI, future Python package, and future SaaS;
- it makes later standard exports and scoring wrappers cleaner;
- it keeps the core C++ library as the source of truth;
- it is a low-risk architecture step before adding more feature complexity.
