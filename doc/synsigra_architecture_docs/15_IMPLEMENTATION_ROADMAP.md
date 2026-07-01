# Implementation Roadmap

Version: 0.1  
Status: Draft  
Scope: Suggested engineering sequence for signal_synth / Synsigra.

## 1. Guiding principle

Do not build the full SaaS before the core library can produce deterministic scenario packages and reports.

Build in this order:

1. deterministic core;
2. scenario contract;
3. CLI;
4. exports and report;
5. PPG;
6. API/backend;
7. SaaS UI;
8. customer comparison;
9. hardware.

## 2. Milestone 0: Repository cleanup

Tasks:

- move top-level docs into `docs/`;
- rename `teszt` to `tests`;
- add root `CMakeLists.txt` if not present;
- separate public headers under `include/signal_synth`;
- add `examples/scenarios`;
- add `schemas/scenario`.

Exit criteria:

- build command is standard;
- tests run from root;
- docs are organized.

## 3. Milestone 1: ECG scenario CLI

Tasks:

- implement scenario JSON validation;
- implement scenario fingerprint;
- render ECG from scenario JSON;
- export CSV + annotations JSON;
- generate simple HTML report.

CLI:

```bash
signal-synth validate examples/scenarios/ecg_clean.json
signal-synth render examples/scenarios/ecg_clean.json --out out/ecg_clean
signal-synth report examples/scenarios/ecg_clean.json --out out/ecg_clean/report.html
```

Exit criteria:

- 5 ECG scenarios render deterministically;
- chunk invariance test passes;
- report includes limitation statement.

## 4. Milestone 2: PPG MVP

Tasks:

- add PPG model;
- derive pulses from beat timeline;
- add ECG-PPG delay;
- add PPG artifacts;
- add PPG annotations;
- update report plots/metrics.

Exit criteria:

- ECG+PPG scenario renders;
- PPG peaks linked to ECG beat IDs;
- delay appears in annotations;
- PPG dropout/motion artifact intervals export.

## 5. Milestone 3: Scenario packs

Create curated scenario packs:

- HRV pack;
- R-peak stress pack;
- PPG motion pack;
- ECG-PPG delay pack;
- combined worst-case pack.

Exit criteria:

- at least 20 scenarios;
- all have descriptions;
- all have expected metrics;
- batch render passes.

## 6. Milestone 4: Backend API

Tasks:

- minimal FastAPI or equivalent backend;
- call CLI or library;
- validate scenario;
- run render job;
- store output locally;
- return report/download links.

Exit criteria:

- local API can render scenario;
- frontend can call API;
- errors are structured.

## 7. Milestone 5: Web UI

Tasks:

- scenario editor;
- waveform preview;
- event/artifact panels;
- report viewer;
- export download.

Exit criteria:

- non-developer can create clean ECG+PPG scenario;
- preview works;
- export package downloads.

## 8. Milestone 6: Customer algorithm comparison

Tasks:

- import R-peak detections;
- compare to ground truth;
- compute sensitivity/PPV/timing error;
- include comparison in report.

Exit criteria:

- CSV detections upload works;
- comparison metrics render;
- artifact-specific summary generated.

## 9. Milestone 7: SaaS hardening

Tasks:

- auth;
- projects;
- organizations;
- quota;
- audit logs;
- signed downloads;
- export watermarking;
- basic billing plan model.

Exit criteria:

- design partner can use hosted system;
- exports are traceable;
- quota limits work.

## 10. Milestone 8: Enterprise/on-prem

Tasks:

- Docker Compose;
- offline license;
- local object storage;
- audit export;
- admin config.

Exit criteria:

- customer can run it locally;
- API/UI/report works without cloud.

## 11. Milestone 9: USB analog kit

Tasks:

- define USB protocol;
- basic DAC board;
- calibration mode;
- waveform streaming;
- pre-compliance plan.

Exit criteria:

- generated scenario can be output as analog signal;
- captured signal correlates with digital source;
- calibration metadata is produced.

## 12. First 30 days

Recommended focus:

1. repo cleanup;
2. scenario schema v1;
3. CLI validate/render;
4. CSV/JSON export;
5. 10 examples;
6. deterministic tests;
7. HTML report.

Avoid in first 30 days:

- full SaaS auth;
- billing;
- hardware;
- too many clinical conditions;
- polished UI.

## 13. First commercial test

After Milestone 3, offer paid pilots:

> We test your HRV/R-peak/PPG algorithm against controlled synthetic ECG/PPG scenario packs and deliver an engineering evidence report.

Pilot delivery can be semi-manual. Product automation comes after learning.
