#!/usr/bin/env bash
set -euo pipefail

find include src tests benchmarks -name '*.cpp' -o -name '*.hpp' | xargs clang-format -i
echo "Formatted all C++ sources."
