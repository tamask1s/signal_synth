# R-peak and RR detector evidence packs

Synsigra keeps a detector claim focused without making it artificially
peak-only. Both packs require the two outputs a practical peak detector can
directly provide:

- R-peak event times;
- observable beat-to-beat RR intervals derived from consecutive R peaks.

Neither pack requires signal-quality classification, HRV, morphology,
delineation or diagnosis.

## Which pack to choose

### `r_peak_stress_v1` — first evidence run

Use this four-case pack first. It covers:

- a clean 70 bpm control;
- 45 bpm bradycardic and 120 bpm tachycardic rate controls;
- persistent baseline wander and powerline interference.

The immutable protocol requires one R-peak event file and one RR measurement
file per case: eight outputs in total. Standard RR aggregate MAE is capped at
25 ms. A complete submission can therefore report `Evidence eligible: yes`
without any signal-quality output.

Evidence eligibility and policy success are different:

- **Evidence eligible** means the complete package-authoritative matrix was
  submitted without caller-selected filters or thresholds.
- **PASS/FAIL** means the algorithm did or did not meet the immutable packaged
  numeric policy.

A failed gate therefore remains a valid, auditable evidence result.

### `r_peak_noise_frontier_v1` — find the robustness boundary

Use this 500-second pack after the first evidence run. It has one clean anchor
and eight paired 60-second composite-noise cases:

| Case | Target SNR | Baseline wander | Powerline | R-peak F1 min | PPV / sensitivity min | RR match floor | RR MAE max |
|---|---:|---:|---:|---:|---:|---:|---:|
| `mixed_snr_m3` | −3 dB | 0.08 | 0.02 | 0.90 | 0.85 | 0.85 | 25 ms |
| `mixed_snr_m4` | −4 dB | 0.16 | 0.04 | 0.85 | 0.80 | 0.80 | 25 ms |
| `mixed_snr_m5` | −5 dB | 0.25 | 0.07 | 0.78 | 0.73 | 0.73 | 25 ms |
| `mixed_snr_m7` | −7 dB | 0.35 | 0.10 | 0.70 | 0.65 | 0.65 | 25 ms |
| `mixed_snr_m8` | −8 dB | 0.50 | 0.18 | 0.65 | 0.60 | 0.60 | 25 ms |
| `mixed_snr_m9` | −9 dB | 0.65 | 0.26 | 0.62 | 0.58 | 0.58 | 25 ms |
| `mixed_snr_m10` | −10 dB | 0.80 | 0.34 | 0.60 | 0.55 | 0.55 | 25 ms |
| `mixed_snr_m11` | −11 dB | 0.95 | 0.42 | 0.55 | 0.50 | 0.50 | 30 ms |

The 30 ms RR allowance applies only to the isolated −11 dB worst tier. It is
not a global relaxation. The −6 dB point is intentionally omitted: the
−5-to−7 transition keeps the package compact and avoids implying direct
equivalence with databases that use a different −6 dB noise protocol.

Target SNR is:

```text
20 log10(clean AC RMS / added-noise RMS)
```

It is calibrated independently for every selected ECG lead and external-noise
interval. More-negative values add more noise. Each noisy record cycles
through 18 consecutive project-owned baseline, muscle and electrode-motion
windows while persistent analytic baseline wander and powerline interference
are also present. Consequently this is a **composite Synsigra protocol**, not
an interchangeable label for a published database's nominal SNR.

All noisy records retain the same cardiac timebase, deterministic PVC cadence,
window schedule and source offsets. Only target SNR and analytic artifact
severity increase. This paired construction makes the noise frontier, rather
than a changing rhythm, the main independent variable.

Every tier is an independent acceptance stratum. Performance on clean or
easier cases cannot compensate for failure at a harder tier. Reports expose
per-case R-peak TP, FP, FN, sensitivity, PPV, F1 and timing error plus RR
coverage, status agreement, tolerance rate, MAE and P95 error.

## Why there is one policy, not “classical” and “deep-learning” modes

The verifier scores the declared output, not the implementation architecture.
Allowing an algorithm author to select the more favorable post-hoc profile
would weaken comparability. The monotone thresholds above are pre-specified
engineering gates for this exact deterministic pack.

Published results are useful context but not directly transferable. The
official MIT-BIH Noise Stress Test Database uses six-decibel steps down to
−6 dB and alternates clean and noisy segments, while detector results also
depend strongly on noise construction and event-matching tolerance:

- <https://physionet.org/content/nstdb/1.0.0/>
- <https://pmc.ncbi.nlm.nih.gov/articles/PMC10934794/>

## Claim boundary

Both packs provide deterministic synthetic engineering evidence for R-peak
event detection and observable beat-to-beat RR measurement. They do not
validate signal-quality classification, HRV, patient-noise prevalence,
diagnostic performance, clinical safety or regulatory conformity.
