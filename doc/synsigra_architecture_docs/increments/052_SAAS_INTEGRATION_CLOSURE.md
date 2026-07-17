# SaaS Integration Closure

**Document ID:** SYN-ARCH-INC-052

**Version:** 1.0

**Status:** Implemented, local verification complete

**Owner role:** Core / SaaS Integration

**Date:** 2026-07-17

**Traceability:** `TRC-SAAS-CLOSURE-001`

**Issue:** [signal_synth#73](https://github.com/tamask1s/signal_synth/issues/73)

## 1. Decision

The supported core-to-SaaS boundary consists of one versioned C++ integration
contract and one JSON CLI receipt:

- `synsigra_integration_contract_json()` exposes the authoritative producer
  identity and nested contract versions;
- `signal-synth contract` prints the same compact JSON document;
- `signal-synth pack challenge ...` prints one compact JSON success receipt;
- stdout contains machine-readable JSON only; diagnostics remain on stderr;
- the only comparison target identifiers are the canonical ground-truth names.

No line-oriented success protocol, comparison target alias, old render-result
wrapper, or golden released-package compatibility fixture remains supported.

## 2. Contract

The integration contract version is `synsigra_core_integration_v1`. Its JSON
object contains:

- `schema_version` and `contract`;
- generator name, semantic version, Git commit, and build identity;
- C++ facade, challenge-package, scoring-manifest, scenario-authoring, and
  scenario-template contract versions;
- the canonical challenge command, success media type, and comparison targets.

The challenge receipt contains:

- the same schema and integration contract version;
- `status: challenge_rendered`;
- output directory, package ID, scenario count, pack fingerprint, and package
  fingerprint;
- the same generator identity as the preflight contract;
- challenge-package and scoring-manifest contract versions.

The SaaS must reject an unknown integration contract, a non-JSON success
payload, missing fields, a generator identity mismatch, or nested contract
versions it does not implement.

## 3. C++ Facade

The facade version is `1.0.0`. Its generic event comparator supports:

- `r_peak`;
- `ppg_systolic_peak`;
- `ppg_pulse_onset`.

ECG beat classification remains on its dedicated classification scorer and
canonical CLI target because its confusion-matrix result cannot be represented
truthfully by the generic event-comparison facade result.

Invalid facade enum values no longer fall back to R-peak. Name lookup returns
an empty string, default-tolerance lookup returns zero, and comparison returns
an explicit options error.

Rendering uses `ecg_document_render_result`; artifact construction uses
`ecg_export_result`. The previous overload that allowed an export result to be
passed into rendering was removed, including all internal call sites.

## 4. Canonical Comparison CLI

The supported command is:

```text
signal-synth compare <r_peak|ppg_systolic_peak|ppg_pulse_onset|ecg_beat_classification> ...
```

Generated scoring manifests and the Python wrapper invoke these exact names.
Aliases such as `rpeaks`, `ppg-peaks`, `ppg-onsets`, and `beat-classes` fail.

## 5. Removed Compatibility Surface

- line-oriented challenge success receipts;
- target-name aliases in detection import and CLI dispatch;
- the `ecg_export_result` rendering overload;
- standalone configuration of the `teszt` subdirectory;
- the compact historical golden challenge fixture and its update machinery.

The current Python scoring integration test and CLI integration contract test
exercise generated current packages instead of accepting historical output.

## 6. Scenario Schema Boundary

Scenario schema versions 1-3 are not removed by this increment. They are used
by scenarios and templates in the active catalog, so their parsers are active
data-model contracts rather than dormant compatibility shims. Migrating the
catalog to schema version 4 would change canonical documents and fingerprints
and requires a separate catalog migration with DataBrowser review.

## 7. DataBrowser Impact

None. The changed CLI, facade, export wrapper, Python verifier metadata, and
tests are outside `integrations/databrowser/sync_files.txt`. The DataBrowser
adapter already calls the canonical `ecg_document_render_result` overload.
No SVN synchronization is required.

## 8. Verification

- `TEST-FACADE-001` checks facade version, exact contract fields, all targets,
  and rendering through the canonical result type;
- `TEST-INTEGRATION-CONTRACT-001` parses both CLI JSON documents, compares
  producer identities, checks the generated manifest, and rejects an alias;
- `TEST-CLI-001` checks the JSON challenge receipt and canonical generated
  scoring command;
- `TEST-PYTHON-SCORING-001` checks Python-to-CLI scoring with canonical names;
- `TEST-PACK-METADATA-EXPORT-001` checks the current verifier evidence list;
- release and sanitizer CTest suites provide broad regression evidence.

Local verification on 2026-07-17:

- release CTest: 41/41 passed, including installed-package smoke;
- ASan/UBSan CTest: 40/40 passed with leak detection disabled and
  `TEST-BUILD-001` excluded because an external consumer of an instrumented
  static archive must itself be linked with sanitizer runtimes.

## 9. Acceptance Criteria

- one authoritative integration version is visible through C++ and CLI;
- challenge success stdout is valid compact JSON and contains no other text;
- producer identity is comparable before and after a worker job;
- all comparison interfaces use canonical target names only;
- render and export error domains are separate;
- no historical challenge fixture is required to validate current behavior;
- DataBrowser generation behavior is unchanged.
