#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
CODE_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
RUNNER="$SCRIPT_DIR/Stage/worlds/experiment/run_fixed_density.py"

UV_BIN="${FORMATION_UV_BIN:-uv}"
STAGE_BIN="${FORMATION_STAGE_BIN:-$SCRIPT_DIR/stage-install/bin/stage}"
RESULT_ROOT="${FORMATION_RESULT_ROOT:-$SCRIPT_DIR/Stage/formation/results/fixed_density_v1}"
WORKERS="${FORMATION_WORKERS:-4}"
WALL_TIMEOUT="${FORMATION_WALL_TIMEOUT:-1200}"
SMOKE_WALL_TIMEOUT="${FORMATION_SMOKE_WALL_TIMEOUT:-120}"
UV_CACHE_DIR_VALUE="${FORMATION_UV_CACHE_DIR:-/tmp/ssci2027-uv-cache}"
MPLCONFIGDIR_VALUE="${FORMATION_MPLCONFIGDIR:-/tmp/ssci2027-matplotlib}"
XDG_CACHE_HOME_VALUE="${FORMATION_XDG_CACHE_HOME:-/tmp/ssci2027-xdg-cache}"

usage() {
  cat <<'EOF'
Usage:
  ./stage4/run-formation-experiments.sh prepare [runner options]
  ./stage4/run-formation-experiments.sh smoke [image_id]
  ./stage4/run-formation-experiments.sh run [runner options]
  ./stage4/run-formation-experiments.sh retry-wall-timeouts
  ./stage4/run-formation-experiments.sh analyze
  ./stage4/run-formation-experiments.sh report
  ./stage4/run-formation-experiments.sh test

Commands:
  prepare  Validate the manifest and generate all batch worlds without Stage.
  smoke    Run one image at N=50 and N=500; image_id defaults to spotify.
  run      Run or resume all 750 experiments, then write aggregate and summary CSVs.
  retry-wall-timeouts
           Retry only wall_timeout results with one worker and no outer wall timeout.
           The 7200-second simulation timeout remains active. Previous JSON/log
           artifacts are moved under retry_archive/ before each retry.
  analyze  Rebuild summary_by_n.csv from the existing per-run JSON files.
  report   Validate the 750-run design, generate publication statistics and
           LaTeX fragments, and export three result figures to the paper fig/ directory.
  test     Run the fixed-density Python unit tests.

The Stage runner always supplies -g, which disables the GUI. Valid completed
JSON files are resumed rather than repeated.

Environment overrides:
  FORMATION_STAGE_BIN       Stage executable
  FORMATION_RESULT_ROOT     output directory
  FORMATION_WORKERS         parallel Stage processes (default: 4)
  FORMATION_WALL_TIMEOUT    timeout per run in wall-clock seconds (default: 1200)
  FORMATION_SMOKE_WALL_TIMEOUT  smoke timeout per run (default: 120)
  FORMATION_UV_BIN          uv executable (default: uv)
  FORMATION_UV_CACHE_DIR    uv cache directory
  FORMATION_MPLCONFIGDIR    matplotlib cache directory
  FORMATION_XDG_CACHE_HOME  fontconfig cache directory used by plotting
EOF
}

require_executable() {
  local executable="$1"
  local label="$2"
  if [[ "$executable" == */* ]]; then
    if [[ ! -x "$executable" ]]; then
      printf '%s is not executable: %s\n' "$label" "$executable" >&2
      return 1
    fi
  elif ! command -v "$executable" >/dev/null 2>&1; then
    printf '%s was not found on PATH: %s\n' "$label" "$executable" >&2
    return 1
  fi
}

validate_positive_number() {
  local value="$1"
  local label="$2"
  if ! [[ "$value" =~ ^[0-9]+([.][0-9]+)?$ ]] || [[ "$value" == "0" ]] || [[ "$value" == "0.0" ]]; then
    printf '%s must be a positive number: %s\n' "$label" "$value" >&2
    return 1
  fi
}

run_uv() {
  UV_CACHE_DIR="$UV_CACHE_DIR_VALUE" MPLCONFIGDIR="$MPLCONFIGDIR_VALUE" \
    XDG_CACHE_HOME="$XDG_CACHE_HOME_VALUE" \
    "$UV_BIN" run python "$RUNNER" "$@"
}

run_batch() {
  run_uv run \
    --stage "$STAGE_BIN" \
    --output-root "$RESULT_ROOT" \
    --workers "$WORKERS" \
    --wall-timeout "$WALL_TIMEOUT" \
    "$@"
}

write_summary() {
  run_uv analyze \
    --input "$RESULT_ROOT" \
    --output "$RESULT_ROOT/summary_by_n.csv"
}

ACTION="${1:-help}"
if [[ $# -gt 0 ]]; then
  shift
fi

require_executable "$UV_BIN" "uv"

case "$ACTION" in
  prepare)
    require_executable "$STAGE_BIN" "Stage"
    validate_positive_number "$WORKERS" "FORMATION_WORKERS"
    validate_positive_number "$WALL_TIMEOUT" "FORMATION_WALL_TIMEOUT"
    run_batch --prepare-only "$@"
    ;;
  smoke)
    require_executable "$STAGE_BIN" "Stage"
    validate_positive_number "$SMOKE_WALL_TIMEOUT" "FORMATION_SMOKE_WALL_TIMEOUT"
    IMAGE_ID="${1:-spotify}"
    if [[ $# -gt 1 ]]; then
      printf 'smoke accepts at most one image_id\n' >&2
      exit 2
    fi
    run_uv run --stage "$STAGE_BIN" --output-root "$RESULT_ROOT" \
      --workers 1 --wall-timeout "$SMOKE_WALL_TIMEOUT" --only-n 50 --image-id "$IMAGE_ID"
    run_uv run --stage "$STAGE_BIN" --output-root "$RESULT_ROOT" \
      --workers 1 --wall-timeout "$SMOKE_WALL_TIMEOUT" --only-n 500 --image-id "$IMAGE_ID"
    write_summary
    ;;
  run)
    require_executable "$STAGE_BIN" "Stage"
    validate_positive_number "$WORKERS" "FORMATION_WORKERS"
    validate_positive_number "$WALL_TIMEOUT" "FORMATION_WALL_TIMEOUT"
    run_batch "$@"
    write_summary
    ;;
  retry-wall-timeouts)
    require_executable "$STAGE_BIN" "Stage"
    if [[ $# -ne 0 ]]; then
      printf 'retry-wall-timeouts does not accept positional arguments\n' >&2
      exit 2
    fi
    if [[ ! -d "$RESULT_ROOT/runs" ]]; then
      printf 'result runs directory does not exist: %s\n' "$RESULT_ROOT/runs" >&2
      exit 1
    fi
    run_uv run \
      --stage "$STAGE_BIN" \
      --output-root "$RESULT_ROOT" \
      --workers 1 \
      --no-wall-timeout \
      --retry-status wall_timeout
    write_summary
    ;;
  analyze)
    if [[ $# -ne 0 ]]; then
      printf 'analyze does not accept positional arguments\n' >&2
      exit 2
    fi
    write_summary
    ;;
  report)
    if [[ $# -ne 0 ]]; then
      printf 'report does not accept positional arguments\n' >&2
      exit 2
    fi
    run_uv report \
      --input "$RESULT_ROOT" \
      --output-dir "$RESULT_ROOT/publication"
    ;;
  test)
    if [[ $# -ne 0 ]]; then
      printf 'test does not accept positional arguments\n' >&2
      exit 2
    fi
    cd "$CODE_ROOT"
    UV_CACHE_DIR="$UV_CACHE_DIR_VALUE" MPLCONFIGDIR="$MPLCONFIGDIR_VALUE" \
      XDG_CACHE_HOME="$XDG_CACHE_HOME_VALUE" \
      "$UV_BIN" run python -m unittest discover \
      -s stage4/Stage/worlds/experiment/tests -v
    ;;
  help|-h|--help)
    usage
    ;;
  *)
    printf 'Unknown command: %s\n\n' "$ACTION" >&2
    usage >&2
    exit 2
    ;;
esac
