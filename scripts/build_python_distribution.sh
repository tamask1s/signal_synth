#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."

PYTHON="${PYTHON:-python3}"

rm -rf dist build *.egg-info python/*.egg-info
"$PYTHON" -m build --sdist --wheel

echo "Built Python distribution artifacts:"
ls -1 dist/
