# Synthetic PPG model specification

## Intended use

The PPG module generates a deterministic single optical pulse channel for
peak, timing, HR/HRV, ECG-to-PPG delay, export, and visualization QA. Amplitude
is expressed in arbitrary units.

It is an engineering waveform model. It does not model blood pressure, SpO2,
vascular disease, patient-specific hemodynamics, or clinical optical-device
performance.

## Shared timeline

`ppg_generator` receives a completed `clinical_ecg_record`. Each complete PPG
pulse is linked to one ventricular beat by:

- ECG beat index;
- exact ECG R time;
- configured pulse onset delay.

The PPG generator does not create or modify RR intervals. ECG and PPG
therefore share the exact generated beat timeline.

## Waveform

For each complete pulse:

1. pulse onset is `R time + pulse_delay`;
2. a half-cosine rises from baseline to the primary systolic peak;
3. a half-cosine decays toward baseline;
4. an optional Gaussian-shaped dicrotic component is added.

Overlapping pulses sum. Only pulses whose configured onset through offset fit
inside the output record receive waveform samples and annotations.

## Ground truth

Construction annotations identify:

- pulse onset;
- primary systolic peak;
- optional dicrotic feature;
- pulse offset.

A separate measured annotation identifies the maximum sample value in the
completed pulse window. The measured peak may differ from the construction
peak because of sampling and the dicrotic component.

Every annotation includes its sample index, time, value, ECG beat index, and
ECG R time.

## Determinism and limits

The first model has no random state. For fixed ECG record, PPG configuration,
build, and floating-point environment, output samples and annotations are
reproducible.

Acquisition artifacts, respiratory amplitude modulation, perfusion changes,
multi-wavelength optics, pulse-transit physiology, and accelerometer coupling
are not implemented yet.
