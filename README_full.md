# signal_synth

[![Verification](https://github.com/tamask1s/signal_synth/actions/workflows/verification.yml/badge.svg)](https://github.com/tamask1s/signal_synth/actions/workflows/verification.yml)

`signal_synth` is the C++ core and local verification toolkit behind Synsigra: a deterministic synthetic biosignal testbench for ECG, PPG, HRV, signal-quality, rhythm, beat-classification, wearable, and acquisition-fault algorithm QA.

It is intended for engineering verification workflows where test inputs, annotations, scoring policy, and reports must be reproducible. It is not a medical diagnostic device, patient monitor, clinical validation service, or declaration of regulatory compliance.

## What this repository provides

- A C++11 static library with deterministic biosignal generation and scoring primitives.
- The `signal-synth` CLI for scenario validation, fingerprinting, rendering, export, authoring metadata, pack analysis, and comparison workflows.
- A product-facing scenario JSON contract with strict schema parsing, canonical serialization, SHA-256 document identity, and validation diagnostics.
- ECG generation from a phase-domain model and a structured clinical ECG engine with rhythm, conduction, fiducial, morphology, and 12-lead projection support.
- PPG generation linked to the ECG ventricular timeline, with pulse transit timing, morphology, perfusion, weak/missing pulse, modulation, and ground-truth support.
- Challenge-package assembly with manifest/provenance, scoring contracts, package integrity checks, case summaries, reports, and common waveform export formats.
- A Python `synsigra` package with one-command local verification through `synsigra-verify`.
- SaaS-safe scenario authoring metadata and templates, so UI code can render forms without duplicating C++ enums, ranges, or target rules.

## Current product boundary

Use this project to generate deterministic engineering fixtures, curated stress packs, ground-truth annotations, and repeatable local scoring reports.

Do not use it as:

- clinical validation evidence by itself;
- a diagnostic algorithm;
- a patient-data system;
- a replacement for real-world clinical, wearable, safety, or regulatory validation;
- proof of MDR/FDA/medical-device compliance.

The model is a deterministic engineering phantom. It deliberately preserves construction truth and measured annotations; it is not fitted to a patient population and does not claim clinical realism sufficient for diagnostic validation.

## Repository layout

```text
.
├── apps/signal_synth_cli/          # signal-synth command-line frontend
├── src/                            # C++ library headers and implementations
├── examples/
│   ├── scenarios/                  # scenario JSON examples
│   ├── packs/                      # curated example pack manifests
│   ├── detections/                 # example user-output files
│   ├── hrv/                        # HRV examples
│   └── python/                     # Python example scripts
├── python/synsigra/                # local package loader and verifier SDK
├── teszt/                          # CTest behavioral and smoke tests
├── doc/synsigra_architecture_docs/ # architecture, traceability, roadmap docs
├── SCENARIO_AUTHORING.md           # SaaS/UI scenario-authoring contract
├── PRODUCT_DIRECTION.md            # product boundary and release gates
├── MODEL_SPECIFICATION.md          # base ECG model notes
├── CLINICAL_ECG_SPECIFICATION.md   # clinical ECG engine notes
├── ECG_SCENARIO_SPECIFICATION.md   # scenario contract notes
├── PPG_MODEL_SPECIFICATION.md      # PPG model notes
├── LEGAL_PROVENANCE.md             # provenance and release constraints
└── DATA_LICENSES.md                # data and dependency policy
```

## Quick start: build and test the C++ library

```sh
cmake -S . -B build -DSIGNAL_SYNTH_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Library-only build:

```sh
cmake -S . -B build -DSIGNAL_SYNTH_BUILD_TESTS=OFF -DSIGNAL_SYNTH_BUILD_CLI=OFF
cmake --build build
```

Installable CMake package:

```sh
cmake -S . -B build -DSIGNAL_SYNTH_BUILD_TESTS=OFF
cmake --build build
cmake --install build --prefix /tmp/signal_synth-prefix
```

Consumer project:

```cmake
find_package(signal_synth CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE signal_synth::signal_synth)
```

## Quick start: render a scenario

Validate, fingerprint, and render a scenario:

```sh
./build/signal-synth validate examples/scenarios/ecg_clean.json
./build/signal-synth fingerprint examples/scenarios/ecg_clean.json
./build/signal-synth render examples/scenarios/ecg_clean.json --out /tmp/ecg_clean_export
```

The render command creates deterministic engineering artifacts such as:

- canonical scenario and metadata;
- waveform files;
- annotations and ground-truth metrics;
- warnings and validation reports;
- HTML reports;
- WFDB, EDF+, and BDF+ exports where applicable.

Compact schema-v3/v4 scenarios retain WFDB while omitting redundant large waveform formats.

## Scenario authoring for UI/SaaS clients

The C++ layer remains authoritative for scenario controls, ranges, enum values, scoring prerequisites, and cross-field validation rules. UI clients should consume the generated authoring contract instead of maintaining a parallel parameter catalog.

```sh
./build/signal-synth authoring schema > authoring_schema.json
./build/signal-synth authoring templates > scenario_templates.json
./build/signal-synth pack analyze examples/packs/ecg_rhythm_v1.json > pack_analysis.json
```

`authoring schema` describes form groups, field definitions, defaults, units, ranges, enum options, conditional visibility rules, target scoring support, prerequisites, and validation rules.

`authoring templates` returns complete valid scenario documents with editable paths.

`pack analyze` returns per-case target compatibility, local-scoring versus reference-only status, duration, sample rate, sample count, channel count, size estimates, peak-memory estimates, errors, and warnings.

See `SCENARIO_AUTHORING.md` for the integration contract.

## Curated example packs

The repository includes curated pack manifests under `examples/packs/`. These are intended both as generator regression inputs and as SaaS-ingestable product-pack seeds.

| Pack | Main target(s) | Purpose |
|---|---|---|
| `r_peak_stress_v1` | `r_peak`, `signal_quality` | R-peak smoke, rate, and artifact stress testing. |
| `hrv_v1` | `hrv`, `signal_quality` | Five-minute HRV QA for time-domain, Poincare, spectral, exclusion-policy, and scoring workflows. |
| `ecg_rhythm_v1` | rhythm / ECG analysis | Rhythm and conduction scenario coverage. |
| `ecg_beat_classification_v1` | beat classification | Beat-label and class-scoring fixtures. |
| `ecg_morphology_stress_v1` | morphology / ECG analysis | Morphology and stress scenarios. |
| `signal_quality_v1` | `signal_quality` | Quality masks, artifact intervals, and reference-only versus scoreable cases. |
| `ppg_alignment_v1` | `ppg_systolic_peak`, `ecg_ppg_alignment`, `signal_quality` | Linked ECG/PPG pulse timing and peak-detector QA. |
| `wearable_stress_v1` | `r_peak`, `ppg_systolic_peak`, `ecg_ppg_alignment`, `hrv` | Wearable ECG/PPG cardiorespiratory, synchronization, and long-duration stress. |
| `combined_worst_case_v1` | multi-target | Combined stress cases for broader regression coverage. |

Always run `signal-synth pack analyze` before exposing a pack in the SaaS UI. The analysis output is the canonical way to distinguish scoreable targets from reference-only ground-truth artifacts.

## Local verification with the Python SDK

Install the local verifier from this repository:

```sh
python -m pip install ./python
```

Verify a downloaded challenge package against user detector outputs:

```sh
synsigra-verify challenge.synsigra detections/ verification_out/
```

The default threshold profile is `regression`. Built-in profiles are:

- `smoke`
- `regression`
- `stress`
- `benchmark`

Custom policies can be supplied as versioned JSON:

```sh
synsigra-verify challenge.synsigra detections/ verification_out/ \
  --profile path/to/profile.json
```

The verifier uses only the downloaded challenge package and the user's detection outputs. It does not require the generator source tree, does not run proprietary detector code, and is suitable for local CI gating. Failed quality gates return a non-zero process exit code and write machine-readable results.

## Detection-output expectations

Challenge packages define their accepted user-output schemas through scoring metadata. Typical detector outputs are one file per case, named by case ID.

Minimal R-peak CSV:

```csv
time_seconds
0.82
1.68
2.54
```

Common optional columns include:

- `sample_index`
- `channel`
- `label`
- `confidence`

Target-specific contracts may differ for PPG peaks, HRV metrics, beat classification, or signal-quality masks. Use the package scoring manifest and generated case summaries as the source of truth.

## Direct CLI scoring and comparison

The C++ CLI can compare detector outputs directly when the relevant scenario and detection files are available.

Example R-peak comparison:

```sh
./build/signal-synth compare r_peak \
  challenge/cases/clean_70/scenario.json \
  detections/clean_70.csv \
  --out verification-clean-70
```

Other target-specific comparison commands depend on the selected scenario and pack, for example PPG peak, beat-classification, and HRV scoring flows.

## C++ API surface

Important public headers include:

- `signal_synth.h` — legacy generator API.
- `ecg_model.h` — phase-domain ECG model.
- `clinical_ecg.h` — structured clinical timeline and 12-lead ECG model.
- `ecg_scenario.h` / `ecg_scenario_json.h` — product-facing scenario contract and JSON identity.
- `ecg_export.h`, `ecg_wfdb_export.h`, `ecg_edf_bdf_export.h` — export layers.
- `hrv_metrics.h`, `hrv_scoring.h` — HRV metrics and scoring support.
- `ecg_beat_classification.h` — beat-classification labels and scoring helpers.
- `ppg_model.h` — ECG-linked PPG model.
- `signal_quality.h` — artifact and quality-label support.
- `challenge_package.h`, `challenge_assembly.h` — challenge-package assembly and integrity contract.
- `scenario_authoring.h` — UI/SaaS authoring metadata, templates, and pack analysis.

## Model capabilities

### ECG

The ECG stack includes:

- deterministic, chunk-invariant streaming;
- seeded LF/HF oscillator-bank RR processes;
- deterministic and probabilistic premature-beat scenarios;
- compensatory pauses and configurable premature-beat morphology;
- P/Q/R/S/T phase-domain morphology;
- baseline respiration coupling;
- exact continuous event times and sampled indices;
- measured fiducials with sub-sample interpolation;
- 12-lead clinical ECG projection;
- rhythm and conduction scenario support;
- morphology metrics and phenotype assertions.

### Clinical ECG

The clinical layer includes separate atrial and ventricular timelines, stable cross-references, sinus rhythm, AF, flutter, SVT, VT, paced rhythm, PAC/PVC scenarios, AV block cases, bundle branch morphology branches, QT correction formulas, global construction fiducials, measured P/Q/R/S/T peaks, and lead-region morphology metrics.

Unsupported or incompatible scenario combinations are rejected explicitly instead of producing mislabeled signals.

### PPG and wearable scenarios

The PPG layer is linked to the ECG ventricular timeline and supports variable pulse transit timing, pulse morphology, perfusion stress, weak or missing pulses, ECG/PPG alignment scenarios, artifact intervals, and signal-quality annotations. Current open work continues to refine motion artifacts, accelerometer-style references, and richer benchmark scoring.

## CI and verification evidence

GitHub Actions runs behavioral `TEST-*` suites and installed-package smoke checks on Linux. The repository includes architecture and traceability documentation under `doc/synsigra_architecture_docs/`, including a traceability SOP and demonstration matrix.

This evidence is for engineering verification. It is not a declaration of MDR compliance, clinical safety, or diagnostic validity.

## Relationship to `signal_synth_saas`

`signal_synth` is the authoritative generator, package, authoring, and local-verification core. `signal_synth_saas` should consume:

- curated pack manifests from `examples/packs/`;
- scenario templates and UI form metadata from `signal-synth authoring ...`;
- pack compatibility and size estimates from `signal-synth pack analyze ...`;
- generated challenge packages and scoring metadata;
- the Python `synsigra` verifier for local customer verification workflows.

The SaaS should not duplicate scenario catalogs, parameter ranges, scoring support rules, or target compatibility logic.

## Documentation map

- `PRODUCT_DIRECTION.md` — product positioning, architecture sequence, and release gates.
- `SCENARIO_AUTHORING.md` — authoring schema/templates and pack-analysis contract.
- `MODEL_SPECIFICATION.md` — base ECG model specification.
- `CLINICAL_ECG_SPECIFICATION.md` — clinical ECG model specification.
- `ECG_SCENARIO_SPECIFICATION.md` — scenario JSON and condition contract.
- `PPG_MODEL_SPECIFICATION.md` — PPG model specification.
- `LEGAL_PROVENANCE.md` — legal/provenance constraints.
- `DATA_LICENSES.md` — dataset and dependency licensing policy.
- `doc/synsigra_architecture_docs/` — architecture, traceability, verification, SaaS, and roadmap documents.
- `python/README.md` — Python local verification SDK notes.

## Development policy

Before adding model code, data-derived parameters, datasets, learned residuals, third-party tools, or release artifacts, review `LEGAL_PROVENANCE.md` and `DATA_LICENSES.md`.

Every release-facing scenario family should have:

- deterministic unit tests;
- measurable phenotype assertions;
- conflict/validation tests;
- stable schema and fingerprint tests;
- regression packs;
- provenance and license review;
- downstream algorithm-utility evidence where claims are made.

## License

See the repository license and the provenance/data-license documents before redistribution or commercial use.
