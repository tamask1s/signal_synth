# Stable C++ Facade

**Document ID:** SYN-ARCH-INC-018

**Version:** 0.1

**Status:** Verified

**Owner role:** Platform / SDK

**Date:** 2026-07-04

**Proposed traceability ID:** `TRC-FACADE-001`

**Implementation issue:** [signal_synth#33](https://github.com/tamask1s/signal_synth/issues/33)

**Implementation commit:** `c11ada2daa2bc1ee9a85bd848ab9eb107f156363`

**Verified CI run:** [Verification 28705853662](https://github.com/tamask1s/signal_synth/actions/runs/28705853662)

## 1. Decision

Add a stable C++ facade for validation, render/export, and event-detector
comparison. The facade is the intended boundary for future Python bindings,
local SDK workflows, and hosted SaaS workers.

The facade does not replace the existing internal ECG/PPG/scenario/export
modules. It wraps them behind product-facing types that avoid exposing
`ecg_render_bundle`, `clinical_ecg_record`, `ecg_scenario_document`, or other
internal classes to SDK users.

## 2. Applicable Requirements

- `REQ-CHAL-001`: C++ remains the deterministic source of generation, export,
  and scoring behavior.
- `REQ-CHAL-002`: Python wraps stable C++ or CLI/facade behavior and does not
  reimplement generation/scoring.
- `REQ-CHAL-009`: Package format and later SaaS jobs use stable contracts.

## 3. Public Contract

Add:

- `src/synsigra_api.h`;
- `src/synsigra_api.cpp`.

Facade functions:

- `synsigra_validate_scenario_json`;
- `synsigra_render_scenario_json`;
- `synsigra_compare_scenario_detections`;
- `synsigra_default_compare_tolerance_seconds`;
- `synsigra_compare_target_name`;
- `synsigra_api_version`.

Facade types contain only standard C++11 library types:

- `std::string`;
- `std::vector`;
- scalar numeric fields.

This keeps the header suitable for direct C++ use, package-smoke compilation,
and a future Python binding layer.

## 4. Data Flow

```text
scenario JSON
  -> synsigra_validate_scenario_json
  -> strict scenario parser
  -> canonical JSON + fingerprints

scenario JSON
  -> synsigra_render_scenario_json
  -> strict parser
  -> render_ecg_document
  -> build_ecg_export_bundle
  -> facade artifact list

scenario JSON + detection events
  -> synsigra_compare_scenario_detections
  -> strict parser
  -> render_ecg_document
  -> compare_detections_to_render
  -> comparison artifact list
```

## 5. Non-Goals

- Do not implement the Python package in this increment.
- Do not implement hosted SaaS.
- Do not create challenge package manifests; that is issue `#34`.
- Do not move signal-generation or scoring semantics out of C++.

## 6. Compatibility

Existing lower-level APIs remain available. The facade is additive.

The CLI may continue to use lower-level modules until a later cleanup, but the
same validate/render/compare behavior must be expressible through the facade.

## 7. Verification

Add `TEST-FACADE-001`:

- validates scenario JSON through facade;
- renders ECG+PPG export artifacts through facade;
- derives R-peak detections from exported annotations;
- compares detections through facade;
- verifies identity, artifact, and metric contracts;
- verifies malformed scenario JSON returns structured messages.

Extend `TEST-BUILD-001` package smoke to include the installed facade header.

Verified on 2026-07-04 with:

- release build and local CTest: 19/19 passed;
- sanitizer build and local CTest: 18/18 passed with
  `ASAN_OPTIONS=detect_leaks=0`, `LSAN_OPTIONS=detect_leaks=0`, and
  `TEST-BUILD-001` excluded;
- GitHub Actions `Verification` run `28705853662`: Ubuntu C++11 and Windows
  C++11 jobs passed.

## 8. DataBrowser/SVN Impact

The facade source/header are shared core files. They should be synchronized to
the DataBrowser/SVN working copy after verification, and the CodeBlocks project
file should list them if the Windows adapter build uses the same source set.

## 9. Risks and Limitations

- The facade is still C++ class/struct based, not a C ABI. A C ABI can be added
  later if Python binding strategy requires it.
- Detection JSON/CSV v2 import is not part of this increment.
- Challenge package manifest/archive creation is not part of this increment.

## 10. Change Log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-07-04 | Added initial stable C++ facade design |
