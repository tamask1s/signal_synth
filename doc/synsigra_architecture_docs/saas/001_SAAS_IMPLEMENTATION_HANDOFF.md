# Synsigra SaaS Implementation Handoff

**Document ID:** SYN-SAAS-HANDOFF-001

**Version:** 3.0

**Status:** Core v6 migration baseline defined; exact verified commit pending

**Date:** 2026-07-18

**SaaS migration issue:** [signal_synth_saas#68](https://github.com/tamask1s/signal_synth_saas/issues/68)

**Core closure issue:** [signal_synth#91](https://github.com/tamask1s/signal_synth/issues/91)

**Exact runtime baseline:** `TO_BE_RECORDED_AFTER_CORE_VERIFICATION`

## 1. Product Boundary

Synsigra is a B2B/developer tool for synthetic ECG/PPG algorithm QA. The first
hosted workflow is deliberately offline-first:

1. Synsigra generates a curated scenario pack.
2. The customer downloads waveform, ground truth, submission templates, and
   the pre-specified verification protocol in one immutable challenge.
3. The customer runs its proprietary algorithm locally.
4. The customer scores declared CSV/JSON output locally with the pure-Python
   `synsigra` verifier.

The service does not execute customer algorithms and does not need generator
source in the customer package. It is not a diagnostic product, patient-data
store, clinical validation service, certification service, or generic
synthetic cardiology data platform.

## 2. Authoritative Layer Model

Core C++ library:

- signal and ground-truth generation;
- scenario compilation and deterministic identity;
- WFDB, EDF+/BDF+, CSV, JSON and report rendering;
- scoring primitives and challenge manifest assembly.

`signal-synth` CLI:

- the only recommended SaaS worker boundary;
- machine-readable preflight through `signal-synth contract`;
- strict pack validation, analysis, challenge generation and stable errors;
- no HTTP, database, tenancy, queue or object-storage responsibility.

Pure-Python `synsigra` package:

- customer-facing and generator-free;
- strict challenge/archive trust boundary;
- strict `synsigra_submission_v1` input;
- local scoring and evidence reports for every published scoreable target.

SaaS:

- authentication, organizations and quotas;
- curated catalog/API/UI;
- isolated render jobs and approved external-noise asset registry;
- immutable artifact storage and download authorization;
- audit metadata, but no regeneration or reinterpretation of core truth.

## 3. Frozen Contract Tuple

The SaaS migration must require this exact tuple. Do not accept an older value
and do not add compatibility adapters:

| Contract | Required value |
|---|---|
| Generator | `0.9.0-dev` |
| Installed CMake package | `0.9.0` (exact; no CMake version compatibility range) |
| C++ facade | `1.4.0` |
| Core integration | `synsigra_core_integration_v6` |
| Pack schema | `2` |
| Scenario schemas | `2` through `9` |
| Challenge package | `synsigra_challenge_package_v3` |
| Challenge manifest JSON schema | `1` |
| Scoring manifest | `synsigra_scoring_manifest_v2` |
| Verification protocol | `synsigra_verification_protocol_v1` |
| Submission | `synsigra_submission_v1` |
| Submission formats | `synsigra_submission_formats_v1` |
| Authoring metadata | `synsigra_authoring_v18` |
| Scenario templates | `synsigra_templates_v5` |
| Curated catalog | `3.0` |
| Python verifier | `0.9.0` |

The scoring manifest and submission schemas did not change. Pack schema v1,
challenge package v2, integration v5, catalog 2.x and verifier 0.8 are removed
baselines, not alternative inputs.

## 4. Worker Contract

At image build and worker startup, execute:

```text
signal-synth contract
```

Parse the one-line JSON strictly and compare every implemented contract value,
not only the top-level integration ID. Refuse work if the binary, linked core,
catalog or verifier tuple differs.

Generate a curated pack with:

```text
signal-synth pack challenge <trusted-pack-path> --out <new-directory>
```

For approved external-noise packs:

```text
signal-synth pack challenge <trusted-pack-path> --out <new-directory> --noise-assets <approved-private-directory>
```

Success writes one compact JSON receipt to stdout:

```json
{"schema_version":1,"contract":"synsigra_core_integration_v6","status":"challenge_rendered","output_directory":"<path>","package_id":"<pack-id>","scenario_count":4,"pack_fingerprint":"sha256:<64-hex>","package_fingerprint":"sha256:<64-hex>","generator":{"name":"signal_synth","version":"0.9.0-dev","git_commit":"<commit>","build_identity":"<identity>"},"contracts":{"challenge_package":"synsigra_challenge_package_v3","scoring_manifest":"synsigra_scoring_manifest_v2","verification_protocol":"synsigra_verification_protocol_v1"}}
```

Failure has non-zero exit status, empty stdout, and stderr beginning with
`error=<stable-code>`. The output directory must be new. Preserve the generated
relative paths exactly when archiving.

## 5. Worker Security Boundary

The initial SaaS accepts a curated `pack_id`, not a customer-supplied pack path.
Resolve that ID from the pinned catalog to a trusted image path. Pack scenario
paths are authoring inputs and are not a filesystem sandbox API.

Run each render in an isolated worker with:

- no user-controlled filesystem path arguments;
- bounded CPU, memory, wall time and output size;
- a fresh output directory;
- no network unless explicitly required by orchestration;
- read-only core/catalog content;
- only operator-approved noise assets mounted read-only;
- cleanup on failure and immutable publication on success.

External-noise source bytes never enter a challenge. The service resolves each
declared asset ID from its private registry, verifies SHA-256 and license
review, and blocks publication if core truth reports `local_only` or
`release_allowed=false`.

## 6. Challenge Package v3

Representative layout:

```text
<challenge>/
  manifest.json
  pack.json
  scoring_manifest.json
  verification_protocol.json       # only when declared by the pack
  provenance.json
  ENGINEERING_CLAIM_BOUNDARY.txt
  summary.json
  summary.csv
  index.html
  realism_population.json
  user-output-template/
    submission.json
    formats.json
    outputs/<case-id>/<target>.json
  cases/<case-id>/
    scenario.json
    case_summary.json
    waveform.csv
    annotations.json
    ground_truth_metrics.json
    measurement_truth.json          # measurement targets
    provenance.json
    warnings.json
    report.html
    README.txt
    synsigra.hea / synsigra.dat / synsigra.atr
    synsigra.edf / synsigra.bdf
    <declared wearable, optical, cardiorespiratory or noise truth artifacts>
```

`manifest.json` declares `contract: synsigra_challenge_package_v3` and lists
every package file except itself. Every entry has exactly `path`, `role`,
`media_type`, `sha256`, and `size_bytes`. The old `required` flag was removed:
a listed artifact exists and is integrity-bound; an absent optional artifact
is simply not listed.

Important singleton roles:

- `pack_json`;
- `scoring_manifest_json`;
- `submission_manifest_json`;
- `submission_formats_json`;
- `verification_protocol_json` when present;
- `realism_population_json`.

Do not infer these from filenames. Challenge package v3 consumers reject
unknown roles; new role names require a package-contract revision. The generic
`other` role remains available for integrity-bound artifacts without dedicated
consumer behavior.

## 7. Verification Protocol

Pack schema v2 may declare `verification_protocol_path`. Challenge assembly
copies it to `verification_protocol.json` and assigns the dedicated role. The
common v1 envelope provides:

- protocol and pack identity;
- context of use and engineering evidence boundary;
- pre-specified threshold profile;
- required targets;
- objective acceptance rules;
- stress matrix and truth policy;
- optional domain-specific matrices and negative cases.

Catalog 3.0 includes the same normalized protocol document and source hash, so
the UI can explain a pack before generation. The challenge copy remains
authoritative for a specific downloaded package. The SaaS must not synthesize,
relax or silently override acceptance criteria.

## 8. Customer Verification Workflow

Distribute the generated `user-output-template/` unchanged and distribute the
pure-Python `synsigra` 0.9.0 wheel separately. The canonical customer command
is:

```text
synsigra-verify <challenge-or-archive> <submission-directory> <result-directory> --profile <profile>
```

The loader rejects duplicate JSON keys, unknown manifest fields/roles,
non-canonical or case-colliding paths, ZIP duplicates and prefix conflicts,
encrypted/symlink entries, unlisted files, missing files, symlink escapes,
size mismatch and SHA-256 mismatch. Do not weaken these checks in SaaS-created
archives.

Stable customer output families:

- point events: `point_events_json_v1`, `point_events_csv_v1`;
- intervals: `interval_events_json_v1`, `interval_events_csv_v1`;
- HRV: `hrv_metrics_json_v1`;
- measurements: `measurement_values_json_v1`,
  `measurement_values_csv_v1`.

Store raw per-target comparison JSON/CSV/HTML and the top-level verification
summary when a customer chooses to upload reports. Do not require proprietary
algorithm upload in v1.

## 9. Curated Catalog Contract

Ingest `examples/catalog/curated_pack_metadata_v1.json` version 3.0 as an
immutable release snapshot. It contains:

- declared and effective targets;
- scoreable versus reference-only behavior;
- accepted customer formats and primary metrics;
- case/rate/channel/size estimates;
- release status and intended/not-recommended use;
- exact generator/verifier compatibility;
- output artifact roles;
- normalized verification protocol when available.

Do not maintain a hand-written duplicate target or pack registry in SaaS code.
Store the catalog version and source hash with each generation job.

## 10. Persistence and Clean Migration

This is a private-beta breaking reset. Before loading the v6 baseline:

1. stop workers and API writes;
2. delete development jobs, packages, artifact rows, result rows, cached
   authoring metadata, cached catalog JSON, and old object-storage artifacts;
3. remove migration/adapter code used only for integration v5, pack schema v1,
   challenge v2, catalog 2.x or verifier 0.8;
4. build a worker image from the exact core commit in this handoff;
5. load only catalog 3.0 and verifier 0.9.0;
6. create fresh database schema/state and fresh object-storage namespace;
7. persist full startup preflight JSON, worker image digest, catalog hash,
   request, receipt, manifest, package fingerprint and timestamps per job.

Suggested immutable package record fields:

- organization/user and job ID;
- curated pack ID/version and catalog version/hash;
- pack and package fingerprints;
- core integration tuple and startup preflight JSON;
- generator version, git commit and worker image digest;
- normalized command arguments and approved noise asset identities;
- manifest storage key/hash and archive storage key/hash;
- creation/completion time and stable failure object.

## 11. SaaS Acceptance Matrix

Required before enabling customer access:

1. Startup rejects one deliberately wrong integration contract.
2. Every catalog 3.0 pack can be analyzed; representative ECG, HRV, PPG,
   wearable, noise, RR and QTc packs can be generated.
3. Generated directories load and integrity-check with installed `synsigra`
   0.9.0 before archival.
4. Downloaded archives load and integrity-check after object-storage roundtrip.
5. Perfect fixture submissions pass and deliberately biased/missing outputs
   fail for event, interval, delineation, HRV and measurement families.
6. RR/noise, QTc and HRV challenges expose the expected protocol role,
   identity and pre-specified profile.
7. Tampered bytes, extra archive payload, duplicate member and unsafe path are
   rejected.
8. Cross-organization artifact access is rejected and audited.
9. Local-only external noise cannot be published; approved rendered-output
   noise can be published without source bytes.
10. A deleted/retried failed job leaves no downloadable partial package.

## 12. Explicit Non-Goals and Residual Work

- no diagnosis, patient monitoring, clinical validation or regulatory
  sufficiency claim;
- no customer algorithm execution or PHI workflow;
- no arbitrary uploaded pack execution in the initial service;
- no backward compatibility with the reset development baseline;
- no package signing yet: SHA-256 protects integrity after a trusted manifest
  is obtained, but publisher authenticity still depends on authenticated HTTPS
  delivery and trusted object metadata;
- true streaming long-record export remains
  [signal_synth#78](https://github.com/tamask1s/signal_synth/issues/78);
- optional QTDB interoperability remains
  [signal_synth#88](https://github.com/tamask1s/signal_synth/issues/88).

## 13. Source References

- `doc/synsigra_architecture_docs/increments/067_CORE_CONTRACT_HARDENING_AND_SAAS_HANDOFF.md`
- `doc/synsigra_architecture_docs/increments/035_SAAS_CHALLENGE_PACKAGE_ASSEMBLY.md`
- `doc/synsigra_architecture_docs/srs/001_OFFLINE_CHALLENGE_AND_PYTHON_SCORING_SRS.md`
- `doc/synsigra_architecture_docs/srs/002_FORMATS_AND_IO_CONTRACTS_SRS.md`
- `examples/catalog/curated_pack_metadata_v1.json`
- `python/README.md`
