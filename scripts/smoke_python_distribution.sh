#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."

ROOT="$PWD"
PYTHON="${PYTHON:-python3}"
FIXTURE="$ROOT/python/tests/fixtures/distribution_smoke"
WHEELS=( "$ROOT"/dist/synsigra-*.whl )
if [[ ${#WHEELS[@]} -ne 1 || ! -f "${WHEELS[0]}" ]]; then
    echo "Expected exactly one built Synsigra wheel under dist/." >&2
    exit 2
fi

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

"$PYTHON" -m venv "$TMPDIR/venv"
# shellcheck disable=SC1091
source "$TMPDIR/venv/bin/activate"
python -m pip install --no-deps "${WHEELS[0]}" >/dev/null

cd "$TMPDIR"
synsigra-verify --help > help.txt
grep -q "challenge" help.txt
grep -q "submission_dir" help.txt
grep -q "output_dir" help.txt
grep -q -- "--profile" help.txt

python - "$FIXTURE/challenge" "$TMPDIR/challenge.zip" <<'PY'
import os
import sys
import zipfile

source, destination = sys.argv[1:]
with zipfile.ZipFile(destination, "w", zipfile.ZIP_DEFLATED) as archive:
    for root, directories, files in os.walk(source):
        directories.sort()
        for name in sorted(files):
            path = os.path.join(root, name)
            archive.write(path, os.path.relpath(path, source))
PY

synsigra-verify "$TMPDIR/challenge.zip" "$FIXTURE/submissions/pass" "$TMPDIR/pass-results" --mode diagnostic --profile regression
test -f "$TMPDIR/pass-results/evidence.json"
test -f "$TMPDIR/pass-results/index.html"
test -f "$TMPDIR/pass-results/details/clean.html"
test "$(find "$TMPDIR/pass-results" -type f | wc -l)" -eq 3
python - "$TMPDIR/pass-results/evidence.json" <<'PY'
import json
import sys

with open(sys.argv[1], "r") as stream:
    evidence = json.load(stream)
assert evidence["success"] is True
assert evidence["policy"]["passed"] is True
PY

if synsigra-verify "$TMPDIR/challenge.zip" "$FIXTURE/submissions/fail" "$TMPDIR/fail-results" --mode diagnostic --profile regression; then
    echo "Expected failing submission to return a non-zero exit code." >&2
    exit 1
fi
test -f "$TMPDIR/fail-results/evidence.json"
test -f "$TMPDIR/fail-results/index.html"
python - "$TMPDIR/fail-results/evidence.json" <<'PY'
import json
import sys

with open(sys.argv[1], "r") as stream:
    evidence = json.load(stream)
assert evidence["success"] is False
assert evidence["policy"]["passed"] is False
assert evidence["policy"]["failed_check_count"] > 0
PY

python - <<'PY'
import importlib.metadata
import synsigra
assert "site-packages" in synsigra.__file__, synsigra.__file__
assert importlib.metadata.version("synsigra") == "0.11.0"
assert callable(synsigra.load_measurements)
assert callable(synsigra.score_measurements)
PY

echo "Python distribution smoke test passed"
