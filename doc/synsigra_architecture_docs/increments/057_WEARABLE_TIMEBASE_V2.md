# Multi-rate Wearable Timebase V2

**Document ID:** SYN-INC-057

**Version:** 1.0

**Status:** Implemented

**Owner role:** Engineering

**Date:** 2026-07-17

**Implementation issue:** [signal_synth#77](https://github.com/tamask1s/signal_synth/issues/77)

## 1. Decision

Wearable acquisition is a layer after latent ECG, PPG, physiology, and
analytic artifact rendering. The latent timeline remains the authority for
physiology and annotations. Each enabled device stream independently samples
that timeline through an explicit clock model.

Scenario schema version 5 owns one `wearable` object with `ecg`, `ppg`, and
`accelerometer` stream configurations. Each enabled stream declares:

- nominal sample rate;
- clock offset and linear drift;
- bounded deterministic timestamp jitter;
- packet size;
- deterministic packet-loss trigger probability and burst length;
- independent seed.

The old `ppg.clock_drift_ppm` field is removed. PPG pulse-delay controls are
physiological; device clock error belongs only to `wearable.ppg`. No
compatibility alias or dual interpretation is retained.

## 2. Clock Model

For stream sample index `i`, nominal rate `f`, offset `o`, and drift `d` ppm:

```text
clock_scale = 1 + d * 1e-6
latent_time(i) = i / (f * clock_scale)
ideal_device_time(i) = o + i / f
reported_device_time(i) = ideal_device_time(i) + deterministic_jitter(i)
```

Offset changes device timestamps but not physiological acquisition time.
Drift changes acquisition cadence and accumulates relative clock error.
Jitter is index-addressed, so rendering order and chunk size cannot change it.
Its configured bound is restricted to keep timestamps strictly increasing.

The latent waveform is sampled by deterministic linear interpolation. Enabled
device rates may not exceed the latent render rate. The interpolation method
and mapping equations are exported as contract metadata; no hidden resampling
policy is inferred by adapters.

## 3. Packet Model

Packets are fixed-size contiguous sample-index ranges; the final packet may be
shorter. A packet starts a loss burst when an index-addressed pseudorandom draw
is below `packet_loss_probability`. The configured
`packet_loss_burst_packets` determines the burst length. Packet loss does not
delete latent truth or renumber samples.

The in-memory stream retains every resampled value and marks each sample as
received or dropped. Device-facing sample files contain received rows only.
Ground-truth files retain the complete mapping and packet schedule.

## 4. Public C++ Contract

`wearable_timebase.h/.cpp` owns:

- stream and timebase configuration validation;
- sample, packet, and channel records;
- chunk-invariant stream rendering and fingerprints;
- nearest-sample event mapping;
- ECG-R/PPG-onset/PPG-peak alignment truth.

`ecg_render_bundle` owns a `wearable_timebase_record`. Schema versions below 5
produce no wearable record. Schema version 5 renders all configured streams
after signal-quality and physiology coupling, so device samples represent the
final observable latent waveform.

## 5. Export Contract

Schema-v5 exports add:

```text
wearable_ecg_samples.csv
wearable_ppg_samples.csv
wearable_accelerometer_samples.csv   # when enabled
wearable_timestamp_truth.csv
wearable_timebase_truth.json
wearable_alignment_truth.json        # when ECG and PPG are enabled
```

Device-facing sample CSV rows contain sample index, packet index, reported
device timestamp, and signal values, and omit dropped packets. Timestamp truth
contains every sample's latent, ideal-device, and reported-device time plus its
received flag. Timebase truth declares formulas, stream configuration,
channels, fingerprints, and the complete packet schedule.

The existing `waveform.csv`, WFDB, EDF, and BDF artifacts remain common latent
reference exports. They are not silently reinterpreted as asynchronous device
streams.

## 6. Alignment And Measurement Truth

Per-beat alignment truth maps latent ECG R, PPG onset, and measured PPG peak
events to their nearest device samples. It reports both:

- physiological latent PTT/peak delay; and
- observed device-timestamp delta and observed-minus-physiological error.

This makes constant offset, accumulated drift, sampling quantization, jitter,
and packet receipt distinguishable from physiological PTT. Schema-v5
`ecg_ppg_alignment` measurement truth adds these device-domain quantities
without changing the generic measurement submission contract.

## 7. Python And Challenge Packages

The generator-free Python package only reads package artifacts. `ChallengeCase`
provides accessors for wearable stream samples, timestamp truth, timebase
truth, and alignment truth. It does not contain the generator or recreate the
clock model.

Challenge manifests use explicit roles for wearable samples, timestamp truth,
timebase truth, and alignment truth. Package hashes cover the files exactly.

## 8. DataBrowser Integration

The generation-only core subset gains `wearable_timebase.h/.cpp`. A dedicated
`GenerateWearableScenarioJSON` API creates:

- a multi-rate signal variable with received ECG, PPG, and accelerometer
  samples, with dropped samples shown as zero and marked by packet-loss
  intervals;
- a multi-rate timing variable containing timestamp error and packet
  availability for each stream.

`081_Wearable_Multirate_Timebase.txt` visualizes both variables. Export,
challenge, scoring, and Python-only files are not copied into the SVN project.

## 9. Compatibility And Migration

- No alias is provided for `ppg.clock_drift_ppm`.
- Existing repository scenarios are rewritten to remove that field.
- Wearable examples move to schema version 5 and the curated wearable pack is
  replaced by `wearable_timebase_v2`.
- Non-wearable schema versions continue to render the common latent timeline
  and produce no asynchronous device artifacts.
- The implementation remains C++11 and GCC 4.9 compatible.

## 10. Verification

Stable tests cover:

- strict schema-v5 parse/write and migration rejection;
- independent rates, offsets, drift, jitter, and packet schedules;
- timestamp monotonicity and configured bounds;
- exact replay and identical fingerprints across alternate chunk sizes;
- resampling values and received-row export behavior;
- physiological versus device-domain ECG/PPG alignment;
- challenge roles and package integrity;
- Python artifact access through an installed generator-free wheel;
- DataBrowser generation-core compile smoke and a v5 render smoke.

Verification completed on 2026-07-17:

- 53/53 repository CTest cases passed;
- `TEST-WEARABLE-TIMEBASE-001` and `TEST-WEARABLE-RENDER-001` passed under
  AddressSanitizer and UndefinedBehaviorSanitizer;
- `TEST-DATABROWSER-GCC49-001` passed with the C++11 generation subset;
- the generator-free Python wearable challenge round trip and package
  integrity test passed;
- the `synsigra` 0.6.0 sdist and wheel built successfully, and the installed
  wheel passed the isolated distribution smoke test.

The feature provides deterministic synthetic engineering QA evidence. It does
not claim wearable hardware, patient physiology, or clinical validity.
