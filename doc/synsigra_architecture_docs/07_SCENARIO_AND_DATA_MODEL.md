# Scenario and Data Model

Version: 0.1  
Status: Draft  
Scope: Product-facing scenario contract, metadata, annotations and export data.

## 1. Purpose

The scenario model is the product contract. It must be:

- human-readable;
- versioned;
- deterministic;
- strict enough to prevent mislabeled signals;
- stable enough for SaaS, CLI and regression tests;
- expressive enough for ECG/PPG algorithm QA.

## 2. Scenario object

Minimum scenario JSON:

```json
{
  "schema_version": "1.0",
  "scenario_id": "hrv_clean_lfhf_001",
  "title": "Clean ECG with LF/HF HRV modulation",
  "description": "Synthetic ECG scenario for HRV algorithm testing.",
  "seed": 123456,
  "duration_seconds": 300,
  "sample_rate_hz": 500,
  "channels": ["ecg_ii", "ppg_green"],
  "timeline": {
    "heart_rate_bpm": 70,
    "minimum_rr_seconds": 0.45,
    "maximum_rr_seconds": 1.5,
    "hrv": {
      "enabled": true,
      "sdnn_seconds": 0.05,
      "lf_hf_ratio": 2.0,
      "seed": 777
    }
  },
  "ecg": {
    "enabled": true,
    "lead_mode": "single_lead",
    "morphology": {
      "p_amplitude_mv": 0.1,
      "qrs_amplitude_mv": 1.0,
      "t_amplitude_mv": 0.3,
      "qrs_width_ms": 90
    }
  },
  "ppg": {
    "enabled": true,
    "pulse_delay_ms": 180,
    "pulse_width_ms": 300,
    "amplitude": 1.0
  },
  "events": [],
  "artifacts": [],
  "export": {
    "formats": ["csv", "json"],
    "include_ground_truth": true,
    "include_watermark_metadata": true
  }
}
```

## 3. Scenario sections

### 3.1 Identity

Fields:

- schema_version;
- scenario_id;
- title;
- description;
- author optional;
- created_at optional;
- tags.

### 3.2 Determinism

Fields:

- seed;
- generator_version recorded in output, not necessarily input;
- floating_point_policy optional;
- scenario_fingerprint generated after canonicalization.

### 3.3 Time and sampling

Fields:

- duration_seconds;
- sample_rate_hz;
- start_time_seconds default 0;
- chunking should not affect output.

### 3.4 Channels

Supported initial channel names:

- `ecg_i`;
- `ecg_ii`;
- `ecg_iii`;
- `ecg_v1` to `ecg_v6` later;
- `ppg_green`;
- `ppg_ir`;
- `ppg_red`;
- `artifact_marker`;
- `rr_interval`;
- `event_impulse`.

Initial MVP can support only `ecg_ii` and `ppg_green`.

## 4. Events

Event object:

```json
{
  "type": "premature_beat",
  "time_seconds": 42.0,
  "parameters": {
    "premature_rr_ratio": 0.65,
    "compensatory_pause_ratio": 1.25,
    "qrs_amplitude_scale": 1.2
  }
}
```

Event types:

- premature_beat;
- missed_beat;
- compensatory_pause;
- rhythm_segment;
- af_like_irregularity;
- tachy_segment;
- brady_segment;
- amplitude_change;
- morphology_change.

## 5. Artifacts

Artifact object:

```json
{
  "type": "baseline_wander",
  "start_seconds": 30,
  "end_seconds": 90,
  "channels": ["ecg_ii"],
  "severity": 0.5,
  "parameters": {
    "frequency_hz": 0.25,
    "amplitude_mv": 0.2
  }
}
```

Artifact types:

- baseline_wander;
- powerline;
- emg_noise;
- electrode_motion;
- dropout;
- saturation;
- quantization;
- ppg_motion;
- ppg_ambient_light;
- ppg_low_perfusion.

## 6. Annotations

### 6.1 Beat annotation

```json
{
  "annotation_type": "beat",
  "beat_id": 120,
  "time_seconds": 103.428,
  "sample_index": 51714,
  "beat_type": "sinus",
  "rr_seconds": 0.842,
  "linked_events": []
}
```

### 6.2 ECG fiducial annotation

```json
{
  "annotation_type": "ecg_fiducial",
  "beat_id": 120,
  "wave": "R",
  "construction_time_seconds": 103.428,
  "construction_sample_index": 51714,
  "measured_time_seconds": 103.428,
  "measured_sample_index": 51714,
  "measured_value_mv": 0.998,
  "channel": "ecg_ii"
}
```

### 6.3 PPG annotation

```json
{
  "annotation_type": "ppg_fiducial",
  "beat_id": 120,
  "fiducial": "systolic_peak",
  "time_seconds": 103.612,
  "sample_index": 51806,
  "linked_ecg_r_time_seconds": 103.428,
  "pulse_delay_ms": 184,
  "channel": "ppg_green"
}
```

### 6.4 Artifact annotation

```json
{
  "annotation_type": "artifact_interval",
  "artifact_type": "ppg_motion",
  "start_seconds": 60,
  "end_seconds": 75,
  "channels": ["ppg_green"],
  "severity": 0.8
}
```

## 7. Output package

Directory structure:

```text
scenario_id/
  scenario.json
  metadata.json
  waveform.csv
  annotations.json
  rr_tachogram.csv
  hrv_metrics.json
  ground_truth_metrics.json
  warnings.json
  report.html
  README.txt
```

Optional later:

```text
  report.pdf
```

## 8. Metadata

`metadata.json` should include:

- generator name;
- generator version;
- git commit;
- scenario fingerprint;
- seed;
- render timestamp;
- license ID if SaaS;
- customer/project ID if SaaS;
- export ID;
- disclaimers;
- units;
- channel definitions;
- sample rate;
- duration.

## 9. Scenario fingerprint

The fingerprint should be based on canonical scenario JSON:

- sort object keys;
- normalize numeric formatting;
- remove non-semantic fields if necessary;
- include schema version;
- include generator major/minor compatibility policy.

Use a stable hash like SHA-256.

## 10. Validation rules

Reject scenario if:

- unsupported channel requested;
- unsupported condition requested;
- contradictory events exist;
- duration/sample rate invalid;
- event outside duration;
- impossible RR range;
- artifact channel missing;
- PPG requested without beat timeline;
- clinical condition label requested without supported phenotype.

Warn if:

- RR clipping occurs;
- morphology may overlap;
- signal amplitude clips;
- artifact dominates signal;
- HRV target differs from observed metrics.

## 11. Versioning policy

Use semantic scenario schema versions:

- `1.0`: initial ECG/PPG scenario contract;
- backward-compatible fields: minor increments;
- breaking changes: major increments.

All outputs must retain original scenario JSON and schema version.
