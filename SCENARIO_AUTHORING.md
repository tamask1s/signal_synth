# Scenario Authoring API

The authoring contract lets a UI build validated Synsigra scenarios without
copying C++ enums, ranges, or target rules into the SaaS codebase.

## CLI

```sh
signal-synth authoring schema
signal-synth authoring templates
signal-synth pack analyze examples/packs/ecg_rhythm_v1.json
```

All three commands write one JSON document to standard output and diagnostics
to standard error. Their schema and catalog version fields are independent of
the waveform generator version.

`authoring schema` contains:

- form groups and field definitions;
- control and value types, defaults, units, ranges, steps, and enum options;
- visibility conditions for conditional controls;
- the complete ECG condition and artifact catalogs;
- scoring support and prerequisites for every product target;
- cross-field rules that a UI can evaluate before server submission.

`authoring templates` returns complete valid scenario documents. A client
should allow changes only at each template's `editable_paths`, then submit the
result through normal scenario validation.

`pack analyze` parses every referenced scenario and returns:

- target compatibility and local-scoring/reference-only status;
- case duration, sample rate, sample count, and channel count;
- estimated CSV, binary-signal, and complete uncompressed package sizes;
- structured errors and warnings suitable for form display.

Size values are deterministic planning estimates, not storage quotas or exact
compressed download sizes.

## C++ API

Include `scenario_authoring.h` and use:

```cpp
signal_synth::scenario_authoring_metadata_json();
signal_synth::scenario_template_catalog_json();
signal_synth::analyze_scenario_pack(manifest, scenarios, analysis);
signal_synth::scenario_pack_analysis_json(analysis);
```

The C++ layer remains authoritative. SaaS code should cache these JSON
documents by their version fields instead of maintaining a second parameter
catalog.
