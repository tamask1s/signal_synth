# Episode Timeline PSVT/SVARR Pack

**Document ID:** SYN-ARCH-INC-013

**Version:** 0.2

**Status:** Verified

**Owner role:** Biosignal Algorithms / Verification

**Date:** 2026-07-02

**Proposed traceability ID:** `TRC-ECG12-009`

**Implementation issue:** [signal_synth#25](https://github.com/tamask1s/signal_synth/issues/25)

**Implementation commit:** `32a55d5e4a97fb1131a05632b4c46e1207376857`

**Verified CI run:** [Verification 28596859164](https://github.com/tamask1s/signal_synth/actions/runs/28596859164)

## 1. Decision

Implement the next ECG clean-reference increment as an episode timeline layer
for:

- `PSVT`: paroxysmal supraventricular tachycardia;
- `SVARR`: a broad supraventricular-arrhythmia stress phenotype.

`PSVT` shall be rendered as sinus baseline before and after an exact interval
of narrow-complex SVT. `SVARR` shall not become an undefined catch-all
waveform. In this increment, a direct `SVARR` request compiles to a documented
canonical supraventricular episode stress case and infers `PSVT` when the
episode subtype is the same narrow-complex tachy run.

`ABQRS` remains a derived broad form statement. It is not a directly
generatable waveform.

## 2. Product rationale

The product is a synthetic ECG/PPG ground-truth testbench. The next useful
capability is therefore not another stationary morphology flag, but a
reproducible interval with exact start/end ground truth. This gives algorithm
users a controlled case for:

- rhythm-transition detection;
- tachy episode onset/offset timing;
- HR/HRV behavior across mixed rhythm records;
- downstream interval masks in reports and exports.

This also establishes the annotation pattern that later acquisition artifacts
will reuse.

## 3. Scenario contract

Extend `ecg_qa_scenario` with optional episode controls:

- `episode_start_seconds`, default `2.0`;
- `episode_duration_seconds`, default `4.0`;
- `episode_rate_bpm`, default `170.0`;
- `episode_type`, initially `none`, `psvt`, or `svarr`.

Condition compilation rules:

- requesting `PSVT` enables a `psvt` episode when no explicit type was set;
- requesting `SVARR` enables a `svarr` episode when no explicit type was set;
- an explicit episode type must match the requested episode condition family;
- the episode start and duration must produce at least two episode beats inside
  the rendered sample window;
- episode rate must be above the sinus baseline and above 100 bpm.

The scenario fingerprint and canonical JSON include all episode fields.
Unknown, unused, or contradictory episode fields fail validation.

## 4. Clinical timeline model

Add record-level episode annotations with:

- episode kind;
- owning condition;
- start and end time;
- first and last beat indices;
- start and end sample indices.

The PSVT episode timeline keeps the existing multi-source wave renderer. It
changes only beat timing and per-beat rhythm labels:

- baseline beats are sinus with visible P waves;
- episode beats use `clinical_rhythm_supraventricular_tachycardia`;
- episode beats are conducted, narrow QRS, and have no visible P wave;
- transition is abrupt at beat boundaries, with no sample-level signal step.

The first implementation does not add gradual warm-up/cool-down or mixed
atrial mechanisms.

## 5. Phenotype assertions

Add assertion coverage for:

- exact episode coverage count;
- baseline sinus beats outside the interval;
- episode SVT beats inside the interval;
- episode heart rate;
- narrow QRS duration;
- P-wave suppression during the episode.

Assertions use beat annotations and sampled timeline data, not the requested
condition label alone.

## 6. Export and DataBrowser integration

The portable export writes episode intervals into `annotations.json`. The
DataBrowser adapter adds episode visualization to the existing
`annotation_output` switch:

- `1`: markers for episode intervals;
- `2`: an extra episode-mask channel;
- `3`: no episode visualization.

The script `074_ECG_Episode_Rhythm_Phenotypes.txt` shows PSVT and SVARR with
annotation channels and keeps function calls on one line.

## 7. Compatibility and limitations

This increment intentionally rejects composition with:

- AFIB, AFLT, SVTAC, PACE;
- AV block and LPR;
- PAC/PVC/PRC/BIGU/TRIGU;
- clean conduction packs;
- infarction, hypertrophy, ischemia/ST-T morphology packs.

Those combinations need separate composition contracts.

`SVARR` remains a broad engineering stress statement. It is not a diagnostic
claim and does not imply clinical coverage of all supraventricular arrhythmias.

## 8. Verification

Create `TEST-ECG-EPISODE-001` covering:

- PSVT and SVARR support levels;
- typed scenario validation and invalid parameter rejection;
- episode interval exactness and deterministic reproducibility;
- baseline and episode rhythm fractions;
- episode HR and narrow-QRS assertions;
- P-wave visibility inside and outside the episode;
- scenario fingerprint changes for episode start, duration, rate, and type;
- JSON parse/write round trip with canonical episode fields;
- export `annotations.json` episode intervals;
- DataBrowser script presence.

Extend catalog-wide scenario support tests so `PSVT` and `SVARR` are no longer
treated as unsupported, while `ABQRS` remains catalog-only.

Verified on 2026-07-02 with:

- release build and local CTest: 15/15 passed;
- sanitizer build and local CTest: 14/14 passed with
  `ASAN_OPTIONS=detect_leaks=0` and `TEST-BUILD-001` excluded because
  LeakSanitizer cannot run under this environment's ptrace constraints;
- GitHub Actions `Verification` run `28596859164`: Ubuntu C++11 and Windows
  C++11 jobs passed;
- DataBrowser/SVN working-copy synchronization checked by byte-compare for the
  copied portable source files, `SignalProc_RSPT.cpp`, and script
  `074_ECG_Episode_Rhythm_Phenotypes.txt`.

## 9. Exit criteria

1. `PSVT` and `SVARR` generate deterministic clean ECG records with exact
   episode ground truth.
2. Every new phenotype assertion passes locally at supported sample rates.
3. Portable JSON and export contracts include the episode fields.
4. The DataBrowser API and script visualize episode intervals.
5. Release, sanitizer, package-smoke, Ubuntu, and Windows verification pass.
6. Git and SVN working copies are synchronized and recorded.

## 10. Change log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-07-02 | Proposed episode-timeline pack for PSVT and SVARR |
| 0.2 | 2026-07-02 | Verified implementation, CI, tests, and SVN synchronization |
