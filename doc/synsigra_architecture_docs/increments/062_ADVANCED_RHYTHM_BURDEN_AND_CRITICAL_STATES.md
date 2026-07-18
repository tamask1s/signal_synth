# Advanced Rhythm Burden and Critical-State Stress

Issue: https://github.com/tamask1s/signal_synth/issues/83

## Intent

This increment adds deterministic engineering stress for multi-episode rhythm algorithms. It is not a diagnostic rhythm simulator and does not establish clinical performance.

## Public scenario contract

`ecg.rhythm_episodes` is the only rhythm-episode input. The previous single-episode scalar fields were removed rather than retained as compatibility aliases.

Each episode contains:

- `type`: `afib`, `psvt`, `svarr`, `vt`, `vf`, or `asystole`;
- exact half-open start and duration;
- transition duration;
- rate for beat-producing states, or zero for VF/asystole;
- deterministic seed.

Episodes are canonicalized by start time, must not overlap, and must fit entirely within the requested record. AF and VF require at least 20 ms of transition time so their continuous waveform components cannot start or stop with a step. Diagnostic conditions remain the fixed 71-statement PTB-XL-derived catalog; VT, VF and asystole are engineering stress states, not new diagnostic assertions.

## Generation layers

The scenario layer validates and fingerprints the episode list. The clinical timeline layer schedules sinus baseline segments and episode-specific beats. AF is irregular and P-wave absent, PSVT/SVARR are narrow-complex supraventricular episodes, and VT uses wide ventricular-origin beats. VF is a smooth, seeded multi-component waveform without beat annotations. Asystole is beat-free and waveform-free away from boundary tails.

Every episode, including VF and asystole, remains present in `clinical_episode_annotation`; beat indices are optional for beat-free states. Exact state and transition samples are exported in `annotations.json`.

## Verification contracts

`rhythm_episode` uses the existing interval contract and reports sensitivity, precision, temporal IoU, onset/offset error, false alarms per hour and class confusion.

`rhythm_burden` uses `synsigra_measurement_truth_v1`. It exposes total and per-class:

- `burden_duration` in seconds;
- `burden_fraction` as a record fraction;
- `episode_count`.

Customer predictions use the existing `measurement_values_json_v1` or `measurement_values_csv_v1` formats. No new customer output format or scoring API was introduced.

## Product integration

`advanced_rhythm_burden_v1` contains recurrent AF/PSVT, recurrent VT, and VF/asystole cases. The curated catalog and integration contract advertise both scoreable targets. SaaS must render `rhythm_episodes` as a list editor, expose the new pack and target, and remove the retired scalar episode controls.

DataBrowser uses `GenerateECGScenarioJSON`; script `085_ECG_Advanced_Rhythm_Burden.txt` shows named interval markers and interval channels.

## Compatibility and limits

- Scenario JSON containing the retired scalar episode fields is rejected.
- Rhythm episodes currently require a sinus baseline and do not compose with ectopy, AV-block timelines, static non-sinus rhythm, HRV variability, or dynamic repolarization episodes.
- VF morphology is deterministic detector stress, not a patient-derived or clinically validated waveform model.
- The implementation remains C++11 and GCC 4.9 compatible.
