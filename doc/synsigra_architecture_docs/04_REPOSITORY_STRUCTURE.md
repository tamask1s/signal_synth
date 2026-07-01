# Recommended Repository Structure

Version: 0.1  
Status: Draft  
Target repository: `signal_synth`

## 1. Current observation

The current repository is a public C++11 signal-generation library with `src`, `teszt`, several top-level Markdown specification files, and a README describing:

- legacy `signal_synth` generators;
- `ecg_model` for a stateful phase-domain ECG and five-channel validation package;
- `clinical_ecg` for structured clinical timeline, 3D cardiac vector model, 12-lead projection and fiducials;
- `ecg_scenario` for product-facing QA scenario contract, strict validation, reproducibility fingerprint and audit report.

The existing technical direction is not obviously wrong. In fact, determinism, chunk-invariant streaming, explicit construction/measured fiducials and strict scenario rejection are good foundations.

The main structural issue: the repository is still shaped like a prototype library, not yet like a product platform.

## 2. Recommended root structure

```text
signal_synth/
  README.md
  LICENSE
  CMakeLists.txt
  cmake/
  include/
    signal_synth/
      core/
      ecg/
      ppg/
      scenario/
      export/
      report/
  src/
    core/
    ecg/
    ppg/
    scenario/
    export/
    report/
  apps/
    signal_synth_cli/
    report_viewer/              # optional later
  bindings/
    python/                     # later
  web/
    frontend/                   # SaaS UI later
    backend/                    # SaaS/API wrapper later
  hardware/
    analog_usb_kit/             # later
      firmware/
      electronics/
      calibration/
      compliance/
  tests/
    unit/
    integration/
    golden/
    scenario/
    regression/
  examples/
    scenarios/
    exports/
    reports/
  schemas/
    scenario/
    export/
    report/
  docs/
    product/
    architecture/
    algorithms/
    api/
    v_and_v/
    regulatory/
    security/
    hardware/
    legal/
  tools/
    format/
    generate_golden/
    validate_scenario/
  third_party/
    README.md
  packaging/
    docker/
    python/
    debian/                     # optional later
```

## 3. Immediate migration from current repo

### Top-level Markdown files

Move current top-level specification files into `docs/`:

```text
CLINICAL_ECG_SPECIFICATION.md  -> docs/algorithms/clinical_ecg_specification.md
DATA_LICENSES.md              -> docs/legal/data_licenses.md
ECG_SCENARIO_SPECIFICATION.md -> docs/algorithms/ecg_scenario_specification.md
LEGAL_PROVENANCE.md           -> docs/legal/legal_provenance.md
MODEL_SPECIFICATION.md        -> docs/algorithms/ecg_model_specification.md
PRODUCT_DIRECTION.md          -> docs/product/product_direction.md
synsigra_srs.md               -> docs/product/srs.md
```

Keep the root README short and link into these documents.

### Test folder

Rename:

```text
teszt/ -> tests/
```

If `teszt` contains CMake entry points, either:

- move its contents into `tests/`, or
- temporarily keep compatibility with a root `CMakeLists.txt` target.

Use English names for external/commercial repo clarity.

### Source layout

Current `src` can remain, but it should be split into domains:

```text
src/
  core/
  ecg/
  ppg/
  scenario/
  export/
  report/
```

Public headers should be in:

```text
include/signal_synth/...
```

This helps future consumers integrate the library cleanly.

## 4. Naming recommendation

### Keep repository name?

`signal_synth` is acceptable as the library/repo name.

Use product branding separately:

- product/brand: `Synsigra`;
- core library: `signal_synth`;
- SaaS app: `synsigra_app`;
- CLI: `signal-synth`.

This avoids premature rename risk. The repo can remain `signal_synth` if it becomes the canonical engineering repo.

### Watch out for `clinical_ecg`

The name `clinical_ecg` may create expectation that the generated signal is clinically validated or patient-population-fitted.

Options:

1. Keep `clinical_ecg` internally, but document it as an engineering phantom.
2. Rename to `structured_ecg` or `cardiac_phantom`.
3. Use `clinical_ecg` only for condition catalog and labels, not for product claims.

Preferred:

```text
clinical_ecg -> cardiac_phantom
```

or at least:

```text
docs/algorithms/clinical_ecg_specification.md
```

with a strong limitation statement.

## 5. Suggested module boundaries

### `core`

- deterministic random utilities;
- timebase;
- sample buffer;
- numerical helpers;
- hashing/fingerprinting;
- version metadata.

### `ecg`

- phase-domain ECG;
- structured/cardiac phantom;
- fiducial measurement;
- ECG annotations;
- ECG noise components.

### `ppg`

- PPG pulse model;
- ECG-to-PPG coupling;
- pulse transit/arrival delay;
- respiratory modulation;
- PPG artifacts.

### `scenario`

- scenario schema;
- versioning;
- validation;
- condition catalog;
- compatibility checks;
- scenario fingerprint.

### `export`

- CSV;
- JSON;
- annotation tables;
- future WFDB/EDF.

### `report`

- report data model;
- HTML renderer;
- future PDF renderer;
- disclaimers and limitations.

### `apps`

- CLI generation app;
- scenario validation tool;
- batch report generator.

## 6. Documentation placement

Put the new architecture pack under:

```text
docs/
  product/
    srs.md
    product_direction.md
  architecture/
    system_architecture.md
    repository_structure.md
    core_library_architecture.md
    saas_architecture.md
  algorithms/
    algorithm_design.md
    ecg_model_specification.md
    ecg_scenario_specification.md
    ppg_model_specification.md
  api/
    api_design.md
  v_and_v/
    verification_and_validation_plan.md
  security/
    security_privacy_and_license.md
  hardware/
    hardware_roadmap.md
  regulatory/
    mdr_readiness_and_quality.md
  roadmap/
    implementation_roadmap.md
  legal/
    legal_provenance.md
    data_licenses.md
```

## 7. Minimum near-term repo target

Before SaaS work, aim for:

```text
signal_synth/
  include/signal_synth/...
  src/...
  tests/...
  examples/scenarios/*.json
  apps/signal_synth_cli/
  schemas/scenario/v1.schema.json
  docs/product/srs.md
  docs/architecture/system_architecture.md
```

## 8. CMake recommendation

Root `CMakeLists.txt` should define:

- core library target;
- CLI target;
- test targets;
- installable headers;
- option flags.

Suggested options:

```cmake
option(SIGNAL_SYNTH_BUILD_TESTS "Build tests" ON)
option(SIGNAL_SYNTH_BUILD_CLI "Build CLI tools" ON)
option(SIGNAL_SYNTH_BUILD_EXAMPLES "Build examples" ON)
option(SIGNAL_SYNTH_BUILD_PYTHON "Build Python bindings" OFF)
```

## 9. Don’t do yet

Do not add the SaaS frontend into the core library path. Keep SaaS in `web/` or a separate repo later.

Do not add hardware firmware next to core signal code. Keep it in `hardware/`.

Do not mix regulatory documents with marketing docs. Keep regulatory in `docs/regulatory/`.

## 10. Decision

Use `signal_synth` as the final engineering repository if you accept a product/library split:

- `signal_synth` = library and technical core;
- `Synsigra` = commercial product/brand;
- `Synsigra Testbench` = SaaS and workflow product.
