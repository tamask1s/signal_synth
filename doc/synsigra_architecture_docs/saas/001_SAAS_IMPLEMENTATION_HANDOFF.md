# Synsigra SaaS Implementation Handoff

**Document ID:** SYN-SAAS-HANDOFF-001

**Version:** 0.1

**Status:** Planning handoff for a future implementation session

**Date:** 2026-07-04

## 1. Product Boundary

Synsigra should be positioned as a B2B/developer-tool platform for synthetic
biosignal ground-truth QA, not as a diagnostic product and not as a clinical
validation service.

Initial SaaS model:

1. Synsigra generates a scenario or pack.
2. The user downloads a challenge package containing waveform and ground truth,
   or retrieves the same through an API.
3. The user runs their own algorithm locally.
4. The user scores locally with the Synsigra Python package against packaged
   ground truth.

Do not build clinical claims, patient-data storage, diagnosis workflows, or
algorithm certification claims into the first SaaS.

## 2. Layer Model

Core C++ library:

- authoritative signal generation;
- scenario compilation;
- waveform export;
- ground-truth export;
- scoring primitives where implemented.

CLI:

- local render/validate/fingerprint;
- pack render/score;
- challenge package assembly;
- smoke-testable contract for automation.

Python package:

- user-facing local challenge loading;
- detection-output loading;
- scoring execution through the packaged CLI or future native bindings;
- report/artifact access.

SaaS API:

- scenario/pack catalog;
- challenge package generation job;
- artifact download;
- API retrieval of generated waveform/ground truth;
- audit metadata and reproducibility manifest;
- no upload of proprietary user algorithm required in the first model.

Web UI:

- browse scenarios and packs;
- configure safe parameter presets;
- generate and download challenge packages;
- view generated reports and audit metadata;
- manage API keys and usage.

## 3. First SaaS Increment

Build a thin hosted orchestration layer around the existing offline-first
library contracts:

- backend job accepts a scenario-pack request;
- backend invokes the existing CLI in an isolated worker;
- generated artifacts are stored as immutable package objects;
- response returns package manifest, download URL, render identity, generator
  version, and scenario fingerprints;
- local Python scoring remains the recommended user validation path.

Avoid implementing server-side user-algorithm execution in v1.

## 4. Recommended Tech Shape

Backend:

- HTTP API with explicit versioning, e.g. `/v1/packs`, `/v1/jobs`,
  `/v1/artifacts`;
- job queue for render work;
- object storage for artifacts;
- relational database for users, jobs, package metadata, API keys, and audit
  events;
- worker image contains the compiled Synsigra CLI and fixed generator version.

Python SDK:

- `synsigra.Client` for authenticated package requests and downloads;
- `synsigra.load_challenge()` for local directories and archives;
- local scoring remains compatible with downloaded packages.

Security:

- API-key auth initially, organization scoping from day one;
- package artifacts are immutable and scoped to organization/user;
- no patient data claims and no PHI workflow.

Auditability:

- store request JSON, canonical scenario JSON, pack fingerprint, generator
  version, git commit, CLI command, output manifest hash, and creation time;
- every package should be reproducible from manifest plus generator version.

## 5. Minimum API Objects

Scenario request:

- scenario JSON or pack ID;
- optional seed overrides only where allowed;
- requested export formats: WFDB and EDF+/BDF+ by default;
- optional report format.

Generation job:

- job ID;
- status: queued, running, succeeded, failed;
- scenario/pack fingerprint;
- generator version;
- artifact list;
- error object with stable code and message.

Challenge package:

- manifest;
- waveform files;
- ground-truth files;
- reports;
- standard export files;
- local scoring instructions.

## 6. Implementation Preconditions

Before starting SaaS code, confirm:

- all open export format conformance work is either closed or explicitly
  marked optional;
- challenge package manifest is stable enough for hosted artifact storage;
- Python scoring package can score downloaded challenge directories without
  repository checkout assumptions;
- CLI returns stable machine-readable error codes for job failures;
- generator version and git commit are exposed in every package.

## 7. Suggested First Session Tasks

1. Create a small SaaS architecture record under this folder with chosen stack.
2. Add an API SRS for offline challenge generation, excluding server-side user
   algorithm execution.
3. Scaffold backend service with one endpoint: create render job from an
   existing pack ID.
4. Add worker wrapper around `signal-synth pack render`.
5. Store output in local filesystem object-store abstraction first.
6. Add integration test that requests a pack, waits for completion, downloads
   manifest, and scores locally with Python.

## 8. Explicit Non-Goals For V1

- no medical diagnostic workflow;
- no patient records;
- no clinical validation claims;
- no user algorithm upload/execution;
- no hardware output;
- no generic ECG datastore positioning.
