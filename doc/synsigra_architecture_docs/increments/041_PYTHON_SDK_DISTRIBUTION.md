# Python SDK Distribution

**Document ID:** SYN-INC-041

**Version:** 1.0

**Status:** Implemented

**Owner role:** Engineering

**Date:** 2026-07-06

**Implementation issue:** [signal_synth#65](https://github.com/tamask1s/signal_synth/issues/65)

## 1. Decision

Distribute the generator-free local verifier as the pure-Python `synsigra`
wheel with the `synsigra-verify` console command. The canonical beta workflow
is:

```text
pip install synsigra
synsigra-verify package.zip detections/ verification-results/ --profile regression
```

The verifier consumes only challenge package ground truth and user algorithm
outputs. It does not discover or invoke `signal-synth`.

## 2. Runtime Contract

- CPython 3.8 through 3.11;
- pure Python wheel, with no runtime third-party dependencies;
- exit `0` on pass, `1` on verification failure, `2` on CLI usage error;
- stable top-level outputs: `verification_summary.json`,
  `verification_summary.csv`, and `verification_report.html`;
- per-case evidence below `verification/`.

Generator-backed convenience APIs remain separate and resolve the native CLI
through explicit `cli_path`, `SYNSIGRA_CLI`, or `PATH`.

## 3. Verification

Release CI builds wheel and sdist once per Python version, installs the wheel
into a clean virtual environment, and tests:

- import and complete CLI help;
- ZIP challenge integrity and successful local scoring;
- machine-readable output creation;
- non-zero exit on a deterministic threshold-policy failure;
- import from site-packages rather than the source tree.

The committed fixture contains only minimal generated ground truth and
detection output. It contains no generator source or patient data.

## 4. Distribution Boundary

This increment creates build artifacts for private beta and CI use. It does
not publish to PyPI, reserve a public package name, or establish clinical or
regulatory claims.

## 5. DataBrowser And SVN Impact

None. No DataBrowser API is added.

## 6. Verification Evidence

Locally verified on 2026-07-06:

- the committed challenge passes package integrity and regression scoring;
- the passing detections return exit code `0` and a successful policy result;
- the failing detections return exit code `1` and failed policy checks;
- `teszt/python_scoring_test.py` passes against the native test CLI;
- distribution scripts pass shell syntax checks and package modules compile.

The local host provides only unsupported CPython 3.6 without `venv`.
Wheel/sdist construction and clean wheel installation are therefore verified
by the Python 3.8 and 3.11 GitHub Actions matrix.
