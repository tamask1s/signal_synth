# signal_synth

[![Verification](https://github.com/tamask1s/signal_synth/actions/workflows/verification.yml/badge.svg)](https://github.com/tamask1s/signal_synth/actions/workflows/verification.yml)

`signal_synth` is a C++11 signal-generation library under active development.
The existing `signal_synth` API provides the legacy generators. `ecg_model`
provides the stateful phase-domain ECG and five-channel validation package.
`clinical_ecg` adds a structured clinical timeline, seven-source 3D cardiac
vector model, 12-lead projection, and construction/measured fiducials. `ecg_scenario`
provides the versioned product-facing QA scenario contract, complete PTB-XL
condition catalog, strict validation, reproducibility fingerprint, and audit
report. `ecg_morphology` measures deterministic beat-by-lead morphology from
the generated 12-lead signal. `ecg_scenario_json` provides strict schema-v1
JSON parsing, canonical serialization, and SHA-256 document identity.

## ECG model status

The current model provides:

- deterministic, chunk-invariant streaming;
- a seeded LF/HF oscillator-bank RR process with configurable SDNN;
- deterministic periodic or probabilistic premature-beat scenarios;
- compensatory pauses and configurable premature-beat morphology;
- configurable P, Q, R, S, and T phase-domain morphology;
- fourth-order Runge-Kutta integration;
- baseline respiration coupling;
- exact continuous event time plus the first sampled index at or after it;
- measured discrete extrema with parabolic sub-sample interpolation;
- deterministic 5% onset/offset boundaries for measured P and T waves;
- a ready-to-display five-channel validation package;
- copyable and resettable per-instance state.

An `ecg_model_annotation` marks the configured P/Q/R/S/T model event.
`measure_ecg_fiducials` separately measures the strongest local extremum in
each event window and reports both its exact sample and a parabolic sub-sample
estimate. Model events and measured fiducials therefore remain distinguishable.

## Clinical ECG status

The `clinical_ecg` API provides:

- separate atrial and ventricular timelines with stable cross-references;
- sinus rhythm, AF, flutter, SVT, VT, paced rhythm, PAC and PVC scenarios;
- first-degree, Mobitz I, Mobitz II and complete AV block;
- LBBB and RBBB morphology branches;
- fixed, Bazett, Fridericia, Framingham and Hodges QT correction;
- deterministic RR variability and periodic premature/pause scenarios;
- seven independently configurable 3D source components and their summed VCG;
- a 3D VCG projected to the standard 12 lead names;
- exact Einthoven and Goldberger identities at default lead gains;
- global construction fiducials and measured P/Q/R/S/T peaks per lead;
- condition-specific rhythm/conduction phenotype assertions with measured
  values and pass/fail ranges;
- beat-by-lead P/QRS/ST/T morphology metrics and standard lead-region groups;
- parameterized inferior/anterior/lateral Q-wave, low-voltage, and
  high-voltage QA phenotypes;
- copyable records with 12 leads, 24 source/VCG channels, and structured
  annotations.

The `ecg_scenario` API separates catalog coverage from waveform support. It
rejects unsupported or incompatible conditions rather than silently producing
a mislabeled signal. See `ECG_SCENARIO_SPECIFICATION.md`.

The clinical engine is a deterministic engineering phantom. Its morphology is
not fitted to a patient population, and the precordial projection is a compact
lead-field approximation rather than a torso volume-conductor simulation.
Consequently, it is suitable for software validation inputs but is not itself
clinical validation evidence.

## Build and test

```sh
cmake -H. -B/tmp/signal_synth-build -DSIGNAL_SYNTH_BUILD_TESTS=ON
cmake --build /tmp/signal_synth-build
cd /tmp/signal_synth-build
ctest --output-on-failure
```

Installable consumers can use the exported CMake package:

```sh
cmake -H. -B/tmp/signal_synth-build -DSIGNAL_SYNTH_BUILD_TESTS=OFF
cmake --build /tmp/signal_synth-build
cmake -DCMAKE_INSTALL_PREFIX=/tmp/signal_synth-prefix -P /tmp/signal_synth-build/cmake_install.cmake
```

```cmake
find_package(signal_synth CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE signal_synth::signal_synth)
```

Validate and fingerprint a portable scenario document:

```sh
/tmp/signal_synth-build/signal-synth validate examples/scenarios/ecg_clean.json
/tmp/signal_synth-build/signal-synth fingerprint examples/scenarios/ecg_clean.json
```

Use `-DSIGNAL_SYNTH_BUILD_CLI=OFF` for a library-only build.

GitHub Actions runs the five behavioral `TEST-*` suites and the installed
package smoke suite on Linux and Windows, and preserves the CTest logs as
finite-retention artifacts. See the
[traceability SOP](doc/synsigra_architecture_docs/17_TRACEABILITY_SOP.md) and
[demonstration matrix](doc/synsigra_architecture_docs/18_TRACEABILITY_MATRIX.md).
This is engineering verification evidence, not clinical validation or a claim
of MDR compliance.

See `MODEL_SPECIFICATION.md`, `CLINICAL_ECG_SPECIFICATION.md`,
`ECG_SCENARIO_SPECIFICATION.md`, and `PRODUCT_DIRECTION.md`. Review
`LEGAL_PROVENANCE.md` and `DATA_LICENSES.md` before adding model code,
dependencies, datasets, or release artifacts.
