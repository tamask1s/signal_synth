# WFDB Waveform Export

**Document ID:** SYN-ARCH-INC-023

**Version:** 0.1

**Status:** Implementing

**Owner role:** Platform / SDK

**Date:** 2026-07-04

**Traceability ID:** `TRC-FMT-WFDB-001`

**Implementation issue:** [signal_synth#36](https://github.com/tamask1s/signal_synth/issues/36)

## 1. Decision

Add deterministic WFDB-format export artifacts to every render bundle.

The first implementation writes:

- `synsigra.hea`: WFDB header;
- `synsigra.dat`: multiplexed binary format-16 signal file;
- `synsigra.atr`: native WFDB-style R-peak annotation file;
- `wfdb_metadata.json`: Synsigra sidecar linking WFDB files to scenario,
  render identity, channel scaling, and full ground truth.

The full Synsigra ground truth remains `annotations.json`; WFDB native
annotations are intentionally limited to R peaks in this increment because P,
QRS boundary, T, PPG, artifact, and phenotype assertion metadata are richer
than the portable WFDB annotation subset.

## 2. Applicable Requirements

- `REQ-FMT-001`;
- `REQ-FMT-002`;
- `REQ-FMT-005..009`;
- `REQ-GTFMT-001..005`.

## 3. Format Contract

WFDB record name:

- default exported record name: `synsigra`;
- direct C++ API record names are sanitized to ASCII letters, digits, `_`, and
  `-`;
- empty sanitized names fall back to `synsigra`.

Signal file:

- WFDB format `16`;
- little-endian signed 16-bit multiplexed samples;
- ECG channels use `1000 ADC / mV`;
- PPG green uses `10000 ADC / normalized unit`;
- ADC zero is `0`;
- header checksums use the WFDB 16-bit checksum convention;
- channel order is ECG lead order `I, II, III, aVR, aVL, aVF, V1..V6`, then
  optional `ppg_green`.

Annotation file:

- native WFDB-style annotation file name: `synsigra.atr`;
- exported native annotation target: R peak;
- full construction and measured ground truth remains in `annotations.json`;
- `wfdb_metadata.json` declares this split explicitly.

## 4. API and Layering

Add `ecg_wfdb_export.h/.cpp`:

```cpp
bool build_wfdb_export_bundle(const ecg_render_bundle& render, const std::string& record_name, wfdb_export_bundle& output, ecg_export_result& result);
```

`build_ecg_export_bundle` calls this writer and appends WFDB artifacts to the
existing render package. This keeps the CLI, facade, pack render, and Python
challenge workflows on the same C++ source of truth.

No DataBrowser/SVN API is added in this increment.

## 5. Verification

Add `TEST-WFDB-EXPORT-001`:

- direct WFDB writer API;
- record-name sanitization;
- header record line, identity comments, channel gains, units, and PPG channel;
- binary signal size;
- first-sample scaling for ECG and PPG;
- annotation EOF marker;
- sidecar metadata contract;
- transactional failure behavior.

Extend existing tests:

- `TEST-ECG-EXPORT-001`: render bundles include WFDB artifacts in deterministic
  order;
- `TEST-FACADE-001`: facade render artifacts expose WFDB header;
- `TEST-BUILD-001`: installed public headers include `ecg_wfdb_export.h`;
- `TEST-CLI-001`: CLI render path writes the larger artifact bundle.

The local environment does not currently provide WFDB command-line tooling
such as `rdsamp` or `rdann`, so external-tool read verification remains a
portable CI/environment gap for a later conformance job.

## 6. Non-Goals

- No EDF+/BDF+ writer; that is issue #37.
- No DICOM export.
- No full-fidelity WFDB encoding of all Synsigra annotations.
- No DataBrowser visualization API.
- No hosted SaaS service.

## 7. Risks and Limitations

- WFDB native annotation support is intentionally narrow in this increment.
- Very large inter-annotation gaps are encoded with simple WFDB interval spacer
  words; broad compatibility should still be checked with native WFDB tooling
  once available in CI.
- Binary payloads are carried in the existing artifact content string. This is
  compatible with current CLI writing but the artifact type name remains
  historical.

## 8. Change Log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-07-04 | Added WFDB waveform export design |
