# SaaS Curated Pack Metadata

**Document ID:** SYN-INC-043

**Version:** 1.0

**Status:** Implemented

**Owner role:** Engineering

**Date:** 2026-07-06

**Implementation issue:** [signal_synth#68](https://github.com/tamask1s/signal_synth/issues/68)

## 1. Decision

Export a SaaS-ingestable curated-pack metadata document from the core
repository instead of duplicating pack knowledge inside the SaaS codebase.

The release/build-time exporter is:

```text
python3 scripts/export_curated_pack_metadata.py --cli <signal-synth> --out examples/catalog/curated_pack_metadata_v1.json
```

The output is a stable JSON contract with:

- pack identity, release status, changelog, deprecation message;
- declared targets and effective case-level targets;
- scoreable target contracts and reference-only target contracts;
- supported detector/user-output schemas;
- supported and recommended threshold profiles;
- case IDs, durations, sampling rates, sample counts, channel counts;
- approximate package size and peak memory estimate;
- recommended-for and not-recommended-for text;
- generator, challenge-package, and scoring-manifest compatibility fields.

## 2. Contract Shape

Top-level metadata:

```text
schema_version: 1
metadata_type: synsigra_curated_pack_catalog
metadata_version: synsigra_curated_pack_metadata_export_v1
packs: [...]
```

Per-pack metadata:

```text
metadata_type: synsigra_curated_pack_metadata
pack_id, version, name, description
release_status, release_date, changelog, deprecation_message
declared_targets
targets
catalog_scoring_mode
scoring_mode
scoreable_targets
reference_only_targets
detector_output_schemas
threshold_profile_contract
cases
estimated_package
generator_compatibility
```

`declared_targets` comes from the pack manifest. `targets` is the effective
union of case-level targets after pack analysis. This distinction is important
because a pack may primarily be sold as an R-peak pack while still emitting
reference-only signal-quality ground truth for one case.

`scoring_mode` is computed from the effective target contracts:

- `local`: scoreable targets only;
- `reference_only`: reference artifacts only;
- `mixed`: both scoreable and reference-only targets.

The original catalog value remains available as `catalog_scoring_mode`.

## 3. Initial Beta Pack

Issue #68 first completed this contract for `r_peak_stress_v1`. Issue #70
extends the same contract to the full beta release set in
`044_SAAS_RELEASE_SET_EXPORT.md`.

`r_peak_stress_v1` has complete product-facing metadata:

- release status: `beta`;
- release date: `2026-07-06`;
- scoreable target: `r_peak`;
- reference-only target: `signal_quality` for `baseline_powerline`;
- detector schemas: `detection_json_v1`, `detection_csv_v2`;
- profiles: `smoke`, `regression`, `stress`, `benchmark`;
- exact case IDs and generated size estimates;
- recommended and not-recommended use text;
- generator/scoring/challenge compatibility declarations.

## 4. Update Process

For intentional curated-pack metadata changes:

1. Update `examples/catalog/verification_packs_v1.json` and any pack/scenario
   definitions.
2. Regenerate:

```text
python3 scripts/export_curated_pack_metadata.py --cli <signal-synth> --out examples/catalog/curated_pack_metadata_v1.json
```

3. Run:

```text
python3 teszt/pack_metadata_export_test.py
```

with `SIGNAL_SYNTH_CLI` and `SIGNAL_SYNTH_SOURCE_DIR` set.

4. Review changed release fields, scoreable/reference-only target contracts,
   size estimates, and pack fingerprints before commit.

## 5. SaaS Boundary

The SaaS should ingest `curated_pack_metadata_v1.json` as product metadata.
It should not hard-code scoreable target lists, detector schemas, profile
support, or size estimates from C++ internals.

The generator remains authoritative for validation and package rendering.
The metadata export is a release artifact, not a clinical claim.

## 6. DataBrowser And SVN Impact

None. No DataBrowser API or script is added.

## 7. Verification

`TEST-PACK-METADATA-EXPORT-001` regenerates the committed metadata from:

- the curated catalog;
- core pack JSON files;
- `signal-synth pack validate`;
- `signal-synth pack analyze`.

The test fails if the committed SaaS metadata snapshot drifts from the current
core definitions. It checks the complete beta release-set metadata contract
added after issue #68.
