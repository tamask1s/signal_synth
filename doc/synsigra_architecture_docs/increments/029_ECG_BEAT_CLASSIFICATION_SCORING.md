# ECG Beat Classification Ground Truth and Scoring

**Document ID:** SYN-ARCH-INC-029

**Version:** 1.0

**Status:** Verified

**Owner role:** Core generation / Algorithm QA

**Date:** 2026-07-04

**Traceability ID:** `TRC-ECG-BC-001`

**Implementation issue:** [signal_synth#44](https://github.com/tamask1s/signal_synth/issues/44)

**Implementation commit:** `80cb11396f0e52e7339c3453dfb4791fb4c75140`

**CI verification:** [GitHub Actions run 28712750254](https://github.com/tamask1s/signal_synth/actions/runs/28712750254)

## 1. Decision

Add a dedicated beat-classification QA module on top of existing exact R-peak
and beat-origin ground truth.

Canonical v1 classes:

- `normal`;
- `supraventricular_ectopic`;
- `ventricular_ectopic`;
- `paced`;
- `escape`;
- `unscored`.

`unscored` preserves construction truth for generated beats whose morphology
does not belong to the supported v1 classifier taxonomy. It participates in
timing matching and the confusion matrix but is excluded from score
denominators.

## 2. Applicable Requirements

- `REQ-ECG-BC-001`;
- `REQ-ECG-BC-002`;
- `REQ-ECG-BC-003`;
- `REQ-ECG-BC-004`.

## 3. Ground-Truth Mapping

| Clinical origin | Beat class |
|---|---|
| conducted | `normal` |
| PAC | `supraventricular_ectopic` |
| PVC | `ventricular_ectopic` |
| paced | `paced` |
| junctional or ventricular escape | `escape` |
| VT or unsupported future origin | `unscored` |

The mapping is centralized in the new public beat-classification module and is
reused by JSON, WFDB, and EDF+/BDF+ export.

## 4. User-Output Contract

Reuse detection JSON schema version 1 and CSV version 2 with target
`ecg_beat_classification`. Every event requires:

- `time_seconds`;
- canonical `label`;
- optional sample index, channel, and confidence.

Example:

```json
{
  "schema_version": 1,
  "algorithm": {"name": "customer_classifier", "version": "1.0"},
  "target": "ecg_beat_classification",
  "events": [
    {"time_seconds": 0.5, "label": "normal"}
  ]
}
```

## 5. Scoring Policy

1. Ground-truth and user events are paired one-to-one by minimum absolute time
   error within a configurable tolerance, default 75 ms.
2. A matched scored beat is correct only when the canonical classes are equal.
3. A wrong matched class contributes one false negative to the actual class and
   one false positive to the predicted scored class.
4. Unmatched scored ground truth contributes a false negative.
5. Unmatched scored user output contributes a false positive.
6. Matches to `unscored` ground truth are reported but do not affect scoring.
7. Reports include the full confusion matrix, per-class precision/recall/F1,
   overall micro precision/recall/F1, accuracy, timing error, unmatched events,
   scenario identity, and algorithm metadata.

## 6. Export Contract

- `annotations.json` adds canonical `beat_class` beside the detailed origin.
- WFDB `.atr` uses standard beat annotation codes for normal, PAC, PVC,
  junctional/ventricular escape, paced, and unknown beats.
- EDF+/BDF+ annotations use `beat:<canonical_class>` labels.
- Full detailed construction truth remains in `annotations.json`.

## 7. Interfaces

C++:

```cpp
bool beat_classification_events_from_detection(const detection_io_document& document, std::vector<ecg_classified_beat_event>& output, std::vector<std::string>& messages);
bool score_ecg_beat_classification(const ecg_render_bundle& render, const std::vector<ecg_classified_beat_event>& predictions, const ecg_beat_classification_options& options, ecg_beat_classification_result& result);
```

CLI:

```text
signal-synth compare beat-classes scenario.json classifications.json --out score-directory
```

Python:

```python
compare_beat_classes(case, detections, out_dir=None, cli_path=None, tolerance_ms=None)
```

## 8. Verification

Add `TEST-ECG-BEAT-CLASS-001`:

- complete origin-to-class mapping;
- strict canonical label import;
- perfect classification;
- timing jitter within tolerance;
- wrong-class confusion matrix entries;
- unmatched ground truth and user events;
- unscored behavior;
- deterministic JSON/CSV/HTML reports.

Extend export, detection IO, CLI, Python, and installed-package smoke tests.

Required completion evidence:

- release and sanitizer builds;
- full release CTest;
- sanitizer CTest excluding package build smoke;
- Linux and Windows CI;
- `git diff --check`.

Implemented interfaces and local integration now include:

- public C++ class mapping, scorer, and JSON/CSV/HTML reports;
- detection JSON/CSV target `ecg_beat_classification`;
- CLI and Python entry points;
- canonical beat classes in `annotations.json`;
- WFDB standard annotation codes and EDF+/BDF+ beat labels;
- perfect clean-ECG example classifications.

Verified locally on 2026-07-04:

- `cmake --build build-release`: passed;
- `ctest --output-on-failure` in `build-release`: 29/29 passed;
- `cmake --build build-sanitize`: passed;
- `ASAN_OPTIONS=detect_leaks=0 ctest -E TEST-BUILD-001 --output-on-failure`
  in `build-sanitize`: 28/28 passed;
- CLI example: 12/12 correct, accuracy 1.0, micro F1 1.0;
- `git diff --check`: passed.

Verified in CI on 2026-07-04:

- Ubuntu C++11 configure/build/test: passed;
- Windows C++11 configure/build/test: passed.

## 9. Integration and Non-Goals

- No DataBrowser API or script is required; this is a local SDK/CLI/Python
  scoring feature.
- No SVN synchronization is required.
- No diagnostic classifier validation claim.
- No AAMI-compliance claim; the v1 taxonomy is an explicit engineering QA
  contract.

## 10. Change Log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-07-04 | Accepted beat-classification QA architecture |
| 0.9 | 2026-07-04 | Implemented C++, CLI, Python, export, example, and test surfaces |
| 1.0 | 2026-07-04 | Completed Linux and Windows CI verification |
