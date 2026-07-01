# Synsigra Testbench — Software Requirements Specification (SRS)

**Document ID:** SYN-SRS-001  
**Product:** Synsigra Testbench  
**Version:** 0.2-draft
**Status:** Draft for MVP planning  
**Date:** 2026-07-01  
**Owner:** TBD  

---

## 1. Document Control

### 1.1 Purpose of this Document

This Software Requirements Specification defines the initial requirements for **Synsigra Testbench**, a B2B software platform for controlled synthetic biosignal generation and algorithm test evidence generation.

The document is intended to serve as:

- MVP product specification;
- engineering design input;
- basis for repository structure and implementation planning;
- basis for future traceability between requirements, risks, tests, and reports;
- optional starting point for future medical-device-grade documentation if the intended use changes.

### 1.2 Important Regulatory Disclaimer

The initial product is **not intended to be a medical device**, a diagnostic system, a certified patient simulator, or a standalone conformity-assessment tool.

Initial intended use:

> Synsigra Testbench is intended for research, engineering development, algorithm testing, robustness testing, synthetic data generation, and generation of test evidence for ECG/PPG/wearable signal-processing algorithms under controlled synthetic conditions.

Initial non-intended use:

> Synsigra Testbench is not intended for diagnosis, treatment, patient monitoring, patient-specific clinical decision-making, or as a standalone basis for medical-device conformity assessment.

If the product is later positioned as medical-device software, medical-device accessory, certified validation tool, or patient-simulator product, this SRS must be revised under a controlled quality-management process.

### 1.3 Relationship to Future MDR Readiness

This SRS is a useful starting document for a future MDR-oriented development process, but it is **not sufficient by itself** for MDR compliance.

A future MDR path would additionally require at least:

- precise intended purpose;
- medical-device qualification and classification analysis;
- quality-management system;
- risk-management process and risk-management file;
- software development plan;
- software architecture and detailed design;
- software verification and validation plan;
- usability engineering, if applicable;
- cybersecurity and data-integrity documentation;
- post-market surveillance and vigilance processes, if applicable;
- technical documentation according to applicable EU MDR requirements.

For future medical-device software, requirements should be traceable to risks, design, implementation, verification tests, validation evidence, and release records.

---

## 2. Product Overview

### 2.1 Product Name

Working brand: **Synsigra**  
Initial product: **Synsigra Testbench**

Possible modules:

- Synsigra ECG
- Synsigra PPG
- Synsigra Scenario Packs
- Synsigra API
- Synsigra Reports
- Synsigra Analog Kit, later hardware option

### 2.2 Product Positioning

Synsigra Testbench is a synthetic biosignal testbench for ECG, PPG, HRV, wearable, and signal-quality algorithms.

Preferred positioning:

> Synthetic biosignal testbench for ECG, PPG and wearable algorithms.

More detailed positioning:

> A controlled, reproducible ECG/PPG scenario engine with explicit ground truth, annotations, exports, API access, and engineering test reports for algorithm development and robustness testing.

Avoided positioning:

- generic synthetic cardiology data platform;
- ECG data store;
- patient simulator;
- diagnostic simulator;
- clinical validation platform;
- medical-device conformity assessment tool.

### 2.3 Business Goal

The product should support early revenue through:

- paid pilots;
- algorithm stress-test reports;
- SaaS access;
- API access;
- scenario-pack licensing;
- future enterprise/on-prem deployments;
- future USB analog-output hardware kits.

### 2.4 Core Value Proposition

The core value is not simply waveform generation.

The core value is:

- controlled scenario definition;
- deterministic reproducibility;
- explicit ground truth;
- synthetic ECG and PPG generation from a shared beat timeline;
- artifact/noise/stress scenario packs;
- algorithm QA workflow;
- exportable datasets with metadata;
- engineering test reports;
- future regression-testing API;
- future analog-output hardware-in-the-loop testing.

---

## 3. Background and Competitive Context

### 3.1 Existing Solutions

Existing open-source and academic systems already provide parts of the required functionality, including:

- synthetic ECG waveform generation;
- ECGSYN-style HRV/morphology control;
- basic ECG simulation in Python toolkits;
- synthetic PPG generation;
- ECG/PPG academic frameworks with labels;
- synthetic cardiology data platforms focused on privacy-preserving clinical data;
- commercial hardware patient simulators.

Therefore, Synsigra must not compete only as a waveform generator.

### 3.2 Differentiation

Synsigra should differentiate through productization and workflow:

- SaaS UI;
- developer API;
- scenario library;
- regression testing;
- ground-truth evidence reports;
- explicit export metadata;
- traceability;
- enterprise/on-prem option;
- future USB analog output;
- custom test scenarios for customer algorithms.

### 3.3 Strategic Constraint

The fetal ECG line is explicitly out of scope for the initial product.

---

## 4. Stakeholders and Users

### 4.1 Stakeholders

| Stakeholder | Interest |
|---|---|
| Founder / Product Owner | Product direction, MVP, commercial viability |
| Algorithm Developer | Synthetic test signals, ground truth, API |
| QA / Verification Engineer | Reproducible tests, reports, evidence |
| Wearable Company | PPG/ECG/HRV robustness testing |
| Medical-device R&D Team | Engineering test evidence, but not clinical validation |
| Research Lab | Synthetic data generation and controlled experiments |
| Enterprise Customer | On-prem deployment, auditability, licensing |
| Regulatory/Quality Reviewer, future | Traceability, intended-use control, risk linkage |

### 4.2 User Personas

#### Persona A — Algorithm Developer

Needs:

- quick synthetic ECG/PPG signals;
- exact annotations;
- Python/API access;
- deterministic scenarios;
- export to local algorithm pipelines.

#### Persona B — QA Engineer

Needs:

- fixed regression scenarios;
- repeatable test packs;
- pass/fail criteria;
- reports;
- traceability between scenario, output, version, and test result.

#### Persona C — Wearable Product Engineer

Needs:

- PPG motion-artifact scenarios;
- dropout/saturation scenarios;
- ECG/PPG coupling;
- HR/HRV ground truth;
- future analog/hardware output.

#### Persona D — Researcher

Needs:

- controlled synthetic experiments;
- publishable aggregate results;
- reproducible seeds;
- clear limitations.

#### Persona E — Enterprise / Medtech Customer

Needs:

- private deployment;
- license control;
- audit logs;
- documentation package;
- controlled claims;
- data and scenario confidentiality.

---

## 5. Scope

### 5.1 In Scope for MVP

- synthetic ECG generation;
- basic PPG/pulse waveform generation from the same beat timeline;
- deterministic scenario configuration;
- ground-truth annotations;
- basic noise and artifact injection;
- scenario preview;
- CSV and JSON export;
- simple HTML report;
- local API;
- predefined scenario library;
- automated tests for deterministic behavior and known outputs.

### 5.2 Out of Scope for MVP

- fetal ECG;
- clinical diagnosis;
- patient monitoring;
- medical-device certification;
- final medical-device conformity assessment;
- real patient data ingestion;
- user-uploaded private medical datasets;
- full 12-lead ECG;
- certified patient simulator functionality;
- USB analog hardware output;
- on-prem enterprise installer;
- full SaaS billing;
- advanced user management;
- AI model training service.

### 5.3 Future Scope

- WFDB export;
- EDF export;
- team accounts;
- API keys;
- cloud SaaS deployment;
- batch generation;
- CI/regression integration;
- customer algorithm upload or remote runner;
- advanced PPG model;
- accelerometer-correlated artifact channel;
- analog USB output device;
- enterprise/on-prem deployment;
- tool-validation package;
- independent lab measurement for hardware output.

---

## 6. Definitions and Abbreviations

| Term | Meaning |
|---|---|
| ECG | Electrocardiogram |
| PPG | Photoplethysmogram |
| HR | Heart rate |
| HRV | Heart-rate variability |
| RR interval | Time interval between consecutive R peaks |
| R peak | Main positive peak of QRS complex in ECG |
| PPG peak | Pulse waveform peak in PPG |
| Scenario | Full parameter set defining generated signals, artifacts, events, and outputs |
| Ground truth | Known true events/metrics generated by the scenario engine |
| Annotation | Time-indexed event label, such as R peak, PPG peak, artifact interval, beat type |
| Test report | Engineering evidence generated from a scenario or test run |
| Validation | In this MVP context: engineering test evidence only, not clinical validation |
| SaMD | Software as a Medical Device |
| MDR | EU Medical Device Regulation |
| SRS | Software Requirements Specification |

---

## 7. System Overview

### 7.1 High-Level Architecture

The system shall be structured into independent layers:

```text
synsigra/
  core/          signal generation, HRV, ECG, PPG, events, noise, annotations
  schemas/       JSON schema and data models
  exports/       CSV, JSON, later WFDB/EDF
  reports/       HTML/PDF reports
  api/           local/cloud API
  web/           UI
  tests/         automated verification tests
  examples/      predefined scenarios
  docs/          product and technical documentation
```

### 7.2 Core Design Principle

The core signal-generation engine shall be UI-independent and deterministic.

Given the same:

- generator version;
- scenario definition;
- seed;
- sample rate;
- duration;

it shall generate the same outputs within defined numerical tolerance.

---

## 8. Operating Environment

### 8.1 MVP Environment

The MVP shall support local development and local execution.

Suggested stack:

- Python core implementation;
- FastAPI or equivalent local API;
- browser-based UI;
- JSON scenario files;
- CSV/JSON exports;
- pytest or equivalent test framework.

### 8.2 Future Environment

Future deployment may include:

- cloud SaaS;
- containerized on-prem installation;
- API gateway;
- license server;
- team accounts;
- enterprise audit logs;
- USB hardware integration.

---

## 9. General Product Requirements

### REQ-GEN-001 — Deterministic Generation

The system shall generate deterministic output for the same scenario and seed.

Priority: Must  
MVP: Yes

### REQ-GEN-002 — Scenario-Based Operation

The system shall use explicit scenario objects as the primary input to all signal generation.

Priority: Must  
MVP: Yes

### REQ-GEN-003 — Explicit Ground Truth

The system shall generate explicit ground-truth annotations and metrics alongside waveform data.

Priority: Must  
MVP: Yes

### REQ-GEN-004 — UI-Independent Core

The signal-generation core shall be usable without the web UI.

Priority: Must  
MVP: Yes

### REQ-GEN-005 — Versioned Output

Each generated output shall include generator version, scenario ID, scenario hash, and seed.

Priority: Must  
MVP: Yes

### REQ-GEN-006 — Non-Clinical Disclaimer

The UI, reports, and exports shall include an appropriate disclaimer that outputs are synthetic and not clinical validation evidence by themselves.

Priority: Must  
MVP: Yes

---

## 10. Functional Requirements — Scenario Model

### REQ-SCN-001 — Scenario Metadata

The scenario shall include metadata fields:

- scenario ID;
- scenario name;
- description;
- author or owner;
- created timestamp;
- generator version compatibility;
- license or distribution class;
- tags.

Priority: Must  
MVP: Yes

### REQ-SCN-002 — Timing Parameters

The scenario shall define:

- duration in seconds;
- sample rate in Hz;
- start time offset;
- random seed.

Priority: Must  
MVP: Yes

### REQ-SCN-003 — Cardiac Timeline Parameters

The scenario shall define a beat timeline using:

- baseline heart rate;
- HRV modulation parameters;
- optional LF-like component;
- optional HF-like component;
- optional custom RR events.

Priority: Must  
MVP: Yes

### REQ-SCN-004 — Event Schedule

The scenario shall support time-indexed events, including:

- ectopic beat;
- missed beat;
- irregular interval;
- amplitude change;
- artifact interval;
- dropout interval.

Priority: Must  
MVP: Yes

### REQ-SCN-005 — Multiple Signal Channels

The scenario shall support at least:

- one ECG channel;
- one PPG channel.

Priority: Must  
MVP: Yes

### REQ-SCN-006 — Scenario Hash

The system shall calculate a deterministic scenario hash from a canonical representation of the scenario.

Priority: Must  
MVP: Yes

---

## 11. Functional Requirements — ECG Generation

### REQ-ECG-001 — ECG Waveform Output

The system shall generate a synthetic ECG waveform as a sampled time series.

Priority: Must  
MVP: Yes

### REQ-ECG-002 — Configurable Morphology

The system shall allow configuration of ECG morphology parameters, including at minimum:

- R amplitude;
- QRS width or equivalent shape parameter;
- T-wave amplitude or equivalent shape parameter;
- optional P-wave amplitude or equivalent shape parameter.

Priority: Must  
MVP: Yes

### REQ-ECG-003 — Beat-Level Ground Truth

The system shall output R-peak timestamps and sample indices.

Priority: Must  
MVP: Yes

### REQ-ECG-004 — HRV Modulation

The ECG beat timeline shall support frequency and amplitude modulation relevant for HRV testing.

Priority: Must  
MVP: Yes

### REQ-ECG-005 — Noise Injection

The ECG generator shall support at least:

- baseline wander;
- powerline noise;
- EMG-like noise;
- white noise;
- amplitude dropout.

Priority: Must  
MVP: Yes

### REQ-ECG-006 — Arrhythmia-Like Events

The ECG generator shall support at least:

- ectopic beat;
- missed beat;
- irregular RR sequence;
- AF-like irregularity as synthetic non-diagnostic simulation.

Priority: Should  
MVP: Partial

### REQ-ECG-007 — ECG Annotation Export

The ECG output shall include annotations for:

- R peaks;
- beat type;
- artifact intervals;
- dropout intervals.

Priority: Must  
MVP: Yes

---

## 12. Functional Requirements — PPG Generation

### REQ-PPG-001 — PPG Waveform Output

The system shall generate a synthetic PPG waveform as a sampled time series.

Priority: Must  
MVP: Yes

### REQ-PPG-002 — Shared Beat Timeline

The PPG waveform shall be generated from the same cardiac beat timeline as ECG.

Priority: Must  
MVP: Yes

### REQ-PPG-003 — ECG-to-PPG Delay

The system shall support configurable delay between ECG R peak and PPG pulse feature.

Priority: Must  
MVP: Yes

### REQ-PPG-004 — PPG Ground Truth

The system shall output PPG pulse peak timestamps and sample indices.

Priority: Must  
MVP: Yes

### REQ-PPG-005 — PPG Artifact Support

The system shall support at least:

- motion-like artifact;
- amplitude modulation;
- dropout;
- saturation or clipping;
- baseline variation.

Priority: Should  
MVP: Partial

### REQ-PPG-006 — Future Accelerometer Coupling

The system should support future generation of an accelerometer-like artifact reference channel.

Priority: Could  
MVP: No

---

## 13. Functional Requirements — Ground Truth and Metrics

### REQ-GT-001 — Ground Truth Object

Each generation run shall produce a ground-truth object containing:

- R-peak list;
- RR intervals;
- PPG peak list;
- artifact intervals;
- event labels;
- scenario parameters;
- generator version;
- scenario hash.

Priority: Must  
MVP: Yes

### REQ-GT-002 — HRV Reference Metrics

The system shall compute reference HRV metrics from the generated true RR intervals, including at least:

- mean HR;
- mean RR;
- SDNN;
- RMSSD;
- pNN50, if meaningful for the scenario;
- optional LF/HF-like scenario parameters.

Priority: Must  
MVP: Yes

### REQ-GT-003 — Artifact Interval Ground Truth

The system shall explicitly identify intervals where injected artifacts are present.

Priority: Must  
MVP: Yes

### REQ-GT-004 — Metric Reproducibility

Ground-truth metrics shall be reproducible for identical scenario and seed.

Priority: Must  
MVP: Yes

---

## 14. Functional Requirements — Export

### REQ-EXP-001 — CSV Export

The system shall export waveform samples as CSV.

Minimum columns:

- time_s;
- ecg;
- ppg;
- optional noise/artifact columns if enabled.

Priority: Must  
MVP: Yes

### REQ-EXP-002 — JSON Metadata Export

The system shall export metadata as JSON.

Priority: Must  
MVP: Yes

### REQ-EXP-003 — Annotation Export

The system shall export annotations as JSON.

Priority: Must  
MVP: Yes

### REQ-EXP-004 — Scenario Export

The system shall export the scenario definition used for generation.

Priority: Must  
MVP: Yes

### REQ-EXP-005 — License and Watermark Metadata

Exported files shall include metadata fields for:

- customer/license ID, if available;
- scenario hash;
- generator version;
- seed;
- timestamp;
- usage restrictions.

Priority: Should  
MVP: Partial

### REQ-EXP-006 — WFDB Export

The system should support WFDB export in a future release.

Priority: Could  
MVP: No

### REQ-EXP-007 — EDF Export

The system should support EDF export in a future release.

Priority: Could  
MVP: No

---

## 15. Functional Requirements — Reports

### REQ-RPT-001 — HTML Report

The system shall generate a human-readable HTML report for a scenario generation run.

Priority: Must  
MVP: Yes

### REQ-RPT-002 — Report Contents

The report shall include:

- product name and version;
- scenario name and ID;
- scenario hash;
- seed;
- timestamp;
- signal plots or plot references;
- ground-truth HRV metrics;
- event table;
- artifact intervals;
- export file list;
- non-clinical disclaimer.

Priority: Must  
MVP: Yes

### REQ-RPT-003 — Engineering Evidence Wording

The report shall use controlled wording such as:

> Test evidence under specified synthetic ground-truth scenario conditions.

The report shall not state:

- clinically validated;
- certified medical-device validation;
- diagnostic accuracy;
- final conformity assessment.

Priority: Must  
MVP: Yes

### REQ-RPT-004 — PDF Report

The system should support PDF report export in a future release.

Priority: Should  
MVP: No

---

## 16. Functional Requirements — API

### REQ-API-001 — Generate Scenario Endpoint

The system shall provide a local API function or endpoint to generate signal data from a scenario object.

Priority: Must  
MVP: Yes

### REQ-API-002 — Export Endpoint

The system shall provide a local API function or endpoint to export generated data and annotations.

Priority: Must  
MVP: Yes

### REQ-API-003 — Report Endpoint

The system shall provide a local API function or endpoint to generate a report from a generation result.

Priority: Should  
MVP: Partial

### REQ-API-004 — Batch Generation

The system should support batch generation in a future release.

Priority: Should  
MVP: No

### REQ-API-005 — Customer Algorithm Evaluation

The system may support customer algorithm evaluation in a future release.

Possible modes:

- customer uploads detected events;
- customer runs local SDK;
- customer integrates API;
- future secure execution environment.

Priority: Could  
MVP: No

---

## 17. Functional Requirements — Web UI

### REQ-UI-001 — Scenario Builder UI

The system shall provide a simple web UI for editing core scenario parameters.

Priority: Must  
MVP: Yes

### REQ-UI-002 — Signal Preview

The UI shall show plots for:

- ECG;
- PPG;
- RR timeline or tachogram;
- event markers.

Priority: Must  
MVP: Yes

### REQ-UI-003 — Export Controls

The UI shall allow the user to export CSV and JSON outputs.

Priority: Must  
MVP: Yes

### REQ-UI-004 — Scenario Library

The UI shall allow users to load predefined scenarios.

Priority: Should  
MVP: Partial

### REQ-UI-005 — Report View

The UI shall allow users to view the generated report.

Priority: Should  
MVP: Partial

---

## 18. Functional Requirements — Licensing and Usage Control

### REQ-LIC-001 — Export Metadata

The system shall embed usage metadata in generated output files where technically feasible.

Priority: Should  
MVP: Partial

### REQ-LIC-002 — Terms of Use Compatibility

The product shall support terms that prohibit:

- redistribution of raw generated data;
- public dataset release;
- resale of generated data;
- use of generated outputs to train competing synthetic biosignal generators.

Priority: Must  
MVP: Documentation

### REQ-LIC-003 — Allowed Use Statement

The product shall support terms that allow:

- internal R&D;
- algorithm testing;
- algorithm training;
- aggregate performance publication;
- internal validation and verification work.

Priority: Must  
MVP: Documentation

### REQ-LIC-004 — Future License Server

The system may support a future license server and API-key management.

Priority: Could  
MVP: No

---

## 19. Non-Functional Requirements

### REQ-NFR-001 — Reproducibility

Generated outputs shall be reproducible across runs for the same scenario and seed within defined numerical tolerance.

Priority: Must  
MVP: Yes

### REQ-NFR-002 — Testability

Core generation functions shall be covered by automated tests.

Priority: Must  
MVP: Yes

### REQ-NFR-003 — Modularity

Signal-generation logic shall be separated from UI and API layers.

Priority: Must  
MVP: Yes

### REQ-NFR-004 — Performance

The MVP shall generate a 5-minute ECG+PPG scenario at 500 Hz in less than 5 seconds on a typical developer laptop.

Priority: Should  
MVP: Yes

### REQ-NFR-005 — Numerical Transparency

The system shall document the mathematical assumptions behind each generator component.

Priority: Should  
MVP: Partial

### REQ-NFR-006 — Data Privacy

The MVP shall not require patient data. Generated synthetic data shall not be represented as real patient data.

Priority: Must  
MVP: Yes

### REQ-NFR-007 — Security

The MVP local app shall avoid unnecessary external network calls.

Priority: Should  
MVP: Yes

### REQ-NFR-008 — Maintainability

The repository shall use clear module boundaries, typed data models, and automated tests.

Priority: Must  
MVP: Yes

### REQ-NFR-009 — Auditability, Future

Future enterprise versions shall support audit logs for scenario generation, export, and report generation.

Priority: Could  
MVP: No

---

## 20. Proposed Data Model

### 20.1 Scenario Object

Example draft schema:

```json
{
  "schema_version": "0.1",
  "scenario_id": "clean_sinus_001",
  "name": "Clean sinus rhythm",
  "description": "Clean ECG and PPG generated from a shared beat timeline.",
  "tags": ["ecg", "ppg", "clean", "sinus"],
  "seed": 12345,
  "duration_s": 300,
  "sample_rate_hz": 500,
  "cardiac_timeline": {
    "baseline_hr_bpm": 70,
    "hrv": {
      "enabled": true,
      "lf_amplitude_ms": 40,
      "lf_frequency_hz": 0.1,
      "hf_amplitude_ms": 20,
      "hf_frequency_hz": 0.25,
      "random_rr_std_ms": 5
    },
    "events": []
  },
  "ecg": {
    "enabled": true,
    "morphology": {
      "r_amplitude_mv": 1.0,
      "qrs_width_ms": 90,
      "t_amplitude_mv": 0.3,
      "p_amplitude_mv": 0.1
    },
    "noise": {
      "baseline_wander": { "enabled": false },
      "powerline": { "enabled": false },
      "emg": { "enabled": false },
      "white": { "enabled": false }
    }
  },
  "ppg": {
    "enabled": true,
    "pulse_delay_ms": 180,
    "amplitude": 1.0,
    "shape": {
      "rise_time_ms": 120,
      "decay_time_ms": 350,
      "dicrotic_notch_enabled": true
    },
    "noise": {
      "motion": { "enabled": false },
      "dropout": { "enabled": false },
      "saturation": { "enabled": false }
    }
  },
  "exports": {
    "include_csv": true,
    "include_json": true,
    "include_report": true
  }
}
```

### 20.2 Generation Result Object

```json
{
  "result_id": "...",
  "scenario_id": "clean_sinus_001",
  "scenario_hash": "...",
  "generator_version": "0.1.0",
  "seed": 12345,
  "sample_rate_hz": 500,
  "duration_s": 300,
  "signals": {
    "time_s": [],
    "ecg": [],
    "ppg": []
  },
  "ground_truth": {
    "r_peaks": [],
    "ppg_peaks": [],
    "rr_intervals_ms": [],
    "events": [],
    "artifact_intervals": [],
    "hrv_metrics": {}
  },
  "metadata": {
    "created_at": "...",
    "license_id": null,
    "usage_restrictions": "internal-use-only"
  }
}
```

### 20.3 Annotation Object

```json
{
  "type": "r_peak",
  "time_s": 1.234,
  "sample_index": 617,
  "label": "normal",
  "confidence": 1.0,
  "source": "generator_ground_truth"
}
```

---

## 21. Initial Predefined Scenarios

### REQ-PACK-001 — Clean Sinus Rhythm

Clean ECG and PPG with no injected noise or artifact.

### REQ-PACK-002 — HRV LF/HF Modulation

ECG and PPG with controlled LF/HF-like HRV modulation.

### REQ-PACK-003 — Amplitude Modulation

ECG and PPG amplitude modulation over time.

### REQ-PACK-004 — Baseline Wander

ECG with baseline wander.

### REQ-PACK-005 — Powerline Noise

ECG with powerline noise.

### REQ-PACK-006 — EMG-Like Noise

ECG with EMG-like high-frequency noise.

### REQ-PACK-007 — Motion Artifact

PPG and/or ECG with motion-like artifact interval.

### REQ-PACK-008 — Dropout

ECG and/or PPG amplitude dropout interval.

### REQ-PACK-009 — Ectopic Beat

Timeline with one or more ectopic beats.

### REQ-PACK-010 — AF-Like Irregularity

Synthetic irregular RR sequence for non-diagnostic robustness testing.

### REQ-PACK-011 — ECG-to-PPG Delay Shift

Scenario with changing ECG-to-PPG pulse delay.

### REQ-PACK-012 — Combined Worst Case

Multiple artifacts and irregular events combined.

---

## 22. Verification Requirements

### REQ-VER-001 — Deterministic Output Test

Given a fixed scenario and seed, repeated generation shall produce identical outputs within tolerance.

### REQ-VER-002 — Scenario Hash Test

The same canonical scenario shall produce the same hash.

### REQ-VER-003 — R-Peak Consistency Test

Generated R-peak annotations shall match the internal beat timeline.

### REQ-VER-004 — RR Metric Consistency Test

Computed RR intervals shall match consecutive R-peak differences.

### REQ-VER-005 — HRV Metric Known-Answer Test

For a known RR series, HRV metrics shall match independently calculated expected values.

### REQ-VER-006 — PPG Delay Test

PPG pulse peaks shall occur at configured delay from ECG R peaks within tolerance.

### REQ-VER-007 — Noise Interval Test

Injected artifacts shall be present only during configured intervals unless explicitly configured otherwise.

### REQ-VER-008 — Export Roundtrip Test

Exported scenario and metadata shall be sufficient to reproduce the output.

### REQ-VER-009 — Report Content Test

Generated reports shall include scenario hash, seed, generator version, and disclaimer.

### REQ-VER-010 — Non-Clinical Claim Test

Reports and UI copy shall not include prohibited claims such as “clinically validated” or “certified diagnostic validation”.

---

## 23. Acceptance Criteria for MVP

The MVP is acceptable when:

1. A user can load or create at least 10 predefined scenarios.
2. The system generates ECG and PPG signals from a shared cardiac timeline.
3. The system exports CSV waveform data.
4. The system exports JSON metadata and annotations.
5. The system generates R-peak and PPG peak ground truth.
6. The system calculates basic HRV metrics.
7. The system generates a basic HTML report.
8. Re-running the same scenario and seed reproduces the same result.
9. Automated tests cover deterministic generation and core ground-truth consistency.
10. Reports and UI avoid medical-device validation claims.

---

## 24. Initial Implementation Plan

### Phase 1 — Core Library

Implement:

- scenario data model;
- deterministic seed handling;
- beat timeline generator;
- ECG waveform generator;
- basic PPG waveform generator;
- annotation model;
- HRV metric calculation;
- CSV/JSON export;
- unit tests.

### Phase 2 — Scenario Packs and Reports

Implement:

- predefined scenarios;
- report model;
- HTML report;
- signal plot generation;
- report disclaimers;
- export bundle.

### Phase 3 — Local API and Web UI

Implement:

- local API;
- scenario editor UI;
- preview plots;
- export buttons;
- report view.

### Phase 4 — Early Pilot Support

Implement:

- customer-specific scenarios;
- repeatable test packs;
- improved documentation;
- paid pilot workflow;
- controlled license text.

---

## 25. Future Hardware Requirements Placeholder

Future Synsigra Analog Kit may provide USB-controlled analog signal output.

Initial hardware constraints:

- USB-powered;
- no radio;
- no battery;
- no mains adapter;
- one analog ECG-like output first;
- later PPG-like output if useful;
- engineering/R&D signal-source positioning;
- not certified patient simulator;
- not patient-connected;
- not diagnostic.

Hardware requirements shall be captured in a separate Hardware Requirements Specification before implementation.

Potential future hardware verification:

- output amplitude accuracy;
- offset;
- noise floor;
- DAC resolution;
- jitter;
- frequency response;
- electrical safety assumptions;
- EMC pre-compliance;
- RoHS documentation;
- serial number and calibration metadata.

---

## 26. Risk Notes for Future Risk Management

This section is not a full risk-management file. It records early risk topics for later analysis.

Potential product risks:

- users may overinterpret synthetic test evidence as clinical validation;
- users may publish generated data despite license restrictions;
- generated signals may be physiologically unrealistic outside documented limitations;
- algorithms may overfit to synthetic scenarios;
- reports may be misused in regulatory submissions;
- export metadata may be stripped;
- SaaS users may upload sensitive proprietary algorithms or data in future versions;
- future hardware may create electrical, EMC, or misuse risks.

Initial controls:

- controlled intended-use wording;
- disclaimers;
- documentation of limitations;
- explicit license restrictions;
- metadata/watermarking;
- deterministic traceability;
- clear report wording;
- no patient data in MVP;
- no medical-device claims.

---

## 27. Documentation Requirements

The MVP repository shall include:

- README;
- SRS;
- scenario schema documentation;
- generator model documentation;
- export format documentation;
- report format documentation;
- limitation statement;
- license/usage restriction draft;
- developer setup instructions;
- automated test instructions.

Future documentation may include:

- software development plan;
- software architecture document;
- risk-management plan;
- verification plan;
- validation plan;
- cybersecurity plan;
- release checklist;
- tool-validation package;
- technical file index.

### REQ-DOC-001 — Development Traceability

Implemented work shall be traceable from a stable requirement or documented
engineering objective through design input, issue, implementation commit, and
verification procedure.

Priority: Must

MVP: Yes

### REQ-DOC-002 — Verification Evidence Identity

Automated verification evidence shall identify the tested commit, test
procedure ID, execution platform, result, and known retention limitation.

Priority: Must

MVP: Yes

These controls demonstrate an auditable engineering workflow. They do not by
themselves establish MDR compliance, clinical validation, or a qualified test
environment.

---

## 28. Open Questions

1. Should the first implementation be pure Python or Python core with optional C++ acceleration later?
2. Should the MVP use a local-first app or cloud SaaS immediately?
3. Which export format should follow after CSV/JSON: WFDB or EDF?
4. How detailed should ECG morphology control be in v0.1?
5. How physiologically realistic should the first PPG model be?
6. Should customer algorithm evaluation be upload-based or API-based?
7. What exact license terms should govern generated data?
8. What is the minimum paid pilot package?
9. Which 20 companies/labs should be contacted first?
10. Should hardware be shown as a concept in the first pitch, or hidden until demand is confirmed?

---

## 29. First Coding Tasks

1. Create repository structure.
2. Define scenario Pydantic models or equivalent typed schema.
3. Implement canonical JSON serialization and scenario hash.
4. Implement deterministic beat timeline generator.
5. Implement minimal ECG waveform generator.
6. Implement minimal PPG waveform generator from same timeline.
7. Implement R-peak, RR, and PPG-peak annotations.
8. Implement HRV metrics.
9. Implement CSV and JSON export.
10. Implement 10 deterministic unit tests.
11. Add three predefined example scenarios.
12. Generate first HTML report from command line.

---

## 30. Controlled Claim Library

### Allowed Claims

- Synthetic ECG/PPG generation under controlled scenario conditions.
- Ground-truth annotations for generated synthetic signals.
- Engineering test evidence for algorithm development.
- Reproducible scenarios for regression testing.
- HRV/R-peak/PPG peak algorithm stress testing.

### Avoided Claims

- Clinically validated algorithm.
- Certified validation tool.
- Medical-device conformity assessment.
- Diagnostic accuracy validation.
- Patient simulator.
- Real patient data equivalent.
- Clinical decision support.

---

## 31. Revision History

| Version | Date | Description |
|---|---|---|
| 0.1-draft | 2026-07-01 | Initial MVP SRS draft |
| 0.2-draft | 2026-07-01 | Added development traceability and verification-evidence identity requirements |
