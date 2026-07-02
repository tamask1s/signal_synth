# Way of Working

This repository uses lightweight traceability for implementation work. The
detailed procedure is `doc/synsigra_architecture_docs/17_TRACEABILITY_SOP.md`.
This file is the short checklist to check before and after every change.

## Required Flow

1. Create or identify the GitHub issue before implementation.
2. Link the relevant traceability ID, architecture increment, requirements,
   acceptance criteria, and known limitations in the issue or linked document.
3. Keep implementation scoped to the issue. If the work changes shape, update
   the issue and architecture record before claiming it is verified.
4. Every traceable commit must include the full GitHub issue link on the first
   commit-message line, so GitHub commit summaries show a directly clickable
   link. Use this form:

   ```text
   type(scope): concise change summary [issue https://github.com/tamask1s/signal_synth/issues/123]
   ```

5. Use one primary issue link on the first line. Additional issue lines are
   allowed in the body only when one commit genuinely belongs to multiple
   approved issues.
6. Do not rely on short forms such as `#123`; cross-repository and exported
   evidence should remain unambiguous.
7. Run the relevant local tests before commit. For broad changes, run release
   CTest and sanitizer CTest unless a documented environment limitation applies.
8. Synchronize the DataBrowser/SVN working copy whenever shared source files or
   DataBrowser scripts are changed, and record manual evidence separately from
   automated CI evidence.
9. Push only after the local working tree is understood. If history was
   rewritten, force-push intentionally and update issue comments if old commit
   hashes were previously recorded.
10. Close the issue only after acceptance criteria, local verification, required
    CI jobs, traceability matrix updates, and residual limitations are recorded.

## Commit Message Template

```text
type(scope): concise change summary [issue https://github.com/tamask1s/signal_synth/issues/123]
```

Additional explanatory paragraphs may be added after the first line when they
help future audit or review. The first-line issue link is mandatory for
implementation, verification, documentation, synchronization, and follow-up fix
commits that belong to a traceable work item.
