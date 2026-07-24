# R-peak and RR detector evidence packs

Synsigra keeps a detector claim focused without making it artificially
peak-only. Both packs require the two outputs a practical peak detector can
directly provide:

- R-peak event times;
- observable beat-to-beat RR intervals derived from consecutive R peaks.

Neither pack requires signal-quality classification, HRV, morphology,
delineation or diagnosis.

## Which pack to choose

For new evaluations, start with the two simple packs. Their official decision
unit is one complete case. They never pool cases, split one signal into
acceptance bins, average percentages, or let a strong case compensate for a
failed case.

### `r_peak_rr_simple_stress_v1` — simple first run

This is the compact, human-readable first-run detector pack:

| Case | What changes | Why it is present |
|---|---|---|
| `clean_70` | Clean 70 bpm control | Nominal anchor |
| `slow_45` | Clean 45 bpm bradycardia | Slow-rate boundary |
| `fast_120` | Clean 120 bpm tachycardia | Fast-rate boundary |
| `baseline_powerline` | Baseline wander and powerline interference | Persistent structured interference |
| `moderate_noise` | Full-record baseline and powerline noise, EMG bursts, RR variability, PVCs | Realistic mixed acquisition stress |
| `variable_rate` | Sinus baseline with AF and PSVT episodes | Strong rate and regularity changes |
| `mobitz_ii_pauses` | Deterministic non-conducted atrial events | A genuine missing QRS has no R-peak truth; the surrounding long RR remains scoreable |
| `combined_stress` | Rate changes plus baseline, powerline and EMG artifacts | Several stressors in one independent case |

The report opens with eight independent verdicts. Metric-level gates and the
two case-target detail pages remain available as audit detail. A good detector
can reasonably pass every case, but none of the thresholds is selected after
seeing the submitted result.

### `r_peak_rr_snr_ladder_v1` — simple robustness curve

This pack contains 14 paired 60-second signals: one clean control, fractional
−0.2 and −0.5 dB transition points, and every integer target SNR from −1
through −11 dB. Every case uses the same variable-rate cardiac truth and
periodic PVC cadence. In noisy cases,
project-owned baseline, muscle, and electrode-motion noise covers the complete
0–60 second record without clean gaps; the source type changes every three
seconds.

| SNR | R-peak F1 min | PPV / sensitivity min | RR median absolute error max |
|---:|---:|---:|---:|
| Clean | 0.98 | 0.98 | 10 ms |
| −0.2 dB | 0.97 | 0.95 | 15 ms |
| −0.5 dB | 0.96 | 0.94 | 18 ms |
| −1 dB | 0.95 | 0.92 | 20 ms |
| −2 dB | 0.93 | 0.90 | 25 ms |
| −3 dB | 0.90 | 0.85 | 30 ms |
| −4 dB | 0.85 | 0.80 | 40 ms |
| −5 dB | 0.78 | 0.73 | 55 ms |
| −6 dB | 0.74 | 0.69 | 70 ms |
| −7 dB | 0.70 | 0.65 | 85 ms |
| −8 dB | 0.65 | 0.60 | 100 ms |
| −9 dB | 0.62 | 0.58 | 120 ms |
| −10 dB | 0.60 | 0.55 | 140 ms |
| −11 dB | 0.55 | 0.50 | 160 ms |

The main report is one row per case, with R-peak F1, sensitivity, PPV, timing
MAE, RR coverage, RR status agreement, RR tolerance rate, RR MAE, RR median,
thresholds, and the case verdict. RR MAE, P95 and tolerance rate remain visible
diagnostics in the detail report, but the noise ladder's robust RR error gate
is the median. This prevents a single correctly localized split or merge from
turning the rest of a case into a misleading cascade while still showing its
full error cost.

## RR association policy

Every valid RR row defines an interval from
`time_seconds − value` through its ending R-peak at `time_seconds`. Reference
and submitted intervals associate when their overlap covers more than half of
the shorter interval:

- one false-positive R peak splits one truth RR into local submitted fragments;
- one missed R peak merges adjacent truth RRs into one local submitted interval;
- multiple local splits or merges remain multiple numeric comparisons;
- truth and prediction coverage count unique intervals, so neither can exceed
  100%;
- all non-RR and non-valid-status measurements keep strict one-to-one identity
  and time-anchor matching.

This is deliberately an association, not a device for hiding detector errors.
The split or merge produces large local RR errors; it simply cannot shift every
subsequent otherwise-correct RR pair.

## Detailed legacy packs

The older packs remain available when their additional aggregation and
artifact-bin diagnostics are useful. They are not the recommended starting
point for a human review.

### `r_peak_stress_v1` — legacy aggregate stress report

This older four-case pack covers:

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

### `r_peak_noise_frontier_v1` — detailed composite robustness boundary

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
coverage, status agreement, tolerance rate, MAE, median and P95 error.

## Why there is one policy, not “classical” and “deep-learning” modes

The verifier scores the declared output, not the implementation architecture.
Allowing an algorithm author to select the more favorable post-hoc profile
would weaken comparability. The monotone thresholds above are pre-specified
engineering gates for this exact deterministic pack.

Published results are useful context but not directly transferable. The
official MIT-BIH Noise Stress Test Database uses six-decibel steps down to
−6 dB and alternates clean and noisy segments, while detector results also
depend strongly on noise construction and event-matching tolerance. The
150 ms R-peak event window is comparable to the default WFDB `bxb` reporting
window. WFDB also records that proposed RR error statistics were dropped from
the EC38/EC57 standards; consequently the RR limits above are transparent
Synsigra engineering gates, not claimed EC57 requirements.

- <https://physionet.org/content/nstdb/1.0.0/>
- <https://physionet.org/physiotools/wag/bxb-1.htm>
- <https://www.accessdata.fda.gov/scripts/cdrh/cfdocs/cfstandards/detail.cfm?standard__identification_no=31679>
- <https://pmc.ncbi.nlm.nih.gov/articles/PMC6818956/>
- <https://pmc.ncbi.nlm.nih.gov/articles/PMC10934794/>

## Claim boundary

All four packs provide deterministic synthetic engineering evidence for R-peak
event detection and observable beat-to-beat RR measurement. They do not
validate signal-quality classification, HRV, patient-noise prevalence,
diagnostic performance, clinical safety or regulatory conformity.
