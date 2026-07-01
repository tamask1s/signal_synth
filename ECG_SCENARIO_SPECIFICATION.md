# ECG QA scenario specification

## Purpose

`ecg_scenario` is the stable product-facing contract above the lower-level
`clinical_ecg` waveform configuration. It represents reproducible ECG
algorithm-QA scenarios, not diagnoses or clinical claims.

The typed ECG scenario engine uses schema version 2 for the complete
71-statement PTB-XL SCP-ECG catalog and explicit Q-wave territory parameter.
The portable document contract separately supports document schema v1
(ECG-only) and v2 (explicit linked PPG configuration).
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
`STACH`, `SARRH`, `SBRAD`, `BIGU`, `TRIGU`, `QWAVE`, `LVOLT`, `HVOLT`,
`LVH`, `RVH`, `SEHYP`, `VCLVH`, `LAO/LAE`, and `RAO/RAE`. A report warning
identifies every accepted parameterized condition. `native_only` policy
rejects them.

The territorial infarction/injury pack additionally parameterizes `IMI`,
`ASMI`, `ILMI`, `AMI`, `ALMI`, `LMI`, `IPLMI`, `IPMI`, `PMI`, `INJAS`,
`INJAL`, `INJIN`, `INJLA`, and `INJIL`.

`PRC(S)` requires a PAC or PVC origin. `BIGU` and `TRIGU` also require one
ectopic origin and set cadence to two and three beats respectively. `2AVB`
requires an explicit Mobitz I or Mobitz II pattern.

An explicitly requested sinus rate above 100 or below 60 bpm requires `STACH`
or `SBRAD` respectively. Flutter ventricular rate is compiled into a matching
atrial rate and integer conduction ratio. RR variability is rejected for
flutter and AV-block timelines until those timelines implement it.

Variable severity is currently implemented only for `LNGQT`, `1AVB`, `LPR`,
`PAC`, `PVC`, `STACH`, `SARRH`, `SBRAD`, `QWAVE`, `LVOLT`, `HVOLT`, `LVH`,
`RVH`, `SEHYP`, `VCLVH`, `LAO/LAE`, and `RAO/RAE`. A non-default severity on
any other condition is rejected so that metadata cannot claim an effect that
was not rendered.

The same variable-severity contract applies to all 14 territorial
infarction/injury conditions.

`QWAVE` requires an explicit inferior, anterior, or lateral territory. Its
septal source orientation and duration are changed before normal 12-lead
projection; leads are never painted directly. `LVOLT` and `HVOLT` scale only
the septal, main ventricular, and terminal QRS sources. Their assertions use
measured lead voltages. These are canonical engineering phenotypes and not
diagnostic interpretations.

The hypertrophy QA pack applies source-specific gain/orientation changes or
atrial timing/amplitude changes. It verifies the final sampled leads using a
left-ventricular voltage index, V1 R/S ratio, septal QRS voltage, measured P
duration, or inferior P amplitude. Only one hypertrophy phenotype is accepted
per scenario in this first pack, and it does not compose with the other clean
morphology phenotypes or complex rhythm/conduction forms. The metrics and
limits are versioned engineering QA rules, not clinical diagnostic criteria.

The infarction/injury pack uses explicit inferior, septal, anterior, lateral,
and combined lead masks. Non-posterior infarction conditions use measured
territorial Q evidence. `PMI`, `IPMI`, and `IPLMI` use an explicitly named
reciprocal anterior R-wave proxy because the output has no V7-V9 posterior
leads. Injury conditions use measured territorial ST-J depression. These
forms are parameterized engineering stress phenotypes and do not establish
myocardial infarction or acute coronary syndrome.

## Normalization and validation

Conditions are stored in canonical code order. The report contains both
explicit and inferred conditions. Current implications include:

- PAC or PVC implies `PRC(S)`;
- sinus rate/arrhythmia and AV-block statements imply `SR`;
- `NORM` implies `SR`;
- complete LBBB or RBBB implies `ABQRS`.
- Q-wave and low/high-voltage morphology implies `ABQRS`.
- ventricular or septal hypertrophy morphology implies `ABQRS`.
- infarction morphology implies `ABQRS`;
- non-posterior and combined Q-wave infarction morphology implies `QWAVE`.

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

The portable schema-v1 JSON document adds a separate SHA-256 fingerprint over
canonical UTF-8 JSON. This identifies the exact document including metadata.
It does not replace the normalized 64-bit generation fingerprint and is not a
digital signature. Unknown, duplicate, malformed, or unsupported JSON content
is rejected transactionally.

Portable document schema v2 appends an explicit PPG object. Schema-v1
canonical bytes remain unchanged and cannot silently carry PPG settings.
PPG fields participate in the document SHA-256 while the existing 64-bit ECG
generation fingerprint remains an ECG-engine identity.

## Phenotype assertions

Scenario engine version 5 evaluates requested supported conditions against the
generated record. Assertions use beat and atrial-event annotations, exact
fiducials, multi-source VCG data, and measured 12-lead morphology rather than
treating the requested label as proof that a phenotype was rendered.

The assertion set covers rhythm class, mean heart rate, RR variability, P-wave
presence, atrial-to-ventricular ratio, ectopic origin/cadence/coupling, PR
interval, dropped atrial events, Mobitz pattern, ventricular escape, QRS width,
bundle-branch terminal-source polarity, QTc, pacing evidence, territorial Q
amplitude/duration/lead count, low/high QRS voltage, ventricular/septal voltage
phenotypes, V1 R/S ratio, P duration, and inferior P amplitude. Each report
entry contains the owning condition, assertion code, measured value, accepted
range, unit, label, and pass/fail status.

The assertion set also covers territorial infarction Q evidence, posterior
reciprocal R-wave proxy evidence, and territorial injury ST-J evidence.

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
