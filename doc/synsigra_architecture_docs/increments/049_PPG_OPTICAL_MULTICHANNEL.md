# PPG Optical Multi-Channel Extension

Status: Implemented

Issue: https://github.com/tamask1s/signal_synth/issues/54

## Purpose

Add optional red and infrared PPG channels for engineering signal QA without
making clinical SpO2 or oximeter-validation claims.

The green PPG channel remains the primary scored PPG channel. Red and infrared
channels are generated from the same ECG-linked pulse timeline as channel-specific
optical variants.

## Architecture

- `ppg_config` keeps the legacy green channel controls as the primary PPG
  contract.
- `ppg_config::red` and `ppg_config::infrared` are optional fixed optical
  channel configs with:
  - `enabled`;
  - `amplitude_gain`;
  - `baseline_au`;
  - `delay_ms`;
  - `noise_std_au`;
  - `seed`.
- `ppg_record` exposes indexed channel access:
  - `channel_count()`;
  - `channel_name(index)`;
  - `channel_samples(index)`;
  - channel-specific metadata accessors;
  - channel-specific fiducial accessors.
- Legacy `samples()` and `annotations()` continue to return the green channel.

## Ground Truth Contract

- `annotations.json.ppg_fiducials` remains green-only for backward-compatible
  `ppg_systolic_peak` and `ppg_pulse_onset` scoring.
- `annotations.json.ppg_channel_fiducials` contains non-green channel fiducials.
- `metadata.json`, `wfdb_metadata.json`, and `edf_bdf_metadata.json` list every
  generated optical channel and its gain/baseline/delay/noise settings.
- `ground_truth_metrics.json.ppg.channel_count` reports the rendered PPG channel
  count.

## Export Contract

- `waveform.csv` includes `ppg_green_au`, plus `ppg_red_au` and/or
  `ppg_infrared_au` when enabled.
- WFDB, EDF+ and BDF+ waveform exports include the added PPG channels.
- Native WFDB/EDF/BDF annotation files remain green PPG peak oriented; full
  multi-channel ground truth is in `annotations.json`.

## Claim Boundary

The red and infrared channels are engineering optical channels. The generator
does not claim clinical oxygen saturation realism, oximeter equivalence,
patient physiology validation, or medical-device conformity evidence.

## Verification

- `ppg_model_test` covers deterministic multi-channel generation, channel names,
  delay/gain behavior, channel-specific samples and invalid channel settings.
- `ppg_physiology_test` covers schema-v4 JSON roundtrip, render, metadata,
  annotations, waveform CSV, WFDB and EDF/BDF metadata contracts.
- `examples/scenarios/ppg_multichannel_optical_v4.json` is a runnable scenario
  for manual and SaaS integration checks.
