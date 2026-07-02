# Scenario Pack and Batch QA Package

**Document ID:** SYN-ARCH-INC-016

**Version:** 0.2

**Status:** Verified

**Owner role:** Platform / Verification

**Date:** 2026-07-02

**Proposed traceability ID:** `TRC-PACK-001`

**Implementation issue:** [signal_synth#27](https://github.com/tamask1s/signal_synth/issues/27)

**Implementation commit:** `56509c38779e937b0fc16b414047a0b812defc39`

**Verified CI run:** [Verification 28613327848](https://github.com/tamask1s/signal_synth/actions/runs/28613327848)

## 1. Decision

Implement the first scenario-pack layer above individual ECG/PPG scenario
documents. A pack is a deterministic manifest that references existing
scenario JSON files, assigns stable case IDs, declares QA targets, and can be
validated, fingerprinted, and rendered as one batch.

The pack layer does not introduce new physiological models. It packages already
versioned scenarios into reproducible stress-test collections and exports
pack-level summaries for customer-facing QA workflows.

## 2. Product Rationale

The product value is a synthetic biosignal ground-truth testbench, not a data
store. After single-scenario render/export/report support, the next useful
B2B/developer-tool capability is a repeatable validation pack:

- R-peak detector stress cases;
- HRV and beat-timeline stress cases;
- ECG/PPG alignment cases;
- signal-quality and artifact cases;
- combined worst-case engineering cases;
- pack-level audit trail and reproducible output identity.

This enables users to run a named pack, keep the generated artifacts, and
compare algorithm outputs against explicit ground truth without assembling
individual scenarios by hand.

## 3. Pack Manifest Contract

Add portable pack manifest schema version 1 with:

- `schema_version`: currently exactly `1`;
- `pack_id`: safe identifier;
- `name`;
- `version`;
- `description`;
- `targets`: pack-level target labels;
- `scenarios`: array of referenced scenario entries.

Each scenario entry contains:

- `id`: safe unique case ID inside the pack;
- `path`: relative path to an existing scenario JSON document;
- `targets`: scenario-level target labels.

The parser is strict:

- unknown fields are rejected;
- duplicate JSON object keys are rejected;
- missing fields and wrong types are rejected;
- duplicate scenario IDs are rejected;
- the canonical JSON and `sha256:` pack fingerprint are deterministic.

## 4. CLI Contract

Extend `signal-synth` with:

```text
signal-synth pack validate <pack.json>
signal-synth pack render <pack.json> --out <new-directory>
```

`pack validate` prints a machine-readable status line, pack ID, version,
scenario count, and pack fingerprint.

`pack render` creates a new output directory and renders each referenced
scenario into a subdirectory named by the pack scenario ID. The root directory
contains:

- `pack.json`: canonical pack manifest;
- `summary.json`: machine-readable pack summary;
- `summary.csv`: tabular pack summary;
- `index.html`: human-readable batch report index.

Each scenario subdirectory keeps the existing single-scenario export/report
contract.

## 5. Curated Pack v1 Contents

Add five first-class example packs:

- `r_peak_stress_v1`;
- `hrv_v1`;
- `ppg_alignment_v1`;
- `signal_quality_v1`;
- `combined_worst_case_v1`.

Together they reference 20 scenario entries across clean R-peak, slow/fast HR,
HRV variability, ECG/PPG pulse timing, acquisition artifact, and combined
stress cases.

These are engineering QA packs. They are not clinical validation datasets and
must not be marketed as diagnostic coverage.

## 6. DataBrowser and SVN Integration

This increment is primarily CLI/library-facing. It does not require a new
DataBrowser API because existing DataBrowser scripts visualize individual
scenario features.

The portable pack source files may be copied into the SVN ecosystem for source
parity, but existing `SignalProc_RSPT.cpp` does not need to call the pack API
yet. Future DataBrowser pack visualization can wrap `ecg_pack` once the Windows
application needs batch rendering from scripts.

## 7. Verification

Create `TEST-ECG-PACK-001` covering:

- typed pack manifest write and parse round trip;
- deterministic pack fingerprint generation;
- duplicate scenario ID rejection;
- all curated pack manifests parse successfully;
- every referenced scenario parses and renders;
- the curated pack set contains at least 20 rendered scenario entries;
- curated coverage includes artifact and PPG scenarios.

Extend `TEST-CLI-001` to cover `pack validate` and `pack render`, including
root summary file creation.

Existing package-smoke coverage must compile the public `ecg_pack.h` header
from an installed package.

Verified on 2026-07-02 with:

- release build and local CTest: 17/17 passed;
- sanitizer build and local CTest: 16/16 passed with
  `ASAN_OPTIONS=detect_leaks=0`, `LSAN_OPTIONS=detect_leaks=0`, and
  `TEST-BUILD-001` excluded;
- manual `combined_worst_case_v1` pack render smoke output;
- GitHub Actions `Verification` run `28613327848`: Ubuntu C++11 and Windows
  C++11 jobs passed;
- DataBrowser/SVN working-copy synchronization checked by byte-compare for
  copied `ecg_pack.cpp` and `ecg_pack.h`; `SignalProc_RSPT.cbp` was updated to
  include them. `svn status` could not be executed because the local `svn`
  client is not installed.

## 8. Exit Criteria

1. Pack manifests have strict validation and deterministic fingerprints.
2. CLI batch render produces per-scenario exports plus root summaries.
3. Curated v1 packs cover R-peak, HRV, PPG alignment, signal quality, and
   combined stress workflows.
4. Release, sanitizer, package-smoke, Ubuntu, and Windows verification pass.
5. Traceability matrix, issue record, and implementation commit are linked.

## 9. Known Limitations

- Pack render is batch-oriented and leaves already written scenario outputs if
  a later scenario fails.
- Pack manifests reference local scenario files; remote URI resolution and
  embedded scenario documents are out of scope.
- Pack summaries include generation metrics, not algorithm-under-test result
  scoring.
- No formal release archive or signed V&V report is produced by this increment.

## 10. Change Log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-07-02 | Proposed and implemented scenario pack and batch QA package v1 |
| 0.2 | 2026-07-02 | Verified local release/sanitizer tests, CI, and DataBrowser source synchronization |
