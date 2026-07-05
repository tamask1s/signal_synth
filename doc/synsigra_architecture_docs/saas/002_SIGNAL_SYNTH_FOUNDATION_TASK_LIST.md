# signal_synth Foundation Task List For SaaS Completeness

Context: `signal_synth_saas` already proves the intended offline-first B2B
workflow: generate a deterministic challenge package, download waveform plus
ground truth, run the customer algorithm locally, and score locally. This list
tracks what the core generator/library still needs for a professional
verification-pack product.

Issue map:

- [#59](https://github.com/tamask1s/signal_synth/issues/59): scoring manifest,
  case summaries, and package integrity verification.
- [#60](https://github.com/tamask1s/signal_synth/issues/60): installable
  Python SDK and one-command local verification.
- [#61](https://github.com/tamask1s/signal_synth/issues/61): curated SaaS
  verification packs from existing ECG/HRV capabilities.
- [#62](https://github.com/tamask1s/signal_synth/issues/62): threshold
  profiles and richer pack-level scoring summaries.
- [#63](https://github.com/tamask1s/signal_synth/issues/63): scenario
  templates and schema metadata for SaaS authoring.
- [#64](https://github.com/tamask1s/signal_synth/issues/64): reproducible
  randomization, respiration coupling, and long-duration controls.
- Existing PPG-specific issues: [#50](https://github.com/tamask1s/signal_synth/issues/50),
  [#51](https://github.com/tamask1s/signal_synth/issues/51),
  [#52](https://github.com/tamask1s/signal_synth/issues/52),
  [#53](https://github.com/tamask1s/signal_synth/issues/53),
  [#54](https://github.com/tamask1s/signal_synth/issues/54).

## 1. Curated Verification Pack Coverage

- [ ] Promote the existing HRV work into curated, release-versioned packs:
      LF/HF modulation, SDNN/RMSSD/pNN50, SD1/SD2, artifact exclusion,
      ectopy exclusion, low/high variability, and nonstationary windows.
- [ ] Add a PPG peak-detection pack with clean, motion-corrupted,
      low-perfusion, pulse-amplitude modulation, dropped-pulse, and ECG+PPG
      timing cases.
- [ ] Add ECG beat-classification packs for normal, PAC, PVC, paced capture,
      non-capture, fusion-like engineering proxies, and ambiguous/excluded
      windows.
- [ ] Add rhythm packs for AFib, flutter with variable conduction, SVT
      episodes, pauses, bigeminy/trigeminy, paced rhythms, and transition
      annotations.
- [ ] Add signal-quality packs with explicit scoreable quality labels, not
      only artifact reference intervals.
- [ ] Add morphology/stress packs for conduction, ST-T/ischemia, hypertrophy,
      infarction, lead faults, and acquisition faults as user-selectable
      product packs.

## 2. Scoring Completeness

- [ ] Define pack-level pass/fail threshold profiles per target, e.g.
      smoke, regression, stress, and benchmark.
- [ ] Add first-class pack-level scoring for HRV outputs, not only per-case
      metric scoring.
- [ ] Add first-class pack-level scoring for beat classification confusion
      matrices and per-class F1.
- [ ] Add signal-quality scoring contracts: interval overlap, sample mask
      accuracy, quality-class accuracy, and artifact-class confusion.
- [ ] Add PPG scoring summaries parallel to R-peak scoring, including timing
      error distribution and missed/extra pulse statistics.
- [ ] Add exclusion-window semantics to all relevant scoring targets so users
      know what must not be counted.
- [ ] Emit machine-readable scoring manifests that name accepted user-output
      schemas, target names, tolerances, exclusions, and expected report files.

## 3. Python User Package

- [ ] Package `python/synsigra` as an installable wheel with console scripts.
- [ ] Provide a no-build path for users who only want local scoring against a
      downloaded package.
- [ ] Hide direct `SIGNAL_SYNTH_CLI` setup behind clear discovery/configuration
      or bundled native binaries.
- [ ] Add a single user-facing command such as
      `synsigra verify package.zip detections/ out/`.
- [ ] Add typed Python objects for package, case, waveform access, annotations,
      scoring reports, and summary tables.
- [ ] Add example notebooks or scripts for R-peak, HRV, PPG peak, and beat
      classification workflows.

## 4. Challenge Package Contract

- [ ] Add `scoring_manifest.json` to the package root with targets, required
      detection filenames, accepted schemas, tolerances, excluded intervals,
      and supported score commands.
- [ ] Add per-case `case_summary.json` with concise user-facing facts:
      duration, channels, sampling rate, targets, artifact intervals, and
      compatible scoring modes.
- [ ] Add package-level provenance fields for generator git commit/container
      digest, not only generator version where available.
- [ ] Add optional package signing or at least detached manifest checksum for
      long-lived release artifacts.
- [ ] Add a strict package verifier that checks all file hashes and reports
      package integrity before scoring.

## 5. Scenario And Pack Authoring

- [ ] Make scenario schema discovery user-friendly: generated schema docs,
      examples, parameter ranges, and error messages suitable for SaaS UI.
- [ ] Add scenario templates for common algorithm QA goals instead of requiring
      users to edit raw JSON from scratch.
- [ ] Add a safe parameter form model that the SaaS can render without knowing
      C++ internals.
- [ ] Add pack composition validation that explains target incompatibilities,
      duration/cost estimates, and scoring support before job creation.
- [ ] Add stable difficulty tags and feature tags for catalog search/filtering.

## 6. Realism And Domain Expansion

- [ ] Improve PPG morphology and sensor realism for wearable algorithm tests:
      multi-wavelength channels, motion artifact, low perfusion, baseline
      drift, saturation/clipping, and contact loss.
- [ ] Add ECG/PPG synchronization scenarios with pulse transit time variation,
      arrhythmia-linked missing pulses, and sensor delay/clock drift.
- [ ] Add respiration and activity modulation hooks that affect ECG baseline,
      PPG amplitude, and HRV together.
- [ ] Add controlled randomization envelopes with reproducible seeds and
      recorded parameter draws.
- [ ] Add longer-duration generation paths for HRV and wearable endurance
      tests without excessive memory or package size.

## 7. Developer And Release Quality

- [ ] Define a release checklist for curated packs: morphology review,
      scoring sanity, native format conformance, package integrity, and docs.
- [ ] Add golden package fixtures for selected release packs.
- [ ] Add regression tests that SaaS-produced ZIP files can be verified by the
      Python package after download.
- [ ] Add compatibility tests between released `signal_synth` and
      `signal_synth_saas` generator contracts.
- [ ] Document which claims are engineering QA claims and which are explicitly
      not clinical validation claims.
