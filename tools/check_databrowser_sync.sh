#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
databrowser_root=${1:-"$repo_root/../PS/Development/Projects/DataBrowser_psaa"}
manifest="$repo_root/integrations/databrowser/sync_manifest.sha256"
failed=0
checked=0

if [ ! -f "$manifest" ]; then
    printf 'Missing sync manifest: %s\n' "$manifest" >&2
    exit 1
fi

while IFS='|' read -r expected source target
do
    case "$expected" in
        ''|'#'*) continue ;;
    esac
    checked=$((checked + 1))
    source_path="$repo_root/$source"
    target_path="$databrowser_root/$target"
    if [ ! -f "$source_path" ]; then
        printf 'MISSING Git: %s\n' "$source"
        failed=1
        continue
    fi
    source_hash=$(sha256sum "$source_path" | cut -d ' ' -f 1)
    if [ "$source_hash" != "$expected" ]; then
        printf 'STALE manifest: %s\n' "$source"
        failed=1
    fi
    if [ ! -f "$target_path" ]; then
        printf 'MISSING DataBrowser: %s\n' "$target"
        failed=1
        continue
    fi
    target_hash=$(sha256sum "$target_path" | cut -d ' ' -f 1)
    if [ "$source_hash" != "$target_hash" ]; then
        printf 'DIFF: %s -> %s\n' "$source" "$target"
        failed=1
    fi
done < "$manifest"

if [ "$failed" -ne 0 ]; then
    printf 'DataBrowser sync check failed (%s mapped files).\n' "$checked" >&2
    exit 1
fi
printf 'DataBrowser sync check passed (%s mapped files, SHA-256 identical).\n' "$checked"
