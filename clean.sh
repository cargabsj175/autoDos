#!/usr/bin/env bash
# Clean AutoDOS build outputs on Linux/macOS.
# Usage:
#   ./clean.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "Cleaning AutoDOS build outputs..."

rm -rf \
    "${SCRIPT_DIR}/build" \
    "${SCRIPT_DIR}/dist" \
    "${SCRIPT_DIR}/linux/build" \
    "${SCRIPT_DIR}/linux/build-linux" \
    "${SCRIPT_DIR}/linux/dist" \
    "${SCRIPT_DIR}/macos/build" \
    "${SCRIPT_DIR}/macos/dist" \
    "${SCRIPT_DIR}/macos/staging" \
    "${SCRIPT_DIR}/macos/AutoDOS.iconset" \
    "${SCRIPT_DIR}/macos/AutoDOS.icns"

echo "Clean complete."
