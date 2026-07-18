# Core Contract Hardening and SaaS Handoff Closure

**Document ID:** SYN-ARCH-INC-067

**Issue:** [signal_synth#91](https://github.com/tamask1s/signal_synth/issues/91)

**Version:** 0.9

**Status:** Locally verified; immutable commit and CI record pending

**Owner role:** Engineering / Quality

**Date:** 2026-07-18

## 1. Decision

Close the current core/SaaS boundary as one breaking, internally consistent
baseline instead of preserving transitional schemas or filename conventions.
The release advances to:

- generator `0.9.0-dev` and installed CMake package `0.9.0`;
- C++ facade `1.4.0`;
- `synsigra_core_integration_v6`;
- pack schema version 2;
- `synsigra_challenge_package_v3`;
- authoring metadata `synsigra_authoring_v18`;
- curated catalog `3.0`;
- generator-free Python verifier `0.9.0`.

The scoring manifest remains `synsigra_scoring_manifest_v2` and customer
submissions remain `synsigra_submission_v1`; neither contract changes in this
increment.

## 2. Audit Findings

The repository-wide review found four closure gaps:

1. curated HRV, noisy RR, and QTc packs define machine-readable acceptance
   protocols beside the pack, but challenge assembly does not deliver them;
2. scoring and submission contract files are discovered through fixed paths
   rather than explicit challenge-manifest roles;
3. the C++ challenge parser is strict, while the customer-facing Python loader
   accepts duplicate JSON keys, unknown fields and roles, ambiguous archive
   names, unlisted payloads, and filesystem symlink traversal;
4. CMake installation has no package version file, PPG source/header entries
   are appended separately, and a second unused Python `pyproject.toml`
   duplicates the authoritative build configuration.

Target-family lists are also repeated inside the Python verifier. This makes a
new target easy to add to one dispatch branch but omit from another.

## 3. Requirements

- `CORECLOSE-REQ-001`: pack schema v2 shall optionally declare one local
  verification protocol file; schema v1 shall be rejected.
- `CORECLOSE-REQ-002`: protocol-enabled challenge packages shall contain the
  exact normalized protocol as `verification_protocol.json` with a dedicated
  manifest role.
- `CORECLOSE-REQ-003`: scoring manifest, submission manifest, submission
  formats, and verification protocol shall have dedicated manifest roles.
- `CORECLOSE-REQ-004`: the Python loader shall validate the complete challenge
  manifest before exposing package objects.
- `CORECLOSE-REQ-005`: directory and archive loading shall reject unsafe,
  non-canonical, duplicate, case-colliding, conflicting, unlisted, missing, or
  symlinked package paths.
- `CORECLOSE-REQ-006`: integrity verification shall bind every listed artifact
  to path, byte size, and SHA-256 before scoring.
- `CORECLOSE-REQ-007`: Python target families shall be defined once and reused
  for dispatch and result classification.
- `CORECLOSE-REQ-008`: installed CMake consumers shall be able to request
  package version 0.9 and shall continue to compile as C++11.
- `CORECLOSE-REQ-009`: only one root Python build-system declaration shall
  remain; the wheel shall remain pure Python and generator-free.
- `CORECLOSE-REQ-010`: the SaaS handoff shall identify one immutable core
  commit, all required contract versions, migration deletions, worker security
  constraints, and end-to-end acceptance checks.

## 4. Public Contract

### 4.1 Pack schema v2

The top-level optional field is:

```json
"verification_protocol_path": "<safe local JSON filename>"
```

The path is relative to the pack file and is deliberately restricted to a
single filename. A pack cannot use it to copy arbitrary worker files. Packs
without pre-specified acceptance criteria omit the field.

Every delivered protocol uses the common
`synsigra_verification_protocol_v1` envelope with `protocol_id`, `pack_id`,
context of use, pre-specified profile, required targets, acceptance rules,
stress definition, truth policy, and engineering evidence boundary. Domain
specific fields remain allowed inside that envelope.

### 4.2 Challenge package v3

New singleton package roles are:

- `scoring_manifest_json`;
- `submission_manifest_json`;
- `submission_formats_json`;
- `verification_protocol_json`.

The Python API resolves these files by role. It does not infer their location
from a filename. `verification_protocol()` raises `KeyError` when a pack does
not declare a protocol; it does not manufacture default acceptance criteria.

Challenge paths use portable forward-slash form. Empty components, `.`, `..`,
backslashes, drive prefixes, control characters, duplicate case-folded names,
and file/directory prefix conflicts are invalid.

The top-level manifest identifies itself explicitly with
`contract: synsigra_challenge_package_v3`; schema shape is not used as an
implicit version detector.

### 4.3 Python trust boundary

`load_challenge()` treats a downloaded package as untrusted input. It performs
strict JSON duplicate-key and schema validation before returning a package.
ZIP extraction validates every member before writing any member. Integrity
verification rejects unlisted files and symlinks in addition to missing,
size-mismatched, and hash-mismatched files.

Scoring continues to verify integrity before loading a submission. Generator
source and native code are not part of the Python distribution.

### 4.4 Build and installation

The CMake project owns version `0.9.0` and installs both config and config
version files. Pre-1.0 consumers must request the exact package baseline:

```cmake
find_package(signal_synth 0.9.0 EXACT CONFIG REQUIRED)
```

There is one authoritative root `pyproject.toml`; package metadata remains in
`setup.cfg`.

## 5. Compatibility and Migration

This is an intentional private-beta breaking change:

- pack schema v1 is removed rather than parsed through a compatibility path;
- challenge consumers must require package contract v3 and integration v6;
- SaaS code must discover singleton artifacts by manifest role;
- old generated challenge packages, database rows, cached catalog snapshots,
  worker images, and verifier wheels are deleted instead of migrated;
- users regenerate packages with the new worker and install verifier 0.9.0.

Scenario schema 2 through 9 and scoring manifest v2 remain unchanged.

## 6. Verification

- C++ pack and challenge tests cover schema/role serialization and rejection.
- Python package tests cover valid directory/archive loading and malformed
  manifest, duplicate key, unsafe path, duplicate member, path conflict,
  unlisted file, missing file, tamper, and symlink rejection.
- End-to-end RR/QTc and HRV tests assert the packaged protocol identity and
  pre-specified profile.
- Catalog and CLI contract tests assert the complete v6 version tuple.
- `TEST-BUILD-001` checks versioned installed-package consumption.
- Release CTest, sanitizer behavior CTest, wheel/sdist smoke, warning scan,
  GCC 4.9/C++11 DataBrowser smoke, and catalog regeneration are required.

Local verification on 2026-07-18:

- warning-clean C++11 clean build with `-Wall -Wextra -Wpedantic`;
- release CTest: 63/63 passed, including exact installed-package consumption;
- ASan+UBSan behavior CTest: 62/62 passed with leak detection disabled because
  LeakSanitizer is unavailable under the execution environment's ptrace;
- pure-Python wheel and sdist build plus installed-wheel pass/fail smoke;
- 126 repository JSON documents parsed without duplicate keys;
- Python bytecode compilation passed;
- DataBrowser dry-run: 55 mapped files SHA-256 identical.

## 7. DataBrowser Impact

No generation model, scenario schema, rendering source, DataBrowser API, or
visualization script changes in this increment. Challenge assembly, scoring,
Python packaging, and CMake package metadata stay Git-only. The existing
Git/SVN generation subset shall remain byte-identical and its dry-run sync
check shall pass.

## 8. SaaS Handoff

The service shall accept curated pack IDs only at the worker boundary. An
arbitrary customer pack path must not be passed to the CLI in the initial SaaS
because scenario source paths are an authoring input, not a sandbox API.

The worker preflights `signal-synth contract`, requires the exact v6 tuple,
generates into a new directory, verifies the manifest, archives without path
changes, and publishes the packaged verification protocol beside waveform,
truth, submission templates, and reports. Local verifier 0.9.0 remains the
customer scoring boundary.

## 9. Residual Limitations

- Package authenticity/signing is not implemented; SHA-256 detects content
  corruption after a trusted manifest is obtained but does not identify the
  publisher.
- True streaming export remains issue #78.
- QTDB interoperability remains issue #88.
- Synthetic evidence remains engineering QA, not clinical validation or
  regulatory sufficiency.

## 10. Change Log

- 2026-07-18: version 0.9, implementation and local verification complete;
  immutable commit and GitHub CI evidence pending.
- 2026-07-18: version 0.1, audit findings and implementation design accepted
  under issue #91.
