# Authoritative Verification Protocol and Measurement Contract v2

**Document ID:** SYN-ARCH-INC-068

**Issues:** [signal_synth#92](https://github.com/tamask1s/signal_synth/issues/92), [signal_synth#93](https://github.com/tamask1s/signal_synth/issues/93)

**Implementation baseline:** [`13fd76d`](https://github.com/tamask1s/signal_synth/commit/13fd76d)

**SaaS integration:** [signal_synth_saas#71](https://github.com/tamask1s/signal_synth_saas/issues/71)

**Version:** 1.0

**Status:** Implemented and locally verified

**Owner role:** Software Architecture / Verification

**Date:** 2026-07-18

## 1. Decision

Replace the permissive verification-protocol envelope and the split HRV versus
generic-measurement customer APIs with one coordinated, intentionally breaking
baseline:

- `synsigra_verification_protocol_v2` is the authoritative definition of an
  evidence-mode verification run;
- `synsigra_measurement_values_v2`,
  `synsigra_measurement_truth_v2`, and
  `synsigra_measurement_score_v2` cover record-, lead-, beat-, paired-signal-,
  and window-scoped measurements;
- HRV remains a first-class target, but its user output and scoring use the
  generic measurement contract rather than a dedicated HRV submission format;
- protocol v1 and measurement v1 readers, writers, templates, dispatch paths,
  and compatibility wrappers are removed.

The generator and package assembler remain native C++. The downloadable,
generator-free Python verifier remains the customer scoring boundary.

## 2. Rationale

The current package may contain a pre-specified protocol, but local verification
still accepts a caller-selected profile and arbitrary case/target filters. A
report can therefore look complete while representing a relaxed or partial run.
The protocol also describes acceptance rules in domain-specific prose instead
of carrying the exact machine-checkable threshold profile and case-target
matrix.

Measurement v1 cannot identify a window, implementation method, or preprocessing
policy. HRV compensates with a separate JSON format, scorer, CLI command, report
shape, and policy branch. This duplicates infrastructure and makes comparison
between algorithms ambiguous when methods or windows differ.

## 3. Requirements

- `PROTO2-REQ-001`: an evidence-mode run shall require exactly one packaged
  protocol v2 artifact whose bytes are covered by the challenge manifest.
- `PROTO2-REQ-002`: protocol v2 shall declare the exact required case-target
  matrix, an embedded validated threshold profile, scorer contract, stress
  strata, truth policy, context of use, and evidence boundary.
- `PROTO2-REQ-003`: evidence mode shall reject caller-supplied case filters,
  target filters, or alternative profiles and shall reject incomplete or
  incompatible package matrices before scoring.
- `PROTO2-REQ-004`: diagnostic mode may use filters and a custom profile, but
  reports shall state that the run is not evidence-eligible.
- `PROTO2-REQ-005`: every report shall identify the protocol by ID, contract,
  path, size, SHA-256 fingerprint, execution mode, and matrix completeness.
- `PROTO2-REQ-006`: every required case-target shall belong to at least one
  declared stress stratum, and the embedded profile shall contain a numeric
  acceptance section for every required target.

- `MEAS2-REQ-001`: measurement v2 shall add `window_start_seconds`,
  `window_end_seconds`, `method_id`, and `preprocessing_policy_id` to the
  existing measurement identity.
- `MEAS2-REQ-002`: window-scoped records shall use explicit `window` or
  `window_lead` scopes and shall have a finite, ordered half-open time window.
- `MEAS2-REQ-003`: JSON shall be the recommended output; CSV shall use one
  strict, documented column order with the same semantics.
- `MEAS2-REQ-004`: matching shall require equal name, unit, scope, channel,
  formula, method ID, preprocessing-policy ID, and window identity before
  temporal pairing is considered.
- `MEAS2-REQ-005`: HRV truth, user output, scoring, policy evaluation, reports,
  and templates shall use the generic measurement path. The dedicated HRV
  submission/scorer path shall be deleted.
- `MEAS2-REQ-006`: C++ and Python parsers and scorers shall remain behaviorally
  equivalent for valid, malformed, missing, extra, and duplicate records.

## 4. Public Contracts

### 4.1 Verification protocol v2

The root contains exactly:

```json
{
  "schema_version": 2,
  "contract": "synsigra_verification_protocol_v2",
  "protocol_id": "...",
  "pack_id": "...",
  "context_of_use": "...",
  "scoring_contract": "synsigra_local_verification_v2",
  "required_case_targets": [
    {"case_id": "...", "targets": ["..."]}
  ],
  "acceptance_profile": {
    "schema_version": 1,
    "profile_id": "...",
    "description": "...",
    "targets": {}
  },
  "stress_strata": [
    {"id": "...", "case_ids": ["..."]}
  ],
  "truth_policy": {},
  "evidence_boundary": "..."
}
```

Case IDs, targets, and stratum IDs are unique. The required matrix must equal
the scoring manifest's supported matrix, not merely be a subset. The union of
stress-stratum case IDs must cover all required cases. The profile is validated
by the same profile parser used for policy evaluation.

The packaged file SHA-256 from `manifest.json` is the protocol fingerprint. No
second digest representation is introduced.

### 4.2 Verification modes

`evidence` is the default mode. It uses only the packaged protocol matrix and
embedded acceptance profile. Completion and policy outcome are reported
separately; evidence eligibility requires a complete protocol-conformant run.

`diagnostic` is explicitly selected. It may select cases/targets and may use a
built-in, file-based, or in-memory threshold profile. Its report is always
marked `evidence_eligible: false`, even when every selected threshold passes.

### 4.3 Measurement values v2

JSON has the exact root contract:

```json
{
  "schema_version": 2,
  "contract": "synsigra_measurement_values_v2",
  "measurements": []
}
```

CSV columns are exactly:

```text
name,value,unit,status,scope,time_seconds,beat_index,window_start_seconds,window_end_seconds,channel,formula,method_id,preprocessing_policy_id,confidence
```

`method_id` and `preprocessing_policy_id` are optional safe ASCII identifiers.
When present they are part of identity, matching, aggregation context, and
reported provenance. Window endpoints are mandatory only for `window` and
`window_lead`; temporal beat anchors are forbidden for those scopes.

### 4.4 Generic HRV measurements

HRV metrics are window-scoped measurement records. They declare the core metric
method and NN preprocessing policy. Accepted NN intervals are emitted as normal
beat-scoped `rr_interval` records under the HRV target so detector-to-RR-to-HRV
pipeline diagnostics can be derived from the generic score result.

The native HRV computation engine and generator-side diagnostic exports remain;
only the customer submission/scoring specialization is removed.

## 5. Ownership and Data Flow

1. C++ renders a scenario and creates target truth.
2. Challenge assembly writes measurement truth v2, submission templates, a
   scoring manifest, and the immutable protocol v2 artifact.
3. Python loads the untrusted package, verifies all manifest hashes, validates
   the protocol, and checks protocol/scoring-manifest consistency.
4. Evidence mode scores the complete protocol matrix with its embedded profile.
5. Diagnostic mode scores the requested subset and labels the output as
   non-evidence.
6. Reports expose protocol fingerprint, matrix completeness, method/window
   context, execution completion, policy outcome, and engineering limitations.

## 6. Compatibility and Migration

There is no backward-compatibility layer. Existing challenge packages,
templates, customer outputs, cached curated metadata, SaaS database rows, and
worker/verifier images using protocol v1, measurement v1, scoring manifest v2,
or the dedicated HRV submission format must be deleted and regenerated.

The core integration contract advances from v6 to v7. The scoring manifest and
submission-format catalog advance to versions that advertise only measurement
v2. The submission manifest envelope remains unchanged because its structure is
not modified.

## 7. Verification

- `TEST-PROTOCOL-V2-001`: strict protocol shape, duplicate rejection, exact
  matrix equality, stratum coverage, target/profile coverage, scorer contract,
  and manifest fingerprint reporting.
- `TEST-EVIDENCE-MODE-001`: evidence defaults, forbidden overrides, complete
  execution, diagnostic filtering, and evidence-eligibility reporting.
- `TEST-MEASUREMENT-V2-001`: C++ JSON/CSV parsing, canonical output, validation,
  matching, scoring, window/method identity, and malformed input rejection.
- `TEST-PY-MEASUREMENT-V2-001`: Python parser/scorer parity and reports.
- `TEST-HRV-GENERIC-001`: HRV package generation, generic submission template,
  metric/NN scoring, pipeline diagnostics, and absence of the legacy format.
- `TEST-INTEGRATION-CONTRACT-001`: exact v7 contract tuple and generated catalog.
- release CTest, sanitizer CTest, Python distribution smoke, strict JSON scan,
  warning scan, and GCC 4.9/C++11 smoke shall pass.

## 8. DataBrowser Impact

The change affects package assembly, customer output contracts, scoring, and
reports. It does not change synthesis, rendering, scenario parsing, waveform
export, or DataBrowser APIs. No SVN source synchronization is required. The
existing dry-run manifest check remains part of final verification.

## 9. Risks and Limitations

- Protocol authority makes partial exploratory runs intentionally ineligible as
  evidence; users must select diagnostic mode for iteration.
- Exact method/preprocessing identifiers establish provenance but do not prove
  implementation equivalence or clinical validity.
- Synthetic engineering evidence is not FDA qualification, clinical validation,
  or a substitute for a sponsor-specific protocol and representative data.
- Existing SaaS state cannot be migrated implicitly; the destructive reset and
  core integration v7 adoption are tracked in
  [signal_synth_saas#71](https://github.com/tamask1s/signal_synth_saas/issues/71).

## 10. Implementation Sequence

1. Implement strict protocol v2 parsing and mode-aware verification.
2. Implement C++ and Python measurement v2.
3. Move HRV target generation and scoring to measurement v2; delete legacy
   customer paths.
4. Regenerate protocols, templates, curated metadata, reports, and contract
   snapshots.
5. Run parity, package, end-to-end, sanitizer, and compatibility tests.
6. Publish a dedicated SaaS integration issue and record immutable evidence.

## 11. Change Log

- 2026-07-18: version 0.1, coordinated design accepted for implementation under
  issues #92 and #93.
- 2026-07-18: version 1.0, protocol v2, evidence/diagnostic execution,
  measurement v2, generic HRV scoring, regenerated packs/catalog, and the v7
  integration tuple implemented. Release CTest passed 64/64; the generator-free
  Python 0.10.0 sdist/wheel build and installed-wheel smoke passed; DataBrowser
  SHA-256 sync passed for all 55 mapped files.
- AddressSanitizer/UndefinedBehaviorSanitizer passed all 63 applicable runtime
  tests. LeakSanitizer is unavailable under the execution environment's
  `ptrace`; `TEST-BUILD-001` is intentionally verified only in the normal build
  because its external consumer does not link sanitizer runtimes.
- SaaS migration requirements were frozen in handoff v4.0 and published as
  signal_synth_saas#71; no v6 or v1 compatibility path is permitted.
