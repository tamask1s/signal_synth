# Hardware Roadmap

Version: 0.1  
Status: Draft  
Scope: Optional USB analog output kit for later product phases.

## 1. Hardware position

Hardware should not be the first product.

It should become an optional premium extension after software demand is validated.

Correct positioning:

> USB-controlled analog ECG/PPG-like signal output for engineering/R&D algorithm and acquisition-chain testing.

Avoid:

- certified patient simulator;
- diagnostic simulator;
- clinical ECG simulator;
- medical-device conformity-assessment tool;
- patient-connected device.

## 2. Roadmap

### Phase 0: Internal breadboard/prototype

- prove DAC output;
- compare digital vs analog capture;
- no sale;
- no CE process yet if not placed on market.

### Phase 1: Engineering prototype

- 1–3 devices;
- used for demos;
- basic enclosure;
- USB-powered;
- single-channel analog output.

### Phase 2: Design partner dev kit

- 5–20 devices;
- engineering evaluation agreement;
- limited support;
- collect requirements.

### Phase 3: CE-marked small series

- final PCB/enclosure;
- EMC testing;
- RoHS documentation;
- technical file;
- EU Declaration of Conformity;
- user manual;
- serial-number traceability.

### Phase 4: Enterprise hardware bundle

- calibrated analog output;
- calibration report;
- hardware-in-loop scenario packs;
- on-prem integration.

## 3. Recommended v1 hardware constraints

Keep compliance simple:

- USB-powered only;
- no Bluetooth/Wi-Fi;
- no battery;
- no mains adapter;
- no patient connection;
- no electrodes applied to humans;
- low-voltage analog outputs;
- current-limited outputs;
- ESD protection;
- stable reference output;
- serial number and firmware version.

## 4. Output channels

V1:

- one ECG-like analog output;
- ground/reference.

V2:

- ECG + PPG analog outputs;
- trigger/sync output;
- marker output.

V3:

- multi-lead ECG phantom output;
- calibration mode;
- hardware self-test.

## 5. Firmware

Firmware responsibilities:

- receive waveform chunks or scenario-generated samples;
- output calibrated samples;
- maintain timing;
- expose firmware version;
- expose device serial number;
- support calibration mode.

Avoid putting complex scenario generation in firmware initially. Keep generation in host software.

## 6. Calibration metadata

Each device should eventually have:

- serial number;
- DAC resolution;
- output gain;
- offset;
- measured noise floor;
- linearity summary;
- sample rate tolerance;
- calibration date;
- calibration procedure version.

## 7. Compliance notes

Likely non-medical USB device requirements:

- EMC;
- RoHS;
- WEEE obligations if sold in EU;
- low-voltage directive likely not applicable to USB 5V-only device, but general product safety still matters;
- RED only if wireless is added.

The legal manufacturer remains responsible even if EMS and labs are outsourced.

## 8. Technical file contents

For non-medical CE route, maintain:

- intended use;
- block diagram;
- schematic;
- PCB layout;
- BOM;
- RoHS declarations;
- firmware version;
- risk assessment;
- EMC test report;
- user manual;
- label;
- EU Declaration of Conformity;
- production test procedure;
- serial traceability;
- complaint/field issue process.

## 9. Architecture

```text
Synsigra Software
  -> render scenario
  -> analog output stream
  -> USB protocol
  -> device buffer
  -> DAC
  -> analog conditioning
  -> output connector
```

## 10. USB protocol

Initial simple protocol:

- identify device;
- set sample rate;
- start stream;
- send sample block;
- stop stream;
- query status;
- calibration mode.

Use framing and checksums.

## 11. Hardware risks

| Risk | Mitigation |
|---|---|
| Hardware drains cash | Do after paid software pilots |
| EMC redesign needed | Do pre-compliance early |
| Medical-device implication | Strong R&D-only intended use |
| Customer expects Fluke-grade simulator | Position as algorithm/dev kit |
| Analog output inaccurate | Calibration and report limits |
| Support burden | Small design-partner batch first |

## 12. Decision

Hardware is strategically useful, but only after:

- SaaS/core value is validated;
- at least several customers ask for analog output;
- the software scenario/report workflow is stable.
