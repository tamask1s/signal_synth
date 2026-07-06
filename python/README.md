# Synsigra Python SDK

`synsigra` is the local Python SDK for loading Synsigra challenge packages and verifying user algorithm outputs. It is the customer-facing no-build path for scoring downloaded packages from Synsigra / `signal_synth_saas`.

It does **not** execute proprietary detector code and it does **not** require the C++ generator source tree for local scoring.

## Install

From the repository root during beta:

```bash
python -m pip install .
```

From a private-beta release artifact:

```bash
python -m pip install synsigra-0.2.0-py3-none-any.whl
```

Verify installation:

```bash
synsigra-verify --help
```

## One-command verification

```bash
synsigra-verify package.zip detections/ verification-results/ --profile regression
```

Arguments:

- `package.zip` or `challenge.synsigra`: downloaded Synsigra challenge package or package directory;
- `detections/`: one user-output file per case/target, using the schema described by the package scoring metadata;
- `verification-results/`: output directory for JSON, CSV, and HTML reports;
- `--profile`: threshold policy. Built-ins are `smoke`, `regression`, `stress`, and `benchmark`.

Useful filters:

```bash
synsigra-verify package.zip detections/ out/ --case clean_70 --target r_peak
synsigra-verify package.zip detections/ out/ --profile path/to/custom-profile.json
```

Use `--force` to replace an existing output directory.

## CI behavior

The verifier prints a compact status summary and exits with:

- `0` when package integrity, scoring, and the selected threshold profile pass;
- non-zero when package integrity fails, required detections are missing, scoring fails, or the selected threshold profile fails.

Exit codes are:

- `0`: package integrity, scoring, and threshold policy passed;
- `1`: integrity, input, scoring, or threshold-policy failure;
- `2`: invalid command-line usage reported by `argparse`.

The output directory contains:

- `verification_summary.json`: canonical machine-readable overall result;
- `verification_summary.csv`: compact case-target table;
- `verification_report.html`: human-readable report;
- `verification/<case-target>/`: per-target JSON, CSV, and HTML evidence.

## Supported environments

The beta wheel is pure Python and supports CPython 3.8 through 3.11 on Linux,
macOS, and Windows. Release CI installs and tests the wheel on clean Linux
Python 3.8 and 3.11 environments.

`synsigra-verify` never invokes `signal-synth`; it scores only downloaded
package ground truth and user outputs. Generator-backed convenience functions
such as `compare_rpeaks` and `score_pack` require a separately installed
`signal-synth` executable, selected with their `cli_path` argument or the
`SYNSIGRA_CLI` environment variable.

## Scope boundary

This package produces synthetic engineering QA evidence. It is not a diagnostic device, patient monitor, clinical validation system, certified medical-device validator, or standalone conformity-assessment tool.
