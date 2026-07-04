# Local Python Scoring Package

**Document ID:** SYN-ARCH-INC-022

**Version:** 0.1

**Status:** Implementing

**Owner role:** Platform / SDK

**Date:** 2026-07-04

**Proposed traceability ID:** `TRC-PY-001`

**Implementation issue:** [signal_synth#35](https://github.com/tamask1s/signal_synth/issues/35)

## 1. Decision

Add the first local Python package for offline challenge loading and scoring.
The package is intentionally a thin convenience layer over the C++ command-line
tool. It does not reimplement waveform generation, event matching, or scoring
metrics.

This keeps the C++ core and CLI as the deterministic source of truth while
giving users a simple notebook/CI-friendly API.

## 2. Applicable Requirements

- `REQ-PY-001..008`;
- `REQ-SCORE-001..006`;
- supporting `REQ-CHAL-001..009`.

## 3. Public Python Contract

Add:

- `pyproject.toml`;
- `python/synsigra/`;
- `examples/python/score_challenge.py`.

Primary API:

```python
import synsigra as ss

challenge = ss.load_challenge("challenge-dir-or.synsigra")
detections = ss.load_detections("clean_ecg.json", target="r_peak")
report = ss.compare_rpeaks(challenge.case("clean_ecg"), detections)
report.write("out")
```

Supported operations in this increment:

- load challenge packages from a directory;
- load challenge packages from a zip-compatible `.synsigra` archive;
- expose case metadata and waveform CSV columns/rows;
- load CSV or JSON detection outputs;
- score R-peak detections through `signal-synth compare`;
- score PPG systolic peak detections through `signal-synth compare`;
- score pack-level detections through `signal-synth pack score`;
- expose JSON/CSV/HTML report artifacts through Python-native objects.

## 4. Layering

Python may parse package metadata and user detection files for convenience, but
it must not become an independent scoring implementation. The scoring functions
call the configured `signal-synth` executable and read the generated artifacts.

CLI discovery order:

1. explicit `cli_path` argument;
2. `SYNSIGRA_CLI`;
3. `SIGNAL_SYNTH_CLI`;
4. `signal-synth` on `PATH`.

## 5. DataBrowser/SVN Impact

No DataBrowser API is added in this increment and no visualization script is
required. The Python package is user-facing SDK code and is not copied to the
DataBrowser/SVN working copy.

## 6. Verification

Add `TEST-PYTHON-SCORING-001`:

- builds a local challenge package directory from rendered ECG and ECG/PPG
  scenarios;
- loads the challenge directory;
- loads a zip-compatible `.synsigra` archive;
- exposes waveform CSV columns and rows;
- loads JSON detections;
- scores R-peak detections through Python;
- scores PPG systolic peak detections through Python;
- verifies Python R-peak scoring output matches direct CLI output;
- verifies report artifact copy-out.

## 7. Non-Goals

- No hosted SaaS service.
- No user algorithm hosting.
- No direct C++ extension module binding.
- No independent Python scoring implementation.
- No package publishing workflow.

## 8. Risks and Limitations

- The first package depends on the `signal-synth` executable being available.
- Waveform loading is CSV-based convenience; WFDB/EDF/BDF support remains
  future work.
- Challenge archive support expects a zip-compatible archive layout.
- Detection validation convenience in Python is intentionally lighter than the
  C++ importer; scoring still goes through the strict CLI importer.

## 9. Change Log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-07-04 | Added first local Python scoring package design |
