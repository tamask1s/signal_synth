# Algorithm Design Overview

Version: 0.1  
Status: Draft  
Scope: High-level algorithm design for ECG, PPG, HRV, artifacts, annotations and reports.

## 1. Design position

The algorithm layer should generate controlled, reproducible engineering phantoms.

It should not claim:

- patient-population realism;
- diagnostic validity;
- clinical evidence;
- conformity-assessment sufficiency.

The intended claim is narrower:

> For a specified synthetic scenario, Synsigra generates ECG/PPG-like signals with known construction events, measured fiducials, artifact intervals, and ground-truth metrics.

## 2. Beat timeline

A shared beat timeline should drive ECG and PPG generation.

### Beat fields

Each beat should include:

- beat ID;
- nominal R time;
- RR interval;
- beat type:
  - sinus;
  - premature atrial;
  - premature ventricular;
  - compensatory;
  - missed/blocked;
  - paced later;
  - irregular/AF-like later;
- morphology modifiers;
- semantic tags.

### Why shared timeline matters

It enables:

- ECG R-peak truth;
- PPG pulse truth;
- ECG-PPG delay tests;
- HRV metrics consistency;
- wearable fusion tests;
- artifact and dropout stress cases.

## 3. RR / HRV process

The project already contains a seeded LF/HF oscillator-bank RR process. Keep this direction.

Recommended behavior:

- nominal RR from baseline heart rate;
- deterministic low-frequency modulation;
- deterministic high-frequency modulation;
- configurable LF/HF-like power ratio;
- configurable SDNN target;
- clipping to min/max RR with warning/count;
- deterministic seed.

Important report fields:

- configured SDNN;
- observed SDNN;
- configured LF/HF-like ratio;
- observed spectral summary if implemented;
- clipping count;
- ectopic/premature count;
- missed/blocked count.

## 4. ECG waveform model

Use a layered ECG model:

```text
Beat timeline
  -> wave events: P, Q, R, S, T
  -> morphology per wave
  -> continuous construction signal
  -> sampled ECG
  -> measured fiducials
```

### ECG events

Minimum events:

- P peak;
- Q valley;
- R peak;
- S valley;
- T peak;
- optional onset/offset for P/QRS/T.

### Morphology parameters

Expose product-safe parameters first:

- QRS amplitude;
- QRS width;
- T amplitude;
- T width;
- P amplitude;
- P width;
- PR interval;
- QT behavior;
- baseline amplitude;
- global gain.

Do not expose every internal source/vector parameter in the SaaS MVP.

### 12-lead ECG

12-lead output is valuable, but should remain framed as synthetic/phantom.

Initial product can start with:

- Lead II or normalized single-lead ECG;
- later 3-lead;
- later 12-lead phantom.

## 5. PPG waveform model

Add PPG early because wearable companies often care more about optical pulse measurements than ECG.

### PPG generation

Generate PPG from the same beat timeline.

For each beat:

- PPG onset occurs after ECG R time by configurable pulse arrival delay;
- pulse peak follows onset according to pulse shape;
- pulse width and decay are configurable;
- pulse amplitude can vary with respiration/activity/artifact.

### Minimal PPG parameters

- pulse_delay_ms;
- pulse_width_ms;
- systolic_amplitude;
- diastolic_component_strength;
- baseline;
- respiratory_amplitude_modulation;
- beat_to_beat_amplitude_variation;
- motion_artifact_level;
- dropout_intervals;
- saturation_intervals.

### PPG truth

Ground truth should include:

- pulse onset;
- systolic peak;
- optional dicrotic notch;
- pulse offset;
- linked ECG beat ID;
- pulse delay.

## 6. ECG-PPG coupling

This is an important differentiator.

Support scenarios where:

- PPG delay is fixed;
- PPG delay varies slowly;
- PPG delay changes with simulated activity/stress;
- PPG amplitude drops during motion;
- ECG remains clean while PPG is corrupted;
- both ECG and PPG are corrupted differently.

This enables testing:

- ECG vs PPG HR detection;
- PPG-only HRV degradation;
- multimodal fusion algorithms;
- artifact rejection.

## 7. Noise and artifacts

Separate physiological variability from measurement artifacts.

### ECG artifacts

- baseline wander;
- powerline interference;
- EMG-like high-frequency noise;
- electrode contact noise;
- amplitude dropout;
- saturation/clipping;
- impulsive motion artifact;
- quantization noise.

### PPG artifacts

- motion artifact;
- ambient light interference;
- optical dropout;
- saturation;
- low perfusion / low amplitude;
- contact pressure changes;
- accelerometer-correlated artifact later.

### Artifact annotations

Every artifact should have:

- start time;
- end time;
- affected channels;
- artifact type;
- severity;
- seed;
- parameter summary.

## 8. Arrhythmia and rhythm scenarios

Initial useful scenarios:

- sinus rhythm;
- sinus rhythm with HRV;
- periodic premature beat;
- probabilistic premature beat;
- compensatory pause;
- missed beat;
- AF-like irregular RR;
- ectopy series;
- brady/tachy segment;
- conduction block later.

Important: distinguish synthetic rhythm labels from clinical claims.

Use:

- “AF-like irregular RR stress scenario” if not clinically faithful;
- “PVC-like premature wide beat scenario” if morphology is approximate.

## 9. Ground truth model

### Construction ground truth

Generated from the model itself.

Examples:

- ideal R event time;
- intended P/QRS/T event times;
- beat type;
- artifact boundaries;
- PPG pulse onset/peak linked to beat.

### Measured ground truth

Measured from the sampled generated signal.

Examples:

- measured R peak sample;
- measured P/T extrema;
- measured PPG systolic peak;
- measured onsets/offsets by deterministic rule.

Both are useful. Reports must not mix them.

## 10. Algorithm comparison layer

Later feature: allow customer to upload algorithm output.

Accepted formats:

- R-peak CSV;
- PPG peak CSV;
- beat classification CSV;
- HR time series;
- signal-quality intervals.

Compute:

- sensitivity;
- positive predictive value;
- false positives;
- false negatives;
- timing error distribution;
- beat classification confusion matrix;
- HRV metric deviation;
- artifact-specific performance.

## 11. Scenario packs

### HRV Pack

- clean LF/HF modulation;
- low SDNN;
- high SDNN;
- clipping edge;
- ectopy contamination.

### R-Peak Pack

- clean;
- low amplitude;
- high T wave;
- baseline wander;
- EMG noise;
- premature beat;
- missed beat.

### PPG Wearable Pack

- low perfusion;
- motion artifact;
- dropout;
- amplitude modulation;
- ECG clean / PPG corrupted;
- pulse delay variation.

### Hardware-in-loop Pack

- amplitude calibration;
- DAC resolution stress;
- quantization;
- timing jitter;
- analog output clipping.

## 12. Algorithm roadmap

Phase 1:

- deterministic ECG;
- R/RR ground truth;
- HRV modulation;
- basic noise/artifacts;
- CSV/JSON export;
- HTML report.

Phase 2:

- PPG;
- ECG-PPG coupling;
- PPG artifact model;
- pulse annotations.

Phase 3:

- customer output comparison;
- batch regression runner;
- scenario packs.

Phase 4:

- analog hardware output;
- calibration-aware reports.

## 13. Guardrails

Avoid building a scientifically overcomplex model before product validation.

Prioritize:

- deterministic behavior;
- clear annotation semantics;
- scenario usefulness;
- report quality;
- API stability.

Do not prioritize initially:

- maximum physiological realism;
- full torso models;
- all ECG pathologies;
- all 12 leads;
- clinical validation claims.
