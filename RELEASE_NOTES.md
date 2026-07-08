# Release Notes

This file records public engineering-verification release notes for the
`signal_synth` core generator and local verifier contracts.

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
