# Deterministic ECG Render, Export, and HTML Report

**Document ID:** SYN-ARCH-INC-004

**Version:** 0.1

**Status:** Implementing

**Owner role:** Export / Reporting / Developer Tools

**Date:** 2026-07-01

**Proposed traceability ID:** `TRC-EXP-001`

**Implementation issue:** [signal_synth#16](https://github.com/tamask1s/signal_synth/issues/16)

## 1. Decision

Add a library-level render/export facade and a CLI `render` command that turns
one validated schema-v1 ECG scenario into a deterministic engineering evidence
package:

```text
scenario.json
metadata.json
waveform.csv
annotations.json
ground_truth_metrics.json
warnings.json
report.html
README.txt
```

The library produces artifact bytes without filesystem access. The CLI owns
directory creation and file writing.

## 2. Requirements

- `REQ-GEN-001..006`;
- `REQ-ECG-001..004`, `REQ-ECG-006..007`;
- `REQ-GT-001..002`, `REQ-GT-004`;
- `REQ-EXP-001..005`;
- `REQ-RPT-001..003`;
- `REQ-API-001..003`;
- `REQ-NFR-001..005`, `REQ-NFR-008`;
- `REQ-VER-001..005`, `REQ-VER-008..010`;
- `REQ-DOC-001..002`.

Artifact interval requirements remain empty-but-explicit until the acquisition
layer exists.

## 3. Library contract

Add `ecg_export.h/.cpp`:

```cpp
struct ecg_render_bundle;
struct ecg_text_artifact;
struct ecg_export_bundle;
struct ecg_export_result;

bool render_ecg_document(const ecg_scenario_document& document, ecg_render_bundle& output, ecg_export_result& result);
bool build_ecg_export_bundle(const ecg_render_bundle& render, ecg_export_bundle& output, ecg_export_result& result);
```

`ecg_render_bundle` owns:

- normalized input document and canonical JSON identity;
- `clinical_ecg_record`;
- `ecg_scenario_report`;
- measured `ecg_morphology_report`;
- deterministic HRV/beat summary metrics.

`ecg_export_bundle` is an ordered list of UTF-8 text artifacts. It has no path,
filesystem, clock, customer, or SaaS dependency.

Both functions are transactional.

## 4. Artifact contracts

### Waveform CSV

- header: `sample_index,time_seconds,I_mv,...,V6_mv`;
- one row per sample;
- 12 standard lead names and mV units;
- classic locale and round-trip numeric precision;
- LF line endings on all platforms.

### Annotations JSON

- schema version and both scenario identities;
- beat annotations with timing, origin, RR, PR, QRS, QT/QTc and presence flags;
- atrial events and cross-references;
- construction and measured fiducials with lead, sample, time, amplitude;
- empty `artifact_intervals` array until artifacts are implemented.

### Ground-truth metrics JSON

- sample/beat/atrial/fiducial counts;
- mean HR and RR;
- SDNN, RMSSD, and pNN50 from generated RR truth;
- RR clipping count;
- origin counts;
- phenotype assertion values/ranges/status;
- explicit metric units.

### Metadata and warnings

- generator/product name and version;
- document SHA-256, generation fingerprint, run fingerprint;
- schema and engine versions;
- sample rate, duration, channel names and units;
- deterministic seed;
- warning/error records;
- controlled intended-use and limitation text.

No wall-clock timestamp is inserted into deterministic artifact bytes.
External SaaS/export audit records may add an export timestamp and ID without
changing the render identity.

### HTML report

The report includes:

- title and identity table;
- intended use and non-clinical limitation;
- scenario and condition summary;
- actual Lead II waveform preview as deterministic inline SVG;
- beat/HRV metrics;
- phenotype assertions;
- warnings and limitations;
- artifact file list.

All dynamic text is HTML-escaped. The report has no external assets, scripts,
network calls, or nondeterministic timestamps.

## 5. CLI contract

```text
signal-synth render <scenario.json|-> --out <new-directory>
```

On success:

```text
status=rendered
output_directory=...
artifact_count=8
document_fingerprint=...
generation_fingerprint=...
run_fingerprint=...
```

Rules:

- output directory must not exist;
- parent directory must exist;
- write binary artifact bytes unchanged;
- use temporary filenames, then rename after all writes succeed;
- remove files and the newly created directory after a failed write;
- never overwrite existing user data;
- render/semantic failure returns 4, output I/O failure returns 3.

## 6. Determinism and versioning

- Identical canonical document, generator build, and floating-point
  environment produce byte-identical artifacts.
- Artifact schema versions are explicit and independent from scenario schema.
- Add a runtime generator version string; it is not a promise of stable ABI.
- Manifest order and report section order are fixed.
- The run fingerprint remains the engine-owned 64-bit render identity.

## 7. Verification

Add `TEST-ECG-EXPORT-001` and extend `TEST-CLI-001`:

- render transactionality and repeatability;
- 12 equal CSV channels and row count;
- annotation sample/time consistency;
- independent known-answer HRV metrics;
- JSON escaping and finite-number guarantees;
- report disclaimer and prohibited-claim scan;
- inline SVG contains actual finite signal points;
- exact artifact names/order;
- byte-identical repeated package;
- CLI new-directory behavior and no-overwrite policy;
- failure cleanup;
- installed CLI render smoke;
- Linux/Windows output contract.

Golden verification shall use structural checks and selected hashes. Full
large waveform files are not committed.

## 8. Compatibility and integration

- Existing typed generation and JSON APIs remain unchanged.
- DataBrowser does not consume the export layer in this increment, so no
  `SignalProc_RSPT.cpp` API or script is required.
- The future DataBrowser adapter may save the same artifact bytes.
- No SVN synchronization is needed unless shared core files are changed.

## 9. Non-goals

- PPG, artifact injection, WFDB, EDF, PDF, ZIP, SaaS audit ID, customer
  watermark, or algorithm comparison.
- Clinical validation report or patient-equivalence claim.
- Arbitrary report templates or JavaScript plots.
- Overwriting or merging output directories.

## 10. Acceptance criteria

1. A schema-v1 scenario renders to all eight artifacts.
2. Repeated render/export is byte-identical in one defined environment.
3. CSV, annotations, metrics, warnings, metadata, and report agree with the
   generated record and fingerprints.
4. Report contains actual signal preview and required controlled wording.
5. CLI never overwrites an existing path and cleans failed new outputs.
6. Normal, sanitizer, Linux, and Windows verification pass.
7. Traceability and format specifications are updated.

## 11. Risks and limitations

| Risk | Control |
|---|---|
| Export truth diverges from record | Serialize only from owned render bundle |
| CSV/report locale differs | Classic locale and fixed line endings |
| HTML injection from metadata | Escape every dynamic string |
| Nondeterministic audit timestamp | Keep runtime audit metadata outside deterministic bytes |
| Partial output mistaken for complete | New directory, temp files, cleanup on failure |
| Report overinterpreted clinically | Fixed disclaimer and prohibited-claim test |

This package is synthetic engineering test evidence, not clinical validation.

## 12. Change log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-07-01 | Initial proposed design |
