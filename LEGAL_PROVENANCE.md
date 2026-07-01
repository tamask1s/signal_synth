# Legal and scientific provenance

This repository contains an independent implementation of an ECG synthesis
model. It is intended to remain suitable for proprietary commercial
distribution.

## Clean-room boundary

The phase-domain model in `src/ecg_model.cpp` was implemented from the
mathematical description in:

P. E. McSharry, G. D. Clifford, L. Tarassenko, and L. A. Smith,
"A Dynamical Model for Generating Synthetic Electrocardiogram Signals,"
IEEE Transactions on Biomedical Engineering, 50(3), 289-294, 2003.
DOI: 10.1109/TBME.2003.808805.

The ECGSYN software distributed by PhysioNet is licensed under GPL-3.0. Its
source code is outside this project's implementation input: it must not be
copied, translated, adapted, or used as a source-level reference for this
repository. The scientific paper and its equations may be used as the
published model specification.

The clinical timeline, conduction, morphology, fiducial, 3D vector, and
12-lead projection code in `src/clinical_ecg.cpp` is an independent
implementation based on general electrophysiology definitions and equations
recorded in `CLINICAL_ECG_SPECIFICATION.md`. It does not include code, model
weights, parameter tables, or data copied from another ECG generator.

The SCP-ECG condition catalog in `src/ecg_scenario.cpp` uses statement codes,
ordering, categories, and descriptions from PTB-XL version 1.0.3
`scp_statements.csv`, distributed under CC BY 4.0. Attribution is recorded in
`ECG_SCENARIO_SPECIFICATION.md`. No PTB-XL waveform is bundled in this
repository.

Contributors must record any additional implementation source, dataset, or
third-party dependency before adding code derived from it.

The schema-v1 JSON parser, canonical writer, and SHA-256 implementation in
`ecg_scenario_json.cpp` are project-owned implementations written from public
JSON and SHA-256 algorithm specifications. No third-party JSON or
cryptographic source code is embedded.

The PPG waveform and linked-fiducial implementation in `ppg_model.cpp` is a
project-owned engineering model based on elementary smooth pulse components.
It contains no copied waveform, fitted patient data, third-party generator
code, or model weights.

## Commercial release gate

Before commercial release, obtain a jurisdiction-specific freedom-to-operate
and license review. This repository does not itself establish patent
clearance, regulatory suitability, or a right to make clinical-performance
claims.

Synthetic annotations are ground truth by construction for the configured
model events. They are not clinical truth and do not establish that a signal
is physiologically representative of a patient population.

## References

- Paper: https://doi.org/10.1109/TBME.2003.808805
- ECGSYN license record: https://physionet.org/content/ecgsyn/1.0.0/
- EU software copyright directive:
  https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX:32009L0024
- WIPO freedom-to-operate overview:
  https://www.wipo.int/en/web/wipo-magazine/articles/ip-and-business-launching-a-new-product-freedom-to-operate-34956

This document records engineering provenance and is not legal advice.
