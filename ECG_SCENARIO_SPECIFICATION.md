# ECG QA scenario specification

## Purpose

`ecg_scenario` is the stable product-facing contract above the lower-level
`clinical_ecg` waveform configuration. It represents reproducible ECG
algorithm-QA scenarios, not diagnoses or clinical claims.

Schema version 1 contains the complete 71-statement PTB-XL SCP-ECG catalog.
The catalog order, codes, statement classes, and names are based on PTB-XL
version 1.0.3 `scp_statements.csv`, licensed under CC BY 4.0.

Catalog presence does not imply waveform support. Every condition has one of
three explicit support levels:

- `catalog_only`: known to the API but generation is rejected;
- `parameterized`: a documented canonical phenotype using existing controls;
- `native`: a dedicated timeline or morphology path exists.

This prevents unsupported conditions from silently producing a normal or
mislabelled signal.

## Current support

Native conditions are `NORM`, `1AVB`, `2AVB`, `3AVB`, `PVC`, `PAC`, `SR`,
`AFIB`, `PACE`, `AFLT`, and `SVTAC`.

Parameterized conditions are `LNGQT`, `CRBBB`, `CLBBB`, `LPR`, `PRC(S)`,
`STACH`, `SARRH`, `SBRAD`, `BIGU`, and `TRIGU`. A report warning identifies
every accepted parameterized condition. `native_only` policy rejects them.

`PRC(S)` requires a PAC or PVC origin. `BIGU` and `TRIGU` also require one
ectopic origin and set cadence to two and three beats respectively. `2AVB`
requires an explicit Mobitz I or Mobitz II pattern.

An explicitly requested sinus rate above 100 or below 60 bpm requires `STACH`
or `SBRAD` respectively. Flutter ventricular rate is compiled into a matching
atrial rate and integer conduction ratio. RR variability is rejected for
flutter and AV-block timelines until those timelines implement it.

Variable severity is currently implemented only for `LNGQT`, `1AVB`, `LPR`,
`PAC`, `PVC`, `STACH`, `SARRH`, and `SBRAD`. A non-default severity on any
other condition is rejected so that metadata cannot claim an effect that was
not rendered.

## Normalization and validation

Conditions are stored in canonical code order. The report contains both
explicit and inferred conditions. Current implications include:

- PAC or PVC implies `PRC(S)`;
- sinus rate/arrhythmia and AV-block statements imply `SR`;
- `NORM` implies `SR`;
- complete LBBB or RBBB implies `ABQRS`.

The validator rejects incompatible primary rhythms, conflicting AV-block
degrees, normal-plus-abnormal combinations, unsupported fidelity, ectopy
without an origin, and combinations that the lower timeline cannot compose.
Validation is strict: no condition or scenario parameter is silently ignored.

## Reproducibility and audit

Each scenario has a deterministic 64-bit FNV-1a fingerprint over:

- schema version;
- sample rate and random seed;
- heart rate and RR variability;
- ectopy cadence and Mobitz subtype;
- fidelity policy;
- sorted condition codes and quantized severities.

The scenario fingerprint is a reproducibility and audit identifier, not a
cryptographic signature. The report separately records the scenario engine
version and a run fingerprint over scenario, engine version, and generated
sample count. Generated records remain reproducible for a fixed scenario,
engine version, generator build, compiler, and floating-point environment.

The report includes the fingerprint, normalized conditions, inferred flags,
condition severities, warnings/errors, and generated sample count. Compilation
and generation are transactional: failure preserves the destination config or
signal record.

## Phenotype assertions

Scenario engine version 2 evaluates requested supported conditions against the
generated record. Assertions use beat and atrial-event annotations, exact
fiducials, and multi-source VCG data rather than treating the requested label as
proof that a phenotype was rendered.

The assertion set covers rhythm class, mean heart rate, RR variability, P-wave
presence, atrial-to-ventricular ratio, ectopic origin/cadence/coupling, PR
interval, dropped atrial events, Mobitz pattern, ventricular escape, QRS width,
bundle-branch terminal-source polarity, QTc, and pacing evidence. Each report
entry contains the owning condition, assertion code, measured value, accepted
range, unit, label, and pass/fail status.

`success()` reports validation and waveform-generation success.
`phenotype_passed()` independently reports whether every evaluated phenotype
assertion passed. A generated waveform is retained when an assertion fails,
which allows failed QA cases to be inspected and exported. Assertions are
ground-truth consistency checks for this generator, not clinical diagnostic
criteria or external validation claims.

## Attribution

PTB-XL version 1.0.3:

Wagner, P., Strodthoff, N., Bousseljot, R.-D., Samek, W., and Schaeffter, T.
"PTB-XL, a large publicly available electrocardiography dataset."
PhysioNet, 2022. DOI: 10.13026/kfzx-aw45.
