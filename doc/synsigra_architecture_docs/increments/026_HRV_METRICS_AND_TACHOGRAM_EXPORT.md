# HRV Metrics and Tachogram Export

**Document ID:** SYN-ARCH-INC-026

**Version:** 1.0

**Status:** Verified

**Owner role:** Platform / SDK

**Date:** 2026-07-04

**Traceability ID:** `TRC-HRV-MET-001`

**Implementation issue:** [signal_synth#41](https://github.com/tamask1s/signal_synth/issues/41)

**Implementation commit:** `74b5575ab903c3e9d01be0026a356637cd0cebe9`

**CI verification:** [GitHub Actions run 28710996667](https://github.com/tamask1s/signal_synth/actions/runs/28710996667)

## 1. Decision

Add a reusable C++ HRV analysis module and export the RR tachogram as explicit
ground truth.

The implementation introduces `hrv_metrics.h/.cpp` as a core module below the
export layer. It consumes `clinical_ecg_record` beat annotations plus optional
`signal_quality_waveforms` artifact intervals and produces:

- per-beat RR interval rows with beat time, RR, clipping, ectopic,
  artifact-overlap, and exclusion flags;
- time-domain HRV metrics;
- Poincare metrics;
- deterministic LF/HF frequency-domain metrics;
- metric definition, exclusion policy, spectral method, units, and analysis
  window metadata.

The render/export layer stores the result in `ecg_render_bundle::hrv`, mirrors
summary values into `ecg_ground_truth_metrics`, and emits:

- `rr_tachogram.csv`;
- `hrv_metrics.json`;
- `rr_tachogram` inside `annotations.json`;
- expanded HRV fields inside `ground_truth_metrics.json`.

## 2. Applicable Requirements

- `REQ-HRV-GT-001`;
- `REQ-HRV-GT-002`;
- `REQ-HRV-GT-003`;
- `REQ-HRV-GT-004`;
- `REQ-HRV-MET-001`;
- `REQ-HRV-MET-002`;
- `REQ-HRV-MET-003`;
- `REQ-HRV-MET-004`;
- `REQ-HRV-MET-005`.

## 3. Metric Contract

Accepted RR intervals exclude:

- clipped intervals;
- ectopic intervals;
- missing-QRS or nonpositive RR intervals;
- intervals overlapped by ECG artifact intervals.

Exported time-domain metrics:

- mean RR;
- mean HR;
- SDNN;
- RMSSD;
- pNN50;
- SD1;
- SD2;
- SD1/SD2.

Exported frequency-domain metrics:

- LF power in seconds squared;
- HF power in seconds squared;
- LF/HF ratio;
- total LF-through-HF power.

Spectral policy:

- accepted RR intervals are linearly interpolated to 4 Hz;
- the interpolated sequence is mean-centered;
- a Hann window is applied;
- power is computed by direct deterministic periodogram, not platform-specific
  FFT;
- LF band is `[0.04, 0.15)` Hz;
- HF band is `[0.15, 0.40]` Hz;
- short windows or insufficient accepted intervals produce zero spectral
  powers rather than unstable estimates.

## 4. API and Layering

Public core API:

```cpp
bool analyze_hrv_from_ecg(const clinical_ecg_record& record, const signal_quality_waveforms* signal_quality, hrv_analysis_result& output);
```

The HRV module does not parse scenarios, build files, score users, or know
about SaaS packaging. The export layer owns artifact naming and JSON/CSV
serialization.

No DataBrowser/SVN API is added in this increment.

## 5. Verification

Add `TEST-HRV-METRICS-001`:

- variable clean RR produces nonzero time-domain and spectral metrics;
- constant RR normalizes SDNN/RMSSD/SD1/SD2 to zero;
- clipped intervals are flagged and excluded;
- ectopic intervals are flagged and excluded;
- ECG artifact-overlapped intervals are flagged and excluded;
- empty records are handled transactionally.

Extend existing tests:

- `TEST-ECG-EXPORT-001` verifies `rr_tachogram.csv`, `hrv_metrics.json`,
  expanded annotations, expanded ground-truth metrics, and artifact order;
- `TEST-FACADE-001` verifies the stable facade exposes the new artifacts;
- `TEST-BUILD-001` verifies installed public header visibility.

Verified locally on 2026-07-04:

- `cmake --build build-release`: passed;
- `cmake --build build-sanitize`: passed;
- `ctest --output-on-failure` in `build-release`: 26/26 passed;
- `ASAN_OPTIONS=detect_leaks=0 ctest -E TEST-BUILD-001 --output-on-failure`
  in `build-sanitize`: 25/25 passed;
- `git diff --check`: passed.

Verified in CI on 2026-07-04:

- Ubuntu C++11 configure/build/test: passed;
- Windows C++11 configure/build/test: passed.

## 6. Non-Goals

- No user HRV scoring or benchmark report.
- No HRV benchmark scenario pack.
- No Python HRV scoring API.
- No DataBrowser visualization API.
- No hosted SaaS service.

## 7. Risks and Limitations

- The spectral method is deterministic and documented, but it is still a first
  product baseline rather than a clinical HRV interpretation claim.
- Artifact overlap currently uses ECG artifact intervals; PPG-only artifacts do
  not exclude ECG RR intervals.
- The periodogram is intentionally direct and portable; very long signals may
  eventually need an optimized spectral backend.

## 8. Change Log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-07-04 | Added HRV metrics and RR tachogram export design |
| 1.0 | 2026-07-04 | Verified implementation and CI results |
