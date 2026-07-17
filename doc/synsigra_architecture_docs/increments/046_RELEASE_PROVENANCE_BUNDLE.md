# Release Provenance Bundle

**Document ID:** SYN-INC-046

**Version:** 1.0

**Status:** Verified

**Owner role:** Engineering

**Date:** 2026-07-08

**Implementation issue:** [signal_synth#69](https://github.com/tamask1s/signal_synth/issues/69)

## 1. Decision

Add explicit release and package provenance artifacts to the core generator,
scenario export and challenge-package flows.

The provenance contract is split into:

- public release notes in `RELEASE_NOTES.md`;
- reusable provenance checklist and claim-boundary documentation in
  `PROVENANCE_BUNDLE.md`;
- machine-readable `provenance.json` artifacts in scenario exports and pack
  challenges;
- human-readable `ENGINEERING_CLAIM_BOUNDARY.txt` artifacts in scenario
  exports and pack challenges.

## 2. Rationale

Challenge packages are intended to support algorithm QA and local verifier
workflows. A customer must be able to archive not just the waveforms and
ground truth, but also:

- which generator build produced the package;
- which package and scoring contracts apply;
- which scenario or pack fingerprint was rendered;
- what the package verifies;
- what the package explicitly does not verify.

Putting this information in generated artifacts avoids relying on transient
release notes or SaaS-side metadata as the sole audit source.

## 3. Public Contract

The core C++ API exposes compact identity helpers:

```text
signal_synth_generator_version()
signal_synth_generator_git_commit()
signal_synth_build_identity()
signal_synth_package_contract_version()
signal_synth_scoring_manifest_contract_version()
signal_synth_verifier_version()
signal_synth_engineering_claim_boundary_text()
```

`signal_synth_generator_git_commit()` is populated from CMake when built in a
Git checkout and returns `unknown` otherwise. The value is not a clinical
release identifier; it is an engineering reproducibility aid.

Every scenario export includes:

- `provenance.json`;
- `ENGINEERING_CLAIM_BOUNDARY.txt`.

Every pack challenge includes:

- package-level `provenance.json`;
- package-level `ENGINEERING_CLAIM_BOUNDARY.txt`;
- per-case `cases/<case_id>/provenance.json`;
- per-case `cases/<case_id>/ENGINEERING_CLAIM_BOUNDARY.txt`.

The challenge manifest hashes these files and assigns explicit metadata/readme
roles rather than leaving them as generic artifacts.

## 4. Data Flow

Scenario rendering builds the normal `ecg_render_bundle`. `build_ecg_export_bundle`
then serializes:

1. scenario JSON and optional resolved/randomization JSON;
2. `metadata.json`;
3. `provenance.json`;
4. waveform, annotation, tachogram, metrics and warning artifacts;
5. `ENGINEERING_CLAIM_BOUNDARY.txt`;
6. HTML report and native waveform exports.

`signal-synth pack challenge` reuses each case export bundle and adds a
package-level provenance file derived from the validated pack manifest,
rendered case rows and pack fingerprint.

Curated pack metadata and authoring analysis now advertise
`provenance_json` and `engineering_claim_boundary_txt` as required output
artifact roles.

## 5. Compatibility

The change is additive at the file level. Existing artifact names keep their
meaning. Consumers that ignore unknown files can continue to read waveform,
annotation and metric artifacts. Consumers that pin exact artifact counts must
update to include the two required provenance files.

The package fingerprint changes when a challenge package is regenerated
because the new files are hashed in the manifest.

## 6. Non-goals

This increment does not implement:

- package signing;
- immutable release storage;
- a regulated QMS;
- clinical validation evidence;
- medical-device conformity assessment;
- SaaS-side release publication logic.

## 7. Verification

Implemented verification covers:

- scenario export artifact ordering and provenance contents;
- public facade artifact visibility;
- challenge artifact role mapping;
- CLI render and pack challenge contracts;
- Python local-verifier package loading with provenance files present;
- curated metadata output artifact roles;
- current CLI integration contract and generated challenge receipt.

Local release verification on 2026-07-08:

```text
ctest --output-on-failure
```

Result: 40/40 tests passed.
