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

Contributors must record any additional implementation source, dataset, or
third-party dependency before adding code derived from it.

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
