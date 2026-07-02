# DataBrowser ZAX GCC 4.9 Compatibility

**Document ID:** SYN-ARCH-INC-014

**Version:** 0.1

**Status:** Verified adapter fix

**Owner role:** DataBrowser Integration / Build Compatibility

**Date:** 2026-07-02

## 1. Context

The Windows DataBrowser application is still built with 32-bit MinGW GCC 4.9.
The `SignalProc_RSPT.cpp` adapter used one large `ZAX_JSON_SERIALIZABLE` list
for `zax_clinical_ecg_config`. After the clinical ECG model grew, that list
expanded to more than 60 properties. GCC 4.9's libstdc++ tuple implementation
recursively instantiates `std::_Tuple_impl` through `type_traits`; on the
32-bit build this exceeded the default template depth of 900.

The portable `signal_synth` core was not the source of the error. The failure
was specific to the DataBrowser ZAX adapter.

## 2. Decision

Keep the public DataBrowser JSON contract flat and unchanged, but split the
adapter into smaller ZAX-serializable sections:

- enum selectors;
- timing;
- morphology;
- rhythm;
- scenario;
- lead transform/gains;
- source model.

`zax_clinical_ecg_config` remains the public adapter type used by
`GenerateClinicalECG12` and `GenerateClinicalECGPPG`. It parses every section
from the same flat JSON object, then applies the section values to the portable
`signal_synth::clinical_ecg_config`.

## 3. Compatibility Contract

Existing script calls remain valid. Keys such as `qrs_duration_ms`,
`heart_rate_bpm`, `lead_v1_gain`, and `sources` keep their previous names and
top-level placement.

The implementation also adds an explicit `<tuple>` include to
`SignalProc_RSPT.cpp`, because the ZAX parser depends on `std::tuple` and the
old build should not rely on indirect includes.

## 4. Verification

The full DataBrowser module cannot be built in this Linux workspace because the
application-only headers are not available here. The adapter-specific template
failure was verified with an isolated ZAX smoke compile that includes the
portable `clinical_ecg.h` types and the patched adapter structs:

- default local GCC C++11 compile: passed;
- local GCC C++11 compile with `-ftemplate-depth=300`: passed.

The old failing shape was a single tuple with roughly 64 properties. The
patched adapter keeps the largest generated ZAX tuple below that scale, so it
stays well under the MinGW GCC 4.9 default template depth of 900.

The patched `SignalProc_RSPT.cpp` was synchronized back to the DataBrowser/SVN
working copy and byte-compared with the prepared file.

## 5. Change Log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-07-02 | Split DataBrowser clinical ECG ZAX adapter for GCC 4.9 template-depth compatibility |
