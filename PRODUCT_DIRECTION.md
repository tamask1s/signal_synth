# Product direction

## Positioning

The product is a B2B SaaS and developer-tool platform for synthetic biosignal
generation and algorithm QA. ECG is first, PPG/pulse waveform follows, and
optional analog USB output may be added later.

The product is a synthetic ECG/PPG ground-truth testbench for HRV, peak
detection, arrhythmia, signal-quality, wearable, storage, visualization, and
processing validation. It is not a medical diagnostic product, a clinical
validation service, a generic synthetic cardiology data platform, or an ECG
data store.

The product value is:

- versioned scenario definitions;
- explicit construction and measured ground truth;
- deterministic reproduction from seed and model version;
- curated stress-test packs;
- local library and service API;
- machine-readable and human-readable QA reports;
- complete configuration, model, and data provenance.

## Architecture

The generation pipeline remains layered:

1. scenario and condition contract;
2. physiological timeline and clean latent sources;
3. clean projected biosignal and exact annotations;
4. correlated patient, record, and beat variation;
5. acquisition, noise, and artifact layer with masks;
6. optional learned realism residual constrained by ground truth;
7. exporters, API, reports, and optional hardware output.

Every stage must preserve the clean signal and its annotations. A noisy output
must never replace or obscure its clean reference in a validation package.

## Delivery sequence

The first package is the versioned ECG scenario contract and complete
71-statement catalog. Unsupported conditions fail explicitly.

The next package replaces the single cardiac vector with a multi-source model,
then implements the condition families in dependency order: rhythm and
conduction, morphology, hypertrophy, infarction/injury, and ischemia/ST-T.

Population randomization follows clean condition support. Long-duration
regimes and episode transitions follow the population model. Baseline wander,
noise, electrode faults, motion artifacts, clipping, quantization, and lead
errors are added only as a separate acquisition layer.

Learned refinement, text-to-scenario, and inpainting are later optional
services. They may improve realism but must not create unaudited ground truth.

## Release gates

Each scenario family requires deterministic unit tests, measurable phenotype
assertions, conflict tests, stable schema/fingerprint tests, and regression
packs. Commercial releases additionally require provenance, dataset/license,
privacy, subgroup, long-duration stability, and downstream algorithm-utility
reports.
