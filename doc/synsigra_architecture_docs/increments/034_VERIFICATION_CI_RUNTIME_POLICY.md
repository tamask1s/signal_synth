# Verification CI Runtime Policy

**Document ID:** SYN-ARCH-INC-034

**Version:** 1.0

**Status:** Verified

**Owner role:** Verification / Developer workflow

**Date:** 2026-07-05

**Traceability IDs:** `TRC-CI-RUNTIME-001`

**Implementation issue:** [signal_synth#57](https://github.com/tamask1s/signal_synth/issues/57)

## 1. Decision

Default GitHub Actions verification shall prioritize fast, high-signal Linux
coverage for this repository. Windows is not a critical product build target for
the standalone `signal_synth` repository, so it shall not run full verification
on every push or pull request.

## 2. CI Contract

- Push and pull-request CI runs Ubuntu C++11 full verification.
- Ubuntu full verification includes native WFDB `rdsamp`/`rdann` conformance
  by provisioning WFDB Software Package 10.7.0 before CTest.
- Windows verification is retained as a manual `workflow_dispatch` smoke job.
- The Windows smoke job builds the project and runs `TEST-BUILD-001` only.

## 3. Rationale

The native WFDB test increased the Linux job duration because CI now downloads
and builds WFDB 10.7.0. That cost is acceptable because it closes an important
standard-format conformance gap. The previous Windows full job added wall-clock
time without being a release gate for this repository.

This split keeps the important exporter conformance coverage while reducing
default CI wall-clock time and avoiding routine Windows capacity use.

## 4. Verification

Evidence:

- local workflow syntax/diff review: passed;
- GitHub Actions run `28729524128`: passed;
- Ubuntu full verification ran in `1m33s` and included WFDB 10.7.0 native-tool
  provisioning plus full CTest;
- Windows smoke was skipped on push as intended and remains available through
  manual `workflow_dispatch`.

## 5. Change Log

| Version | Date | Change |
|---|---|---|
| 1.0 | 2026-07-05 | Defined Linux full default CI and manual Windows smoke policy |
