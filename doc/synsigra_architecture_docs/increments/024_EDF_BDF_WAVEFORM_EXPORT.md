# EDF+/BDF+ Waveform Export

**Document ID:** SYN-ARCH-INC-024

**Version:** 0.1

**Status:** Implementing

**Owner role:** Platform / SDK

**Date:** 2026-07-04

**Traceability ID:** `TRC-FMT-EDF-001`

**Implementation issue:** [signal_synth#37](https://github.com/tamask1s/signal_synth/issues/37)

## 1. Decision

Add deterministic EDF+ and BDF+ waveform export artifacts to every render
bundle.

The implementation writes:

- `synsigra.edf`: EDF+ compatible waveform file;
- `synsigra.bdf`: BDF+ compatible higher-resolution waveform file;
- `edf_bdf_metadata.json`: Synsigra sidecar linking the standard files to
  scenario identity, render identity, channel scaling, and full ground truth.

The native EDF+/BDF+ annotation signal carries compact event markers for
`r_peak` and `ppg_systolic_peak`. The complete construction and measured
ground truth remains in `annotations.json`.

## 2. Applicable Requirements

- `REQ-FMT-003`;
- `REQ-FMT-004`;
- `REQ-FMT-005..009`;
- `REQ-GTFMT-001..005`.

## 3. Format Contract

Record name:

- default exported record name: `synsigra`;
- direct C++ API record names are sanitized to ASCII letters, digits, `_`, and
  `-`;
- empty sanitized names fall back to `synsigra`.

Signals:

- channel order is ECG lead order `I, II, III, aVR, aVL, aVF, V1..V6`, then
  optional `ppg_green`, then the annotation signal;
- EDF+ uses 16-bit little-endian samples;
- BDF+ uses 24-bit little-endian samples;
- EDF ECG samples use `1000 ADC / mV`;
- EDF PPG samples use `10000 ADC / normalized unit`;
- BDF ECG samples use `100000 ADC / mV`;
- BDF PPG samples use `1000000 ADC / normalized unit`;
- start date/time is deterministic and does not encode wall-clock time.

Annotation strategy:

- EDF annotation signal label: `EDF Annotations`;
- BDF annotation signal label: `BDF Annotations`;
- native annotation labels in this increment: `record_start`, `r_peak`,
  `ppg_systolic_peak`;
- `edf_bdf_metadata.json` declares that full ground truth remains in
  `annotations.json`.

## 4. API and Layering

Add `ecg_edf_bdf_export.h/.cpp`:

```cpp
bool build_edf_bdf_export_bundle(const ecg_render_bundle& render, const std::string& record_name, edf_bdf_export_bundle& output, ecg_export_result& result);
```

`build_ecg_export_bundle` calls this writer and appends EDF/BDF artifacts to
the existing render package. The CLI, facade, pack render, and Python challenge
workflow therefore keep using the C++ export layer as the deterministic source
of truth.

No DataBrowser/SVN API is added in this increment.

## 5. Verification

Add `TEST-EDF-BDF-EXPORT-001`:

- direct EDF/BDF writer API;
- record-name sanitization;
- EDF+ and BDF+ fixed-header fields;
- header byte count and signal count;
- annotation signal labels;
- first-sample scaling for ECG and PPG in EDF and BDF;
- native annotation labels for R peaks and PPG peaks;
- sidecar metadata contract;
- transactional failure behavior.

Extend existing tests:

- `TEST-ECG-EXPORT-001`: render bundles include EDF/BDF artifacts in
  deterministic order;
- `TEST-FACADE-001`: facade render artifacts expose EDF/BDF files;
- `TEST-BUILD-001`: installed public headers include
  `ecg_edf_bdf_export.h`;
- `TEST-CLI-001`: CLI render path writes EDF/BDF files.

The local environment does not currently provide EDFBrowser or another native
EDF/BDF command-line reader, so external-tool read verification remains a
portable CI/environment follow-up.

## 6. Non-Goals

- No DICOM export.
- No full-fidelity EDF/BDF encoding of all Synsigra annotations.
- No DataBrowser visualization API.
- No hosted SaaS service.

## 7. Risks and Limitations

- Native annotation support is intentionally compact in this increment.
- The files are deterministic engineering exchange artifacts, not clinical
  evidence records.
- External reader conformance should be added once a stable EDF/BDF validation
  tool is available in CI.

## 8. Change Log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-07-04 | Added EDF+/BDF+ waveform export design |
