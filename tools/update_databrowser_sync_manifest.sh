#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
file_map="$repo_root/integrations/databrowser/sync_files.txt"
manifest="$repo_root/integrations/databrowser/sync_manifest.sha256"
temporary="$manifest.tmp"
trap 'rm -f "$temporary"' EXIT HUP INT TERM

printf '%s\n' '# SHA-256|Git source|DataBrowser target' > "$temporary"
while IFS='|' read -r source target
do
    case "$source" in
        ''|'#'*) continue ;;
    esac
    if [ ! -f "$repo_root/$source" ]; then
        printf 'Missing Git source: %s\n' "$source" >&2
        exit 1
    fi
    hash=$(sha256sum "$repo_root/$source" | cut -d ' ' -f 1)
    printf '%s|%s|%s\n' "$hash" "$source" "$target" >> "$temporary"
done < "$file_map"
mv "$temporary" "$manifest"
trap - EXIT HUP INT TERM
printf 'Updated %s\n' "$manifest"
