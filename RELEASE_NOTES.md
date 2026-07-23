# Release Notes

This file records public engineering-verification release notes for the
`signal_synth` core generator and local verifier contracts.

## Curated R-peak evidence release

Status: active beta pack baseline, 2026-07-23.

- Upgraded `r_peak_stress_v1` to version 1.1 with a package-authoritative,
  R-peak-only evidence protocol. Its submission template and required matrix
  contain four R-peak event outputs and no signal-quality, RR or HRV output.
- Added `r_peak_noise_frontier_v1`, a clean anchor plus four paired 60-second
  all-lead cases at −7, −8, −9 and −10 dB target SNR.
- Combined calibrated project-owned baseline, muscle and electrode-motion
  noise with monotonically increasing analytic baseline wander and powerline
  interference while holding the cardiac timebase, PVC cadence, intervals and
  source offsets fixed.
- Added a separate acceptance stratum for every SNR tier. The clean aggregate
  can no longer hide failure at one noise level, and the audit report exposes
  the exact tier where sensitivity, PPV, F1 or timing stability falls.
- Advanced the curated catalog to 3.1 with 19 non-duplicate packs. Verifier
  0.14.0 remains sufficient because the scoring and evidence contracts are
  unchanged.

## Python verifier 0.14.0

Status: active customer-reporting baseline.

- Added an expandable per-case contribution ledger to every aggregate
  acceptance criterion, including raw counts, case values, diagnostic gate
  comparisons and direct detail links.
- Added required thresholds and clearly non-authoritative case diagnostics to
  the overview, plus reverse criterion links and case-vs-aggregate values on
  every detail page.
- Exposed packaged measurement tolerance rules as human-readable absolute,
  relative and effective tolerances, distinct from the pairing window.
- Replaced measurement-facing prediction/truth terminology with reference and
  submitted-measurement language while preserving canonical machine fields.
- Added units and plain-language definitions for P95, status agreement,
  coverage, F1 and the other target-specific metrics; mixed-unit measurement
  errors are no longer displayed as a misleading pooled value.

## Python verifier 0.13.0

Status: superseded by 0.14.0; retained as the observable-truth milestone.

- Added one deterministic `synsigra_observable_event_truth_v1` policy across
  every curated pack: R peaks require complete in-record QRS support and more
  than 5% retained signal during all-lead dropout.
- Kept additive/external noise, clipping, saturation and partial-lead
  artifacts fully scoreable; no result-dependent amplitude heuristic or
  post-hoc forgiveness is used.
- Marked RR measurements touching excluded endpoints as `not_evaluable`, and
  exposed excluded truth and nearby excluded detections separately from FN/FP
  counts in JSON and human reports.
- Applied the same exclusion semantics to native C++, Python verification and
  beat classification, with explicit reasons retained for audit.
- Fixed challenge-index links to case reports and standardized every generated
  HTML scoring report on the single neutral-gray engineering-use notice.
- Added `scripts/audit_curated_truth.py`; the release audit renders all 18
  curated packs and checks event truth, boundary rules, measurement status,
  HTML notices and internal links.

## Python verifier 0.11.0

Status: superseded; retained as the initial audit-reporting milestone.

- Replaced the redundant JSON, CSV, and HTML result tree with one canonical
  `evidence.json`, one `index.html` entry point, and one linked detail page per
  case-target below `details/`.
- Added a complete acceptance-criterion ledger with stable criterion IDs,
  actual and required values, margins, verdicts, protocol provenance, coverage,
  and direct links to target evidence.
- Added target-specific evidence views, including ground truth, submitted
  values, errors, effective tolerances, tolerance rationale, and aggregate
  acceptance context for numeric measurements.
- Made every generated result page self-contained, print-friendly, mutually
  navigable, and marked with the single gray engineering-use notice.
- Advanced the clean local-report contract to
  `synsigra_local_verification_v3`; legacy result filenames and report-object
  aliases are intentionally removed.

## 0.10.0-dev

Status: active development baseline.

- Made packaged `synsigra_verification_protocol_v2` authoritative in default
  evidence mode, including an exact case-target matrix, embedded acceptance
  profile, stress-stratum coverage, truth policy and protocol fingerprint.
- Added explicit non-evidence diagnostic mode for custom profiles and filtered
  exploratory runs; evidence reports now distinguish completion, policy result,
  matrix completeness and evidence eligibility.
- Replaced measurement v1 with strict JSON/CSV measurement v2, including
  half-open windows, method IDs and preprocessing-policy IDs in measurement
  identity, matching, aggregation and reports.
- Moved HRV customer output, scoring, templates and reports to the generic
  measurement path and removed the dedicated legacy HRV scorer and format.
- Advanced core integration to v7, scoring manifest to v3, submission formats
  to v2, C++ facade to 1.5.0 and the generator-free verifier to 0.10.0.
- Intentionally removed compatibility with protocol v1, measurement v1,
  scoring manifest v2 and the dedicated HRV submission contract. Existing
  challenges, submissions, caches and SaaS state must be regenerated.

## 0.9.0-dev

Status: active development baseline.

- Replaced pack schema v1 with v2 and made pre-specified verification
  protocols explicit pack inputs and challenge artifacts.
- Added dedicated manifest roles for scoring, submission-format, submission,
  and verification-protocol documents; removed the redundant per-file
  `required` flag.
- Hardened the generator-free Python loader with strict manifest, duplicate-key,
  path, archive-layout, symlink, size, and unlisted-payload validation.
- Centralized Python target-family dispatch and corrected package provenance to
  report verifier `0.9.0` independently from generator `0.9.0-dev`.
- Added a versioned CMake config package and removed the duplicate unused
  Python build-system declaration.
- Advanced the integration contract to v6, challenge package to v3, C++ facade
  to 1.4.0, authoring metadata to v18, and curated catalog to 3.0.

## 0.8.0-dev

Status: superseded by 0.9.0-dev.

- Added the `r_peak_rr_noise_v1` clean, analytic-noise, and calibrated
  external-noise challenge pack with separate R-peak, observable RR, and
  signal-quality scoring.
- Added the `ecg_qtc_verification_v1` pack with fixed, Bazett, Fridericia,
  Framingham, and Hodges correction, rate boundaries, difficult T/U
  morphology, and dynamic long-QT stress.
- Added uniform `rr_interval` and formula-explicit `qtc` measurement targets,
  target-specific threshold sections, and generator-free local scoring.
- Advanced the integration contract to v5, C++ facade to 1.3.0, authoring
  metadata to v17, and curated catalog to 2.6.
- Reused persistent Python build tooling by default; clean isolated builds
  remain available through `SYNSIGRA_BUILD_ISOLATION=1`.

## 0.7.0-dev

Status: active development baseline.

- Added scenario schema 9 with controlled VLF RR modulation.
- Added VLF power, LF/HF normalized units and explicit HRV metric definition
  v2 scoring.
- Replaced `hrv_v1` with the end-to-end `hrv_robustness_v2` R-peak, RR/HRV
  and signal-quality challenge pack.
- Added local-verifier HRV pipeline diagnostics and corrected successive NN
  calculations so excluded intervals are not bridged.

## 0.6.0-dev

Status: active development baseline.

- Replaced target-specific customer output discovery with the strict
  `synsigra_submission_v1` manifest and generated `user-output-template/`.
- Unified customer point and interval payloads while retaining typed adapters;
  algorithm provenance is supplied once and preserved for JSON and CSV.
- Released the generator-free `synsigra` 0.6.0 verifier wheel and one canonical
  `synsigra-verify <challenge> <submission> <results>` workflow.
- Replaced ECG delineation v1 with atrial-aware delineation v2, explicit
  present/absent/not-evaluable truth, temporal matching, and a curated Mobitz
  II non-conducted P-wave case.
- Added uniform JSON/CSV measurement submissions and local scoring for ECG
  intervals, ST/T amplitudes and slopes, frontal axes, phenotype assertions,
  PTT, and ECG-to-PPG peak delay.
- Bumped challenge, scoring-manifest, integration, and curated-catalog
  contracts for the private-beta breaking change.
- Added schema-v5 multi-rate wearable acquisition with independent ECG, PPG,
  and accelerometer clocks, deterministic resampling, timestamp jitter,
  packet-loss truth, and device-versus-physiological ECG/PPG alignment.
- Replaced `wearable_stress_v1` with `wearable_timebase_v2` and added
  generator-free Python access to all wearable sample and timing artifacts.

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
