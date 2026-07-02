# Clinical ECG engine specification

## Record model

`clinical_ecg_generator::generate` creates one `clinical_ecg_record` containing
12 equal-length ECG leads, seven three-axis cardiac source components, their
summed VCG, atrial events, ventricular beat annotations, and fiducials. Lead
order is I, II, III, aVR, aVL, aVF, V1-V6. Signal units are mV and event times
are seconds from the first sample.

Generation is transactional: invalid configuration, zero sample count, or an
unconfigured generator leaves the destination record unchanged. Allocation
failure or non-finite projected output is also reported as failure without
modifying the destination. For a fixed configuration, seed, build, and
floating-point environment, complete records are reproducible.

## Timeline and scenarios

The atrial and ventricular timelines are generated separately. Conducted
atrial events and ventricular beats contain reciprocal indexes. A value of
`-1` means that no event is linked.

Supported rhythm timelines are sinus rhythm, atrial fibrillation, atrial
flutter with a fixed conduction ratio, supraventricular tachycardia,
ventricular tachycardia, and ventricular pacing. The sinus timeline can add
periodic PAC, PVC, junctional escape, ventricular escape or paced beats,
compensatory pauses, and sinus pauses. Seeded RR perturbations are deterministic
normal variates clipped to the configured `minimum_rr_seconds` and
`maximum_rr_seconds`; `rr_was_clipped` records clipping. The same limits apply
after scenario overrides and to the AF ventricular timeline.

First-degree block, Mobitz I, Mobitz II, and complete AV block are supported on
the sinus/atrial timeline. Non-sinus rhythm plus an AV-block mode, and
non-sinus rhythm plus a periodic sinus scenario, are rejected instead of being
silently ignored.

Typical atrial flutter uses adjacent asymmetric F waves spanning the complete
atrial cycle. The slow deflection occupies 73% of the cycle, the return 27%,
and adjacent offset/onset times coincide. The default source polarity produces
continuous predominantly negative flutter activity in the inferior leads,
without a finite isoelectric interval.

## Timing

PR is measured from P onset to QRS onset. Q, R, and S construction times are
fractions of QRS duration. J point and QRS offset are currently identical.
T offset is QRS onset plus QT.

QT may be fixed or derived from QTc:

- Bazett: `QT = QTc * sqrt(RR)`
- Fridericia: `QT = QTc * cbrt(RR)`
- Framingham: `QT = QTc + 0.154 * (RR - 1)`
- Hodges: `QT = QTc - 0.00175 * (HR - 60)`

The configured QRS and T durations impose a lower bound on QT so the generated
fiducials remain ordered.

## Multi-source vector model and leads

Model v2 separates atrial activation, septal activation, main ventricular
activation, terminal ventricular activation, repolarization, injury current,
and pacing into seven smooth 3D source vectors. Every source has an independent
gain, axis offset, and elevation offset. Yaw, pitch, and roll then rotate every
source into the shared body frame.

`clinical_ecg_record::source_data(source, axis)` exposes each rotated source
component and `vcg_data(axis)` exposes their exact sample-wise sum. The 12
leads are projected exclusively from this summed VCG, so source decomposition,
VCG, and lead output remain auditable parts of one generated record.

A zero atrial or repolarization source gain makes the corresponding P or T
construction annotations explicitly absent and suppresses its lead-measured
fiducials. A nonzero source can still project weakly or cancel in an individual
lead; construction ground truth describes the global generated source, while
lead-measured fiducials describe visibility in that lead.

I and II are independent frontal projections. At unit lead gains:

- `III = II - I`
- `aVR = -(I + II) / 2`
- `aVL = I - II / 2`
- `aVF = II - I / 2`

V1-V6 use fixed 3D lead-field vectors. Per-lead gain is applied after
projection; deliberately changing a derived limb-lead gain therefore models a
calibration difference and no longer preserves the ideal algebraic identity.
The chest-lead model is not a geometric torso or electrode simulation.

Complete RBBB delays terminal activation toward a positive V1 terminal force
and negative I/V6 terminal forces while retaining right-precordial secondary
T-wave discordance. Complete LBBB suppresses the lateral initial q, keeps
delayed activation negative in V1 and positive in I/V5/V6, and reverses the
ordinary repolarization vector to produce secondary QRS-T discordance.

## Fiducials

Construction fiducials have `lead_index == -1` and record the exact configured
P onset/peak/offset, QRS onset, Q/R/S peaks, J point, QRS offset, T
onset/peak/offset, and pacing spike. `atrial_index` keeps non-conducted P waves
identifiable even when no ventricular beat exists.

Lead-measured P/Q/R/S/T peaks are found independently on each lead. In the
construction-defined non-overlapping search window, the algorithm removes the
line between the window endpoints and selects the sample with the largest
absolute residual. `present` is true when that residual reaches
`presence_threshold_mv`. This is deterministic signal measurement, not a
clinical delineator.

Events outside the sampled record remain absent from the fiducial list or have
`present == false`; sample indexes never exceed the signal buffer.

## Measured lead morphology

`ecg_morphology` produces one `ecg_lead_morphology` entry per ventricular beat
and lead. It measures baseline-relative P/Q/R/S/T amplitudes, 5%-boundary P/T
durations, Q duration, QRS peak-to-peak and RMS voltage, and ST values at J and
J+60 ms. Regions group limb, precordial, inferior, anterior, septal, and
lateral leads without modifying the generated signal.

Construction timing remains in `clinical_beat_annotation`; morphology entries
are measurements of the projected lead samples. Keeping these layers separate
allows a scenario report to detect when a requested source phenotype did not
survive projection.

## Intended use and limitations

The engine provides known-by-construction test inputs and annotations for
detector, storage, visualization, and processing validation. Current limitations
include no population-fitted parameter distribution, no torso volume conductor,
no mechanistic ischemia/infarct electrophysiology, no respiratory axis motion, no
electromechanical coupling, and no calibrated acquisition artifact layer. The
seven components are engineering source abstractions, not anatomically resolved
dipoles. These limitations preclude clinical-representation claims.
