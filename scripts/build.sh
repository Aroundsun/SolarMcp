#!/usr/bin/env bash
#
# SolarMcp build script
# Usage: ./scripts/build.sh [Debug|Release]
#

set -euo pipefail

BUILD_TYPE="${1:-Debug}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_DIR}/build"

echo "=== SolarMcp Build ==="
echo "Build type: ${BUILD_TYPE}"
echo "Project dir: ${PROJECT_DIR}"
echo ""

# Configure
echo "[1/3] Configuring..."
cmake -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DBUILD_TESTS=ON \
    "${PROJECT_DIR}"

# Build
echo "[2/3] Building..."
cmake --build "${BUILD_DIR}" -j"$(nproc 2>/dev/null || echo 4)"

# Test (optional)
if [[ "${BUILD_TYPE}" == "Debug" ]]; then
    echo "[3/3] Running tests..."
    cd "${BUILD_DIR}"
    ctest --output-on-failure -j"$(nproc 2>/dev/null || echo 4)"
    cd "${PROJECT_DIR}"
else
    echo "[3/3] Skipping tests (Release build)"
fi

echo ""
echo "=== Build complete ==="
echo "Binary: ${BUILD_DIR}/solarmcpd"
echo "Run: ${BUILD_DIR}/solarmcpd --config config.yaml"
