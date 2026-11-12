#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${FORMATION_BUILD_DIR:-$SCRIPT_DIR/build-ubuntu}"
INSTALL_DIR="${FORMATION_INSTALL_DIR:-$SCRIPT_DIR/stage-install}"
BUILD_JOBS="${FORMATION_BUILD_JOBS:-$(nproc)}"

CMAKE_POLICY_ARGS=()
CMAKE_MAJOR="$(cmake --version | awk 'NR == 1 { split($3, version, "."); print version[1] }')"
if [[ "$CMAKE_MAJOR" -ge 4 ]]; then
  CMAKE_POLICY_ARGS=(-DCMAKE_POLICY_VERSION_MINIMUM=3.5)
fi

cmake -S "$SCRIPT_DIR/Stage" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_PLAYER_PLUGIN=OFF \
  -DCPACK_CFG=OFF \
  -DBUILD_FORMATION_TESTS=ON \
  -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
  "${CMAKE_POLICY_ARGS[@]}"

cmake --build "$BUILD_DIR" -j"$BUILD_JOBS"
ctest --test-dir "$BUILD_DIR" --output-on-failure
cmake --install "$BUILD_DIR"
