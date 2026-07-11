#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build}"

cmake -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

cmake --build "${BUILD_DIR}" --parallel "$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)"
echo ""
echo "Build complete. Run tests: ctest --test-dir ${BUILD_DIR}"
echo "Run CLI:       ${BUILD_DIR}/mercury_cli"
