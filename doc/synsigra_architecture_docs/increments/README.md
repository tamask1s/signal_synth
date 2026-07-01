# Architecture Increment Records

Purpose: keep one reviewable design record for every implementation increment
without turning the system-level architecture documents into a chronological
change log.

Each increment record shall be created before implementation and contain:

- document ID, version, status, owner role, and date;
- decision and rationale;
- scope, assumptions, and explicit non-goals;
- applicable SRS requirements and architecture inputs;
- proposed traceability ID and implementation issue;
- public contracts, module ownership, and data flow;
- compatibility and migration rules;
- verification procedures, stable test IDs, and acceptance criteria;
- DataBrowser/SVN integration impact;
- risks, residual limitations, and open issues;
- implementation sequence and change log.

Lifecycle:

```text
Proposed -> Accepted -> Implementing -> Verified -> Superseded
```

An increment may enter `Implementing` only after its GitHub issue exists and
contains the records required by `../17_TRACEABILITY_SOP.md`. It may enter
`Verified` only after the exact implementation commit has passed the required
CI jobs and the traceability matrix has been updated.

These records describe engineering design and verification intent. They do
not establish clinical validity, MDR compliance, or a qualified test
environment.
