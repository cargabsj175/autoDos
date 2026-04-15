#!/usr/bin/env bash
# Cross-compile the Windows AutoDOS build from Linux with MinGW-w64.
# Usage:
#   ./build-windows-from-linux.sh
#   ./build-windows-from-linux.sh Debug

set -euo pipefail

CONFIG="${1:-Release}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build-windows-mingw"
DIST_DIR="${SCRIPT_DIR}/dist-windows"

case "${CONFIG}" in
    Debug|Release|RelWithDebInfo|MinSizeRel) ;;
    *)
        echo "ERROR: invalid config '${CONFIG}'. Use Debug, Release, RelWithDebInfo, or MinSizeRel." >&2
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
echo "=== AutoDOS Windows Cross-Build from Linux ==="
echo

require_command cmake "Install CMake, for example: sudo apt install cmake"
require_command git "Install Git, for example: sudo apt install git"
require_command x86_64-w64-mingw32-g++ "Install MinGW-w64, for example: sudo apt install mingw-w64"
require_command x86_64-w64-mingw32-gcc "Install MinGW-w64, for example: sudo apt install mingw-w64"

if [[ ! -f "${SCRIPT_DIR}/src/miniz.h" || ! -f "${SCRIPT_DIR}/src/miniz.c" ]]; then
    echo "ERROR: src/miniz.h and src/miniz.c are required." >&2
    echo "Download them from https://github.com/richgel999/miniz/releases and place them in src/." >&2
    exit 1
fi

if [[ ! -f "${SCRIPT_DIR}/src/games.json" ]]; then
    echo "WARNING: src/games.json was not found. The build will continue." >&2
fi

cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
    -DCMAKE_BUILD_TYPE="${CONFIG}" \
    -DCMAKE_FIND_ROOT_PATH=/usr/x86_64-w64-mingw32 \
    -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
    -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
    -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY

cmake --build "${BUILD_DIR}" --parallel

EXE_PATH="${BUILD_DIR}/bin/AutoDOS.exe"
if [[ ! -f "${EXE_PATH}" ]]; then
    echo "ERROR: AutoDOS.exe was not found after building." >&2
    exit 1
fi

mkdir -p "${DIST_DIR}"
cp "${EXE_PATH}" "${DIST_DIR}/AutoDOS.exe"

if [[ -f "${SCRIPT_DIR}/src/games.json" ]]; then
    cp "${SCRIPT_DIR}/src/games.json" "${DIST_DIR}/games.json"
fi

if [[ -f "${SCRIPT_DIR}/dosbox/dosbox.exe" ]]; then
    mkdir -p "${DIST_DIR}/dosbox"
    cp -R "${SCRIPT_DIR}/dosbox/." "${DIST_DIR}/dosbox/"
fi

echo
echo "Build complete."
echo "Output: ${DIST_DIR}/AutoDOS.exe"
