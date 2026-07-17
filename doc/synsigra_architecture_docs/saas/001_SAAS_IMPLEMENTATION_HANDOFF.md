# Synsigra SaaS Implementation Handoff

**Document ID:** SYN-SAAS-HANDOFF-001

**Version:** 2.1

**Status:** Core contract closed; SaaS consumer migration required

**Date:** 2026-07-17

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
- scoring primitives where implemented;
- challenge-package manifest assembly through `src/challenge_assembly.h`.

CLI:

- machine-readable integration preflight through `signal-synth contract`;
- local render/validate/fingerprint;
- pack render/score;
- SaaS-ready challenge package assembly;
- smoke-testable contract for automation.

Pack metadata export:

- `examples/catalog/curated_pack_metadata_v1.json` is the SaaS-ingestable
  curated-pack metadata snapshot;
- regenerate it with `scripts/export_curated_pack_metadata.py`;
- it distinguishes declared targets, effective scoreable targets, and
  reference-only ground-truth outputs before job creation.

Python package:

- user-facing local challenge loading;
- strict `synsigra_submission_v1` loading with algorithm provenance supplied
  once per submission;
- pure-Python, generator-free local scoring for every currently scoreable
  target;
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
- use `curated_pack_metadata_v1.json` for pack cards, target badges,
  scoreability/reference-only labels, size estimates, release status, and
  profile support;
- configure safe parameter presets;
- generate and download challenge packages;
- view generated reports and audit metadata;
- manage API keys and usage.

## 3. First SaaS Increment

Build a thin hosted orchestration layer around the existing offline-first
library contracts:

- backend job accepts a scenario-pack request;
- backend invokes `signal-synth pack challenge <pack.json> --out <new-directory>`
  in an isolated worker;
- generated artifacts are stored as immutable package objects;
- response returns package manifest, download URL, render identity, generator
  version, and scenario fingerprints;
- local Python scoring remains the recommended user validation path.

Avoid implementing server-side user-algorithm execution in v1.

## 3.1 Worker Contract

The SaaS worker should use this command as the first integration surface:

```text
signal-synth pack challenge <pack.json> --out <new-directory>
```

Before accepting work, the worker image shall run `signal-synth contract`,
parse the JSON, and require the exact integration contract it implements.
This detects an accidentally mismatched linked library, CLI binary, or image.

Successful challenge stdout is one compact JSON document:

```json
{"schema_version":1,"contract":"synsigra_core_integration_v2","status":"challenge_rendered","output_directory":"<path>","package_id":"<pack-id>","scenario_count":4,"pack_fingerprint":"sha256:<64-hex>","package_fingerprint":"sha256:<64-hex>","generator":{"name":"signal_synth","version":"<version>","git_commit":"<commit>","build_identity":"<identity>"},"contracts":{"challenge_package":"synsigra_challenge_package_v2","scoring_manifest":"synsigra_scoring_manifest_v2"}}
```

Failure behavior follows the repository CLI contract: non-zero exit code,
empty stdout, and `stderr` beginning with `error=<stable-code>`.
The former line-oriented `key=value` success format is not supported.

The output directory must not already exist. The CLI performs rollback of
files/directories created during failed writes. SaaS orchestration may archive
the directory after generation, but should preserve the manifest paths
unchanged inside the archive.

Generated layout:

```text
<out>/
  manifest.json
  pack.json
  scoring_manifest.json
  provenance.json
  ENGINEERING_CLAIM_BOUNDARY.txt
  user-output-template/
    submission.json
    outputs/<case-id>/<target>.json
  summary.json
  summary.csv
  index.html
  cases/<case-id>/
    scenario.json
    case_summary.json
    provenance.json
    waveform.csv
    annotations.json
    rr_tachogram.csv
    hrv_metrics.json
    ground_truth_metrics.json
    warnings.json
    report.html
    README.txt
    synsigra.hea
    synsigra.dat
    synsigra.atr
    wfdb_metadata.json
    synsigra.edf
    synsigra.bdf
    edf_bdf_metadata.json
```

The SaaS must distribute `user-output-template/` unchanged. The UI may explain
formats from each scoring entry's uniform `accepted_formats`,
`recommended_format`, and `recommended_path`, but must not reconstruct paths
or emit target-specific `accepted_*_formats` fields. Customer verification is:

```text
synsigra-verify <challenge> <submission-directory> <result-directory>
```

The customer point-event schemas are `point_events_json_v1` and
`point_events_csv_v1`; interval schemas are `interval_events_json_v1` and
`interval_events_csv_v1`; HRV uses `hrv_metrics_json_v1`. ECG delineation
predictions contain event time, lead and kind only. Atrial/ventricular anchor
identity and truth evaluability remain report-side metadata.

`manifest.json` lists all package and case files except itself. This avoids a
self-referential hash. Every listed file has path, role, media type, SHA-256,
byte size, and required flag. Per-case records include case ID, scenario ID,
scenario path, scenario document fingerprint, render identity, and file paths.

Lower-level native integrations may call `build_challenge_package_manifest(...)`
from `src/challenge_assembly.h`, but the CLI is the recommended SaaS worker
boundary for v1.

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

Suggested SaaS database fields for a generated package object:

- package object ID;
- organization/user ownership;
- source pack ID or uploaded pack reference;
- pack fingerprint;
- package fingerprint from CLI stdout;
- integration contract version and full preflight contract JSON;
- generator version;
- generator git commit or container image digest;
- worker command and normalized arguments;
- creation time and completion time;
- immutable artifact storage URL/key;
- manifest JSON and/or manifest storage key;
- job status and stable error object on failure.

## 6. Core Integration Status

Available from the core:

- challenge package manifest contract exists and is strict;
- `signal-synth pack challenge` produces a complete package directory;
- generated challenge directories are loadable by the Python package;
- WFDB and EDF+/BDF+ exports have deterministic writer tests and native-reader
  smoke tests;
- CLI has JSON success, stable stderr, and exit-code smoke coverage;
- C++ and CLI expose the same versioned integration contract;
- generator version is recorded in the package manifest;
- package-level `provenance.json` records generator git commit when available,
  build identity, package contract, scoring contract and verifier version;
- package-level and per-case `ENGINEERING_CLAIM_BOUNDARY.txt` files record the
  deterministic engineering QA boundary.

Required in the SaaS service layer after this closure:

- replace the key/value challenge stdout parser with strict JSON parsing;
- reject unknown integration contracts and nested contract mismatches;
- compare worker receipt identity with startup preflight identity;
- index generator git commit or container image digest from package provenance
  and worker metadata as searchable audit metadata;
- store the package fingerprint and manifest hash as immutable object metadata;
- decide archive format and object-storage key convention;
- enforce organization/user authorization for package download;
- ensure Python SDK can download and load the archived package shape chosen by
  the SaaS layer.

## 7. SaaS Consumer Migration

1. Build the SaaS and worker against one pinned core commit.
2. Validate `signal-synth contract` once at service/worker startup.
3. Parse challenge stdout as JSON and validate all required fields and exact
   contract versions.
4. Persist the integration contract and receipt with the immutable job audit
   record.
5. Run a fresh-state end-to-end test that generates, downloads, verifies, and
   locally scores a current challenge package.

Do not add a compatibility parser for the old success format. If existing SaaS
state contains only development data, reset it instead of introducing schema,
job, or package migration code.

## 8. Explicit Non-Goals For V1

- no medical diagnostic workflow;
- no patient records;
- no clinical validation claims;
- no user algorithm upload/execution;
- no hardware output;
- no generic ECG datastore positioning.

## 9. References

- `doc/synsigra_architecture_docs/increments/035_SAAS_CHALLENGE_PACKAGE_ASSEMBLY.md`
- `doc/synsigra_architecture_docs/srs/001_OFFLINE_CHALLENGE_AND_PYTHON_SCORING_SRS.md`
- `doc/synsigra_architecture_docs/srs/002_FORMATS_AND_IO_CONTRACTS_SRS.md`
- `doc/synsigra_architecture_docs/18_TRACEABILITY_MATRIX.md`
- [signal_synth#58](https://github.com/tamask1s/signal_synth/issues/58)
- [signal_synth#73](https://github.com/tamask1s/signal_synth/issues/73)
