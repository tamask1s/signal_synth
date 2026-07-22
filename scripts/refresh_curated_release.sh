#!/bin/sh
set -eu

# One deterministic path for core changes that affect curated packs. Building
# the CLI before exporting prevents stale-schema failures. One build job is the
# safe default for the small VPS; BUILD_JOBS may be raised on larger machines.
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=${SIGNAL_SYNTH_BUILD_DIR:-"$repo_dir/build"}
jobs=${BUILD_JOBS:-1}

case "$jobs" in
  ''|*[!0-9]*|0)
    echo "BUILD_JOBS must be a positive integer" >&2
    exit 2
    ;;
esac

cmake -S "$repo_dir" -B "$build_dir" \
  -DCMAKE_BUILD_TYPE=Release \
  -DSIGNAL_SYNTH_BUILD_TESTS=ON \
  -DSIGNAL_SYNTH_BUILD_CLI=ON

# Export only with the binary built from this exact checkout.
cmake --build "$build_dir" --target signal_synth_cli -j"$jobs"
python3 "$repo_dir/scripts/export_curated_pack_metadata.py" \
  --catalog "$repo_dir/examples/catalog/verification_packs_v1.json" \
  --cli "$build_dir/signal-synth" \
  --source-root "$repo_dir" \
  --out "$repo_dir/examples/catalog/curated_pack_metadata_v1.json"

cmake --build "$build_dir" -j"$jobs"
(cd "$build_dir" && ctest -N) | grep -Eq 'Total Tests: [1-9][0-9]*' || {
  echo "CTest did not discover any tests in $build_dir" >&2
  exit 1
}
(cd "$build_dir" && ctest --output-on-failure -j"$jobs")
git -C "$repo_dir" diff --check
