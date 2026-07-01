# SaaS Architecture

Version: 0.1  
Status: Draft  
Scope: Web product architecture for Synsigra Testbench.

## 1. SaaS role

The SaaS should not be a simple dataset store.

It should provide:

- scenario creation;
- signal preview;
- controlled generation;
- export package generation;
- report generation;
- scenario library/packs;
- API access;
- audit trail;
- team/project management.

## 2. Deployment phases

### Phase 1: Local developer web app

- local backend;
- local frontend;
- no accounts;
- local files;
- useful for demos.

### Phase 2: Hosted MVP

- user accounts;
- organizations/projects;
- stored scenarios;
- render jobs;
- exports;
- billing placeholder;
- basic audit log.

### Phase 3: Enterprise

- organization-level controls;
- private projects;
- export policies;
- API keys;
- audit log export;
- optional on-prem container.

### Phase 4: Regulated enterprise

- controlled release version;
- traceable reports;
- validation package;
- change logs;
- stronger access control.

## 3. Recommended stack

A pragmatic MVP stack:

```text
Frontend:
  React or Svelte
  Plotly/uPlot/ECharts for waveform previews

Backend:
  Python FastAPI or C++ CLI wrapped by Python service
  PostgreSQL
  Object storage: S3-compatible or local MinIO for MVP
  Redis/RQ or Celery for async jobs

Core:
  C++ signal_synth library
  CLI wrapper initially
  Python bindings later

Reports:
  HTML first
  PDF via Playwright/wkhtmltopdf later
```

Alternative: keep everything local and use a C++ CLI until product demand is clearer.

## 4. SaaS components

```text
+------------------+
| Frontend         |
+--------+---------+
         |
+--------v---------+
| API Backend      |
+--------+---------+
         |
+--------v---------+      +----------------+
| Job Queue        +----->| Render Worker  |
+--------+---------+      +-------+--------+
         |                        |
+--------v---------+      +-------v--------+
| PostgreSQL       |      | Object Storage |
+------------------+      +----------------+
                                  |
                         +--------v---------+
                         | Report/Export    |
                         +------------------+
```

## 5. Database entities

### User

- id;
- email;
- name;
- auth_provider;
- created_at.

### Organization

- id;
- name;
- plan;
- billing status later.

### Project

- id;
- org_id;
- name;
- description.

### Scenario

- id;
- project_id;
- title;
- schema_version;
- scenario_json;
- scenario_fingerprint;
- created_by;
- updated_at.

### RenderJob

- id;
- scenario_id;
- status;
- requested_by;
- started_at;
- completed_at;
- error.

### RenderResult

- id;
- job_id;
- generator_version;
- scenario_fingerprint;
- metadata_json;
- warnings_json.

### ExportArtifact

- id;
- result_id;
- artifact_type;
- path;
- size_bytes;
- checksum;
- created_at.

### AuditEvent

- id;
- org_id;
- user_id;
- event_type;
- resource_type;
- resource_id;
- timestamp;
- metadata.

## 6. Frontend screens

### 6.1 Dashboard

- recent projects;
- recent scenarios;
- recent reports;
- quota usage.

### 6.2 Scenario Builder

Panels:

- general settings;
- timeline/HRV;
- ECG;
- PPG;
- events;
- artifacts;
- export settings;
- validation messages.

### 6.3 Signal Preview

- ECG plot;
- PPG plot;
- RR plot;
- annotations overlay;
- artifact bands.

### 6.4 Reports

- scenario summary;
- ground truth;
- artifact summary;
- warnings;
- downloadable package.

### 6.5 Scenario Packs

- HRV pack;
- R-peak stress pack;
- PPG motion pack;
- wearable fusion pack.

## 7. Export policy

The SaaS should allow exports but avoid unlimited untracked download.

Include in every export:

- scenario fingerprint;
- generator version;
- export ID;
- user/org/license metadata where appropriate;
- license notice;
- redistribution restrictions in README.

## 8. Auth and tenancy

MVP:

- email/password or GitHub OAuth;
- single organization per user optional.

Later:

- organizations;
- roles:
  - owner;
  - admin;
  - developer;
  - viewer;
- API keys per project;
- audit log.

## 9. Job execution

Render jobs should be async for anything longer than a preview.

Job states:

- queued;
- running;
- completed;
- failed;
- cancelled;
- expired.

## 10. Storage

Store:

- scenario JSON in database;
- generated artifacts in object storage;
- metadata/checksums in database;
- previews optionally cached.

Set retention policy by plan.

## 11. Preview vs full render

Preview should be cheap:

- shorter duration;
- lower sample rate optional;
- no full package;
- immediate response.

Full render should be job-based.

## 12. API-first requirement

The SaaS UI should call the same API that external customers use.

This avoids building a UI-only tool and later retrofitting an API.

## 13. On-prem readiness

Keep dependencies containerizable.

Recommended future deployment:

```text
docker compose:
  frontend
  backend
  worker
  postgres
  minio
  redis
```

Enterprise can later use Kubernetes/Helm.

## 14. SaaS non-goals

Do not build:

- complex billing first;
- multi-region scaling first;
- HIPAA/GDPR-sensitive patient-data workflows first;
- clinical validation workflows first.

The MVP is a technical demo and paid-pilot enabler.
