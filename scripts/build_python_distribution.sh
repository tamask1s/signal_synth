#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."

PYTHON="${PYTHON:-python3}"
BUILD_ARGUMENTS=(--sdist --wheel)
if [[ "${SYNSIGRA_BUILD_ISOLATION:-0}" != "1" ]]; then
    BUILD_ARGUMENTS+=(--no-isolation)
fi

rm -rf dist build *.egg-info python/*.egg-info
"$PYTHON" -m build "${BUILD_ARGUMENTS[@]}"

echo "Built Python distribution artifacts:"
ls -1 dist/
