# DataBrowser Fixed-Label Safety Correction

**Document ID:** SYN-ARCH-INC-010

**Version:** 0.2

**Status:** Implemented; Windows application verification pending

**Owner role:** DataBrowser Integration / Verification

**Date:** 2026-07-01

**Traceability ID:** `TRC-API-002`

**Implementation issue:** [signal_synth#22](https://github.com/tamask1s/signal_synth/issues/22)

## 1. Decision

Keep the legacy DataBrowser fixed-size string ABI unchanged and make the
`SignalProc_RSPT.cpp` adapter bound every string before copying it into that
ABI.

The adapter shall use one compile-time-capacity-aware helper for fixed
`char[N]` arrays. Oversize text is deterministically truncated to `N-1`
characters and always null terminated.

## 2. Crash analysis

The reported trap occurs in `CVector<TString40>::~CVector` during `delete[]`.
That location is the heap-corruption detector, not the original invalid write.

The legacy structures define:

- `TString40::s` as `char[40]`;
- `TMarker::m_label` as `char[32]`;
- `ISignalCodec::m_varname` as `char[200]`.

`create_ecg_assertion_variable` prepends `PASS ` or `FAIL ` to report names and
then uses `strcpy` into `TString40::s`. Confirmed examples include:

- `PASS ISCAL: second negative T lead group`: 40 characters before the null;
- `PASS ANEUR: persistent anterior ST-J elevation proxy`: 52 characters
  before the null.

Both exceed the 39-character payload. The write can corrupt the next label or
the allocation footer; destruction then traps in the observed stack frame.

The marker-list ABI also performs an internal `strcpy` into 32 bytes. The
adapter must therefore pass an already bounded temporary marker label.

## 3. Requirements

- `REQ-API-001..003`;
- `REQ-NFR-003`, `REQ-NFR-008`;
- `REQ-VER-001`, `REQ-VER-008..010`;
- `REQ-DOC-001..002`.

## 4. Scope

In `SignalProc_RSPT.cpp`:

- add `copy_fixed_string(char (&destination)[N], const char* source)`;
- replace all adapter `strcpy` calls targeting fixed DataBrowser arrays;
- bound labels before calling `CMarkerList::AddTMarker`;
- preserve existing exported procedures and parameter order;
- preserve DataBrowser structure sizes and serialization layout.

The correction covers old and new generator functions, not only
`GenerateECGQAScenario`, because the same unsafe pattern appears in source,
annotation, PPG, filter, and variable-name paths.

## 5. Truncation contract

For a destination `char[N]`:

1. null source produces an empty string;
2. at most `N-1` source bytes are copied;
3. `destination[copied_length]` is set to null;
4. no suffix marker is appended because it would reduce the already limited
   label payload and could hide first/second assertion distinctions.

Truncation affects display text only. Assertion values, pass/fail state,
condition ownership, channels, markers, and generated signal samples remain
unchanged.

## 6. Verification

Because the Windows DataBrowser target is not portable to this environment:

- audit `SignalProc_RSPT.cpp` for remaining `strcpy` into fixed ABI fields;
- compile and execute the helper in a standalone guard-buffer harness with
  null, short, exact-payload, exact-capacity, and oversize inputs;
- verify the two known long assertion labels truncate and terminate without
  touching adjacent guard bytes;
- rebuild/run the portable signal_synth release and sanitizer suites;
- retain GitHub Ubuntu/Windows core CI as non-DataBrowser regression evidence;
- require manual Windows DataBrowser rerun of scripts 071/072 for final
  application evidence.

Automated GitHub CI does not compile `SignalProc_RSPT.cpp`; this limitation
must remain explicit.

## 7. DataBrowser and SVN integration

The implementation lives in the SVN-managed
`DataBrowser_psaa/src/SignalProcessors/SignalProc_RSPT.cpp`. No signal_synth
core or script API change is required.

## 8. Risks and controls

| Risk | Control |
|---|---|
| Long text corrupts heap | Compile-time destination capacity and `N-1` copy limit |
| Exact-capacity input lacks null | Explicit terminator write |
| Marker list still overflows internally | Pass a bounded 32-byte temporary |
| Truncation makes assertions ambiguous | Preserve the beginning containing condition and first/second qualifier |
| ABI fix breaks serialized data | Do not change legacy struct definitions |
| Portable CI is represented as app evidence | Record DataBrowser execution as manual only |

## 9. Non-goals

- Expanding `TString40`, `TMarker`, or serialized DataBrowser structures.
- Redesigning channel metadata or assertion naming.
- Replacing all unsafe copies throughout the full DataBrowser application.
- Claiming that portable CI compiles the Windows plugin.

## 10. Acceptance criteria

1. Every fixed-array copy in `SignalProc_RSPT.cpp` is bounded and terminated.
2. Known ST-T assertion labels cannot overwrite adjacent labels.
3. Marker labels cannot exceed the legacy 31-character payload.
4. Exported API and ABI remain unchanged.
5. Standalone guard-buffer verification passes.
6. Portable regressions remain green.
7. The SVN working copy contains the reviewed correction.

## 11. Implementation sequence

1. Record the defect and fixed-ABI constraints.
2. Add the bounded helper and replace unsafe adapter copies.
3. Run static audit and standalone boundary harness.
4. Run portable regressions and synchronize evidence.
5. Commit documentation and close the issue after required checks.

## 12. Verification record

- confirmed `TString40::s` capacity: 40 bytes including the null terminator;
- confirmed `TMarker::m_label` capacity: 32 bytes including the null
  terminator;
- reproduced boundary violations with the 40-character `ISCAL` and
  52-character `ANEUR` assertion display labels;
- standalone fixed-string guard-buffer tests passed under ASan/UBSan for
  null, short, 39-character, 40-character, 52-character, and marker-label
  inputs;
- static audit confirms no remaining `strcpy` call in `SignalProc_RSPT.cpp`;
- the reviewed file was copied into the SVN-managed DataBrowser working copy
  and verified byte-identical to the tested temporary copy.

The portable signal_synth release and sanitizer suites were green immediately
before this adapter-only correction. They do not compile the Windows adapter.
The issue shall remain open until a rebuilt Windows DataBrowser reruns scripts
071/072 without the reported destruction-time trap.

## 13. Change log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-07-01 | Initial crash analysis and correction design |
| 0.2 | 2026-07-01 | Implemented bounded adapter copies and recorded guard-buffer/static-audit evidence |
