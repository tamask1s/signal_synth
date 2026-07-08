# Provenance Bundle

Synsigra release and challenge artifacts use an explicit provenance bundle so
algorithm developers can archive what was generated, by which build, under
which contract, and with which engineering QA claim boundary.

## Scenario Export Files

`signal-synth render <scenario.json> --out <directory>` emits:

- `metadata.json`: compact render metadata and channel layout;
- `provenance.json`: machine-readable generator, verifier, scenario,
  render, contract and claim-boundary identity;
- `ENGINEERING_CLAIM_BOUNDARY.txt`: human-readable intended-use and
  non-claim statement;
- `report.html`: human-readable report that links the same provenance and
  claim boundary.

`provenance.json` includes:

- `generator.name`, `generator.version`, `generator.git_commit`,
  `generator.build_identity`;
- `verifier.version`, `verifier.package_contract_version`,
  `verifier.scoring_manifest_contract_version`;
- `scenario.id`, scenario fingerprints, render identity, run fingerprint,
  schema version, engine version and seed;
- render sample rate, sample count, duration and channel presence;
- a `provenance_checklist` describing required artifacts;
- `claim_boundary` fields for intended use, verified properties,
  non-verified properties and prohibited uses.

## Challenge Package Files

`signal-synth pack challenge <pack.json> --out <directory>` emits the same
per-case scenario export provenance files under `cases/<case_id>/` and adds
package-level files:

- `provenance.json`: pack identity, pack fingerprint, targets, generator
  build identity, verifier contract identity and package checklist;
- `ENGINEERING_CLAIM_BOUNDARY.txt`: package-level engineering QA
  claim-boundary text;
- `scoring_manifest.json`: local-scoring target contract with generator git
  commit and contract versions.

The generated challenge manifest hashes all package and case files, including
the provenance and claim-boundary artifacts.

## Claim Boundary

The package verifies deterministic engineering properties: internal
consistency between generated waveform files, scenario JSON, annotations,
metrics, reports, native exports and the scoring contract.

The package does not verify patient physiology, diagnostic performance on real
populations, clinical safety, clinical effectiveness, MDR compliance, FDA
compliance or regulatory conformity. It is not for diagnosis, patient
monitoring, clinical validation certification or standalone conformity
assessment.

## Release Checklist

Before publishing a core generator/verifier release:

- update `RELEASE_NOTES.md`;
- render at least one representative scenario and inspect `provenance.json`;
- render at least one representative challenge package and inspect the
  package-level and per-case provenance files;
- run the C++/CLI/Python verification suite;
- archive the exact commit, CI run, release notes and generated package
  fingerprints outside transient CI logs when a long-lived release is needed.
