# Synsigra System Architecture

Version: 0.1  
Status: Draft  
Scope: Full Synsigra technical system, including current C++ library, future SaaS, reporting, and optional hardware.

## 1. Architectural goal

Synsigra should be structured as a productizable system around a deterministic biosignal generation core.

Primary system value:

- reproducible ECG/PPG scenarios;
- explicit ground truth;
- scenario-level auditability;
- algorithm QA and regression testing;
- reportable engineering evidence;
- optional analog output for hardware-in-the-loop testing.

## 2. Product boundary

### In scope

- ECG signal generation;
- PPG/pulse waveform generation;
- shared beat timeline;
- HRV/RR process;
- rhythm and event scenarios;
- noise and artifact injection;
- ground-truth annotation generation;
- deterministic scenario fingerprinting;
- export packages;
- evidence report generation;
- SaaS UI/API;
- future on-prem deployment;
- future USB analog output kit.

### Out of scope for current phase

- diagnostic use;
- patient monitoring;
- clinical validation certificate;
- MDR medical-device claim;
- certified patient simulator claim;
- final conformity-assessment testing claim;
- population-fitted clinical realism claim.

## 3. High-level architecture

```text
+--------------------------------------------------------------+
|                        Web Frontend                          |
| Scenario Builder | Preview | Export | Reports | Account UI    |
+-----------------------------+--------------------------------+
                              |
                              v
+--------------------------------------------------------------+
|                         SaaS Backend                         |
| API | Auth | Jobs | Project Storage | Export | Reporting      |
+-----------------------------+--------------------------------+
                              |
                              v
+--------------------------------------------------------------+
|                   Synsigra Generation Service                |
| Scenario validation | fingerprint | generation orchestration  |
+-----------------------------+--------------------------------+
                              |
                              v
+--------------------------------------------------------------+
|                      Core Signal Library                     |
| Beat timeline | ECG | PPG | Noise | Artifacts | Annotations  |
+-----------------------------+--------------------------------+
                              |
                              v
+--------------------------------------------------------------+
|                    Output and Evidence Layer                 |
| CSV | JSON | WFDB later | EDF later | HTML/PDF report        |
+--------------------------------------------------------------+

Optional later:

+--------------------------------------------------------------+
|                     USB Analog Output Kit                    |
| DAC | calibration | analog front-end | device firmware        |
+--------------------------------------------------------------+
```

## 4. Layer responsibilities

### 4.1 Core Signal Library

The core library must be deterministic, UI-independent and usable from:

- CLI;
- SaaS backend;
- test runner;
- future on-prem deployment;
- future hardware output pipeline.

Responsibilities:

- generate sample arrays;
- generate annotations;
- compute ground-truth metrics;
- validate scenario feasibility;
- guarantee reproducibility for fixed version/configuration/seed.

### 4.2 Scenario Contract Layer

This layer defines product-facing scenario JSON. It must not expose accidental internal implementation details.

Responsibilities:

- schema validation;
- compatibility checking;
- unsupported-condition rejection;
- version migration;
- reproducibility fingerprinting;
- allowed/exportable metadata.

### 4.3 Export Layer

Responsibilities:

- CSV waveform export;
- JSON metadata export;
- annotation export;
- scenario export;
- report package assembly;
- future WFDB/EDF export.

### 4.4 Reporting Layer

Responsibilities:

- render scenario summary;
- render ground-truth details;
- render waveform plots;
- render HRV/beat/event metrics;
- render warnings and limitations;
- include disclaimer that report is engineering test evidence, not clinical validation.

### 4.5 SaaS Backend

Responsibilities:

- account and project model;
- scenario persistence;
- job execution;
- export limits;
- license enforcement;
- audit logging;
- access control;
- API versioning.

### 4.6 Web Frontend

Responsibilities:

- create/edit scenarios;
- preview signals;
- display annotations;
- trigger generation;
- download outputs;
- view reports.

### 4.7 Hardware Output Layer

Responsibilities:

- convert generated digital signal into calibrated analog output;
- preserve scenario identity and output version;
- provide calibration metadata;
- not alter core ground-truth semantics.

## 5. Key architectural principles

### Determinism

For fixed:

- generator version;
- scenario JSON;
- seed;
- sample rate;
- floating point mode/platform policy;

the generated result should be reproducible.

### Separation of construction truth and measured truth

The system should distinguish:

- construction events: ideal model-level events;
- measured fiducials: extrema/onsets/offsets measured from generated samples;
- algorithm outputs: customer algorithm detections imported later.

### Strict unsupported-condition behavior

If a scenario condition is not supported, the system should reject it rather than silently producing a mislabeled signal.

### Claims discipline

The system should avoid clinical language unless later supported by regulatory strategy.

Use:

- engineering phantom;
- synthetic scenario;
- algorithm QA;
- stress test;
- ground-truth-controlled test evidence.

Avoid:

- clinically validated;
- diagnostic simulator;
- MDR validator;
- certified ECG patient simulator.

## 6. Recommended deployment models

### Phase 1: Local/CLI + library

- C++ library;
- CLI scenario generator;
- static HTML report;
- local examples.

### Phase 2: SaaS MVP

- FastAPI or similar backend wrapper;
- worker process for generation;
- object storage for exports;
- simple project database;
- React/Vue/Svelte frontend.

### Phase 3: Enterprise/on-prem

- Docker Compose / Helm deployment;
- local license file or license server;
- offline export;
- strict audit logs.

### Phase 4: Hardware bundle

- USB analog output kit;
- calibration report;
- hardware-in-the-loop scenario packs.

## 7. Main risks

| Risk | Mitigation |
|---|---|
| Overclaiming clinical value | Keep intended use engineering/R&D |
| Generator becomes research prototype only | Build CLI/API/report workflow early |
| SaaS data export kills business value | Export controls, watermarking, scenario packs, API/report value |
| Too much clinical ECG complexity too early | Focus on QA scenarios and reproducibility |
| Hardware drains resources | Keep hardware as phase 3/4 |
| Future MDR path blocked by weak docs | Keep requirements, risk, tests, traceability from early stage |
