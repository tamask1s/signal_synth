# Extended ECG Wave and Beat Morphologies

Issue: https://github.com/tamask1s/signal_synth/issues/82

## Intent

This increment adds deterministic engineering stress for ECG morphology and beat algorithms. It does not model diagnoses or establish clinical validity.

## Uniform scenario contract

Schema version 7 adds one optional `ecg.extended_morphology` object. Its `components` array is the only input for U waves, biphasic/notched P and T waves, R-prime, and fragmented QRS. Every component has the same fields: `type`, a non-empty `leads` array, signed `amplitude_mv`, anchor-relative `offset_ms`, and `duration_ms`.

P components are anchored to P onset, QRS components to QRS onset, T components to T onset, and U waves to T offset. Components are deterministic lead-domain overlays with zero value and zero slope at both boundaries. Their construction and measured fiducials identify the exact configured lead and beat. Retained cardiac source and VCG channels intentionally represent the underlying cardiac source model before these explicit lead-domain stress overlays.

The same object contains `fusion_every_n_beats` and `fusion_ventricular_fraction`. Fusion beats retain their conducted atrial event and blend normal and ventricular QRS/T morphology without introducing a premature RR interval.

## Existing verification contracts

No new customer output format or score type is introduced:

- `ecg_delineation` gains secondary P/T, notch, R-prime, QRS-fragment and U-wave labels;
- `morphology_assertions` gains per-beat, per-lead component amplitude and R-relative timing truth;
- `ecg_beat_classification` gains the canonical scored class `fusion` and exports native WFDB fusion annotations.

The curated pack combines wave-component, QRS-component and fusion cases. The DataBrowser adapter and script 086 expose the same configuration for visual inspection.

## Validation and limits

- Component windows must fit their parent P, QRS or T wave; U-wave windows start after T offset.
- A lead cannot receive duplicate components of the same type.
- Fusion cadence requires sinus rhythm with normal AV conduction and cannot compose with periodic ectopy, pacing or rhythm episodes.
- Lead-domain overlays are explicit detector stress, not a source-localization or patient-fitted model.
- The implementation remains C++11 and GCC 4.9 compatible.
