# License-Controlled Hybrid Noise

## Decision

Scenario schema v8 defines external ECG noise asset manifests and calibrated mixing intervals. Asset bytes are never embedded in a scenario or challenge package. Render callers provide the declared CSV assets through an explicit in-memory registry; the CLI is the only layer that maps an operator-selected asset directory to that registry.

## Contracts

- Every asset declares a stable id, source URI, license, SHA-256, sample rate, ordered channel names, and redistribution mode.
- `local_only` blocks challenge-package release. `rendered_output` permits derived waveform release but not source redistribution. `source_and_output` records that both are permitted, although the generator still does not package source bytes.
- Every interval selects one asset channel, one or more ECG leads, source offset, target SNR, taper, and optional symmetric clipping rail.
- Rendering verifies the raw asset checksum and CSV header, linearly resamples deterministically, removes source DC, applies a cosine taper, and scales per target lead from clean AC RMS.
- `external_noise_truth.json` reports target and achieved SNR, clean/noise RMS, clipping counts, provenance, license, and redistribution mode for every interval and lead.

## Boundaries

The physiological record and its fiducial annotations remain unchanged. The observable ECG is mixed after physiology coupling and analytic acquisition artifacts. A pre-mix ECG copy is retained only when external noise is configured and exported as explicit clean reference truth. Third-party noise assets are deployment inputs, not repository or release artifacts.

## SaaS Impact

The service must maintain an approved asset registry, pass exact asset bytes to the C++ facade, reject unavailable assets before queueing work, and enforce `release_allowed` before creating a downloadable challenge. Asset acceptance and license review are operator actions; scenario authors can only reference approved asset ids and checksums.
