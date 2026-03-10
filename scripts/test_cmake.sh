#!/usr/bin/env bash
set -euo pipefail

rm -rf build

cmake -S . -B build -G Ninja -DBUILD_TESTS=ON
cmake --build build

echo "==== Registered tests ===="
ctest --test-dir build -N

echo "==== Running tests ===="
ctest --test-dir build --output-on-failure