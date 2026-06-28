# Synthetic ECG model specification

## RR and HRV process

The configured heart rate defines the nominal interval:

`RR_mean = 60 / heart_rate_bpm`.

When HRV is enabled, the RR perturbation is the sum of two deterministic
oscillator banks. Each bank contains 12 sinusoidal components distributed
across a Gaussian-shaped frequency envelope. One bank represents LF and the
other HF activity. Component amplitudes are normalized so their theoretical
variances have the configured `lf_hf_ratio` and their sum has variance
`rr_standard_deviation_seconds^2`.

Oscillator phases are derived solely from `hrv.seed`. RR is evaluated when an
R event schedules the following beat, then clipped to
`[minimum_rr_seconds, maximum_rr_seconds]`. Clipping can reduce the observed
SDNN and alter LF/HF power, so validation reports must record clipping counts.

Angular velocity is constant between consecutive R events and equals
`2*pi/RR`. Consequently, after the initial partial beat, the continuous time
between consecutive R model events equals the RR value annotated on the later
beat.

## Scenario layer

Premature beats can be scheduled periodically, probabilistically, or by both
methods. Probabilistic decisions are a deterministic function of
`scenario.seed` and the beat index; rendering chunk sizes do not affect them.

A premature beat uses `premature_rr_ratio` and configurable P, QRS, and T
morphology scales. The following beat is labeled `ecg_beat_compensatory` and
uses `compensatory_pause_ratio`. All intervals remain subject to the configured
RR limits.

An event whose effective amplitude is zero remains present in the construction
timeline with `present == false`, but it is excluded from measured fiducials.

## Annotation levels

`ecg_model_annotation` is construction ground truth. It contains the exact
continuous event time, the first sample at or after that time, beat identity,
beat type, RR interval, and whether the wave was generated.

`ecg_measured_fiducial` is signal ground truth. Within the non-overlapping
window around each present model event, `measure_ecg_fiducials` selects the
strongest discrete local extremum relative to the window baseline. It reports:

- the exact discrete sample and its value;
- a three-point parabolic sub-sample time and interpolated value;
- the originating model event, beat, and wave.

For P and T waves it also reports onset and offset using a deterministic
amplitude rule: the boundary is the linearly interpolated 5% crossing of the
peak excursion from the window's linear baseline. These are operational
synthetic boundaries, not a claim that one clinical delineation convention is
universally correct.

The interpolated location is an estimate. The discrete sample index is the
reproducible measured fiducial.

## Validation package channels

`ecg_validation_package` produces five equal-length channels:

1. ECG samples.
2. Model-event impulses: P `0.25`, Q `-0.25`, R `1.0`, S `-0.5`, T `0.5`.
3. Measured impulses using the same peak values, plus P onset `0.10`,
   P offset `0.15`, T onset `0.30`, and T offset `0.40`.
4. The RR interval in seconds, held constant between R events.
5. Beat impulses: sinus `0.2`, compensatory `0.6`, premature `1.0`.

## Reproducibility

For a fixed build, configuration, seed, floating-point environment, and sample
rate:

- reset reproduces the same output;
- copying a generator preserves stream state;
- arbitrary render chunking produces identical samples and annotations;
- no global random state is used.
