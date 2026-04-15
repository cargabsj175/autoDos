#!/usr/bin/env bash
# AutoDOS Linux build script
# Usage:
#   ./build-linux.sh
#   ./build-linux.sh Debug

set -euo pipefail

CONFIG="${1:-Release}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_DIR="${SCRIPT_DIR}/linux"
BUILD_DIR="${SOURCE_DIR}/build-linux"
DIST_DIR="${SOURCE_DIR}/dist"

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
echo "=== AutoDOS Linux Build ==="
echo

require_command cmake "Install CMake, for example: sudo apt install cmake"
require_command git "Install Git, for example: sudo apt install git"
require_command c++ "Install a C++ compiler, for example: sudo apt install g++"

if [[ ! -f "${SOURCE_DIR}/src/games.json" ]]; then
    echo "WARNING: linux/src/games.json was not found. The build will continue." >&2
fi

cmake -S "${SOURCE_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${CONFIG}"
cmake --build "${BUILD_DIR}" --parallel

mkdir -p "${DIST_DIR}"
cp "${BUILD_DIR}/bin/AutoDOS" "${DIST_DIR}/"

if [[ -x "${BUILD_DIR}/bin/AutoDOS-GUI" ]]; then
    cp "${BUILD_DIR}/bin/AutoDOS-GUI" "${DIST_DIR}/"
else
    echo "WARNING: AutoDOS-GUI was not built. Install GLFW3/OpenGL development packages if you need it." >&2
fi

if [[ -f "${SOURCE_DIR}/src/games.json" ]]; then
    cp "${SOURCE_DIR}/src/games.json" "${DIST_DIR}/"
fi

echo
echo "Build complete."
echo "Output: ${DIST_DIR}"
