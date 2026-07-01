# Synsigra Documentation Plan

## 1. Purpose

This document defines the next documentation set for Synsigra / signal_synth.

The project should be documented as an engineering-grade biosignal algorithm QA platform, with enough structure to support future MDR-readiness, without prematurely claiming medical-device status.

## 2. Document hierarchy

### Current / immediate documents

| Document | Purpose | Owner role |
|---|---|---|
| SRS | Product and software requirements | Product/Systems |
| System Architecture | High-level technical decomposition | Architect |
| Repository Structure | Where code and documents live | Architect/Lead Developer |
| Core Library Architecture | C++ core design, layering, API boundaries | Software Architect |
| Algorithm Design | ECG/PPG model principles and limits | Algorithm Lead |
| Scenario and Data Model | JSON scenario contract, annotations, metadata | Product/Backend |
| API Design | Local/remote generation API | Backend |
| SaaS Architecture | Web app, auth, storage, async jobs, deployment | SaaS Architect |
| Export and Reporting | CSV/JSON/WFDB/EDF/report package | Backend/Product |
| Verification and Validation Plan | Generator verification and test evidence | QA/Systems |
| Security, Privacy and License | Access control, export limits, licensing | Security/Product |
| Hardware Roadmap | USB analog output path | Hardware/Systems |
| MDR Readiness and Quality | Future regulatory bridge | Regulatory/Quality |
| Implementation Roadmap | Work sequencing | Product/Engineering |
| Repository Review | Review of current `signal_synth` direction | Architect |

## 3. Future MDR documentation set

If Synsigra later enters MDR scope, the documentation should expand toward a design-control / technical-file structure.

Likely additional documents:

| Document | Why it becomes necessary |
|---|---|
| Intended Purpose Statement | Determines medical-device qualification and claims |
| Software Qualification & Classification Rationale | MDR/MDCG decision basis |
| Risk Management Plan | ISO 14971-style risk process |
| Hazard Analysis | Misuse, misleading evidence, wrong ground truth, export integrity |
| Software Development Plan | IEC 62304-style lifecycle control |
| Software Architecture Description | Formalized architecture and interfaces |
| SOUP / Dependency List | Third-party software and license control |
| Cybersecurity Plan | SaaS, API, data integrity, user access |
| Usability Engineering File | If user interaction affects safety/evidence |
| Traceability Matrix | Requirements → design → tests → risks |
| Verification Protocols and Reports | Generator and system verification evidence |
| Validation Plan and Report | Intended-use validation, not just unit tests |
| Release Notes and Change Control | Versioned release evidence |
| Post-Market / Complaint Handling | If marketed under MDR |
| Technical Documentation Index | MDR Annex II/III dossier navigation |

## 4. Documentation style

Use Markdown first. Keep all docs versioned in Git. Each document should include:

- document title;
- version;
- status;
- owner role;
- scope;
- assumptions;
- explicit non-goals;
- open issues;
- change log.

## 5. Principle

Do not let documentation imply clinical claims. The baseline product claim should remain:

> Synthetic, reproducible, ground-truth-controlled ECG/PPG engineering scenarios for algorithm development, QA and regression testing.
