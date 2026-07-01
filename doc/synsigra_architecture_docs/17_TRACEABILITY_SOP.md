# Traceability and Change-Control SOP

**Document ID:** SYN-SOP-TRACE-001

**Version:** 0.1

**Status:** Demonstration procedure

**Owner role:** Engineering / Quality

**Scope:** Requirements, issues, commits, tests, CI evidence, and releases.

## 1. Purpose and regulatory posture

This procedure demonstrates a lightweight, auditable development chain:

```text
Requirement -> Design -> Issue -> Implementation -> Test -> CI result
```

It is inspired by medical-device design-control practice, but it does not
establish MDR compliance, a QMS, tool qualification, clinical validation, or
conformity-assessment suitability. Synsigra remains a non-medical engineering
testbench under its current intended use.

## 2. Required workflow

1. Create an issue before implementation.
2. Assign a stable traceability ID such as `TRC-ECG-001`.
3. Link applicable SRS requirement IDs and design documents.
4. State measurable acceptance criteria and known limitations.
5. Implement the smallest coherent change and its verification.
6. Reference the issue from every implementing commit.
7. Open a pull request when review infrastructure is available.
8. Record the test procedure and the CI run that produced the result.
9. Close the issue only after acceptance criteria and required CI jobs pass.
10. Update the traceability matrix when requirements, design, tests, or links change.

## 3. Required issue content

Each implementation issue shall contain:

- traceability ID;
- purpose and scope;
- regulatory/claim posture where relevant;
- applicable SRS requirement links;
- design-input links;
- acceptance criteria;
- implementation commit links;
- source/module links;
- verification procedure and stable `TEST-*` IDs;
- CI run or release-evidence link;
- residual limitations, manual evidence, or missing coverage;
- final completion status.

Do not mark planned work as verified. Distinguish automated evidence, manual
integration evidence, and unsupported claims.

## 4. Commit format

Use an imperative or descriptive subject. Add one issue link per traceable
work item in the commit body:

```text
feat(ecg): add deterministic scenario fingerprint

[issue https://github.com/tamask1s/signal_synth/issues/123]
```

Multiple issue lines are allowed when a commit genuinely implements multiple
approved work items. Avoid ambiguous forms such as only `#123` in repositories
that participate in cross-repository traceability.

## 5. Test and evidence IDs

Stable suite IDs use the form `TEST-<AREA>-<NNN>`. The current suites are:

| Test ID | CTest suite | Procedure source |
|---|---|---|
| `TEST-SYNTH-001` | Legacy generator regression and golden profiles | `teszt/signal_synth_test.cpp` |
| `TEST-ECG-MODEL-001` | Stateful ECG, RR/HRV, fiducials, validation package | `teszt/ecg_model_test.cpp` |
| `TEST-ECG-PHANTOM-001` | 12-lead phantom, rhythm, conduction, source model | `teszt/clinical_ecg_test.cpp` |
| `TEST-ECG-MORPH-001` | Measured morphology and lead territories | `teszt/ecg_morphology_test.cpp` |
| `TEST-ECG-SCENARIO-001` | Scenario validation, fingerprints, assertions | `teszt/ecg_scenario_test.cpp` |
| `TEST-ECG-JSON-001` | Strict scenario JSON, canonicalization and SHA-256 | `teszt/ecg_scenario_json_test.cpp` |
| `TEST-ECG-EXPORT-001` | Render bundle, export formats, metrics and HTML report | `teszt/ecg_export_test.cpp` |
| `TEST-CLI-001` | CLI input, output, exit-code and size-limit contract | `teszt/cli_test.cmake` |
| `TEST-BUILD-001` | Installed CMake package discovery, link and execution | `teszt/package_smoke/` |

`CI-VER-001` is the cross-platform GitHub Actions procedure defined in
`.github/workflows/verification.yml`.

## 6. What counts as evidence

- A test source file is a verification procedure, not a test result.
- A workflow file is an execution procedure, not a test result.
- A successful immutable commit plus its GitHub Actions run is a test result.
- The uploaded CTest log is supporting evidence with limited retention.
- A release evidence package must archive required logs outside normal Actions
  retention and identify the tested commit, platform, compiler, and test IDs.
- A manually verified SVN/DataBrowser result must be labeled manual and must
  not be represented as automated CI evidence.

Branch links are suitable for live navigation. Release baselines shall use a
tag or commit permalink so later document edits cannot change the reviewed
evidence.

## 7. Review and change control

A reviewer should check:

- requirement and design links are relevant;
- acceptance criteria are testable;
- tests exercise the claimed behavior;
- failures are not hidden by warnings or fallback behavior;
- clinical/MDR claims remain within the controlled wording;
- generated output and evidence identify the tested revision;
- residual gaps are explicit.

Changes to requirements or test semantics require updating affected issues and
the traceability matrix. History rewriting after an evidence baseline should
be avoided; if unavoidable, all issue, report, and evidence links must be
updated and audited.

## 8. Release rule

A demonstration release may be baselined only when:

- required CI jobs pass on the release commit;
- the traceability matrix has no falsely claimed evidence;
- known gaps are documented;
- the non-clinical limitation remains visible;
- the release record links the exact CI run and commit.

This rule supports an MDR-style demonstration. It is not an MDR release gate.
