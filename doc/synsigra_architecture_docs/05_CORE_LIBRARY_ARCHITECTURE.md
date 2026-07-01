# Core Library Architecture

Version: 0.1  
Status: Draft  
Scope: C++ signal generation core and product-facing scenario integration.

## 1. Purpose

The core library should be a deterministic biosignal synthesis engine that can be used from:

- command-line tools;
- SaaS backend;
- batch/regression test runner;
- future Python bindings;
- future on-prem appliance;
- future USB analog output pipeline.

It should not depend on:

- web framework;
- database;
- user accounts;
- cloud storage;
- UI widgets;
- regulatory/report rendering details.

## 2. Core design goals

| Goal | Meaning |
|---|---|
| Determinism | Same scenario/version/seed produces same output |
| Chunk invariance | Streaming in chunks must match one-shot rendering |
| Annotation integrity | Ground truth must match generated signal semantics |
| Strict validation | Unsupported labels must be rejected |
| Separation of concerns | Model, scenario, export, report are separate |
| Stable public API | Product/SaaS should not call unstable internals |
| Testability | Every major mechanism has deterministic tests |
| Future bindings | C ABI or stable C++ facade can support Python/FFI |

## 3. Proposed packages

```text
include/signal_synth/
  core/
    version.hpp
    timebase.hpp
    deterministic_rng.hpp
    fingerprint.hpp
    signal_buffer.hpp
    numeric.hpp

  ecg/
    ecg_model.hpp
    cardiac_phantom.hpp
    ecg_annotations.hpp
    ecg_fiducials.hpp
    ecg_noise.hpp

  ppg/
    ppg_model.hpp
    ppg_annotations.hpp
    ppg_artifacts.hpp
    ecg_ppg_coupling.hpp

  scenario/
    scenario.hpp
    scenario_validation.hpp
    scenario_fingerprint.hpp
    condition_catalog.hpp

  export/
    export_package.hpp
    csv_export.hpp
    json_export.hpp

  report/
    report_model.hpp
```

## 4. Main abstractions

### 4.1 Scenario

A scenario is a versioned, product-facing test definition.

It should include:

- scenario ID;
- schema version;
- duration;
- sampling configuration;
- channel list;
- seed;
- rhythm/HRV configuration;
- ECG morphology;
- PPG morphology;
- artifacts;
- event schedule;
- export/report options;
- intended use disclaimer.

### 4.2 Render request

A render request is an executable scenario plus runtime options.

Fields:

- scenario;
- output sample rate;
- render start/stop;
- channels;
- export selection;
- generator version.

### 4.3 Render result

A render result should contain:

- sample arrays;
- channel metadata;
- construction annotations;
- measured annotations;
- ground-truth metrics;
- scenario fingerprint;
- warnings;
- limitations;
- generator version.

### 4.4 Annotation levels

The library should keep at least four annotation classes:

| Annotation level | Meaning |
|---|---|
| Construction event | Internal event: R, P, Q, S, T, pulse onset, artifact boundary |
| Signal fiducial | Measured feature in the generated discrete signal |
| Semantic event | Beat type, arrhythmia episode, artifact interval, dropout |
| Customer algorithm output | Imported later for comparison/reporting |

## 5. Public facade

The SaaS/backend should depend on a small stable facade rather than internal classes.

Suggested C++ facade:

```cpp
namespace signal_synth {

struct RenderRequest;
struct RenderResult;
struct ValidationResult;

ValidationResult validate_scenario_json(const std::string& json);
RenderResult render_scenario_json(const std::string& json);
std::string scenario_fingerprint_json(const std::string& json);
std::string export_result_json(const RenderResult& result);
std::string render_report_html(const RenderResult& result);

}
```

If Python bindings are planned, this facade is more important than exposing every C++ class.

## 6. Current repo assessment

Current direction is generally positive:

- deterministic chunk-invariant streaming is a strong technical foundation;
- seeded LF/HF oscillator-bank RR process aligns with HRV test use;
- distinction between configured model events and measured fiducials is exactly the right design;
- strict scenario rejection is better than silently generating mislabeled signals;
- clinical engine limitation statement is good and should remain prominent.

Potential concerns:

1. The `clinical_ecg` name may overstate clinical validity.
2. PTB-XL condition catalog can create the impression of clinically realistic disease generation.
3. 12-lead projection is useful, but should be framed as compact synthetic/phantom projection, not torso-realistic simulation.
4. PPG should become first-class soon if wearable use case matters.
5. Product-facing scenario schema should not expose too much internal morphology complexity early.

## 7. Recommended refactor path

### Step 1: Stabilize ECG core

- keep existing ECG model tests;
- preserve chunk-invariance tests;
- add golden scenario outputs;
- add scenario fingerprint tests.

### Step 2: Introduce clean facade

- implement `validate_scenario_json`;
- implement `render_scenario_json`;
- return structured result.

### Step 3: Add PPG first-class module

- shared beat timeline from ECG/RR process;
- simple pulse morphology;
- configurable pulse delay;
- configurable amplitude modulation;
- basic artifacts.

### Step 4: Add export package

- CSV;
- JSON;
- annotation tables;
- metadata;
- scenario fingerprint.

### Step 5: Add CLI

Commands:

```bash
signal-synth validate scenario.json
signal-synth render scenario.json --out out/
signal-synth report scenario.json --out report.html
signal-synth fingerprint scenario.json
```

## 8. Error handling

Use structured errors:

- invalid schema;
- unsupported condition;
- incompatible condition set;
- unsupported channel;
- invalid numeric range;
- render warning;
- precision limitation;
- export limitation.

Avoid silent fallback.

## 9. Versioning

Every output should include:

- generator semantic version;
- git commit hash if available;
- scenario schema version;
- scenario fingerprint;
- render options;
- deterministic seed;
- platform note if floating-point portability is limited.

## 10. Minimal acceptance criteria

The core library is ready for SaaS integration when:

- 10 predefined scenarios render deterministically;
- chunked and one-shot rendering match;
- scenario fingerprint is stable;
- annotations align with generated samples;
- CSV and JSON export work;
- HTML report can be generated from a render result;
- unsupported scenario requests are rejected with explicit errors.
