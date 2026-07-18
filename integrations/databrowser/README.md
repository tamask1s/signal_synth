# DataBrowser Integration

This directory is the Git source of truth for the Windows DataBrowser adapter
and CodeBlocks project. The adapter consumes only the portable generation and
render subset. WFDB, EDF/BDF, challenge assembly, comparison, and scoring stay
outside the SVN application.

The synchronized file map is `sync_files.txt`. After changing a mapped source:

1. synchronize it to the DataBrowser working copy;
2. regenerate `sync_manifest.sha256` with
   `tools/update_databrowser_sync_manifest.sh`;
3. run `tools/check_databrowser_sync.sh`.

The checker is read-only. It fails when the manifest is stale, a mapped file is
missing, or the Git and DataBrowser copies differ. The default DataBrowser root
is `../PS/Development/Projects/DataBrowser_psaa`; a different root may be passed
as the first argument.

The full 32-bit Windows application is not built by the portable CI. The
`TEST-DATABROWSER-GCC49-001` target separately compiles the exact generation
source subset as strict C++11 and renders every scenario embedded by scripts
077-083. Final UI inspection remains manual integration evidence.
