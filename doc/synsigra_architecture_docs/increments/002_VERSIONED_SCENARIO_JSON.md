# Versioned ECG Scenario JSON Contract

**Document ID:** SYN-ARCH-INC-002

**Version:** 0.1

**Status:** Implementing

**Owner role:** Software Architecture / Scenario API

**Date:** 2026-07-01

**Proposed traceability ID:** `TRC-SCN-002`

**Implementation issue:** [signal_synth#14](https://github.com/tamask1s/signal_synth/issues/14)

## 1. Decision

Add a strict, versioned JSON document contract around `ecg_qa_scenario`.
Parsing and serialization shall be transactional, canonical, deterministic,
and independent of DataBrowser, ZAX, SaaS, and platform APIs.

The document receives a SHA-256 fingerprint over its canonical UTF-8 JSON.
This document fingerprint is distinct from the existing 64-bit generation
fingerprint:

- document SHA-256 identifies the exact portable scenario document;
- `ecg_qa_scenario::fingerprint()` identifies normalized generation semantics;
- later render fingerprints identify a generated run and sample count.

## 2. Rationale

The current typed scenario API already validates generation semantics, but it
cannot be stored, exchanged, used by a CLI, or reproduced outside C++ source.
A product-facing document is required before CLI, export, report, API, SaaS,
or scenario-pack work.

A constrained in-repository codec is selected for this first schema because:

- ZAX is an application integration dependency and not part of the portable
  core;
- no reviewed JSON dependency currently exists in the repository;
- the v1 schema is deliberately small and strict;
- commercial provenance remains explicit;
- unsupported JSON fields and values must fail rather than disappear.

The codec must implement standards-compliant parsing for accepted JSON syntax,
including escapes and Unicode surrogate pairs. It is not exposed as a generic
JSON library.

## 3. Requirements

- `REQ-GEN-001..006`
- `REQ-SCN-001..003`
- `REQ-SCN-006`
- `REQ-ECG-001..004`
- `REQ-ECG-006`
- `REQ-NFR-001..003`
- `REQ-NFR-005`
- `REQ-NFR-008`
- `REQ-VER-001..002`
- `REQ-DOC-001..002`

Event schedules, artifacts, PPG, and export options are reserved for later
schema-compatible increments and are not falsely accepted by this increment.

## 4. Public contract

Add `src/ecg_scenario_json.h/.cpp` with:

```cpp
struct ecg_scenario_document;
struct ecg_scenario_json_message;
struct ecg_scenario_json_result;

bool parse_ecg_scenario_json(const std::string& json, ecg_scenario_document& output, ecg_scenario_json_result& result);
bool write_ecg_scenario_json(const ecg_scenario_document& document, ecg_scenario_json_result& result);
```

The document contains:

- document schema version;
- scenario ID, name, description, author, and tags;
- duration in seconds and derived integral sample count;
- the typed `ecg_qa_scenario`.

The result contains:

- success flag;
- structured messages with stable code, JSON path, and text;
- canonical JSON on success;
- lowercase `sha256:<64 hex digits>` document fingerprint;
- normalized 64-bit generation fingerprint.

Failure shall preserve the destination document.

## 5. Schema v1

```json
{
  "schema_version": 1,
  "scenario_id": "ecg_clean_001",
  "name": "Clean ECG",
  "description": "Deterministic clean ECG engineering scenario.",
  "author": "",
  "tags": ["ecg", "clean"],
  "duration_seconds": 10,
  "sample_rate_hz": 500,
  "seed": 12345,
  "ecg": {
    "heart_rate_bpm": 70,
    "rr_variability_seconds": 0,
    "ectopic_every_n_beats": 0,
    "second_degree_av_pattern": "unspecified",
    "q_wave_territory": "unspecified",
    "fidelity_policy": "allow_parameterized",
    "conditions": [
      {"code": "NORM", "severity": 1}
    ]
  }
}
```

Rules:

- all listed top-level and `ecg` keys are required in schema v1;
- duplicate object keys are errors;
- unknown keys are errors;
- condition codes use canonical SCP strings from the catalog;
- duplicate conditions are errors;
- conditions are canonicalized in catalog order;
- tags are unique, non-empty, and canonicalized lexicographically;
- strings must be valid JSON/UTF-8 and must not contain NUL;
- numeric values must be finite and within typed API ranges;
- `duration_seconds * sample_rate_hz` must be a positive integral sample count
  representable by `unsigned int`;
- schema versions other than 1 fail explicitly;
- trailing non-whitespace data fails.

## 6. Canonicalization

Canonical output shall:

- use UTF-8;
- emit no insignificant whitespace;
- emit keys in the schema order shown above;
- sort tags lexicographically;
- emit conditions in catalog order;
- use canonical catalog code spelling;
- emit JSON escapes deterministically;
- serialize finite doubles with locale-independent round-trip precision;
- normalize negative zero to zero;
- contain exactly one trailing newline policy: no trailing newline in the
  canonical byte sequence.

Parsing any accepted non-canonical representation and writing it again must
produce the same canonical bytes and SHA-256 fingerprint.

## 7. Validation layers

1. JSON lexical and structural validation.
2. Schema key/type validation.
3. Range and cross-field validation.
4. Typed setter validation in `ecg_qa_scenario`.
5. `ecg_scenario_engine::validate` semantic validation.
6. Canonical serialization and fingerprint calculation.

No layer may silently clamp, infer an unsupported field, or discard input.
Existing condition implications remain a semantic engine responsibility and
are not written into the explicit condition array.

## 8. Error model

Stable initial message codes:

- `SCENARIO_JSON_SYNTAX`;
- `SCENARIO_JSON_DUPLICATE_KEY`;
- `SCENARIO_JSON_UNKNOWN_FIELD`;
- `SCENARIO_JSON_MISSING_FIELD`;
- `SCENARIO_JSON_TYPE`;
- `SCENARIO_JSON_RANGE`;
- `SCENARIO_JSON_SCHEMA_VERSION`;
- `SCENARIO_JSON_DUPLICATE_CONDITION`;
- `SCENARIO_JSON_DUPLICATE_TAG`;
- `SCENARIO_JSON_SEMANTIC`;
- `SCENARIO_JSON_INTERNAL`.

Paths use a JSONPath-like form such as `$.ecg.conditions[2].severity`.

## 9. Verification

Extend `TEST-ECG-SCENARIO-001` or add focused source under the same stable
suite to cover:

- default document roundtrip;
- non-canonical input canonicalization;
- stable known SHA-256 vector;
- semantic and document fingerprint distinction;
- key, tag, and condition ordering;
- every JSON scalar type mismatch;
- missing, unknown, and duplicate fields;
- duplicate tags and conditions;
- unsupported schema version;
- invalid escapes, UTF-8, and surrogate pairs;
- number overflow, non-integral sample count, and range boundaries;
- semantic engine rejection;
- transactional destination/result behavior;
- Linux/Windows canonical byte and fingerprint equality.

## 10. Compatibility and integration

- No existing public API or fingerprint changes.
- No generated sample changes.
- The new header is installed by the CMake package.
- DataBrowser does not need this codec because its typed ZAX adapter remains
  valid; no SVN sync is required for an additive unused module.
- Future DataBrowser import/export may call this portable codec explicitly.

## 11. Non-goals

- Generic JSON DOM API.
- JSON Schema validator dependency.
- File I/O or CLI behavior.
- Render/export/report behavior.
- PPG, artifacts, event schedules, or noise fields.
- Backward migration from an earlier JSON schema.
- Cryptographic authenticity or signatures.
- Clinical or regulatory validation claims.

## 12. Implementation sequence

1. Create and accept the implementation issue.
2. Add public document/result types.
3. Implement strict internal JSON parser and UTF-8 handling.
4. Implement schema mapping and semantic validation.
5. Implement canonical writer and SHA-256.
6. Add exhaustive tests and known-answer vectors.
7. Add header to install/package smoke tests.
8. Update specifications, traceability matrix, and legal provenance.
9. Run local normal and sanitizer builds, then Linux/Windows CI.

## 13. Acceptance criteria

1. Valid schema-v1 documents roundtrip to one canonical byte sequence.
2. Canonical SHA-256 is stable on Linux and Windows.
3. Every unsupported, unknown, duplicate, malformed, or semantically invalid
   input fails with a structured message and unchanged destination.
4. Existing generation fingerprints and all current outputs remain unchanged.
5. Installed-package consumers can use the new public API.
6. All seven suites, including a focused JSON suite if added, pass.
7. Traceability and provenance records identify the exact implementation and
   CI result.

## 14. Risks and limitations

| Risk | Control |
|---|---|
| Custom parser accepts malformed JSON | Negative corpus, UTF-8 and escape tests |
| Canonical bytes vary by locale/platform | Classic locale, round-trip precision, CI vectors |
| Metadata and generation hashes are confused | Separate names, types, docs, and report fields |
| Schema grows into internal model leakage | Explicit product-safe fields and versioning |
| Hash interpreted as signature | Document that SHA-256 is identity, not authenticity |

The v1 document represents the current ECG scenario subset only. It does not
yet satisfy the complete multimodal SRS.

## 15. Change log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-07-01 | Initial proposed design |
