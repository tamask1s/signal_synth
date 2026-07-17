# Export and Reporting Design

Version: 0.1  
Status: Draft  
Scope: Export package formats, report contents, watermark metadata and evidence package.

## 1. Purpose

Exports and reports are central product value. The customer should receive not just waveform data, but a structured evidence package.

## 2. Export package

Recommended directory:

```text
synsigra_export_<scenario_id>_<fingerprint>/
  README.txt
  scenario.json
  metadata.json
  provenance.json
  waveform.csv
  annotations.json
  rr_tachogram.csv
  hrv_metrics.json
  ground_truth_metrics.json
  warnings.json
  ENGINEERING_CLAIM_BOUNDARY.txt
  report.html
  synsigra.hea
  synsigra.dat
  synsigra.atr
  wfdb_metadata.json
  synsigra.edf
  synsigra.bdf
  edf_bdf_metadata.json
```

Later:

```text
  waveform.hea
  waveform.dat
  waveform.edf
  report.pdf
  algorithm_comparison.json
```

## 3. CSV waveform format

Initial CSV:

```csv
sample_index,time_seconds,ecg_ii_mv,ppg_green_au,event_marker,rr_seconds
0,0.000,0.0012,0.403,0,0.857
1,0.002,0.0015,0.405,0,0.857
```

Rules:

- first column sample_index;
- second column time_seconds;
- channel names include units;
- missing channels are not included;
- sample count equals duration * sample rate if integer.

## 4. Annotations JSON

Top-level:

```json
{
  "schema_version": "1.0",
  "scenario_fingerprint": "sha256:...",
  "annotations": []
}
```

Annotation categories:

- beat;
- ecg_fiducial;
- ppg_fiducial;
- artifact_interval;
- rhythm_segment;
- morphology_change;
- warning.

## 5. Metadata JSON

Required fields:

```json
{
  "generator": {
    "name": "signal_synth",
    "product": "Synsigra Testbench",
    "version": "0.1.0",
    "git_commit": "unknown",
    "build_identity": "signal_synth/unknown"
  },
  "scenario": {
    "id": "hrv_clean_lfhf_001",
    "schema_version": "1.0",
    "fingerprint": "sha256:..."
  },
  "render": {
    "sample_rate_hz": 500,
    "duration_seconds": 300,
    "timestamp_policy": "not_recorded_for_deterministic_local_export"
  },
  "contracts": {
    "package_contract_version": "synsigra_challenge_package_v2",
    "scoring_manifest_contract_version": "synsigra_scoring_manifest_v2",
    "verifier_version": "0.6.0-dev"
  },
  "license": {
    "export_id": "exp_...",
    "customer_id": "optional",
    "redistribution": "not_allowed"
  },
  "disclaimer": {
    "intended_use": "engineering/R&D algorithm testing",
    "not_for": "diagnosis, patient monitoring, clinical validation certificate"
  }
}
```

## 6. Ground-truth metrics

Examples:

```json
{
  "beats": {
    "count": 350,
    "sinus_count": 340,
    "premature_count": 10
  },
  "hrv": {
    "mean_rr_seconds": 0.857,
    "observed_sdnn_seconds": 0.052,
    "observed_rmssd_seconds": 0.041,
    "rr_clipping_count": 0
  },
  "artifacts": {
    "interval_count": 3,
    "total_artifact_seconds": 45.0
  }
}
```

## 7. Report contents

Report sections:

1. Title and export metadata.
2. Intended use and limitation statement.
3. Scenario summary.
4. Generator version and fingerprint.
5. Waveform preview plots.
6. Beat/RR summary.
7. HRV ground-truth metrics.
8. ECG fiducial summary.
9. PPG fiducial summary.
10. Artifact intervals.
11. Warnings and limitations.
12. License/export notice.
13. Appendix: scenario JSON hash.

## 8. Critical disclaimer language

Use:

> This report describes synthetic engineering test signals generated from the specified scenario. It is intended for research, development, software testing and algorithm QA. It is not a clinical validation certificate, not a diagnostic result, and not standalone evidence of medical-device conformity.

Generated packages must also include `provenance.json` and
`ENGINEERING_CLAIM_BOUNDARY.txt` so archived artifacts retain generator
identity, contract versions and the exact engineering QA claim boundary.

## 9. Watermarking

Every export should include visible metadata and optional hidden/watermarkable metadata.

Visible:

- export ID;
- scenario fingerprint;
- generator version;
- customer/license ID if SaaS;
- terms summary.

Hidden/embedded:

- JSON metadata;
- CSV comment header if compatible;
- report footer;
- future WFDB comments;
- future analog scenario ID in calibration run logs.

## 10. Report reproducibility

A report should be reproducible from:

- scenario JSON;
- generator version;
- seed;
- export options;
- renderer version.

Include all of these in metadata.

## 11. Comparison report later

When customer algorithm outputs are imported, add:

- detection tolerance window;
- matched true positives;
- false positives;
- false negatives;
- timing error histogram;
- artifact-specific performance;
- HRV metric deviation;
- confusion matrix if beat classes are submitted.

Do not call this a clinical validation report.

Suggested title:

> Algorithm Test Evidence Report

or

> Synthetic Scenario Performance Report

## 12. Export retention

For SaaS:

- free/pro: short retention;
- lab: longer retention;
- enterprise: configurable;
- on-prem: local retention.

## 13. Acceptance criteria

Export/report feature is MVP-complete when:

- a scenario can be rendered to CSV/JSON;
- annotations align with waveform;
- metadata includes fingerprint and generator version;
- HTML report is generated;
- warnings and limitations are included;
- export package is zipped;
- package can be regenerated deterministically.

## 14. Current local ECG package

The implemented local schema-v1 ECG package contains:

- canonical `scenario.json`;
- `metadata.json` with document, generation, and run identities;
- 12-lead `waveform.csv` in mV;
- `annotations.json` with beats, atrial events, construction/measured
  fiducials, RR tachogram ground truth, and artifact intervals;
- `rr_tachogram.csv` with beat time, RR, clipping, ectopic, artifact-overlap,
  and exclusion flags;
- `hrv_metrics.json` with metric definitions, units, exclusion policy, SD1,
  SD2, LF, HF, LF/HF, and tachogram data;
- `ground_truth_metrics.json` with HRV summary metrics and phenotype
  assertions;
- `warnings.json`;
- self-contained `report.html` with an actual Lead II SVG preview;
- controlled-use `README.txt`;
- WFDB `synsigra.hea`, `synsigra.dat`, and `synsigra.atr`;
- EDF+/BDF+ `synsigra.edf`, `synsigra.bdf`, and sidecar metadata.

Unsigned 64-bit identity values are encoded in JSON as canonical decimal
strings. In particular, `generation_fingerprint`,
`resolved_generation_fingerprint`, and `ecg_run_fingerprint` contain only the
digits `0` through `9` and no leading sign. This preserves all 64 bits for
JavaScript and JSON libraries limited to signed 64-bit integers. The C++ API
continues to expose these values as `unsigned long long`; only their JSON wire
representation is textual. SHA-256 and composite render identities were
already strings. Readers of historical packages should continue to accept the
legacy numeric representation.

The artifact bytes intentionally omit wall-clock time so local regeneration is
deterministic. A future SaaS audit record may add export time, customer,
license, and download identity outside the render fingerprint.
