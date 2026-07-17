# Generic Measurement Output And Scoring

**Document ID:** SYN-INC-056

**Version:** 1.0

**Status:** Implemented

**Owner role:** Engineering

**Date:** 2026-07-17

**Implementation issue:** [signal_synth#76](https://github.com/tamask1s/signal_synth/issues/76)

## 1. Decision

Scalar and waveform-derived measurements use the existing
`synsigra_submission_v1` workflow. There is no second customer command or
target-specific directory convention. A challenge declares a measurement
output in `submission.json`, describes it in `formats.json`, and the customer
runs:

```text
synsigra-verify <challenge> <submission-directory> <result-directory>
```

The native C++ layer owns measurement truth construction, strict JSON/CSV I/O,
matching, metrics, and engineering CLI reports. Challenge packages carry the
canonical truth artifact. The generator-free Python package consumes that
artifact and implements the same scoring semantics without generator code.

## 2. Customer Payload

`measurement_values_json_v1` contains only `schema_version` and a
`measurements` array. `measurement_values_csv_v1` uses the same record fields:

```text
name,value,unit,status,scope,time_seconds,beat_index,channel,formula,confidence
```

Record fields are:

- `name`: stable measurement name;
- `value`: finite number, present only when `status` is `valid`;
- `unit`: explicit engineering unit;
- `status`: `valid`, `undefined`, `absent`, or `not_evaluable`;
- `scope`: `record`, `lead`, `beat`, `beat_lead`, or `paired_signal`;
- `time_seconds`: optional non-negative temporal anchor;
- `beat_index`: optional canonical unsigned decimal string;
- `channel`: required for lead and paired-signal scopes;
- `formula`: required where the definition depends on a named formula, most
  importantly QTc;
- `confidence`: optional number in `[0,1]`.

Record and lead measurements are unique by `(name,unit,scope,channel,formula)`.
Beat-like measurements add a temporal or beat anchor. Beat indices are an
optional interoperability aid, not a requirement to reuse generator identity.

Supported units in the first contract are `s`, `mV`, `mV/s`, `deg`, `count`,
`ratio`, `%`, `bpm`, `a.u.`, and `bool` (numeric zero or one). QT correction
names are `fixed`, `bazett`, `fridericia`, `framingham`, and `hodges`.

## 3. Truth Status

Truth uses the same four statuses as customer measurements:

- `valid`: a finite numerical reference exists;
- `undefined`: a mathematical definition has no value for this case;
- `absent`: the source wave, beat, or pulse intentionally does not exist;
- `not_evaluable`: the source exists globally but the requested channel or
  record window cannot support a reliable measurement.

Undefined, absent, and not-evaluable truth is explicit and never encoded as
zero. A user status is scored against the truth status. A numeric prediction
for non-valid truth is a status mismatch, not a numerical zero error.

## 4. Truth Adapters

`morphology_assertions` becomes locally scoreable through:

- beat-level PR, QRS, QT, and QTc intervals;
- beat-and-lead ST-J level, J+60 level, ST slope, and T amplitude;
- record-level P, QRS, and T frontal axes;
- record-level phenotype assertion measurements with their expected ranges.

PR is absent without a linked measurable atrial event. QT and QTc are absent
without a measurable T wave. Lead T amplitude is absent when the wave is
globally absent and not-evaluable when the wave exists but is not measurable
in that lead. QTc truth carries its correction formula explicitly.

`ecg_ppg_alignment` becomes locally scoreable through per-beat paired-signal
ECG-R-to-PPG-onset PTT and ECG-R-to-PPG-systolic-peak delay. Intentionally
missing pulses are absent; out-of-record or unmeasurable peaks are
not-evaluable. Channel names identify the signal pair.

The challenge stores all locally scoreable measurement truth in
`cases/<case-id>/measurement_truth.json` under the internal
`synsigra_measurement_truth_v1` contract. The SaaS and customer package must
copy this artifact without reconstructing it.

## 5. Matching And Error Models

Records are grouped by `(name,unit,scope,channel,formula)`.

- record and lead groups match their sole truth/prediction directly;
- beat-like records match exact beat indices when supplied on both sides;
- remaining beat-like records match one-to-one by minimum temporal distance
  inside a configurable pairing window;
- duplicate identities are rejected during input parsing.

Linear measurements report signed error, absolute error, and relative error.
Axes use shortest signed circular error in degrees. A value passes when its
absolute error is within the larger applicable absolute or relative tolerance.
Relative error is undefined when the truth value is zero.

## 6. Metrics And Reports

Overall, per-measurement, per-channel, per-case, and pack summaries report:

- valid, undefined, absent, and not-evaluable truth counts;
- prediction, matched, missing, extra, and status-mismatch counts;
- truth-match and prediction-match fractions, so empty or overproduced outputs
  cannot pass solely because numerical metrics are undefined;
- numerical pair count and tolerance pass count/fraction;
- signed bias, MAE, RMSE, median, 95th percentile, and maximum absolute error;
- assertion range agreement where an expected phenotype range exists.

Zero-denominator metrics are `null` in JSON and `NA` in CSV/HTML. Reports retain
matched, missing, extra, and status-mismatched records for diagnosis.
Overall coverage, status, and tolerance fractions are unit-independent. Raw
error magnitudes across unlike units are descriptive only; acceptance and
scientific interpretation must use the per-measurement or
per-measurement/channel groups.

## 7. Public Interfaces

- C++: `measurement_io.h` and `measurement_scoring.h`;
- CLI: `signal-synth measurement score <target> ...`;
- submission formats: `measurement_values_json_v1` and
  `measurement_values_csv_v1`;
- score type: `measurement`;
- Python: the existing `synsigra-verify` command and package API;
- curated packs: existing morphology and ECG/PPG alignment packs, now locally
  scoreable.

## 8. Failure And Resource Policy

- Unknown JSON members and CSV columns are errors.
- Duplicate JSON keys, measurement identities, and CSV columns are errors.
- Non-finite values, invalid units, invalid status/scope combinations, unsafe
  decimal beat indices, and confidence outside `[0,1]` are errors.
- Reports are written only after parsing, rendering/truth loading, and scoring
  succeed.
- Native code remains C++11 and GCC 4.9 compatible.

## 9. Non-goals And Limitations

- no clinical measurement or diagnostic-performance claim;
- no patient-derived acceptance ranges;
- no server-side execution of customer algorithms;
- no compatibility adapters for pre-release target-specific output paths;
- no requirement that a user expose proprietary beat identities.

## 10. Verification

Tests cover strict JSON/CSV round trips, status and scope validation, temporal
and exact-index matching, circular axes, missing/extra/status mismatches,
undefined truth, all truth adapters, grouped metrics, generated templates,
native CLI reports, Python/C++ parity, curated catalog metadata, package
integrity, and the generator-free installed wheel.
