# DataBrowser Generation-Only Synchronization

**Document ID:** SYN-ARCH-INC-050

**Version:** 0.2

**Status:** Implemented, local verification complete

**Owner role:** Core / DataBrowser Integration

**Date:** 2026-07-17

**Traceability:** `TRC-DATABROWSER-001`

**Issue:** [signal_synth#72](https://github.com/tamask1s/signal_synth/issues/72)

## 1. Decision

DataBrowser remains a visualization adapter over the portable C++ generator.
It shall not compile the distribution, native export, challenge assembly,
comparison, or scoring stack.

Scenario rendering is therefore separated from `ecg_export`:

```text
scenario JSON
    -> ecg_render (resolve, ECG, PPG, artifacts, physiology, HRV)
        -> DataBrowser variable and marker adapter
        -> ecg_export (WFDB, EDF/BDF, reports and package artifacts)
```

`ecg_render.h/.cpp` owns `ecg_render_bundle`, ground-truth summary metrics,
`ecg_document_render_result`, and `render_ecg_document`. `ecg_export.h` includes
this layer and preserves the existing `ecg_export_result` type plus render
overload, including their link-level symbols.

## 2. Generation Subset

The CodeBlocks project compiles only these signal_synth modules:

- legacy ECG model;
- clinical ECG and morphology;
- scenario engine and strict JSON codec;
- render orchestration;
- HRV metrics;
- PPG model;
- scenario stress/physiology coupling;
- signal-quality artifacts.

WFDB, EDF/BDF, challenge package/assembly, facade, detector comparison, pack
scoring, and report export are intentionally excluded.

The Git source of truth for the application-specific files is
`integrations/databrowser/SignalProc_RSPT.cpp` and `SignalProc_RSPT.cbp`.
Mapped portable sources remain owned by `src/`.

## 3. DataBrowser Output Contract

`GenerateECGScenarioJSON(output, scenario_json, annotation_output)` keeps its
signature and output modes:

1. markers;
2. annotation channels;
3. no annotations.

All modes begin with the twelve ECG leads. Every enabled PPG channel follows
the annotations and is enumerated from `ppg_record`, including its core-owned
name and unit. Configured schema-v4 output can therefore contain `ppg_green`,
`ppg_red`, and `ppg_infrared` without adapter-side naming.

Channel mode emits:

- P wave, QRS origin, J point, T wave and episode truth;
- pacing event code, positive for capture and negative for non-capture;
- separate repolarization severity, QT, QTc, ST-J, ST-slope and T-amplitude
  traces;
- acquisition artifact code;
- construction and measured fiducial channels for every PPG optical channel.

Dynamic repolarization values are displayed sample-and-hold until the next
same-kind beat annotation. Marker mode labels rhythm/repolarization episodes,
pacing events, dynamic values by kind, acquisition artifacts, and per-channel
PPG fiducials. Marker labels remain subject to the legacy fixed-label limit.

`GenerateSyntheticECGPPG` also enumerates generated optical channels. Its ZAX
adapter exposes optional red and infrared channel configuration while keeping
the tuple size below the known GCC 4.9 template-depth failure regime.

## 4. Visualization Scripts

- `077_ECG_Morphology_Population.txt` compares three replayable morphology
  draws.
- `078_ECG_Dynamic_Repolarization.txt` shows ischemic ST-T and long-QT episodes
  together with dynamic truth channels.
- `079_PPG_Multichannel_Optical.txt` shows green/red/infrared PPG and their
  channel-specific fiducials.

Each script saves `Var` output before `DisplayData`, displays with `A2`, and
saves the UI image only after display creation.

## 5. Synchronization Contract

`integrations/databrowser/sync_files.txt` is the declarative Git-to-DataBrowser
map. `sync_manifest.sha256` pins the current Git bytes. The read-only
`tools/check_databrowser_sync.sh` fails for:

- a changed Git source with a stale manifest;
- a missing mapped file;
- any Git/DataBrowser SHA-256 mismatch.

`tools/update_databrowser_sync_manifest.sh` intentionally updates only the
manifest. Copying files into the SVN working copy remains an explicit action.

## 6. Verification

`TEST-DATABROWSER-GCC49-001`:

- builds the exact generation subset as an independent static library;
- enforces C++11 without compiler extensions and with pedantic errors;
- parses and renders all six scenarios embedded in scripts 077-079;
- verifies morphology randomization, dynamic repolarization truth, and all
  three optical PPG channels;
- checks the canonical adapter and CodeBlocks dependency boundary.

The current Linux environment provides GCC 7.5, not GCC 4.9. The test is a
GCC-4.9-oriented C++11 smoke and does not replace a 32-bit MinGW application
build. The full Windows UI and ZAX/RSPT integration remain manual evidence.

## 7. Residual Limitations

- PPG acquisition-artifact waveform transformation currently targets the
  legacy green signal-quality buffer. Red/infrared remain generated optical
  channels and are not falsely marked as artifact-corrupted.
- Dynamic DataBrowser channels are visualization traces, not a replacement for
  exported exact annotation JSON.
- SVN revision identity cannot be recorded because the local environment has
  no SVN command-line client.
- This integration supports engineering QA visualization and makes no clinical
  or diagnostic claim.

## 8. Acceptance State

- render and export ownership is separated;
- the DataBrowser project excludes distribution/scoring sources;
- modern ECG/PPG truth is exposed through the existing generic scenario API;
- scripts 077-079 are machine-validated;
- SHA-256 synchronization checking is deterministic;
- portable generation-subset compilation and execution pass.

Final acceptance still requires manual rebuild and visual inspection in the
32-bit Windows DataBrowser environment.

## 9. Change Log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-07-17 | Accepted generation-only synchronization design |
| 0.2 | 2026-07-17 | Implemented render split, adapter updates, scripts, manifest and smoke test |
