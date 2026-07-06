# Synthetic PPG model specification

## Intended use

The PPG module generates a deterministic single optical pulse channel for
peak, timing, HR/HRV, ECG-to-PPG delay, perfusion-state, export, and
visualization QA. Amplitude is expressed in arbitrary units.

It is an engineering waveform model. It does not model blood pressure, SpO2,
vascular disease, patient-specific hemodynamics, or clinical optical-device
performance.

## Shared timeline

`ppg_generator` receives a completed `clinical_ecg_record`. Each expected PPG
pulse is linked to one ventricular beat by:

- ECG beat index;
- exact ECG R time;
- effective pulse onset delay and per-pulse state.

The PPG generator does not create or modify RR intervals. ECG and PPG
therefore share the exact generated beat timeline.

## Waveform

For each generated pulse:

1. pulse onset is `R time + effective pulse delay`;
2. a half-cosine rises from baseline to the primary systolic peak;
3. a half-cosine decays toward baseline;
4. an optional Gaussian-shaped dicrotic component is added.

Slow sinusoidal PTT variation, independent deterministic beat jitter, sensor
clock drift, low-frequency amplitude modulation, and deterministic rise/decay
variation may alter each pulse. Respiratory amplitude modulation is applied by
the shared scenario physiology layer.

Overlapping pulses sum. Only pulses whose effective onset through offset fit
inside the output record and are not intentionally missing receive waveform
samples and fiducial annotations.

## Perfusion episodes

A non-overlapping perfusion episode has exact half-open time bounds and can:

- scale pulse amplitude;
- scale rise and decay time;
- make every Nth episode pulse weak with an additional amplitude scale;
- intentionally omit every Nth episode pulse.

Weak and missing cadence starts at the first pulse in each episode. These are
physiological engineering states, distinct from acquisition dropout.

## Ground truth

Construction annotations identify:

- pulse onset;
- primary systolic peak;
- optional dicrotic feature;
- pulse offset.

Separate measured annotations identify sample-quantized onset and offset plus
the maximum sample value in the completed pulse window. They are refreshed
from the final exported waveform after physiology and acquisition effects.
The measured peak may differ from the construction peak because of sampling,
modulation, artifacts, and the dicrotic component.

Every annotation includes its sample index, time, value, ECG beat index, and
ECG R time.

Every expected pulse also records effective delay, onset/peak/offset,
amplitude, rise/decay duration, low-perfusion flag, state (`valid`, `weak`,
`missing`, or `out_of_record`), and peak-scoring validity. Missing and boundary
pulses never receive fabricated measured fiducials.

## Determinism and limits

For fixed ECG record, PPG configuration, seed, build, and floating-point
environment, output samples and annotations are reproducible. Every per-beat
draw is derived from the PPG seed, beat index, and a stable stream identifier.

The model remains a single green optical engineering channel. It does not
claim validated perfusion, vascular, or optical physiology. Accelerometer
coupling, motion/ambient-light sensor artifacts, and red/infrared channels are
separate planned increments.
