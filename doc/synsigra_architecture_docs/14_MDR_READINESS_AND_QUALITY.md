# MDR Readiness and Quality Plan

Version: 0.1  
Status: Draft  
Scope: Future-readiness only. Current product remains an engineering/R&D tool.

## 1. Current intended use

Current intended use:

> Synsigra is an engineering/R&D tool for generating synthetic ECG/PPG scenarios with ground-truth annotations for algorithm development, testing, QA and regression workflows.

Current non-use:

- diagnosis;
- treatment;
- patient monitoring;
- direct clinical decision support;
- certified medical-device validation;
- standalone conformity-assessment evidence.

## 2. Why this matters

Medical-device qualification depends heavily on intended purpose and claims.

If Synsigra later claims to be a medical-device validation tool, patient simulator, diagnostic validator or clinical evidence generator, it may enter a regulated pathway.

## 3. Future MDR transition trigger

Reassess MDR status if any of these become true:

- marketing claims mention medical-device conformity assessment;
- reports are sold as medical-device validation certificates;
- tool is intended to verify safety/performance of a specific medical device in final release context;
- software outputs are used for diagnosis/treatment/monitoring;
- tool becomes an accessory to a medical device;
- hardware is marketed as patient simulator or clinical ECG simulator.

## 4. Regulatory references to track

- Regulation (EU) 2017/745 MDR;
- MDR Annex II technical documentation;
- MDCG 2019-11 rev.1 for software qualification/classification;
- IEC 62304 for medical-device software lifecycle;
- ISO 14971 for risk management if MDR path begins;
- IEC 62366 usability if user interaction becomes safety-relevant;
- IEC 81001-5-1 / cybersecurity guidance if applicable later.

## 5. Future technical documentation set

If MDR path begins, create:

```text
docs/regulatory/
  intended_purpose.md
  software_qualification_classification.md
  device_description.md
  risk_management_plan.md
  hazard_analysis.md
  software_development_plan.md
  software_architecture_description.md
  software_requirements_specification.md
  software_design_specification.md
  verification_plan.md
  verification_report.md
  validation_plan.md
  validation_report.md
  clinical_evaluation_strategy.md
  cybersecurity_plan.md
  usability_engineering_plan.md
  traceability_matrix.md
  release_notes.md
  post_market_plan.md
```

## 6. Quality system lightweight start

Even before MDR, use lightweight quality practices:

- versioned requirements;
- issue tracking;
- pull request review;
- release notes;
- test evidence;
- changelog;
- dependency provenance;
- risk log;
- decision records;
- traceability matrix for key features.

## 7. Traceability matrix

Maintain:

```text
Requirement ID
  -> architecture component
  -> implementation file/module
  -> test case
  -> test result
  -> release version
```

## 8. Risk log

Initial risks:

| Risk ID | Risk | Mitigation |
|---|---|---|
| R-001 | Customer misuses report as clinical validation | disclaimers, license, report wording |
| R-002 | Generated ground truth incorrect | verification tests, golden scenarios |
| R-003 | Unsupported disease label silently generated | strict scenario validation |
| R-004 | Exported data redistributed publicly | license, watermark, audit, export policy |
| R-005 | SaaS output changes across versions | versioning, fingerprints, release notes |
| R-006 | Hardware output inaccurate | calibration and limits |
| R-007 | Clinical naming overclaims realism | terminology control |

## 9. Design history

Keep design history lightweight but disciplined:

- architecture decision records in `docs/adr/`;
- scenario schema changes;
- algorithm changes;
- release notes;
- known limitations.

## 10. Requirement IDs

Use stable IDs:

```text
SRS-FUNC-ECG-001
SRS-FUNC-PPG-001
SRS-NFR-DET-001
SRS-REG-CLAIM-001
```

## 11. Release levels

### Research build

- internal experiments;
- no external use.

### Engineering beta

- design partners;
- disclaimers;
- limited support;
- known limitations.

### Commercial non-medical release

- stable API;
- terms of service;
- export controls;
- verification report;
- no medical claims.

### Regulated release later

- only after regulatory strategy and QMS decision.

## 12. Wording control

Allowed:

- engineering phantom;
- synthetic ECG/PPG;
- ground-truth-controlled scenario;
- test evidence report;
- algorithm QA.

Avoid:

- clinically validated;
- patient-equivalent;
- diagnostic simulator;
- certified medical-device validator;
- MDR-compliant validator unless actually supported.

## 13. Current conclusion

The SRS and architecture documents are a good start for future MDR readiness, but they are not enough.

The next most important documents for readiness are:

1. Intended Purpose Statement;
2. Risk Log;
3. Traceability Matrix;
4. Verification Plan;
5. Software Architecture Description;
6. SOUP/Dependency List;
7. Release Notes template.

The repository now includes a demonstration traceability SOP and living matrix.
They show how links and automated evidence can be organized, but they do not
close the QMS, risk-management, validation, approval, or evidence-retention
gaps listed in this plan.

## 14. Practical recommendation

Continue as a non-medical engineering/R&D product.

Design the documentation so that future MDR work is not impossible, but do not enter MDR scope accidentally through marketing language.

Follow `17_TRACEABILITY_SOP.md` for repository work and maintain
`18_TRACEABILITY_MATRIX.md` as a navigable demonstration record.
