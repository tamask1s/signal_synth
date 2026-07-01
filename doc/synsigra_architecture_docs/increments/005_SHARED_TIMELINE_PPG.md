# Shared-Timeline PPG Generation

**Document ID:** SYN-ARCH-INC-005

**Version:** 0.1

**Status:** Implementing

**Owner role:** Biosignal Algorithms / Scenario API

**Date:** 2026-07-01

**Proposed traceability ID:** `TRC-PPG-001`

**Implementation issue:** [signal_synth#17](https://github.com/tamask1s/signal_synth/issues/17)

## 1. Decision

Add a deterministic single-channel PPG model driven by the exact ventricular
beat timeline generated for ECG. Every PPG pulse and annotation shall retain
the linked ECG beat index and R time.

Introduce scenario document schema v2 for explicit PPG configuration while
continuing to parse, canonicalize, fingerprint, and render schema-v1 ECG-only
documents without byte or behavior changes.

## 2. Requirements

- `REQ-GEN-001..005`;
- `REQ-SCN-002..003`, `REQ-SCN-005..006`;
- `REQ-PPG-001..004`;
- `REQ-GT-001..002`, `REQ-GT-004`;
- `REQ-EXP-001..004`;
- `REQ-RPT-001..003`;
- `REQ-API-001..003`;
- `REQ-NFR-001..005`, `REQ-NFR-008`;
- `REQ-VER-001`, `REQ-VER-006`, `REQ-VER-008..010`;
- `REQ-DOC-001..002`.

PPG acquisition artifacts and accelerometer coupling remain future scope.

## 3. PPG model contract

Add `ppg_model.h/.cpp` with:

```cpp
struct ppg_config;
struct ppg_annotation;
class ppg_record;
class ppg_generator;
```

Configuration:

- enabled;
- pulse arrival delay;
- systolic rise time;
- post-peak decay time;
- pulse amplitude and baseline;
- dicrotic component delay, width, and relative amplitude.

Output:

- one `ppg_green` channel in arbitrary optical units;
- exact construction onset, systolic peak, dicrotic feature, and offset;
- measured discrete systolic maximum;
- linked ECG beat index and exact ECG R time;
- sample index as first sample at or after construction time;
- deterministic copied/reset-free one-shot record.

## 4. Waveform design

Each eligible ventricular beat creates:

1. a smooth half-cosine systolic rise from onset to configured peak;
2. a smooth half-cosine decay from peak to pulse offset;
3. an optional bounded smooth dicrotic component;
4. configured baseline.

The pulse is evaluated analytically at sample times and summed if pulses
overlap. The construction peak describes the primary pulse component. The
measured peak is the maximum of the final sampled PPG in that beat's
non-overlapping measurement window.

No random state is needed in this increment. Beat-to-beat timing variability
comes exclusively from the shared ECG timeline.

## 5. Schema v2

Schema v2 adds required:

```json
"ppg": {
  "enabled": true,
  "pulse_delay_ms": 180,
  "rise_time_ms": 120,
  "decay_time_ms": 300,
  "amplitude_au": 1,
  "baseline_au": 0,
  "dicrotic_delay_ms": 180,
  "dicrotic_width_ms": 80,
  "dicrotic_amplitude_ratio": 0.15
}
```

Rules:

- schema v1 keeps its exact canonical bytes and implies no PPG;
- schema v2 always writes the explicit PPG object;
- disabled PPG produces no PPG samples or annotations;
- delays/durations must be positive, finite, and bounded;
- dicrotic amplitude ratio may be zero;
- pulse duration must fit within the documented maximum;
- document SHA-256 covers all PPG fields;
- ECG generation fingerprint remains ECG-only;
- export/run identity additionally records the PPG configuration through the
  document fingerprint and artifact bytes.

## 6. Export and report integration

For enabled PPG:

- append `ppg_green_au` to `waveform.csv`;
- add PPG channel metadata;
- export PPG construction/measured annotations;
- export configured and observed ECG-to-PPG delays;
- include PPG count and delay metrics;
- include an actual PPG SVG preview;
- retain the clean ECG and PPG channels separately.

Schema-v1 artifact bytes remain unchanged except for future generator-version
increments explicitly documented elsewhere.

## 7. DataBrowser integration

Add a `SignalProc_RSPT.cpp` API function that:

- accepts product-safe PPG parameters and annotation display mode;
- returns ECG Lead II, PPG, and optional annotation channels/markers;
- assigns channel names in the API;
- uses the portable core types directly.

Add a SigForge script under `DataBrowser_psaa/SigForge/Scripts/` that displays
ECG and PPG in a dedicated window and saves the `Var` before `DisplayData`.
The script shall use `N` or `A2`, not `C`, for display behavior.

Core PPG files and changed shared headers shall be synchronized to the SVN
SignalProcessors directory. DataBrowser verification remains manual evidence
and must be identified separately from GitHub CI.

## 8. Verification

Add `TEST-PPG-001` and extend JSON/export tests:

- configuration boundaries and transactional failure;
- deterministic repeated generation;
- exact ECG beat linkage;
- construction delay known-answer;
- measured peak equals a discrete local/global window maximum;
- annotation order, bounds, and sample/time mapping;
- no annotations when disabled;
- schema-v1 canonical hash regression;
- schema-v2 canonical roundtrip and PPG fingerprint sensitivity;
- export row/channel/annotation/metric consistency;
- actual PPG report SVG;
- normal and ASan/UBSan runs;
- Linux/Windows CI;
- manual DataBrowser visualization record.

## 9. Non-goals

- Blood pressure, SpO2, multi-wavelength optics, vascular physiology, pulse
  wave velocity, patient-specific PTT, respiratory modulation, random
  amplitude variation, motion, low perfusion, dropout, saturation, ambient
  light, or accelerometer coupling.
- Clinical-grade pulse morphology or hemodynamic claims.
- Independent PPG rhythm timeline.

## 10. Acceptance criteria

1. Every generated PPG pulse links to one ECG ventricular beat.
2. Fixed delay and measured peak tests pass at multiple sample rates.
3. Schema v1 remains compatible and schema v2 is canonical/deterministic.
4. Export and report expose separate ECG/PPG truth consistently.
5. DataBrowser API/script visualizes ECG, PPG, and optional annotations.
6. Core files are synchronized to SVN and all automated tests pass.
7. Traceability records distinguish automated core evidence from manual
   Windows/DataBrowser evidence.

## 11. Risks and limitations

| Risk | Control |
|---|---|
| PPG drifts from ECG timing | Generate exclusively from ECG beat annotations |
| Construction and measured peaks are confused | Separate annotation source field |
| Dicrotic component moves global peak | Measure final samples independently |
| Schema v2 breaks v1 users | Preserve exact v1 parser/writer regression |
| PPG interpreted physiologically | Engineering-phantom limitation in report |

The first PPG model is intended for peak/delay/HRV pipeline QA, not vascular
or clinical simulation.

## 12. Change log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-07-01 | Initial proposed design |
