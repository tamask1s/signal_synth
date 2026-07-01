# Scenario CLI Validate and Fingerprint

**Document ID:** SYN-ARCH-INC-003

**Version:** 0.2

**Status:** Verified

**Owner role:** Developer Tools / Scenario API

**Date:** 2026-07-01

**Proposed traceability ID:** `TRC-CLI-001`

**Implementation issue:** [signal_synth#15](https://github.com/tamask1s/signal_synth/issues/15)

## 1. Decision

Add a small `signal-synth` executable with two commands:

```text
signal-synth validate <scenario.json|->
signal-synth fingerprint <scenario.json|->
```

The CLI is an adapter over `ecg_scenario_json`; it shall contain no duplicate
schema, semantic validation, canonicalization, or hashing logic.

## 2. Requirements and design inputs

Requirements:

- `REQ-GEN-002`, `REQ-GEN-004..006`;
- `REQ-SCN-006`;
- `REQ-API-001`;
- `REQ-NFR-002..003`, `REQ-NFR-006..008`;
- `REQ-VER-002`, `REQ-VER-010`;
- `REQ-DOC-001..002`.

Design inputs:

- `08_API_DESIGN.md`, CLI API and implementation order;
- `12_SECURITY_PRIVACY_AND_LICENSE.md`, local/no-patient-data posture;
- `15_IMPLEMENTATION_ROADMAP.md`, Milestone 1;
- `increments/002_VERSIONED_SCENARIO_JSON.md`;
- `17_TRACEABILITY_SOP.md`.

## 3. Command contract

`validate` writes these `key=value` lines to stdout on success:

```text
status=valid
schema_version=1
scenario_id=...
sample_count=...
document_fingerprint=sha256:...
generation_fingerprint=<unsigned decimal>
```

`fingerprint` writes:

```text
document_fingerprint=sha256:...
generation_fingerprint=<unsigned decimal>
```

Errors are written to stderr, one per line:

```text
error=<stable-code> path=<JSON path> message=<text>
```

No command writes or modifies files in this increment.

## 4. Input and safety

- A filename reads binary bytes without newline conversion.
- `-` reads stdin.
- Empty input is rejected by the JSON codec.
- Input larger than 16 MiB is rejected before parsing.
- File open/read errors are distinct from scenario-validation errors.
- No network access, environment-dependent configuration, or patient data is
  required.
- Output uses the classic locale.

Exit codes:

| Code | Meaning |
|---:|---|
| 0 | Success |
| 2 | Invalid command-line usage |
| 3 | File or stdin read failure / input too large |
| 4 | Scenario JSON or semantic validation failure |
| 5 | Unexpected internal failure |

## 5. Build and installation

- Add functional `SIGNAL_SYNTH_BUILD_CLI`, default `ON` for the repository
  build.
- Build target name: `signal_synth_cli`.
- Installed executable name: `signal-synth`.
- Link only `signal_synth::signal_synth`.
- Package-library consumers may disable the CLI.

## 6. Verification

Add `TEST-CLI-001` covering:

- valid file validation;
- valid stdin validation;
- exact fingerprint output;
- non-canonical and canonical inputs produce identical output;
- malformed and semantically invalid scenarios return 4;
- missing file returns 3;
- unknown command and missing arguments return 2;
- stdout/stderr separation;
- 16 MiB limit;
- installed executable presence.

Use a versioned example scenario under `examples/scenarios/`. The test shall
invoke the built executable and shall not replicate codec behavior.

## 7. Compatibility and integration

- No library API or generated output changes.
- DataBrowser remains unaffected; no SVN sync or script is needed.
- The CLI output is stable for schema v1 but is not yet the final HTTP API.
- Controlled non-clinical wording remains in documentation and future reports;
  validate/fingerprint do not make medical claims.

## 8. Non-goals

- Render, export, report, batch, schema migration, or interactive editing.
- Colored/human-formatted diagnostics.
- SaaS or HTTP endpoint.
- DataBrowser API wrapper.
- Shell completion or package-manager distribution.

## 9. Acceptance criteria

1. Both commands satisfy the exact output and exit-code contract.
2. CLI and direct-library fingerprints match byte-for-byte.
3. The example scenario validates on Linux and Windows.
4. `TEST-CLI-001`, all existing suites, and installed package smoke pass.
5. CLI can be disabled without affecting the library build.
6. Traceability and README usage are updated.

## 10. Risks and limitations

| Risk | Control |
|---|---|
| CLI validation diverges from library | Adapter calls only the public codec |
| Scripts parse unstable prose | Fixed key/value stdout and stable exit codes |
| Unbounded file allocation | 16 MiB pre-parse limit |
| Platform newline changes fingerprint | Binary reads and canonical codec output |

The CLI does not yet produce a validation package; it only validates identity.

## 11. Change log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-07-01 | Initial proposed design |
| 0.2 | 2026-07-01 | Accepted implementation `4466251`; Linux and Windows verification passed in CI run `28506600387` |
