#!/usr/bin/env bash
# Exhaustive search of the real route table on small networks. Set SANITIZE=1 for the sanitizers (slower).
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"; BUILD_DIR="$(mktemp -d)"; trap 'rm -rf "$BUILD_DIR"' EXIT
CXX="${CXX:-c++}"
"$CXX" -std=c++17 -O2 ${SANITIZE:+-g -fsanitize=address,undefined -fno-sanitize-recover=undefined} -I"$ROOT_DIR/include" -I"$ROOT_DIR/lib/Protocol/src" -I"$ROOT_DIR/lib/Neighbor/src" -I"$ROOT_DIR/lib/Routing/src" \
  "$ROOT_DIR/test/route_modelcheck_test.cpp" "$ROOT_DIR/lib/Routing/src/RouteTable.cpp" -o "$BUILD_DIR/route_modelcheck"
"$BUILD_DIR/route_modelcheck"
