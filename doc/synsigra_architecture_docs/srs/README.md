# Synsigra Focused SRS Set

This directory contains focused requirement documents that refine
`../01_SRS.md` into implementable packages.

The current goal is SaaS readiness without implementing the hosted SaaS
service itself. The intended first B2B model is offline-first:

1. Synsigra generates a challenge scenario or pack.
2. The user receives waveform data and ground truth.
3. The user runs their own algorithm locally.
4. The local Synsigra Python package scores user output against ground truth.
5. The resulting report can be uploaded, archived, or used in a local QA
   workflow.

## Documents

| Document | Scope |
|---|---|
| `001_OFFLINE_CHALLENGE_AND_PYTHON_SCORING_SRS.md` | Challenge package, local scoring package, workflow, reports |
| `002_FORMATS_AND_IO_CONTRACTS_SRS.md` | WFDB, EDF+/BDF+, CSV/JSON detection contracts |
| `003_HRV_FOUNDATION_AND_SCORING_SRS.md` | HRV scenario engine, metrics, packs, scoring |
| `004_ECG_FOUNDATION_FEATURES_SRS.md` | ECG feature gaps required before SaaS buildout |
| `005_PPG_FOUNDATION_FEATURES_SRS.md` | PPG feature gaps required before SaaS buildout |
| `006_IMPLEMENTATION_ISSUE_MAP.md` | GitHub issue map and recommended implementation order |

## Claim Boundary

These requirements describe an engineering testbench. They do not establish a
medical-device intended purpose, diagnostic use, clinical validation claim, or
standalone conformity-assessment claim.
