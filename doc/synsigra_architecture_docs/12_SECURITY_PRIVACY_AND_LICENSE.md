# Security, Privacy and License Design

Version: 0.1  
Status: Draft  
Scope: SaaS controls, generated data licensing, redistribution restrictions, provenance.

## 1. Product risk

The central business risk is that customers may export large generated datasets and redistribute them publicly, reducing future demand.

The technical response should not rely only on legal terms. It should combine:

- license restrictions;
- export metadata;
- watermarking;
- rate limits;
- scenario packs as service value;
- reports/API/workflow value;
- enterprise contracts.

## 2. Allowed customer uses

Allow:

- internal R&D;
- algorithm development;
- algorithm training;
- HRV/R-peak/PPG peak/arrhythmia testing;
- validation experiments;
- internal QA;
- aggregate publication of performance results.

## 3. Prohibited uses

Prohibit:

- public redistribution of generated raw signals;
- uploading generated datasets to public repositories;
- resale or sublicensing of generated data;
- making a public benchmark dataset from generated outputs;
- using outputs to train or validate a competing synthetic biosignal generator;
- removing export metadata/watermarks;
- misrepresenting generated signals as patient data.

## 4. License object model

Define separate legal objects:

| Object | Example | Protection approach |
|---|---|---|
| Software | core generator, SaaS | software license |
| Scenario library | HRV stress pack | proprietary content / trade secret |
| Generated output | CSV/JSON signal export | internal-use license |
| Report | HTML/PDF evidence | customer-use license |
| Documentation | specifications | copyright/confidentiality |
| Hardware firmware | analog kit firmware | software/hardware license |

## 5. SaaS security baseline

MVP:

- HTTPS;
- authentication;
- project-level access control;
- private scenarios by default;
- signed export URLs;
- audit log for exports;
- rate limits;
- no public dataset publishing feature.

Later:

- organization roles;
- API keys;
- SSO;
- export policy controls;
- audit log export;
- IP allowlists;
- on-prem option.

## 6. Export watermark metadata

Every export should include:

- export ID;
- organization/customer ID where applicable;
- license ID;
- generator version;
- scenario fingerprint;
- timestamp;
- redistribution restriction;
- scenario pack ID if used.

CSV can include a sidecar `metadata.json`. If CSV comments are used, keep compatibility in mind.

## 7. Audit events

Record:

- scenario creation;
- scenario modification;
- render job started;
- export generated;
- export downloaded;
- API key used;
- report generated;
- project shared.

## 8. Privacy

The current product generates synthetic data and should avoid ingesting patient data.

Explicit product rule:

> Do not upload patient ECG/PPG data to Synsigra MVP.

If later customer algorithm comparison requires uploads, accept only algorithm outputs first:

- R-peak timestamps;
- PPG peak timestamps;
- classifications;
- HR time series.

Avoid raw patient signal uploads unless a proper privacy/compliance strategy is implemented.

## 9. Data retention

Recommended retention:

| Data | MVP retention |
|---|---|
| Scenarios | until user deletes |
| Render results | plan-specific, e.g. 7–90 days |
| Exports | temporary signed downloads |
| Audit logs | longer retention |
| Reports | same as render result |
| Billing data | as legally required later |

## 10. Legal provenance

Maintain a strict `LEGAL_PROVENANCE.md`.

Rules:

- no copying from GPL implementation into proprietary core unless license strategy permits;
- no inclusion of third-party datasets without license review;
- no trained model from restricted data without provenance;
- keep references and algorithms clean-room where needed.

## 11. Open-source strategy

Options:

1. Closed core, public docs/samples.
2. Open basic generator, closed scenario packs/SaaS/reporting.
3. Dual license.
4. Internal-only until product-market fit.

Recommended now:

- keep repository public only if comfortable with open development;
- avoid publishing the most valuable scenario packs early;
- separate reusable open-source library from commercial Synsigra service if needed.

## 12. Terms of service clause concept

Non-final legal language:

> Customer may use Generated Outputs solely for internal research, development, testing, validation and training of biosignal analysis algorithms. Customer shall not publish, distribute, sublicense, sell, transfer, or make publicly available any Generated Outputs, Scenario Definitions, Ground Truth Annotations, or substantially similar reconstructed datasets. Customer shall not use the Generated Outputs to develop, train, validate, or commercialize a competing synthetic biosignal generation product or dataset service. Publication of aggregate evaluation results is permitted provided that raw or reconstructable Generated Outputs are not disclosed.

## 13. Security non-goals for MVP

Do not build:

- patient-data compliance workflows;
- HIPAA/GDPR medical record processing;
- public dataset hosting;
- complex marketplace features;
- unbounded anonymous generation.

## 14. Product principle

Make the workflow valuable enough that the data alone is not the product.

Value should live in:

- scenario packs;
- API;
- reports;
- regression testing;
- support;
- customization;
- later hardware.
