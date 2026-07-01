# Demonstration Traceability Matrix

**Document ID:** SYN-TRACE-001

**Version:** 0.1

**Status:** Living demonstration record

**Owner role:** Engineering / Quality

**Scope:** Current implemented baseline and supporting RSPT filter work.

## 1. Use and limitations

This matrix demonstrates navigable traceability. It is not a regulatory
submission, complete risk-control matrix, clinical validation record, or proof
of MDR compliance. Requirement coverage means the linked implementation and
tests contribute evidence; it does not imply that every acceptance criterion
of the full SRS is complete.

The authoritative implementation/result details remain in the linked issues.

## 2. Requirement-to-evidence matrix

| Trace ID | Requirements | Design | Implementation | Verification | Record |
|---|---|---|---|---|---|
| `TRC-CORE-001` | `REQ-GEN-004`, `REQ-NFR-003`, `REQ-NFR-008` | Core library architecture | `src/signal_synth.*` | `TEST-SYNTH-001` | [signal_synth#1](https://github.com/tamask1s/signal_synth/issues/1) |
| `TRC-CORE-002` | `REQ-NFR-002`, `REQ-NFR-008`, `REQ-VER-001` | V&V plan, golden-output verification | `src/signal_synth.cpp` | `TEST-SYNTH-001` | [signal_synth#2](https://github.com/tamask1s/signal_synth/issues/2) |
| `TRC-ECG-001` | `REQ-GEN-001`, `REQ-GEN-003`, `REQ-ECG-001..003`, `REQ-NFR-001` | Algorithm design, ECG waveform model | `src/ecg_model.*` | `TEST-ECG-MODEL-001` | [signal_synth#3](https://github.com/tamask1s/signal_synth/issues/3) |
| `TRC-ECG-002` | `REQ-SCN-003..004`, `REQ-ECG-004`, `REQ-ECG-006`, `REQ-GT-002`, `REQ-GT-004`, `REQ-VER-004..005` | RR/HRV process, beat timeline | `src/ecg_model.*` | `TEST-ECG-MODEL-001` | [signal_synth#4](https://github.com/tamask1s/signal_synth/issues/4) |
| `TRC-GT-001` | `REQ-GEN-003`, `REQ-ECG-003`, `REQ-ECG-007`, `REQ-GT-001`, `REQ-VER-003..004` | Ground-truth model | `src/ecg_model.*` | `TEST-ECG-MODEL-001` | [signal_synth#5](https://github.com/tamask1s/signal_synth/issues/5) |
| `TRC-ECG12-001` | `REQ-ECG-001..003`, `REQ-ECG-006..007`, `REQ-NFR-005` | ECG waveform and 12-lead design | `src/clinical_ecg.*` | `TEST-ECG-PHANTOM-001` | [signal_synth#6](https://github.com/tamask1s/signal_synth/issues/6) |
| `TRC-SCN-001` | `REQ-GEN-002`, `REQ-GEN-005..006`, `REQ-SCN-001..004`, `REQ-SCN-006`, `REQ-VER-002`, `REQ-VER-010` | Scenario contract layer | `src/ecg_scenario.*` | `TEST-ECG-SCENARIO-001` | [signal_synth#7](https://github.com/tamask1s/signal_synth/issues/7) |
| `TRC-ECG12-002` | `REQ-GEN-003`, `REQ-ECG-002`, `REQ-ECG-006..007`, `REQ-NFR-002` | Multi-source cardiac model | `src/clinical_ecg.*`, `src/ecg_scenario.*` | `TEST-ECG-PHANTOM-001`, `TEST-ECG-SCENARIO-001` | [signal_synth#8](https://github.com/tamask1s/signal_synth/issues/8) |
| `TRC-GT-002` | `REQ-GEN-003`, `REQ-ECG-007`, `REQ-GT-001`, `REQ-VER-003` | Annotation integrity principle | `src/clinical_ecg.cpp` | `TEST-ECG-PHANTOM-001` | [signal_synth#9](https://github.com/tamask1s/signal_synth/issues/9) |
| `TRC-DOC-001` | `REQ-GEN-006`, `REQ-NFR-005`, `REQ-NFR-008`, `REQ-VER-010`, SRS section 27 | Documentation plan | `doc/synsigra_architecture_docs/` | Document review; no executable claim | [signal_synth#10](https://github.com/tamask1s/signal_synth/issues/10) |
| `TRC-MORPH-001` | `REQ-GEN-003`, `REQ-ECG-002`, `REQ-NFR-005`, `REQ-VER-003` | Measured ground-truth design | `src/ecg_morphology.*`, `src/ecg_scenario.*` | `TEST-ECG-MORPH-001`, `TEST-ECG-SCENARIO-001` | [signal_synth#11](https://github.com/tamask1s/signal_synth/issues/11) |
| `TRC-QMS-001` | `REQ-GEN-006`, `REQ-NFR-002`, `REQ-NFR-008`, `REQ-DOC-001..002` | V&V plan, quality plan, this SOP | `.github/workflows/verification.yml`, `teszt/CMakeLists.txt` | `CI-VER-001`, all five `TEST-*` suites | [signal_synth#12](https://github.com/tamask1s/signal_synth/issues/12) |
| `TRC-BUILD-001` | `REQ-GEN-004`, `REQ-NFR-002..003`, `REQ-NFR-008`, `REQ-DOC-001..002` | Portable build increment | `CMakeLists.txt`, `cmake/`, `teszt/package_smoke/` | `TEST-BUILD-001`, all existing behavior suites | [signal_synth#13](https://github.com/tamask1s/signal_synth/issues/13) |
| `TRC-SCN-002` | `REQ-GEN-001..006`, `REQ-SCN-001..003`, `REQ-SCN-006`, `REQ-NFR-001..003`, `REQ-NFR-008` | Versioned scenario JSON increment | `src/ecg_scenario_json.*` | `TEST-ECG-JSON-001`, `TEST-BUILD-001` | [signal_synth#14](https://github.com/tamask1s/signal_synth/issues/14) |
| `TRC-DSP-001` | Supporting `REQ-NFR-003`, `REQ-NFR-008` | Shared DSP ownership | RSPT FIR design API | Manual RSPT/DataBrowser integration; automated evidence gap | [rspt_module#1](https://github.com/tamask1s/rspt_module/issues/1) |
| `TRC-DSP-002` | Supporting `REQ-NFR-008` | IIR coefficient contract | RSPT IIR/filter consumers | Manual RSPT/DataBrowser integration; automated evidence gap | [rspt_module#2](https://github.com/tamask1s/rspt_module/issues/2) |

## 3. Verification procedure index

| Evidence ID | Procedure | Primary requirement coverage |
|---|---|---|
| `TEST-SYNTH-001` | `teszt/signal_synth_test.cpp` | Determinism, input safety, legacy golden profiles |
| `TEST-ECG-MODEL-001` | `teszt/ecg_model_test.cpp` | Chunk invariance, RR/HRV, annotations, measured fiducials |
| `TEST-ECG-PHANTOM-001` | `teszt/clinical_ecg_test.cpp` | 12-lead identities, rhythm/conduction, source and annotation integrity |
| `TEST-ECG-MORPH-001` | `teszt/ecg_morphology_test.cpp` | Beat/lead morphology and territory measurement |
| `TEST-ECG-SCENARIO-001` | `teszt/ecg_scenario_test.cpp` | Catalog, validation, fingerprints, phenotype assertions |
| `TEST-ECG-JSON-001` | `teszt/ecg_scenario_json_test.cpp` | Strict parsing, canonicalization, SHA-256, transactional rejection |
| `TEST-BUILD-001` | `teszt/package_smoke/` | Installed package discovery, public-header compilation, link and execution |
| `CI-VER-001` | `.github/workflows/verification.yml` | Linux and Windows configure/build/test execution |

## 4. Evidence policy

The GitHub Actions workflow definition is the procedure. A specific successful
run URL and its `ctest-*` artifacts are the execution evidence. Actions
artifacts currently have finite retention and are not a permanent release
archive. A formal baseline would need an immutable release record containing
the tested commit, environment, logs, deviations, approvals, and matrix version.

## 5. Known traceability gaps

- RSPT FIR/IIR evidence is manual and lives outside this repository's CI.
- SVN DataBrowser integration has no GitHub-native immutable revision link.
- No independent reviewer approval or electronic signature is recorded.
- No formal risk-control verification matrix exists.
- No released V&V report or long-term evidence archive exists.
- The full SRS includes planned PPG, export, report, API, SaaS, and hardware
  requirements that are not claimed complete by this matrix.
