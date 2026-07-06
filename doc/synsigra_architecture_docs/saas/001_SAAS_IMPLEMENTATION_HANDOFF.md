# Synsigra SaaS Implementation Handoff

**Document ID:** SYN-SAAS-HANDOFF-001

**Version:** 1.0

**Status:** Ready for initial SaaS implementation

**Date:** 2026-07-05

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

Successful stdout is line-oriented and machine-readable:

```text
status=challenge-rendered
output_directory=<path>
package_id=<pack-id>
scenario_count=<count>
pack_fingerprint=sha256:<64-hex>
package_fingerprint=sha256:<64-hex>
```

Failure behavior follows the repository CLI contract: non-zero exit code,
empty stdout, and `stderr` beginning with `error=<stable-code>`.

The output directory must not already exist. The CLI performs rollback of
files/directories created during failed writes. SaaS orchestration may archive
the directory after generation, but should preserve the manifest paths
unchanged inside the archive.

Generated layout:

```text
<out>/
  manifest.json
  pack.json
  summary.json
  summary.csv
  index.html
  cases/<case-id>/
    scenario.json
    metadata.json
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
- generator version;
- generator git commit or container image digest;
- worker command and normalized arguments;
- creation time and completion time;
- immutable artifact storage URL/key;
- manifest JSON and/or manifest storage key;
- job status and stable error object on failure.

## 6. Implementation Preconditions

Already satisfied for initial SaaS implementation:

- challenge package manifest contract exists and is strict;
- `signal-synth pack challenge` produces a complete package directory;
- generated challenge directories are loadable by the Python package;
- WFDB and EDF+/BDF+ exports have deterministic writer tests and native-reader
  smoke tests;
- CLI has stable stdout/stderr and exit-code smoke coverage;
- generator version is recorded in the package manifest.

Still required in the SaaS service layer:

- capture generator git commit or container image digest outside the package
  manifest as audit metadata;
- store the package fingerprint and manifest hash as immutable object metadata;
- decide archive format and object-storage key convention;
- enforce organization/user authorization for package download;
- ensure Python SDK can download and load the archived package shape chosen by
  the SaaS layer.

## 7. Suggested First Session Tasks

1. Create a SaaS architecture record under this folder with chosen stack,
   deployment shape, database, queue, and object-storage abstraction.
2. Add an API SRS for offline challenge generation, excluding server-side user
   algorithm execution.
3. Scaffold backend service with one endpoint: create challenge job from an
   existing pack ID.
4. Add worker wrapper around `signal-synth pack challenge`.
5. Store output in local filesystem object-store abstraction first.
6. Add integration test that requests a pack, waits for completion, downloads
   `manifest.json`, downloads the package directory/archive, and loads it with
   `synsigra.load_challenge()`.
7. Add a second integration test that runs a simple local scoring flow against
   the downloaded package using CSV/JSON detection output.

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
