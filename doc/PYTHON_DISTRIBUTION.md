# Synsigra Python distribution guide

This document tracks the release-grade distribution path for the user-facing `synsigra` Python package.

## Package identity

- Package name: `synsigra`
- Console script: `synsigra-verify`
- Supported local verification command:

```bash
synsigra-verify package.zip submission/ verification-results/
```

The verifier must be usable by algorithm developers and CI systems without building the C++ signal generator.

Supported beta runtime: CPython 3.8 through 3.11. The wheel is pure Python and
has no runtime dependency on the C++ generator. Linux is exercised in release
CI; the package uses only standard-library APIs supported on Linux, macOS, and
Windows.

## Beta distribution path

During private beta, install from a checkout:

```bash
python -m pip install .
```

External beta users receive and install the generator-free wheel:

```bash
python -m pip install synsigra-0.10.0-py3-none-any.whl
```

Build local distribution artifacts:

```bash
./scripts/build_python_distribution.sh
```

Smoke test the built wheel/sdist:

```bash
./scripts/smoke_python_distribution.sh
```

For repeated development builds, keep the Git-ignored repo-local `.venv`
instead of recreating a temporary environment:

```bash
python3.8 -m venv .venv
.venv/bin/pip install build==1.2.2.post1 setuptools==75.3.2 wheel==0.45.1
PYTHON=.venv/bin/python ./scripts/build_python_distribution.sh
PYTHON=.venv/bin/python ./scripts/smoke_python_distribution.sh
```

The build script reuses the selected Python environment by default and does
not create or populate an isolated build environment. Use
`SYNSIGRA_BUILD_ISOLATION=1` only for a deliberate clean release check:

```bash
SYNSIGRA_BUILD_ISOLATION=1 PYTHON=.venv/bin/python ./scripts/build_python_distribution.sh
```

## Release checklist

Before publishing or handing the package to external beta users:

1. `python -m pip install .` succeeds from a clean environment.
2. `synsigra-verify --help` prints challenge, submission directory, output directory, profile, case, target, and force options.
3. The verifier runs without requiring the C++ source tree for downloaded challenge packages.
4. Built-in profiles are documented: `smoke`, `regression`, `stress`, `benchmark`.
5. The verifier returns non-zero on failed integrity checks, malformed/missing submission outputs, scoring failures, and failed threshold profiles.
6. Machine-readable output location is documented for CI users.
7. The challenge-generated `user-output-template/` is sufficient to prepare a submission without source-code inspection.
8. The top-level README and `python/README.md` describe the same canonical workflow.
9. The GitHub Actions Python package workflow passes.
10. Protocol-enabled challenge fixtures expose
    `synsigra_verification_protocol_v2` through the manifest role and pass the
    complete embedded evidence policy without caller overrides.
11. Malformed/ambiguous ZIP members, unlisted files, symlinks, and manifest
    duplicate keys fail the package security test.

## CI contract

`synsigra-verify` returns `0` on a passed verification, `1` on package,
input/scoring, or threshold-policy failure, and `2` on invalid CLI usage.

Archive these stable top-level outputs:

- `verification_summary.json` for machine processing;
- `verification_summary.csv` for tabular CI artifacts;
- `verification_report.html` for review.

Per-case evidence is stored below `verification/`.

The `synsigra-verify` command never discovers or executes a native binary.
Generator-backed SDK convenience calls are separate and resolve `signal-synth`
from an explicit `cli_path`, `SYNSIGRA_CLI`, or `PATH`.

## PyPI readiness notes

This patch establishes a buildable package baseline, but it does not publish to PyPI. Before public distribution, decide:

- final license metadata;
- public package name reservation and ownership;
- versioning policy;
- changelog/release-note policy;
- supported Python versions and platforms;
- whether native helper binaries are bundled, discovered, or explicitly out-of-scope.

## Claim boundary

`synsigra` supports synthetic engineering verification workflows. It does not produce clinical validation evidence by itself and must not be marketed as a diagnostic or regulated validation system.
