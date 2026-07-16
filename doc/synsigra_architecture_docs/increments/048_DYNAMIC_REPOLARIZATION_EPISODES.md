# Dynamic Repolarization Episodes

**Document ID:** SYN-INC-048

**Version:** 1.0

**Status:** Implemented

**Owner role:** Engineering

**Date:** 2026-07-16

**Implementation issue:** [signal_synth#49](https://github.com/tamask1s/signal_synth/issues/49)

## 1. Decision

Add smooth, time-varying ECG repolarization scenarios without post-processing
the rendered waveform.

The increment adds two separate controls:

- global QT adaptation, which maps beat RR intervals to QT through a selected
  correction model and target QTc;
- dynamic repolarization episodes, which apply smooth onset/offset envelopes
  to target QT/ST/T morphology derived from the existing repolarization
  phenotype compiler.

The first implementation is intentionally conservative: dynamic
repolarization episodes are supported on a clean sinus/SR baseline and do not
compose with rhythm episodes, ectopy, AV-block timelines, conduction
morphology, infarction/injury, hypertrophy, or static ST-T condition packs.

## 2. Public Contract

The C++ scenario API adds:

```text
ecg_qt_adaptation_model
ecg_repolarization_episode
ecg_qa_scenario::set_qt_adaptation(...)
ecg_qa_scenario::clear_qt_adaptation()
ecg_qa_scenario::qt_adaptation_enabled()
ecg_qa_scenario::qt_adaptation_model()
ecg_qa_scenario::qt_adaptation_qtc_ms()
ecg_qa_scenario::add_repolarization_episode(...)
ecg_qa_scenario::clear_repolarization_episodes()
ecg_qa_scenario::repolarization_episode_count()
ecg_qa_scenario::repolarization_episode(...)
```

Schema-v3+ JSON adds optional fields:

```text
ecg.qt_adaptation
ecg.repolarization_episodes
```

The canonical JSON writer emits these fields only when they are active.

## 3. Ground Truth

`clinical_ecg_record` now exposes dynamic annotations:

```text
clinical_dynamic_annotation
clinical_ecg_record::dynamic_annotation_count()
clinical_ecg_record::dynamic_annotations()
```

Dynamic repolarization episode boundaries are exported through the existing
`episodes` array with kind `dynamic_repolarization`.

Per-beat dynamic ground truth is exported in `annotations.json` as
`dynamic_traces` with these kinds:

```text
repolarization_severity
qt_interval_ms
qtc_ms
st_j_amplitude_mv
st_slope_mv_per_second
t_amplitude_mv
```

## 4. Rendering Model

The generator computes an effective repolarization state at each beat. QT
timing uses that state during beat annotation construction. ST-J, ST slope and
T-wave amplitude use the same state during source rendering.

The waveform still uses the existing compact T-wave and continuous injury-wave
rendering primitives. The episode envelope changes the parameters that feed
those primitives rather than adding a discontinuous signal overlay.

## 5. Example Pack

`examples/packs/ecg_dynamic_repolarization_v1.json` contains:

- `dynamic_repolarization_ischemia_v3`;
- `dynamic_repolarization_long_qt_v3`.

The pack is an engineering example and regression fixture. It is not a
clinical ischemia or long-QT validation claim.

## 6. Verification

Implemented checks cover:

- dynamic episode generation and exported interval boundaries;
- per-beat dynamic trace export;
- severity trace range and transition continuity;
- QT adaptation to RR variation;
- ST/T target morphology;
- waveform continuity across dynamic repolarization episodes;
- fingerprint coverage and replay determinism;
- invalid NORM composition and overlapping episode rejection;
- JSON roundtrip and challenge export annotations.

Primary local targeted checks:

```text
build/teszt/ecg_repolarization_test
build/signal-synth validate examples/scenarios/packs/dynamic_repolarization_ischemia_v3.json
build/signal-synth validate examples/scenarios/packs/dynamic_repolarization_long_qt_v3.json
build/signal-synth pack validate examples/packs/ecg_dynamic_repolarization_v1.json
```

## 7. Non-goals

This is not a diagnostic ischemia, electrolyte or long-QT clinical model. It
is a deterministic synthetic engineering stressor for detector, interval,
ST/T morphology and ground-truth trace QA.
