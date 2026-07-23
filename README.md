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
the generated 12-lead signal. `ecg_scenario_json` provides strict schema-v2
through schema-v9 JSON parsing, canonical serialization, and SHA-256 document
identity. `ppg_model` generates a linked optical pulse channel with variable
PTT, morphology, modulation, perfusion, weak-pulse, and missing-pulse ground
truth from the exact ECG ventricular timeline. PPG QA includes deterministic
motion/sensor artifacts, an accelerometer reference, peak/onset event scoring,
and pulse-interval/rate metrics. Generic interval scoring supports rhythm
episodes and global or channel-specific signal-quality outputs with time
coverage, IoU, boundary error, false-alarm, and confusion metrics.
Lead-specific ECG delineation scoring covers P onset/peak/offset, QRS onset
and offset, J point, and T onset/peak/offset with temporal event matching,
truth-side atrial/ventricular anchors, and explicit present, absent, and
not-evaluable states.
Generic measurement scoring uses one JSON/CSV contract for beat, lead, record,
and paired-signal outputs, including ECG intervals, ST/T morphology, frontal
axes, phenotype assertions, observable RR, formula-explicit QT/QTc, PTT, and
ECG-to-PPG peak delay.
`cardiorespiratory` derives explicit PRV from final measured PPG peaks, exports
HRV-versus-PRV agreement and pulse-quality exclusions, and exposes a shared
seeded respiratory reference coupled independently to RR, ECG baseline, PPG
amplitude/timing, and accelerometer signals.
`wearable_timebase` samples the final latent ECG, PPG, and accelerometer signals
through independent device rates and clocks, with deterministic timestamp
jitter, packet loss, exact latent mappings, and physiological-versus-device
alignment truth.

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
/tmp/signal_synth-build/signal-synth render examples/scenarios/ecg_clean.json --out /tmp/ecg_clean_export
/tmp/signal_synth-build/signal-synth interval score rhythm_episode examples/scenarios/catalog/rhythm_psvt_episode.json intervals.json --out /tmp/episode_score
/tmp/signal_synth-build/signal-synth delineation score examples/scenarios/ecg_clean.json delineations.json --out /tmp/delineation_score
/tmp/signal_synth-build/signal-synth measurement score morphology_assertions examples/scenarios/catalog/morph_clbbb.json measurements.json --out /tmp/measurement_score
```

Discover the SaaS-safe form contract, scenario templates, and pack estimates:

```sh
/tmp/signal_synth-build/signal-synth contract
/tmp/signal_synth-build/signal-synth authoring schema
/tmp/signal_synth-build/signal-synth authoring templates
/tmp/signal_synth-build/signal-synth pack analyze examples/packs/ecg_rhythm_v1.json
/tmp/signal_synth-build/signal-synth pack challenge examples/packs/r_peak_rr_noise_v1.json --out /tmp/r_peak_rr_noise --noise-assets examples/assets/noise
/tmp/signal_synth-build/signal-synth pack challenge examples/packs/ecg_qtc_verification_v1.json --out /tmp/ecg_qtc
```

`signal-synth contract` is the machine-readable worker preflight contract.
See `SCENARIO_AUTHORING.md` for the scenario-authoring contract.

Use `-DSIGNAL_SYNTH_BUILD_CLI=OFF` for a library-only build.

The render command creates deterministic scenario, metadata, waveform,
annotation, metric, warning, provenance, claim-boundary, HTML, WFDB, EDF+,
and BDF+ artifacts. Compact schema-v3/v4/v5 scenarios retain WFDB while omitting
redundant large waveform formats. Reports and exports are deterministic
synthetic engineering verification evidence, not clinical validation.

Release and package provenance is documented in `RELEASE_NOTES.md` and
`PROVENANCE_BUNDLE.md`. Generated packages include `provenance.json` and
`ENGINEERING_CLAIM_BOUNDARY.txt` so archived challenge bundles retain
generator version, git commit when available, build identity, package
contract version, verifier version, fingerprint and engineering QA claim
boundary.

GitHub Actions runs the behavioral `TEST-*` suites and installed package smoke
suite on Linux, and preserves the CTest logs as finite-retention artifacts.
See the
[traceability SOP](doc/synsigra_architecture_docs/17_TRACEABILITY_SOP.md) and
[demonstration matrix](doc/synsigra_architecture_docs/18_TRACEABILITY_MATRIX.md).
This is engineering verification evidence, not clinical validation or a claim
of MDR compliance.

See `MODEL_SPECIFICATION.md`, `CLINICAL_ECG_SPECIFICATION.md`,
`ECG_SCENARIO_SPECIFICATION.md`, `PPG_MODEL_SPECIFICATION.md`, and
`PRODUCT_DIRECTION.md`. Review
`LEGAL_PROVENANCE.md` and `DATA_LICENSES.md` before adding model code,
dependencies, datasets, or release artifacts.

<!-- synsigra-python-sdk-distribution -->
## Python SDK and local verifier distribution

The user-facing Python package is `synsigra`. It is intended for algorithm developers and CI systems that need to verify local detector output against a downloaded Synsigra challenge package without building the C++ generator.

Install from a checkout during beta:

```bash
python -m pip install .
```

External beta users install the generator-free wheel supplied with the
release:

```bash
python -m pip install synsigra-0.14.0-py3-none-any.whl
```

Run local verification:

```bash
synsigra-verify package.zip submission/ verification-results/
```

Each challenge supplies `user-output-template/`; the user records algorithm
identity once in `submission.json` and fills the declared output files. The
verifier reads package ground truth and the explicit submission locally. It
does not invoke the C++ generator. It writes one canonical `evidence.json`, an
audit-friendly `index.html`, and one linked detail HTML per case-target
under `details/`. Exit code `0` means pass, `1` means verification
failure, and `2` means invalid CLI usage. Evidence mode is the default and uses
the immutable protocol carried by the package. Case/target filters and the
`smoke`, `regression`, `stress`, or `benchmark` profiles are available only in
explicit `--mode diagnostic` runs, which are always reported as non-evidence.

Protocol-enabled packs also carry `verification_protocol.json`, resolved by
its manifest role. The Python loader rejects malformed manifests, ambiguous or
unsafe archive paths, symlinks, unlisted files, and hash/size mismatches before
scoring.

See `python/README.md` and `doc/PYTHON_DISTRIBUTION.md` for packaging, smoke-test, and release guidance.
