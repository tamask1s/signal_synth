# SaaS Challenge Package Assembly

**Document ID:** SYN-INC-035

**Version:** 1.0

**Status:** Verified locally

**Owner role:** Engineering

**Date:** 2026-07-05

## 1. Decision

Add a reusable challenge-package assembly layer above scenario-pack rendering.
The generator now has a C++ API and CLI command that render a scenario pack
into a self-contained challenge directory with `manifest.json`, waveform
artifacts, ground truth, reports, standard exports, content SHA-256 values,
generator version, scenario fingerprints, and render identities.

The SaaS implementation shall call this contract instead of reconstructing
package manifests from rendered files.

## 2. Rationale

The previous pieces were individually useful but not sufficient for hosted
handoff:

- `pack render` generated useful artifacts but no challenge manifest;
- the Python scoring test loaded a challenge directory assembled manually in
  test code;
- hosted workers need one stable command that produces immutable package
  objects with explicit roles and hashes.

Keeping assembly in the C++ library prevents duplicate manifest logic across
CLI, Python, and SaaS worker code.

## 3. Scope

In scope:

- public C++ challenge assembly API;
- CLI worker contract for challenge generation;
- strict manifest generation using real content hashes and byte sizes;
- root package files and per-case files;
- Python scoring compatibility with CLI-generated challenge packages;
- release, CLI, package, and Python smoke coverage.

Out of scope:

- hosted SaaS service implementation;
- object-storage upload;
- user-algorithm execution;
- clinical validation claims;
- archive signing or long-term release evidence archival.

## 4. Requirements And Inputs

Applicable requirements:

- `REQ-CHAL-003..009`
- `REQ-PKG-001..005`
- `REQ-PY-001..008`
- `REQ-NFR-002..008`

Architecture inputs:

- `srs/001_OFFLINE_CHALLENGE_AND_PYTHON_SCORING_SRS.md`
- `srs/002_FORMATS_AND_IO_CONTRACTS_SRS.md`
- `increments/016_SCENARIO_PACK_BATCH_QA.md`
- `increments/019_CHALLENGE_PACKAGE_MANIFEST.md`
- `increments/022_LOCAL_PYTHON_SCORING_PACKAGE.md`
- `saas/001_SAAS_IMPLEMENTATION_HANDOFF.md`

Traceability ID:

- `TRC-SAAS-CHAL-001`

Implementation issue:

- [signal_synth#58](https://github.com/tamask1s/signal_synth/issues/58)

## 5. Public Contracts

### C++ API

Header:

- `src/challenge_assembly.h`

Primary types:

- `challenge_package_input_file`
- `challenge_package_case_input`
- `challenge_package_build_options`
- `challenge_package_build_result`

Primary functions:

- `build_challenge_package_manifest(...)`
- `challenge_file_role_for_export_artifact(...)`
- `challenge_package_content_sha256(...)`
- `challenge_package_default_usage_restrictions()`
- `challenge_package_default_not_for()`

The API accepts in-memory file payloads and produces a validated
`challenge_package_manifest` plus canonical JSON. It does not write files
itself; the CLI owns filesystem transactionality.

### CLI

Command:

```text
signal-synth pack challenge <pack.json> --out <new-directory>
```

Success stdout:

```text
status=challenge-rendered
output_directory=<path>
package_id=<pack-id>
scenario_count=<count>
pack_fingerprint=sha256:<64-hex>
package_fingerprint=sha256:<64-hex>
```

Failure behavior follows the existing CLI error contract: non-zero exit code,
empty stdout, and `stderr` beginning with `error=<stable-code>`.

The output directory must not already exist. On write failure, the CLI removes
the files and directories it created.

## 6. Output Layout

The command writes:

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

The manifest does not list `manifest.json` itself. This avoids a
self-referential hash. All other files are listed with role, media type,
`sha256:...`, size, and required flag.

## 7. Data Flow

1. SaaS or local automation supplies an existing pack JSON.
2. CLI parses and validates the pack.
3. CLI renders every referenced scenario in memory using the existing pack
   renderer.
4. CLI maps artifact names to challenge file roles.
5. `build_challenge_package_manifest(...)` validates and canonicalizes the
   package manifest.
6. CLI writes package files, case files, and `manifest.json`.
7. Python `synsigra.load_challenge()` loads the generated directory or archive
   for local scoring.

## 8. Compatibility

The command is additive. Existing `render`, `pack render`, and `pack score`
contracts are unchanged.

The public C++ header is installed with the package and covered by
`TEST-BUILD-001`. SaaS code should treat the CLI as the first integration
surface and the C++ API as the lower-level native surface.

## 9. Verification

Stable procedures:

- `TEST-CHALLENGE-PACKAGE-001`
- `TEST-CLI-001`
- `TEST-PYTHON-SCORING-001`
- `TEST-BUILD-001`
- full release `ctest --output-on-failure`

Local verification on 2026-07-05:

- release build completed;
- full release test suite passed: 31/31;
- targeted challenge package, CLI, and Python scoring tests passed.

CI run URL is recorded in the linked issue after push.

## 10. DataBrowser And SVN Impact

None. This increment adds repo-local library, CLI, Python-test, and
documentation behavior. It does not add a DataBrowser API or script, so no SVN
sync is required.

## 11. Risks And Limitations

- Package signing and immutable release archive are not implemented.
- Hosted object-storage metadata should store the manifest hash and generator
  commit externally as audit fields.
- The CLI currently writes a directory, not a zip/tar archive. SaaS may archive
  the directory after generation without changing manifest semantics.

## 12. Change Log

- 2026-07-05: Initial verified design and implementation record for issue #58.
