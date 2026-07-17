# Realism Validation Framework

**Document ID:** SYN-INC-058

**Version:** 1.0

**Status:** Verified

**Owner role:** Engineering

**Date:** 2026-07-17

**Implementation issue:** [signal_synth#79](https://github.com/tamask1s/signal_synth/issues/79)

## 1. Decision

Every rendered case exposes deterministic engineering-characterization metrics,
and every rendered pack exposes their population aggregate. Metrics remain
separate by domain; the contract deliberately contains `single_score: null`.
The framework does not convert engineering checks into a clinical-realism or
clinical-validation claim.

## 2. Public Contracts

`realism_validation.h/.cpp` owns:

- per-case fidelity, diversity, interlead consistency, cross-signal, and
  downstream-utility metrics;
- optional external reference-cohort descriptors with version, source URI,
  license, content checksum, inclusion criteria, and exclusions;
- deterministic population aggregation;
- JSON, CSV, and human-readable HTML serialization.

The initial metrics cover finite samples, amplitude and selected spectral
components, Einthoven and Goldberger identities, beat morphology variation,
ECG-to-PPG delay variation, wearable clock/loss behavior, and artifact
coverage. Metric names, domains, units, and evaluability are explicit.

## 3. Package And Python API

Each case export contains:

```text
realism_metrics.json
realism_metrics.csv
realism_report.html
```

Pack render and challenge commands additionally write
`realism_population.json`. Challenge manifests use dedicated roles for all
four artifacts. The generator-free Python API exposes per-case metric/report
accessors and a package-level population accessor; it does not reimplement the
metric formulas.

## 4. Reference Policy

External cohorts are never implied. With no supplied cohort, the report says
that the metrics characterize generated signals only. A supplied cohort must
identify its source, license, immutable content checksum, inclusion criteria,
and exclusions. Raw third-party datasets are not bundled by this increment.

## 5. Determinism And Failure Semantics

Metric calculation has no random state. Population aggregation is stable by
metric name. A non-finite signal keeps its finite-fraction result but makes
derived metrics non-evaluable. A failed characterization aborts export rather
than producing a partial package.

## 6. Verification

`TEST-REALISM-VALIDATION-001` verifies clean-signal metrics, deliberate
interlead corruption detection, reference provenance, population aggregation,
claim boundaries, and challenge roles. Existing export, facade, CLI, curated
metadata, challenge-integrity, and installed-Python tests cover the expanded
artifact contract.

Verification completed on 2026-07-17: 54/54 repository CTest cases passed,
including the generation-only C++11 compatibility smoke and generator-free
Python challenge round trip.

## 7. DataBrowser Impact

None. The framework is reporting and package infrastructure, so it is not
copied into the generation-only SVN subset and requires no visualization
script.

## 8. Limitations

The metrics characterize known engineering properties; they do not establish
patient-population equivalence. Detector-specific outcomes can already be
reported through the scoring layer, while a future dashboard may join those
scores with this population summary without changing the case metric files.
