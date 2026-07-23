# R-peak detector evidence packs

Synsigra separates an algorithm's declared scope from unrelated downstream
functions. A peak-only detector should submit R-peak events, not fabricate
signal-quality intervals or RR/HRV measurements merely to complete a package.

## Which pack to choose

### `r_peak_stress_v1` — detector evidence

Use this first for an R-peak-only engineering claim. The four cases cover:

- a clean 70 bpm control;
- 45 bpm bradycardic and 120 bpm tachycardic rate controls;
- persistent baseline wander and powerline interference.

The immutable protocol requires exactly one `r_peak` event file for every
case. A complete, valid submission can therefore report
`Evidence eligible: yes` without any `signal_quality`, `rr_interval`, HRV,
classification or delineation implementation.

Evidence eligibility and policy success are different:

- **Evidence eligible** means the package-authoritative matrix was completed
  without caller-selected filters or thresholds.
- **PASS/FAIL** means the submitted detector did or did not meet the packaged
  numeric policy.

This distinction preserves a valid evidence record even when a detector fails
one performance gate.

### `r_peak_noise_frontier_v1` — robustness comparison

Use this after the detector evidence pack to find the robustness frontier. It
contains one clean anchor and four paired 60-second cases:

| Case | Target SNR | Baseline-wander severity | Powerline severity |
|---|---:|---:|---:|
| `mixed_snr_m7` | −7 dB | 0.35 | 0.10 |
| `mixed_snr_m8` | −8 dB | 0.50 | 0.18 |
| `mixed_snr_m9` | −9 dB | 0.65 | 0.26 |
| `mixed_snr_m10` | −10 dB | 0.80 | 0.34 |

Target SNR is:

```text
20 log10(clean AC RMS / added-noise RMS)
```

It is calibrated independently for each of 12 ECG leads and each external
noise interval. A more-negative value is harder. Every noisy record contains
18 consecutive calibrated windows cycling through project-owned baseline,
muscle and electrode-motion sources. Those windows overlap persistent analytic
baseline wander and powerline interference.

The four records retain the same cardiac timebase, deterministic PVC cadence,
window schedule and source offsets. This paired construction makes the
noise/severity ladder—not a changing rhythm—the main independent variable.

Each tier has a separate pre-specified acceptance stratum. Performance from
the clean anchor or an easier tier cannot numerically compensate for collapse
at a harder tier. The report shows per-tier TP, FP, FN, sensitivity, PPV, F1
and timing error, with direct case-detail links.

## Claim boundary

Both packs provide deterministic synthetic engineering evidence for R-peak
event detection only. They do not validate signal-quality classification,
downstream interval or HRV calculations, patient-noise prevalence, diagnostic
performance, clinical safety or regulatory conformity.
