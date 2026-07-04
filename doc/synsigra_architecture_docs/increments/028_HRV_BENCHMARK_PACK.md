# HRV Benchmark Scenario Pack

**Document ID:** SYN-ARCH-INC-028

**Version:** 1.0

**Status:** Verified

**Owner role:** Core generation / Algorithm QA

**Date:** 2026-07-04

**Traceability ID:** `TRC-HRV-PACK-001`

**Implementation issue:** [signal_synth#43](https://github.com/tamask1s/signal_synth/issues/43)

**Implementation commit:** `a05bdd8aef312c10420558cb3eea8455d0f33e82`

**CI verification:** [GitHub Actions run 28712211912](https://github.com/tamask1s/signal_synth/actions/runs/28712211912)

## 1. Decision

Create `hrv_v1`, a deterministic HRV benchmark pack with nine renderable
five-minute ECG scenarios and one companion negative validation case.

The pack validates the complete product path:

```text
HRV scenario JSON
  -> ecg_qa_scenario
  -> clinical sinus RR timeline
  -> ECG waveform and exact beat annotations
  -> NN exclusion policy
  -> time-domain, Poincare, and spectral HRV ground truth
  -> local user-output scoring
```

The prior HRV schema stored LF/HF and respiratory parameters but mapped only
mean HR, target SDNN, RR bounds, and seed into the clinical timeline. This
increment completes that mapping. HRV-enabled sinus scenarios use deterministic
LF, HF, and respiratory oscillator components whose combined target variance is
the requested SDNN. Non-HRV scenarios retain the existing deterministic normal
RR variability behavior.

## 2. Applicable Requirements

- `REQ-HRV-SCN-001..006`;
- `REQ-HRV-GT-001..004`;
- `REQ-HRV-MET-001..005`;
- `REQ-HRV-SCORE-001..004`;
- SRS 003 section 6 benchmark scenario requirements.

## 3. Pack Contents

| Scenario | Intended profile |
|---|---|
| `clean_baseline` | fixed 60 bpm, zero intended HRV |
| `mild_variability` | 30 ms target SDNN, balanced LF/HF |
| `high_variability` | 80 ms target SDNN, balanced LF/HF |
| `lf_dominant` | 60 ms target SDNN, LF/HF target 4 |
| `hf_dominant` | 60 ms target SDNN, LF/HF target 0.25 |
| `balanced_lf_hf` | 60 ms target SDNN, LF/HF target 1 |
| `respiratory_variation` | explicit 0.2 Hz respiratory RR modulation |
| `ectopic_contamination` | deterministic PVC cadence with NN exclusion |
| `artifact_contamination` | mask-aware exclusion of artifact-overlapped RR intervals |

`hrv_short_window_rejected.json` is deliberately outside the render manifest.
It requests a two-minute HRV analysis and must fail validation at `$.hrv`,
because the version 1 spectral ground-truth contract requires at least
300 seconds.

`hrv_v1_expectations.json` records deterministic reference metrics and numeric
tolerances for every renderable case, plus the expected negative-case error.

## 4. RR Modulation Contract

For HRV-enabled sinus scenarios:

1. target variance is `target_sdnn_seconds^2`;
2. explicit respiratory variance is reserved first;
3. remaining variance is split between LF and HF using `lf_hf_ratio`;
4. each LF and HF band uses three deterministic sinusoids around its configured
   center and bandwidth;
5. oscillator phases derive from the scenario seed;
6. the RR interval is clipped only to the declared minimum and maximum bounds.

The respiratory amplitude must not independently exceed the requested total
SDNN variance. Invalid combinations are rejected by scenario validation.

Ectopic exclusion covers both the interval ending at an ectopic beat and the
following ectopic-adjacent interval. This prevents compensatory pauses from
being treated as NN intervals.

## 5. User Workflows

Validate and render the pack:

```text
signal-synth pack validate examples/packs/hrv_v1.json
signal-synth pack render examples/packs/hrv_v1.json --out hrv-pack-output
```

Score one algorithm result:

```text
signal-synth hrv score examples/scenarios/hrv/hrv_mild_variability.json examples/hrv/hrv_mild_reference_output.json --out hrv-score
```

For full pack scoring, repeat `hrv score` for each manifest scenario with the
corresponding customer output JSON. The local Python `synsigra.score_hrv`
function exposes the same operation for automated pack iteration. Each result
contains JSON, CSV, and HTML evidence.

Expected short-window failure:

```text
signal-synth validate examples/scenarios/hrv/hrv_short_window_rejected.json
```

## 6. Verification

Add `TEST-HRV-PACK-001` to verify:

- all nine manifest cases parse and render;
- repeated renders have identical identities and HRV metrics;
- clean, mild, and high variability levels;
- LF-dominant, HF-dominant, balanced, and respiratory spectral profiles;
- ectopic and artifact exclusion counts;
- absence of RR clipping;
- rejection code and path for the short-window fixture;
- presence of the expectations manifest.

Extend `TEST-ECG-PACK-001` to render the expanded nine-case HRV pack.

Required completion evidence:

- release build and full release CTest;
- sanitizer build and sanitizer CTest excluding package build smoke;
- `git diff --check`;
- Linux and Windows CI.

Verified locally on 2026-07-04:

- `cmake --build build-release`: passed;
- `ctest --output-on-failure` in `build-release`: 28/28 passed;
- `cmake --build build-sanitize`: passed;
- `ASAN_OPTIONS=detect_leaks=0 ctest -E TEST-BUILD-001 --output-on-failure`
  in `build-sanitize`: 27/27 passed;
- reference HRV user output: 6/6 metrics passed;
- `git diff --check`: passed.

Verified in CI on 2026-07-04:

- Ubuntu C++11 configure/build/test: passed;
- Windows C++11 configure/build/test: passed.

## 7. Compatibility and Integration

- Scenario schema version remains 2.
- Pack schema version remains 1.
- Existing non-HRV RR generation is unchanged.
- The HRV metric definition remains `synsigra_hrv_metrics_v1`; the ectopic
  behavior is a correction that makes the already-declared NN exclusion policy
  complete.
- No DataBrowser API or visualization script is added.
- No SVN synchronization is required.

## 8. Non-Goals and Limitations

- No hosted SaaS catalog or scoring service.
- No clinical interpretation or validation claim.
- The deterministic oscillator model targets engineering stress profiles, not
  a physiological autonomic nervous-system simulation.
- Version 1 uses fixed five-minute whole-record analysis rather than arbitrary
  rolling windows.

## 9. Change Log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-07-04 | Defined pack cases and end-to-end HRV modulation path |
| 0.9 | 2026-07-04 | Implemented pack, expectations, exclusion correction, and tests |
| 1.0 | 2026-07-04 | Completed local release and sanitizer verification |
