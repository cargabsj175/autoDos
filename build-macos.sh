#!/usr/bin/env bash
# AutoDOS macOS build script
# Usage:
#   ./build-macos.sh
#   ./build-macos.sh Debug
#   ./build-macos.sh Release --dmg

set -euo pipefail

CONFIG="${1:-Release}"
BUILD_DMG="${2:-}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_DIR="${SCRIPT_DIR}/macos"
BUILD_DIR="${SOURCE_DIR}/build"
DIST_DIR="${SOURCE_DIR}/dist"

case "${CONFIG}" in
    Debug|Release|RelWithDebInfo|MinSizeRel) ;;
    --dmg)
        BUILD_DMG="--dmg"
        CONFIG="Release"
        ;;
    *)
        echo "ERROR: invalid config '${CONFIG}'. Use Debug, Release, RelWithDebInfo, MinSizeRel, or --dmg." >&2
        exit 1
        ;;
esac

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "ERROR: $1 was not found." >&2
        echo "$2" >&2
        exit 1
    fi
}

echo
echo "=== AutoDOS macOS Build ==="
echo

require_command cmake "Install CMake, for example: brew install cmake"
require_command git "Install Git, for example: xcode-select --install"
require_command c++ "Install Xcode Command Line Tools: xcode-select --install"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "ERROR: macOS builds must be run on macOS." >&2
    exit 1
fi

cmake -S "${SOURCE_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${CONFIG}"
cmake --build "${BUILD_DIR}" --target AutoDOS-cli --parallel
cmake --build "${BUILD_DIR}" --target AutoDOS-gui --parallel

mkdir -p "${DIST_DIR}"
cp "${BUILD_DIR}/bin/AutoDOS-cli" "${DIST_DIR}/"
cp "${BUILD_DIR}/bin/AutoDOS-gui" "${DIST_DIR}/"

if [[ -f "${SCRIPT_DIR}/src/games.json" ]]; then
    cp "${SCRIPT_DIR}/src/games.json" "${DIST_DIR}/"
fi

if [[ "${BUILD_DMG}" == "--dmg" ]]; then
    "${SOURCE_DIR}/build-dmg.sh" "${DIST_DIR}"
fi

echo
echo "Build complete."
echo "Output: ${DIST_DIR}"
