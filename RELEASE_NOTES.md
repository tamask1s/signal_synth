# Release Notes

This file records public engineering-verification release notes for the
`signal_synth` core generator and local verifier contracts.

## 0.6.0-dev

Status: active development baseline.

- Replaced target-specific customer output discovery with the strict
  `synsigra_submission_v1` manifest and generated `user-output-template/`.
- Unified customer point and interval payloads while retaining typed adapters;
  algorithm provenance is supplied once and preserved for JSON and CSV.
- Released the generator-free `synsigra` 0.5.0 verifier wheel and one canonical
  `synsigra-verify <challenge> <submission> <results>` workflow.
- Replaced ECG delineation v1 with atrial-aware delineation v2, explicit
  present/absent/not-evaluable truth, temporal matching, and a curated Mobitz
  II non-conducted P-wave case.
- Added uniform JSON/CSV measurement submissions and local scoring for ECG
  intervals, ST/T amplitudes and slopes, frontal axes, phenotype assertions,
  PTT, and ECG-to-PPG peak delay.
- Bumped challenge, scoring-manifest, integration, and curated-catalog
  contracts for the private-beta breaking change.

## 0.5.0-dev

Status: active development baseline.

Engineering-verification scope:

- deterministic synthetic ECG and PPG scenario rendering;
- scenario, pack, challenge-package and local Python verifier contracts;
- WFDB, EDF+/BDF+, CSV, JSON and HTML export artifacts;
- constructed ground truth for fiducials, beats, PPG pulses, HRV metrics,
  signal-quality intervals and supported phenotype assertions.

Provenance and claim-boundary artifacts:

- every scenario export includes `provenance.json`;
- every scenario export includes `ENGINEERING_CLAIM_BOUNDARY.txt`;
- every pack challenge includes package-level `provenance.json`;
- every pack challenge includes package-level `ENGINEERING_CLAIM_BOUNDARY.txt`;
- reports link the same engineering QA claim boundary.

Required provenance checklist:

- generator version;
- generator git commit, or `unknown` when built outside a Git checkout;
- build identity;
- package contract version;
- scoring manifest contract version;
- verifier version;
- scenario or pack fingerprint;
- per-case scenario, annotations, metrics, warning and native waveform
  artifacts;
- concise statement of what the package verifies and does not verify.

Claim boundary:

These artifacts support deterministic synthetic biosignal engineering QA and
algorithm verification. They are not diagnostic outputs, patient monitoring
records, clinical validation certificates, MDR evidence packages, FDA evidence
packages, or standalone conformity-assessment records.
