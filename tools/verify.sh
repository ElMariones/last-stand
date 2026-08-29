#!/usr/bin/env bash
# Build and test, failing loudly. Exists because a failed build leaves the
# previous test binary in place, so `ctest` alone will happily report green
# on stale results.
set -euo pipefail
cd "$(dirname "$0")/.."
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/laststand_tests 2>&1 | tail -2
