# PPG Perfusion And Pulse State

**Document ID:** SYN-INC-038

**Version:** 1.0

**Status:** Verified locally

**Owner role:** Engineering

**Date:** 2026-07-06

**Implementation issue:** [signal_synth#52](https://github.com/tamask1s/signal_synth/issues/52)

## 1. Decision

Represent low perfusion, weak pulses, and missing pulses as physiological PPG
episode state, not as acquisition dropout. A schema-v4 perfusion episode owns:

- exact start and duration;
- pulse amplitude, rise-time, and decay-time scaling;
- optional weak-pulse cadence and weak amplitude scaling;
- optional missing-pulse cadence.

Episodes are non-overlapping and bounded by scenario duration.
Weak and missing cadences restart at the first ECG-linked pulse in each
episode.

## 2. State Semantics

Every expected ECG-linked pulse has exactly one state:

- `valid`: generated normally;
- `weak`: generated with explicit weak-pulse scaling;
- `missing`: intentionally absent;
- `out_of_record`: its complete support would fall outside the output window.

`low_perfusion` is an independent flag and may accompany valid or weak pulses.
Only generated valid/weak pulses set `valid_for_peak_scoring=true`. Missing and
boundary pulses remain exported as expected-pulse ground truth without a
fabricated measured peak.

Acquisition dropout remains a separate artifact interval. This distinction
allows later scoring to separate physiological pulse absence from sensor
corruption.

## 3. Export And Metrics

`annotations.json` exports effective pulse properties and state. Ground-truth
metrics report expected, generated, low-perfusion, weak, missing, and
out-of-record counts. Peak comparison keeps its existing total, clean, and
artifact metrics and adds overlapping low-perfusion and weak-pulse metrics,
plus missing-pulse opportunity and detection counts. Pulse-rate scoring,
pack-level aggregation, and benchmark reporting remain owned by #53.

## 4. Verification

- exact episode bounds and overlap rejection;
- expected/generated/weak/missing count consistency;
- amplitude and morphology changes only inside configured episodes;
- no measured annotations for intentionally missing pulses;
- deterministic replay and schema-v4 round trip.

Local release verification passed all 36 tests on 2026-07-06, including C++ /
generator-free Python scoring parity for the perfusion stress case.

## 5. Limits

Perfusion scale is an engineering stress control and carries no clinical
perfusion or oxygenation claim.

## 6. DataBrowser And SVN Impact

None. No DataBrowser API is added.
