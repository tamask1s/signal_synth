# HRV User-Output Scoring

**Document ID:** SYN-ARCH-INC-027

**Version:** 1.0

**Status:** Verified

**Owner role:** Platform / SDK

**Date:** 2026-07-04

**Traceability ID:** `TRC-HRV-SCORE-001`

**Implementation issue:** [signal_synth#42](https://github.com/tamask1s/signal_synth/issues/42)

**Implementation commit:** `fbc4e420239a778fbe58c88b855f3e4088f19ec1`

**CI verification:** [GitHub Actions run 28711739786](https://github.com/tamask1s/signal_synth/actions/runs/28711739786)

## 1. Decision

Add a versioned HRV user-output contract and local scoring engine in C++.

The user supplies:

- algorithm name and version;
- any supported subset of HRV metrics;
- optional RR intervals with beat times.

The scorer compares this output with the deterministic HRV ground truth stored
in `ecg_render_bundle::hrv`. It emits:

- `hrv_score.json`;
- `hrv_score.csv`;
- `hrv_score_report.html`.

The Python package remains a thin convenience wrapper around the C++ CLI.

## 2. Applicable Requirements

- `REQ-HRV-SCORE-001`;
- `REQ-HRV-SCORE-002`;
- `REQ-HRV-SCORE-003`;
- `REQ-HRV-SCORE-004`.

## 3. User-Output Contract

Schema version 1:

```json
{
  "schema_version": 1,
  "algorithm": {
    "name": "customer_hrv",
    "version": "1.0"
  },
  "metrics": {
    "sdnn_seconds": 0.052,
    "lf_hf_ratio": 1.7
  },
  "rr_intervals": [
    {
      "beat_time_seconds": 1.25,
      "rr_seconds": 0.84
    }
  ]
}
```

Rules:

- unknown fields and unknown metric names are rejected;
- algorithm name is required and non-empty;
- at least one metric or RR interval is required;
- metric values are finite and non-negative;
- RR beat times are non-negative and RR values are positive;
- invalid input does not replace a previously parsed user document.

## 4. Scoring Policy

Metric pass/fail uses:

- absolute error;
- relative error percentage;
- documented per-metric absolute and relative tolerances;
- pass when either tolerance is satisfied.

RR scoring:

- only ground-truth intervals accepted by the HRV exclusion policy are used;
- user and ground-truth intervals are greedily matched by beat time within
  50 ms;
- RR value pass uses 20 ms absolute or 5 percent relative tolerance;
- output reports matched, missing, extra, passed, MAE, RMS, and maximum error.

## 5. API and Integration

Public C++ module:

```cpp
bool parse_hrv_user_output_json(const std::string& json, hrv_user_output& output, std::vector<std::string>& messages);
bool score_hrv_user_output(const ecg_render_bundle& render, const hrv_user_output& user, hrv_score_result& result);
```

CLI:

```text
signal-synth hrv score scenario.json hrv-output.json --out score-directory
```

Python:

```python
score_hrv(case, user_output, out_dir=None, cli_path=None)
```

`user_output` may be a JSON file path or Python dictionary.

No DataBrowser/SVN API is added in this increment.

## 6. Verification

Add `TEST-HRV-SCORING-001`:

- strict valid input parsing;
- perfect metric scoring;
- metric tolerance failure;
- complete RR matching;
- JSON/CSV/HTML output contracts;
- unknown metric, wrong schema, and empty-output rejection;
- incomplete render rejection.

Extend:

- `TEST-CLI-001` with end-to-end HRV score files;
- `TEST-PYTHON-SCORING-001` with dictionary input and complete RR scoring;
- `TEST-BUILD-001` with installed public header/API visibility.

Verified locally on 2026-07-04:

- `cmake --build build-release`: passed;
- `cmake --build build-sanitize`: passed;
- `ctest --output-on-failure` in `build-release`: 27/27 passed;
- `ASAN_OPTIONS=detect_leaks=0 ctest -E TEST-BUILD-001 --output-on-failure`
  in `build-sanitize`: 26/26 passed;
- `git diff --check`: passed.

Verified in CI on 2026-07-04:

- Ubuntu C++11 configure/build/test: passed;
- Windows C++11 configure/build/test: passed.

## 7. Non-Goals

- No hosted SaaS scoring service.
- No clinical HRV interpretation or validation claim.
- No benchmark pack; issue #43 owns curated scenarios.
- No DataBrowser visualization API.

## 8. Risks and Limitations

- Tolerances are versioned engineering QA defaults and may need profile-specific
  policy in later product versions.
- RR matching is intentionally deterministic and simple.
- Metric comparability assumes the user followed the declared analysis window,
  metric definition, spectral method, and exclusion policy.

## 9. Change Log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-07-04 | Added HRV user-output scoring design |
| 1.0 | 2026-07-04 | Verified implementation and CI results |
