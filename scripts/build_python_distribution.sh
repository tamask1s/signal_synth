#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."

rm -rf dist build *.egg-info python/*.egg-info
python3 -m build --sdist --wheel

echo "Built Python distribution artifacts:"
ls -1 dist/
