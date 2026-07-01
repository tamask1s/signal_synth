# Portable Core Build and Package Baseline

**Document ID:** SYN-ARCH-INC-001

**Version:** 0.1

**Status:** Implementing

**Owner role:** Software Architecture / Engineering

**Date:** 2026-07-01

**Proposed traceability ID:** `TRC-BUILD-001`

**Implementation issue:** [signal_synth#13](https://github.com/tamask1s/signal_synth/issues/13)

## 1. Decision

The next implementation increment shall establish one root-level, installable
C++11 library build and make every current automated suite test that library
instead of compiling private copies of source files into each test executable.

This increment shall not change signal-generation behavior, scenario
semantics, public C++ declarations, file locations used by DataBrowser, or the
five existing verification suite identities.

## 2. Rationale and priority

The current repository has a strong deterministic ECG implementation but no
product-grade build boundary:

- the only CMake entry point is `teszt/CMakeLists.txt`;
- each test executable compiles its own subset of production `.cpp` files;
- no reusable library target is built or installed;
- no root build contract exists for a future CLI, API adapter, export layer,
  or PPG module;
- public headers currently live in `src/` because of the existing
  DataBrowser/SVN integration.

Adding scenario JSON, a CLI, exports, reports, PPG, or further morphology on
top of this arrangement would create multiple ad hoc production link paths.
That would weaken the evidence that tests exercise the same binary boundary
that downstream applications use.

This increment therefore implements the first prerequisite identified by the
repository structure, implementation roadmap, repository review, and core
library architecture. It deliberately precedes both additional ECG condition
families and PPG.

## 3. Applicable requirements

| Requirement | Contribution of this increment |
|---|---|
| `REQ-GEN-004` | Establishes a UI-independent library target |
| `REQ-NFR-002` | Makes all current suites exercise the production library |
| `REQ-NFR-003` | Defines the core build boundary used by later adapters |
| `REQ-NFR-008` | Provides a standard root build and install contract |
| `REQ-DOC-001` | Defines issue, implementation, and verification linkage |
| `REQ-DOC-002` | Extends CI evidence to the root build and package boundary |

This increment does not claim completion of the scenario, export, report,
PPG, SaaS, or clinical-realism requirements.

## 4. Assumptions

- C++11 remains the minimum language standard.
- The current public headers and namespaces remain source-compatible.
- Static linking is sufficient for the current DataBrowser, test, and first
  CLI use cases.
- CMake 3.10 remains the minimum supported build-system version unless the
  implementation issue documents and approves a change.
- The current five suites are the accepted behavioral regression baseline.
- No production source requires an external runtime or link dependency.

## 5. Design inputs

- `03_SYSTEM_ARCHITECTURE.md`, especially the UI-independent Core Signal
  Library boundary.
- `04_REPOSITORY_STRUCTURE.md`, especially the minimum near-term repository
  target and root CMake recommendation.
- `05_CORE_LIBRARY_ARCHITECTURE.md`, especially stable public API and future
  adapter requirements.
- `11_VERIFICATION_AND_VALIDATION_PLAN.md`, especially integration,
  cross-platform, and CI verification.
- `15_IMPLEMENTATION_ROADMAP.md`, Milestone 0 and the dependency order leading
  to CLI, exports, reports, and PPG.
- `17_TRACEABILITY_SOP.md`, which controls the implementation issue, commit,
  test, CI, and matrix records.

## 6. Scope

### 6.1 Root build contract

Add a root `CMakeLists.txt` with these initial options:

```cmake
SIGNAL_SYNTH_BUILD_TESTS
SIGNAL_SYNTH_INSTALL
```

Do not add CLI or example options until those targets exist. Every declared
option must control implemented behavior.

The documented verification command becomes:

```sh
cmake -H. -Bbuild -DSIGNAL_SYNTH_BUILD_TESTS=ON
cmake --build build
cd build
ctest --output-on-failure
```

This syntax is intentional because the current documented minimum is CMake
3.10. Commands introduced by newer CMake releases may be used in CI, but shall
not be the only documented consumer path while 3.10 remains supported.

### 6.2 Production library target

Create one static C++11 production target named `signal_synth` and one
namespaced alias named `signal_synth::signal_synth`. The target contains the
current implementation units:

```text
src/signal_synth.cpp
src/ecg_model.cpp
src/clinical_ecg.cpp
src/ecg_morphology.cpp
src/ecg_scenario.cpp
```

Its public header set is:

```text
src/signal_synth.h
src/ecg_model.h
src/clinical_ecg.h
src/ecg_morphology.h
src/ecg_scenario.h
```

The source tree remains the authoritative header location in this increment.
CMake may install these files under `include/signal_synth/`, but shall not
create checked-in duplicate forwarding headers.

The target shall:

- require C++11 without compiler extensions;
- expose only the required build and install include paths;
- preserve current warning settings for repository-owned code;
- have no UI, SaaS, DataBrowser, ZAX, RSPT, network, or platform-specific
  dependency;
- build as a static library; shared-library behavior is outside this
  increment.

Shared-library ABI stability is not claimed by this increment.

### 6.3 Install and CMake package contract

Install:

- the static library;
- the explicit public header set under `include/signal_synth/`;
- a CMake export and package configuration that exposes only
  `signal_synth::signal_synth`.

A downstream CMake project shall be able to use:

```cmake
find_package(signal_synth CONFIG REQUIRED)
target_link_libraries(consumer PRIVATE signal_synth::signal_synth)
```

The installed target shall carry its C++ standard and include-directory usage
requirements. Consumers shall not need repository-relative paths or a list of
production source files.

Package semantic-version compatibility is deferred until the runtime
generator-version policy is designed. This increment shall not invent a
version claim that generated outputs cannot report.

### 6.4 Test linkage

Each existing test executable shall link `signal_synth`. It shall no longer
compile production `.cpp` files directly.

The following stable suite IDs remain unchanged:

- `TEST-SYNTH-001`;
- `TEST-ECG-MODEL-001`;
- `TEST-ECG-PHANTOM-001`;
- `TEST-ECG-MORPH-001`;
- `TEST-ECG-SCENARIO-001`.

This is a verification-strengthening change: missing implementation units,
duplicate symbols, unresolved public dependencies, and accidental
test-specific source selection become visible at the product library boundary.

### 6.5 Install smoke procedure

Add a stable build/package verification procedure `TEST-BUILD-001`.

It shall verify at minimum:

- root configure succeeds;
- the `signal_synth` library builds;
- all five existing suites pass while linked to that library;
- installation to a temporary prefix succeeds;
- every declared public header and the built library are present in the
  install tree;
- a minimal external C++11 consumer can discover the package with
  `find_package(signal_synth CONFIG REQUIRED)`, include the installed headers,
  link `signal_synth::signal_synth`, construct representative public objects,
  and run.

The smoke consumer shall use the installed package only. It shall not include
files from the source tree accidentally.

The exact implementation may be a CMake-driven test fixture or a small
separate consumer project invoked by CI. Its result must be visible in the
preserved CI log.

### 6.6 CI procedure

Update `CI-VER-001` to configure from the repository root on Linux and
Windows. Preserve CTest logs and include `TEST-BUILD-001` in the recorded
result.

No CI job may silently fall back to `teszt/CMakeLists.txt`.

## 7. Compatibility and migration

### 7.1 Source compatibility

No public class, struct, enum, function, default value, or namespace changes
are allowed in this increment.

### 7.2 Generated-output compatibility

No generator algorithm or configuration code is changed. Existing deterministic
tests and golden profiles must remain unchanged. Scenario fingerprints,
sample arrays, annotations, and phenotype assertions must remain identical for
the existing test inputs.

### 7.3 Repository paths

`src/` and `teszt/` remain in place for this increment. Renaming `teszt` or
moving public headers is deferred until all external integration and package
consumers have a controlled migration path.

The old command `cmake -S teszt ...` may remain temporarily functional, but it
is no longer the controlled CI procedure. It must either delegate to the same
library target or be documented as a compatibility path. It must not maintain
a second production source list.

### 7.4 DataBrowser and SVN

DataBrowser continues to compile the same source/header paths copied into its
SVN tree. Because this increment changes build metadata only, no
`SignalProc_RSPT.cpp` API function or visualization script is required.

If implementation discovers that a production source/header change is
unavoidable, that change becomes a separate reviewed scope item and the
affected files must be synchronized to SVN and manually identified as
DataBrowser integration evidence.

## 8. Explicit non-goals

- No source or header directory migration.
- No `teszt` to `tests` rename.
- No JSON parser or scenario serialization.
- No CLI executable.
- No CSV, JSON, WFDB, EDF, HTML, or PDF export.
- No PPG model.
- No new ECG condition, morphology, noise, or artifact.
- No SignalProc/DataBrowser API or script change.
- No SaaS, Python binding, C ABI, or shared-library ABI guarantee.
- No clinical, MDR, or tool-qualification claim.

## 9. Planned implementation files

Expected new or modified files:

```text
CMakeLists.txt
cmake/SignalSynthInstall.cmake
cmake/signal_synth-config.cmake.in
teszt/CMakeLists.txt
teszt/package_smoke/CMakeLists.txt
teszt/package_smoke/main.cpp
.github/workflows/verification.yml
README.md
doc/synsigra_architecture_docs/18_TRACEABILITY_MATRIX.md
```

The exact helper filename may change during implementation, but package and
test ownership must remain separate from generator algorithms.

## 10. Verification and acceptance criteria

The implementation issue shall use these measurable acceptance criteria:

1. Root configure, build, CTest, and install pass with a C++11 compiler.
2. Linux and Windows `CI-VER-001` jobs pass for the exact implementation
   commit.
3. All five existing `TEST-*` suites link the `signal_synth` target and pass.
4. `TEST-BUILD-001` discovers, builds, and runs an external consumer against
   the installed package through `signal_synth::signal_synth`.
5. No test executable compiles a production `.cpp` file directly.
6. Existing deterministic golden files and scenario fingerprints require no
   update.
7. Public C++ declarations and existing source/header paths are unchanged.
8. The traceability matrix links `TRC-BUILD-001` to the accepted issue,
   implementation commit, test procedure, and exact CI result.
9. The issue records that DataBrowser was unaffected or identifies any
   separately approved manual integration check.

## 11. Risks and controls

| Risk | Control |
|---|---|
| Tests pass against a different source set than consumers | One production library target and external install smoke test |
| Root and legacy builds diverge | One authoritative target; compatibility build delegates to it |
| Install tree exposes private implementation | Explicit public header list |
| Windows link behavior differs from Linux | Required two-platform CI |
| Build reorganization changes numerics | No algorithm edits; unchanged golden and fingerprint checks |
| Header move breaks DataBrowser | Defer physical move; preserve current paths |
| Build options exist without behavior | Define only options implemented in this increment |

## 12. Residual limitations

- Passing the package smoke test does not establish long-term ABI stability.
- Linux and Windows CI do not cover all compilers or floating-point
  environments.
- Actions artifacts have finite retention.
- DataBrowser remains a separately built SVN application with manual
  integration evidence.
- The package does not yet offer scenario JSON, CLI, exports, reports, or PPG.

## 13. Open issues requiring design acceptance

1. Confirm `signal_synth` as the installed CMake target and package name.
2. Decide whether the legacy `cmake -Hteszt` command remains supported for
   one transition increment or is removed immediately.
3. Decide whether shared builds are explicitly disabled until export macros
   and ABI policy are designed.

Recommended answers:

1. Use `signal_synth` consistently for repository, target, and installed
   package.
2. Keep the legacy command for one transition increment, backed by the same
   target.
3. Support static builds only in this increment and design shared-library ABI
   separately.

## 14. Following increments

After `TRC-BUILD-001` is verified:

1. Define the versioned scenario document, canonical JSON representation, and
   portable JSON dependency policy.
2. Add `signal-synth validate` and `signal-synth fingerprint`.
3. Add render orchestration, CSV/JSON export package, and controlled HTML
   report.
4. Add first-class PPG from the shared cardiac timeline.
5. Resume additional ECG condition families only when they support a named
   scenario pack or customer QA requirement.

This order converts the existing generator into an auditable product boundary
before increasing model breadth.

## 15. Change log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-07-01 | Initial proposed architecture increment |
