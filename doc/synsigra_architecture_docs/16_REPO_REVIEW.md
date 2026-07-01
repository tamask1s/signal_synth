# Repository Review: `tamask1s/signal_synth`

Version: 0.1  
Status: Draft  
Scope: Architectural review based on the visible repository structure and README/specification descriptions.

## 1. Summary

The repository is already moving in a technically credible direction.

Strong signs:

- deterministic signal generation;
- chunk-invariant streaming;
- seeded HRV/RR process;
- explicit construction events;
- measured fiducials;
- strict scenario rejection;
- reproducibility fingerprint;
- audit report concept;
- legal provenance/data-license awareness.

These are exactly the right foundations for a B2B algorithm QA platform.

## 2. Main concern

The current implementation appears to have moved quickly toward a sophisticated “clinical ECG” model with 12-lead projection and condition catalog.

This is not wrong technically, but it creates product risk:

- it may invite clinical-realism expectations;
- it may complicate the MVP;
- it may distract from ECG/PPG wearable QA;
- it may trigger regulatory/marketing ambiguity.

The README already states that the clinical engine is a deterministic engineering phantom and not clinical validation evidence. Keep that limitation prominent.

## 3. Recommended product interpretation

Treat the current `clinical_ecg` work as:

> a structured synthetic cardiac phantom for engineering QA scenarios.

Do not treat it as:

> a clinically validated disease ECG generator.

## 4. What looks good

### 4.1 Determinism

Documented deterministic, chunk-invariant streaming is a major asset. Keep it non-negotiable.

### 4.2 HRV process

A seeded LF/HF oscillator-bank RR process is aligned with the original product idea: HRV algorithm testing.

### 4.3 Annotation split

The distinction between construction model events and measured fiducials is architecturally correct.

It supports:

- ground truth;
- sampled signal truth;
- reporting;
- algorithm comparison.

### 4.4 Strict scenario API

Rejecting unsupported/incompatible conditions is better than generating plausible-looking but mislabeled signals.

## 5. What I would change

### 5.1 Rename or reframe `clinical_ecg`

Options:

- `cardiac_phantom`;
- `structured_ecg`;
- `synthetic_12lead`;
- `ecg_phantom`.

If not renamed, add strong wording everywhere:

> This module generates deterministic engineering phantoms and is not patient-population-fitted clinical evidence.

### 5.2 Add PPG earlier

The product opportunity is stronger with ECG+PPG/wearable QA.

Add first-class PPG before deepening clinical ECG condition support too much.

### 5.3 Move documents into `docs/`

Top-level markdown proliferation will become hard to manage.

### 5.4 Rename `teszt`

Use `tests`.

### 5.5 Add CLI and examples

A product-facing CLI is the fastest way to turn the library into something demonstrable.

### 5.6 Keep SaaS out of core

Do not mix web/backend code into `src`. Use `web/` or separate service packages.

## 6. Recommended next implementation tasks

Priority order:

1. root repo cleanup;
2. `docs/` organization;
3. scenario schema file;
4. CLI validate/render/report;
5. examples/scenarios;
6. golden tests;
7. PPG module;
8. export package;
9. backend wrapper;
10. web UI.

## 7. Architectural decision

Use the current repo as final core repository if:

- you keep it as the reusable engine;
- product/SaaS code is layered around it;
- documentation is reorganized;
- naming/claims are controlled.

Do not use it as a monolithic repo if you plan heavy SaaS and hardware development unless you enforce a clean top-level structure.

## 8. Suggested final repo identity

```text
signal_synth = core technical repository
Synsigra = commercial/product brand
Synsigra Testbench = SaaS/workflow product
```

This lets you keep the engineering repo name while using a better product name externally.

## 9. Immediate pull request idea

Create a PR titled:

> docs: establish product architecture and repository structure

Contents:

- move existing docs to `docs/`;
- add architecture docs;
- add README doc index;
- no functional code changes.

Second PR:

> build: normalize repository structure and test layout

Contents:

- rename `teszt` to `tests`;
- add root CMake;
- update build instructions.

Third PR:

> app: add signal-synth CLI skeleton

Contents:

- validate command;
- fingerprint command;
- render placeholder;
- report placeholder.

## 10. Verdict

Not a bad path. The current repo is technically stronger than a typical MVP, but it needs product architecture discipline.

The main correction is strategic:

> Less “clinical ECG simulator”, more “deterministic ECG/PPG algorithm QA testbench.”
