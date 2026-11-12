#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
CODE_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
RUNNER="$SCRIPT_DIR/Stage/worlds/experiment/run_large_cases.py"

UV_BIN="${FORMATION_UV_BIN:-uv}"
STAGE_BIN="${FORMATION_STAGE_BIN:-$SCRIPT_DIR/stage-install/bin/stage}"
OUTPUT_ROOT="${FORMATION_LARGE_RESULT_ROOT:-$SCRIPT_DIR/Stage/formation/results/large_cases_v1}"
WORKERS="${FORMATION_LARGE_WORKERS:-2}"
SIM_TIMEOUT="${FORMATION_LARGE_SIM_TIMEOUT:-7200}"
FORCE="${FORMATION_LARGE_FORCE:-0}"
SAVE_GENERATED_IMAGE="${FORMATION_LARGE_SAVE_GENERATED_IMAGE:-0}"
UV_CACHE_DIR_VALUE="${FORMATION_UV_CACHE_DIR:-/tmp/ssci2027-uv-cache}"

usage() {
  cat <<'EOF'
Usage:
  ./stage4/run-large-formation-cases.sh prepare [N:IMAGE[:ARENA_SIDE] ...]
  ./stage4/run-large-formation-cases.sh run     [N:IMAGE[:ARENA_SIDE] ...]
  ./stage4/run-large-formation-cases.sh gui     [N:IMAGE[:ARENA_SIDE]]

Defaults to two qualitative cases:
  500:uts1.png
  1000:uts2.png

The run action disables the GUI and runs up to two independent Stage processes.
The gui action runs one case interactively. Existing terminal result JSON files
are resumed unless FORMATION_LARGE_FORCE=1 is set.
EOF
}

ACTION="${1:-help}"
if [[ $# -gt 0 ]]; then
  shift
fi
case "$ACTION" in
  prepare|run|gui) ;;
  help|-h|--help)
    usage
    exit 0
    ;;
  *)
    printf 'Unknown action: %s\n\n' "$ACTION" >&2
    usage >&2
    exit 2
    ;;
esac

cd "$CODE_ROOT"
RUNNER_ARGS=(
  "$RUNNER"
  "$ACTION"
  --stage "$STAGE_BIN"
  --output-root "$OUTPUT_ROOT"
  --workers "$WORKERS"
  --sim-timeout "$SIM_TIMEOUT"
)
if [[ "$FORCE" == "1" ]]; then
  RUNNER_ARGS+=(--force)
fi
if [[ "$SAVE_GENERATED_IMAGE" == "1" ]]; then
  RUNNER_ARGS+=(--save-generated-image)
fi
for case_spec in "$@"; do
  RUNNER_ARGS+=(--case "$case_spec")
done
UV_CACHE_DIR="$UV_CACHE_DIR_VALUE" "$UV_BIN" run python "${RUNNER_ARGS[@]}"
