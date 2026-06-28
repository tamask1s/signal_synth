# signal_synth

`signal_synth` is a C++11 signal-generation library under active development.
The existing `signal_synth` API provides the legacy generators. The separate
`ecg_model` API is the clean-room foundation for a stateful synthetic ECG and
ground-truth validation package.

## ECG model status

The current model provides:

- deterministic, chunk-invariant streaming;
- a seeded LF/HF oscillator-bank RR process with configurable SDNN;
- deterministic periodic or probabilistic premature-beat scenarios;
- compensatory pauses and configurable premature-beat morphology;
- configurable P, Q, R, S, and T phase-domain morphology;
- fourth-order Runge-Kutta integration;
- baseline respiration coupling;
- exact continuous event time plus the first sampled index at or after it;
- measured discrete extrema with parabolic sub-sample interpolation;
- copyable and resettable per-instance state.

An `ecg_model_annotation` marks the configured P/Q/R/S/T model event.
`measure_ecg_fiducials` separately measures the strongest local extremum in
each event window and reports both its exact sample and a parabolic sub-sample
estimate. Model events and measured fiducials therefore remain distinguishable.

Multilead projection, clinically fitted morphology populations, calibrated
units, extended arrhythmia scenarios, and a formal validation report are not
implemented yet. The current output is therefore an engineering model, not
clinical validation evidence.

## Build and test

```sh
cmake -Hteszt -B/tmp/signal_synth-build
cmake --build /tmp/signal_synth-build
cd /tmp/signal_synth-build
ctest --output-on-failure
```

See `MODEL_SPECIFICATION.md` for behavioral semantics. Review
`LEGAL_PROVENANCE.md` and `DATA_LICENSES.md` before adding model code,
dependencies, datasets, or release artifacts.
