# Lossless Unsigned 64-bit JSON Identity

**Document ID:** SYN-ARCH-INC-051

**Version:** 0.1

**Status:** Implemented, local verification complete

**Owner role:** Core / Export Contract

**Date:** 2026-07-17

**Traceability:** `TRC-JSON-ID-001`

**Issue:** [signal_synth#71](https://github.com/tamask1s/signal_synth/issues/71)

## 1. Decision

Every unsigned 64-bit fingerprint emitted as a standalone JSON value shall use
a canonical decimal string. The affected fields are:

- `generation_fingerprint` in `annotations.json`, `metadata.json`,
  `provenance.json`, and `comparison.json`;
- `resolved_generation_fingerprint` in `metadata.json` and `provenance.json`;
- `ecg_run_fingerprint` in `metadata.json` and `provenance.json`.

The lexical form is one or more ASCII decimal digits with no sign, exponent,
fraction, whitespace, or leading-zero padding. The numeric value and all hash
inputs remain unchanged.

## 2. Rationale

JSON does not impose a portable integer width. Valid generator fingerprints
can exceed both the JavaScript exact-integer limit and the signed 64-bit range.
Emitting them as numeric tokens can therefore cause silent rounding or complete
document rejection in otherwise conforming consumers. A decimal string is
lossless across C, C++, Python, and JavaScript JSON implementations.

## 3. Compatibility

The C++ scenario, render, and facade APIs retain their existing unsigned
integer types. SHA-256 document/package fingerprints and composite
`render_identity` values remain unchanged because they are already strings.
CLI `key=value` diagnostics are not JSON and retain their existing form.

JSON uses decimal strings. Numeric legacy values are not part of the supported
contract; development packages using them must be regenerated. No duplicate or
transitional field is introduced.

## 4. Verification

`TEST-ECG-EXPORT-001` renders a fixture whose generation and run fingerprints
both exceed `INT64_MAX`, then verifies exact decimal strings in annotations,
metadata, and provenance. `TEST-COMPARE-001` verifies the same representation
in comparison JSON. Existing deterministic artifact and package tests guard
against unrelated export changes.

## 5. Integration Impact

No DataBrowser/SVN synchronization is required. The modified export and
comparison modules are deliberately outside the generation-only DataBrowser
source map. The SaaS workaround may be removed only after it supports the new
producer output while retaining legacy package input compatibility.

## 6. Acceptance Criteria

- No standalone unsigned 64-bit fingerprint is emitted as a JSON number.
- Values above signed 64-bit range remain byte-exact decimal text.
- C++ identity APIs and deterministic fingerprints are unchanged.
- Distribution and verifier fixtures use the canonical string representation.
- Release and sanitizer test suites pass.
