# Validation data policy

No third-party ECG recording is bundled with this library by default.
Validation tooling should download or import datasets separately and retain
their original license and attribution metadata.

Datasets used or currently considered for validation:

| Dataset | License | Intended use |
| --- | --- | --- |
| PTB-XL / PTB-XL+ | CC BY 4.0 | PTB-XL condition catalog metadata is used; waveforms remain external candidates for morphology and fiducial validation |
| MIT-BIH Arrhythmia Database | ODC Attribution 1.0 | Rhythm and detector validation |
| MIT-BIH Noise Stress Test Database | ODC Attribution 1.0 | Noise robustness |

Commercial use is possible under these licenses, subject to their attribution
and notice requirements. Raw records and annotations must not be copied into a
release artifact until the intended redistribution has received a separate
license review.

Dataset results must identify the dataset version, record selection, processing
steps, and license. Derived templates or statistics must remain traceable to
that record.

Sources:

- https://physionet.org/content/ptb-xl/
- https://physionet.org/content/ptb-xl-plus/
- https://physionet.org/content/mitdb/
- https://physionet.org/content/nstdb/
- https://creativecommons.org/licenses/by/4.0/
- https://opendatacommons.org/licenses/by/1-0/
