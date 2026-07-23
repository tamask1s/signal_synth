#!/bin/sh
set -eu

# One deterministic path for core changes that affect curated packs. The quick
# mode is the default: CI owns the full matrix, while local work runs only the
# contract/catalog/verifier tests affected by a curated release. Logs stay
# silent on success and are shown only on failure.
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=${SIGNAL_SYNTH_BUILD_DIR:-"$repo_dir/build"}
jobs=${BUILD_JOBS:-1}
mode=${1:---quick}
log=$(mktemp "${TMPDIR:-/tmp}/synsigra-core-refresh.XXXXXX")
trap 'rm -f "$log"' EXIT HUP INT TERM

case "$jobs" in
  ''|*[!0-9]*|0)
    echo "BUILD_JOBS must be a positive integer" >&2
    exit 2
    ;;
esac
case "$mode" in
  --quick|--full) ;;
  *) echo "usage: $0 [--quick|--full]" >&2; exit 2 ;;
esac

failed() {
  status=$1
  echo "core refresh failed; last log lines:" >&2
  tail -80 "$log" >&2
  exit "$status"
}

run() {
  "$@" >>"$log" 2>&1 || failed $?
}

run cmake -S "$repo_dir" -B "$build_dir" \
  -DCMAKE_BUILD_TYPE=Release \
  -DSIGNAL_SYNTH_BUILD_TESTS=ON \
  -DSIGNAL_SYNTH_BUILD_CLI=ON

# Export only with the binary built from this exact checkout.
run cmake --build "$build_dir" --target signal_synth_cli -j"$jobs"
run python3 "$repo_dir/scripts/export_curated_pack_metadata.py" \
  --catalog "$repo_dir/examples/catalog/verification_packs_v1.json" \
  --cli "$build_dir/signal-synth" \
  --source-root "$repo_dir" \
  --out "$repo_dir/examples/catalog/curated_pack_metadata_v1.json"

run cmake --build "$build_dir" -j"$jobs"
(cd "$build_dir" && ctest -N) >>"$log" 2>&1
grep -Eq 'Total Tests: [1-9][0-9]*' "$log" || {
  echo "CTest did not discover any tests in $build_dir" >&2
  exit 1
}
if [ "$mode" = --quick ]; then
  tests='TEST-(FACADE|ECG-EXPORT|CLI|INTEGRATION-CONTRACT|VERIFICATION-CATALOG|PACK-METADATA-EXPORT|PYTHON-SCORING|RR-QTC-PACK|RPEAK-EVIDENCE-PACK|PROTOCOL-V2)-001'
  (cd "$build_dir" && ctest -R "$tests" --output-on-failure -j"$jobs") >>"$log" 2>&1 || failed $?
  test_scope=targeted
else
  (cd "$build_dir" && ctest --output-on-failure -j"$jobs") >>"$log" 2>&1 || failed $?
  test_scope=full
fi
run git -C "$repo_dir" diff --check
printf 'core_refresh=ok mode=%s tests=%s jobs=%s\n' "$mode" "$test_scope" "$jobs"
