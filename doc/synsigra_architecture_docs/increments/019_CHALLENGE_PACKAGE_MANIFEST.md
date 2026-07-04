# Challenge Package Manifest

**Document ID:** SYN-ARCH-INC-019

**Version:** 0.1

**Status:** Implementing

**Owner role:** Platform / SDK

**Date:** 2026-07-04

**Proposed traceability ID:** `TRC-CHAL-001`

**Implementation issue:** [signal_synth#34](https://github.com/tamask1s/signal_synth/issues/34)

## 1. Decision

Implement the first offline challenge package manifest contract. This
increment defines the strict `manifest.json` model that can describe a
directory or future archive package without requiring render-time validation.

The manifest enumerates:

- package identity;
- package type;
- included waveform formats;
- generator version;
- usage restrictions;
- non-clinical limitation text;
- file roles, media types, checksums, and sizes;
- scenario cases and their associated files.

## 2. Applicable Requirements

- `REQ-CHAL-003..009`;
- `REQ-PKG-001..005`.

## 3. Public Contract

Add:

- `src/challenge_package.h`;
- `src/challenge_package.cpp`.

The manifest supports:

- `single_scenario`;
- `scenario_pack`.

File roles include:

- scenario JSON;
- pack JSON;
- metadata JSON;
- waveform CSV;
- annotations JSON;
- ground-truth metrics JSON;
- report HTML;
- README;
- WFDB header/signal/annotation placeholders;
- EDF/BDF placeholders;
- other.

The manifest is deterministic and canonicalized. Its package fingerprint is
the SHA-256 of canonical manifest JSON.

## 4. Strict Validation

The parser/writer reject:

- unknown fields;
- duplicate JSON object keys;
- missing required fields;
- unsupported schema version;
- unsafe package IDs and paths;
- missing ground truth;
- malformed checksums;
- duplicate file paths;
- case references to files not listed in the root file table;
- missing non-clinical limitation wording.

Validation does not read payload files and does not render scenarios. File
existence and checksum calculation are later packaging-layer responsibilities.

## 5. Non-Goals

- No ZIP/`.synsigra` archive writer in this increment.
- No WFDB/EDF/BDF payload writer in this increment.
- No Python package loader in this increment.
- No hosted SaaS implementation.

## 6. Verification

Add `TEST-CHALLENGE-PACKAGE-001`:

- valid manifest write/parse round trip;
- canonical JSON and package fingerprint stability;
- role name/from-name conversion;
- missing ground truth rejection;
- duplicate file path rejection;
- missing case file reference rejection;
- malformed checksum rejection;
- unknown field rejection;
- duplicate JSON key rejection.

Extend `TEST-BUILD-001` package smoke to include the installed challenge
package header.

## 7. DataBrowser/SVN Impact

The manifest module is shared core source. It should be copied to the
DataBrowser/SVN working copy and listed in the CodeBlocks project file for
source parity, although the current Windows adapter does not need to call it
yet.

## 8. Risks and Limitations

- The manifest records checksums but does not calculate them.
- Archive layout is defined by paths but no archive writer exists yet.
- Standard waveform roles are reserved before WFDB/EDF/BDF payload export is
  implemented.

## 9. Change Log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-07-04 | Added first challenge package manifest contract |
