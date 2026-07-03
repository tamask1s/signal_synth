# Algorithm Comparison and Scoring v1

**Document ID:** SYN-ARCH-INC-017

**Version:** 0.1

**Status:** Verified

**Owner role:** Platform / Verification

**Date:** 2026-07-03

**Proposed traceability ID:** `TRC-CMP-001`

**Implementation issue:** [signal_synth#31](https://github.com/tamask1s/signal_synth/issues/31)

**Implementation commit:** `d39f18d8131d4d860144801249c542c65f4694bb`

**Verified CI run:** [Verification 28674991914](https://github.com/tamask1s/signal_synth/actions/runs/28674991914)

## 1. Decision

Add the first algorithm-under-test comparison layer above deterministic
scenario rendering. The comparison layer accepts external event detections,
matches them to generated ground truth, calculates QA metrics, and emits
machine-readable plus human-readable evidence files.

This increment covers event-detector scoring, not waveform classification,
diagnostic validation, or medical-device conformity assessment.

## 2. Product Rationale

The platform value is not only generated ECG/PPG samples. The useful customer
workflow is:

1. define or choose a scenario;
2. render deterministic waveform and ground truth;
3. run a customer algorithm externally;
4. import algorithm event detections;
5. compare against ground truth;
6. report reproducible QA metrics.

This closes the first commercially meaningful testbench loop for R-peak and
PPG peak algorithms before building a hosted backend around the library.

## 3. Scope

Implement:

- `src/ecg_compare.h` and `src/ecg_compare.cpp`;
- comparison targets:
  - ECG R peak;
  - PPG measured systolic peak;
- one-column CSV detection import contract for the CLI:
  - required `time_seconds`;
  - optional `label`;
- deterministic nearest-neighbor matching with one detection per ground-truth
  event;
- default tolerances:
  - R peak: 50 ms;
  - PPG systolic peak: 80 ms;
- optional CLI tolerance override in milliseconds;
- split metrics for total, clean intervals, and artifact intervals;
- output artifacts:
  - `comparison.json`;
  - `comparison.csv`;
  - `comparison_report.html`.

Out of scope for v1:

- beat classification scoring;
- waveform morphology scoring;
- pack-level algorithm-result aggregation;
- multi-column vendor-specific import adapters;
- claims about clinical validation performance.

## 4. Matching Contract

The matcher operates on rendered ground truth, not on exported JSON parsed
back into memory. This keeps the core API independent of export formatting and
prevents duplicate ground-truth interpretation.

Ground truth is collected from:

- `clinical_ecg_record::beats()` for R peaks with `qrs_present == true`;
- `ppg_record::annotations()` for measured systolic peaks.

Candidate matches are all ground-truth/detection pairs within tolerance. They
are sorted by:

1. absolute timing error;
2. ground-truth event order;
3. detection order after stable time sorting.

The first candidate that uses an unmatched ground-truth event and unmatched
detection becomes a true positive. Remaining ground-truth events are false
negatives. Remaining detections are false positives.

## 5. Metrics

For each bin (`total`, `clean`, `artifact`) calculate:

- ground-truth event count;
- detection count;
- true positives;
- false positives;
- false negatives;
- sensitivity/recall;
- positive predictive value/precision;
- F1 score;
- mean absolute timing error;
- median absolute timing error;
- RMS timing error;
- maximum absolute timing error.

Artifact bin membership for true positives and false negatives follows the
ground-truth event time. False-positive bin membership follows the detection
time. This is a deterministic engineering convention and is documented in the
comparison output by separating `clean` and `artifact` summaries.

## 6. CLI Contract

Add:

```text
signal-synth compare <rpeaks|ppg-peaks> <scenario.json|-> <detections.csv> --out <new-directory> [--tolerance-ms <ms>]
```

The command:

- parses the scenario JSON using the existing strict parser;
- renders the scenario through `render_ecg_document`;
- imports detections from CSV;
- compares detections against the requested target;
- creates a new output directory;
- writes `comparison.json`, `comparison.csv`, and `comparison_report.html`;
- prints machine-readable stdout summary lines.

The command does not overwrite existing output directories.

## 7. Verification

Create `TEST-COMPARE-001` covering:

- perfect R-peak detections;
- jittered detections within tolerance;
- missed detection;
- false positive;
- duplicate detection near one ground-truth event;
- artifact-vs-clean split;
- PPG measured systolic peak scoring;
- rejection of PPG scoring for ECG-only scenarios;
- JSON/CSV/HTML report contract.

Extend `TEST-CLI-001` to:

- render a scenario;
- derive a temporary detection CSV from exported annotations;
- run the CLI compare command;
- verify output files and perfect F1 summary.

Verified on 2026-07-03 with:

- release build and local CTest: 18/18 passed;
- sanitizer build and local CTest: 17/17 passed with
  `ASAN_OPTIONS=detect_leaks=0`, `LSAN_OPTIONS=detect_leaks=0`, and
  `TEST-BUILD-001` excluded;
- manual CLI compare smoke against
  `examples/scenarios/ecg_clean.json` and
  `examples/detections/ecg_clean_rpeaks_perfect.csv`: F1 = 1;
- DataBrowser/SVN working-copy synchronization checked by byte-compare for
  copied `ecg_compare.cpp` and `ecg_compare.h`; `SignalProc_RSPT.cbp` was
  updated to include them. `svn status` could not be executed because the
  local `svn` client is not installed.
- GitHub Actions `Verification` run `28674991914`: Ubuntu C++11 and Windows
  C++11 jobs passed.

## 8. Exit Criteria

1. Core comparison API compiles as part of the public library.
2. CLI comparison command produces deterministic report artifacts.
3. Unit and CLI tests pass locally and in CI.
4. Traceability matrix links issue, design, implementation, and tests.
5. Reports keep the intended-use limitation: engineering QA, not diagnosis or
   clinical validation certification.

## 9. Known Limitations

- Pack-level aggregation is not implemented in v1.
- The CSV parser intentionally accepts a minimal unquoted contract.
- Detection times are seconds only; sample-index import can be added later.
- Artifact split is interval-based and does not estimate partial waveform
  quality inside an event window.

## 10. Change Log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-07-03 | Implemented first R-peak and PPG peak comparison/scoring layer |
