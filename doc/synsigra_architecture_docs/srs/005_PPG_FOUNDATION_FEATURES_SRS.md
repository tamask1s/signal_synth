# PPG Foundation Feature SRS

**Document ID:** SYN-SRS-PPG-FOUND-001

**Version:** 0.1

**Status:** Draft implementation input

**Date:** 2026-07-04

**Parent issue:** [signal_synth#32](https://github.com/tamask1s/signal_synth/issues/32)

## 1. Purpose

Define PPG generator and scoring features still needed before serious
wearable-algorithm QA and SaaS buildout.

Current PPG strengths:

- one green PPG channel derived from ECG beat timeline;
- configurable beat-to-beat delay, rise/decay, amplitude and dicrotic feature;
- explicit low-perfusion, weak-pulse and missing-pulse state;
- construction and measured onset/peak/offset annotations;
- deterministic motion/sensor artifact generation with motion reference;
- PPG peak/onset, pulse-interval and pulse-rate scoring with quality bins.

The current model is useful but still simple. The next work should improve
wearable QA relevance while keeping exact ground truth.

## 2. Physiological Modulation Requirements

| ID | Requirement |
|---|---|
| `REQ-PPG-PHYS-001` | PPG pulse transit time shall support deterministic beat-to-beat variation. |
| `REQ-PPG-PHYS-002` | PPG amplitude shall support respiratory and low-frequency modulation. |
| `REQ-PPG-PHYS-003` | Perfusion drop scenarios shall reduce amplitude and/or distort morphology over configured intervals. |
| `REQ-PPG-PHYS-004` | Missed or weak pulse scenarios shall be explicitly annotated. |
| `REQ-PPG-PHYS-005` | PPG morphology variation shall preserve measured peak/onset/offset ground truth. |

## 3. Motion and Sensor Artifact Requirements

| ID | Requirement |
|---|---|
| `REQ-PPG-MOT-001` | PPG motion artifact scenarios shall support an accelerometer-like reference channel. |
| `REQ-PPG-MOT-002` | Motion artifact shall support periodic, burst, and broadband components. |
| `REQ-PPG-MOT-003` | Ambient-light artifact and sensor saturation shall be supported. |
| `REQ-PPG-MOT-004` | Artifact intervals shall include exact masks and affected channels. |
| `REQ-PPG-MOT-005` | Motion artifacts shall be deterministic from seed and scenario parameters. |

## 4. Multi-Channel PPG Requirements

| ID | Requirement |
|---|---|
| `REQ-PPG-MCH-001` | The PPG model shall support optional red and infrared channels after green PPG is stabilized. |
| `REQ-PPG-MCH-002` | Multi-channel PPG shall support channel-specific gain, baseline, delay, and noise. |
| `REQ-PPG-MCH-003` | Any SpO2-like scenario shall be framed as engineering signal simulation, not clinical oxygenation validation. |

## 5. PPG Scoring Requirements

| ID | Requirement |
|---|---|
| `REQ-PPG-SCORE-001` | PPG peak scoring shall support CSV and JSON user detections. |
| `REQ-PPG-SCORE-002` | PPG onset scoring shall be supported after onset ground truth is stable. |
| `REQ-PPG-SCORE-003` | Pulse interval and pulse-rate scoring shall be supported. |
| `REQ-PPG-SCORE-004` | Scoring reports shall split clean, low-perfusion, motion-artifact, and dropout intervals where truth is available. |

## 6. Scenario Pack Requirements

At minimum, create PPG packs for:

- clean fixed delay;
- variable PTT;
- respiratory amplitude modulation;
- perfusion drop;
- motion artifact with accelerometer reference;
- dropout/saturation;
- missed/weak pulses;
- ECG/PPG combined quality stress.

## 7. Acceptance Criteria

1. PPG scenarios can test peak and pulse-rate algorithms under clean and
   artifacted conditions.
2. Motion scenarios include a deterministic accelerometer reference channel.
3. PPG ground truth identifies valid, weak, missed, and artifact-overlapped
   pulses.
4. PPG scoring reports are deterministic and artifact-aware.
5. Multi-channel PPG is supported only after single-channel contracts are
   stable.

## 8. Planned Implementation Issues

- PPG physiology v2 with variable PTT and modulation (implemented by #50);
- PPG motion artifact with accelerometer reference (implemented by #51);
- PPG perfusion/dropout/weak-pulse scenarios (implemented by #52);
- PPG detection JSON/CSV scoring expansion (implemented by #53);
- PPG benchmark pack (implemented by #53);
- optional red/IR PPG extension.
