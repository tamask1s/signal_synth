# SaaS Release-set Export

**Document ID:** SYN-INC-044

**Version:** 1.0

**Status:** Verified

**Owner role:** Engineering

**Date:** 2026-07-06

**Implementation issue:** [signal_synth#70](https://github.com/tamask1s/signal_synth/issues/70)

## 1. Decision

Promote the curated pack catalog from a single-pack SaaS demo artifact to a
beta release-set contract. The core repository remains authoritative for
scenario/pack validation, target support, scoring contracts, fingerprints and
challenge-package rendering. The SaaS repository imports the generated release
artifact and materializes its own deployable `packs/*.json` and
`packs/*.product` files.

## 2. Release Set

The beta release set is:

- `r_peak_stress_v1`;
- `hrv_v1`;
- `ecg_beat_classification_v1`;
- `ecg_rhythm_v1`;
- `signal_quality_v1`;
- `ecg_morphology_stress_v1`;
- `ppg_alignment_v1`;
- `combined_worst_case_v1`;
- `wearable_stress_v1`;
- `ppg_benchmark_v1`.

`examples/catalog/verification_packs_v1.json` is version `1.3`.
`examples/catalog/curated_pack_metadata_v1.json` carries
`release_set_id=synsigra_curated_release_2026_07_06`.

## 3. Contracts

`signal-synth pack analyze <pack.json>` now returns SaaS-ingestable fields in
addition to the previous analysis output:

- `metadata_type=synsigra_pack_analysis`;
- `scoring_mode`;
- `recommended_verifier_profile`;
- `generator_compatibility`;
- `scoreable_targets`;
- `reference_only_targets`;
- `detector_output_schemas`;
- `output_artifacts`;
- case-level `scoreable_targets` and `reference_only_targets`.

The curated metadata exporter adds:

- release-set identity and status;
- scoreable target contracts;
- reference-only target contracts;
- local verifier smoke-test references for every scoreable target;
- product copy, release date and changelog for every release-set pack;
- estimated package size and memory envelope.

## 4. SaaS Import Boundary

The SaaS repository should not maintain hand-written pack sidecars for the
curated beta catalog. It imports the core artifact with:

```text
python3 scripts/import_curated_release_set.py --metadata ../signal_synth/examples/catalog/curated_pack_metadata_v1.json --source-root ../signal_synth --out packs --clean
```

The import script rewrites scenario paths relative to the SaaS `packs`
directory and emits minimal `.product` sidecars containing release status,
fingerprint, generator contract and changelog. The SaaS catalog loader accepts
`beta`, `stable` and `deprecated` release statuses.

## 5. Compatibility

All additions to `pack analyze` are additive JSON fields. Existing consumers
that read the prior `summary`, `targets`, `cases` and `messages` fields remain
compatible.

Changing a released pack's scenario content changes its fingerprint and
requires a semantic version or explicit reviewed beta refresh before SaaS
deployment.

## 6. Verification

Core verification:

- `TEST-AUTHORING-001`;
- `TEST-AUTHORING-CLI-001`;
- `TEST-VERIFICATION-CATALOG-001`;
- `TEST-PACK-METADATA-EXPORT-001`.

Manual release-set render verification:

```text
signal-synth pack challenge <pack.json> --out <tmp-dir>
```

was run for all ten release-set packs.

SaaS verification in this environment:

- import script produced all ten `packs/*.json` and `packs/*.product` files;
- each imported scenario path was checked against the sibling core checkout;
- full SaaS CMake build could not run on this host because CMake 3.16+ and
  `jansson.h` are unavailable.

## 7. Non-goals

This increment does not add SaaS-side scoring execution, SaaS job queue
changes, or clinical validation claims. It only makes the curated beta pack
release set importable and displayable by the SaaS catalog.
