#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

cmake --build build-macos --target stagebinary formation target_qualification -j4

# CMake 4 can treat already-patched macOS Mach-O files as "up-to-date" and
# then apply the same RPATH edits a second time. Refresh the build artifacts so
# installation always copies a clean binary before running install_name_tool.
cmake -E touch \
  build-macos/libstage/stage \
  build-macos/libstage/libstage.4.3.0.dylib \
  build-macos/formation/target_qualification \
  build-macos/formation/formation.so

cmake --install build-macos
