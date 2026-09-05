#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT

c++ -std=c++17 \
  -I"$ROOT_DIR/include" -I"$ROOT_DIR/lib/BoatProtocol/src" -I"$ROOT_DIR/lib/Neighbor/src" \
  -I"$ROOT_DIR/lib/Routing/src" \
  "$ROOT_DIR/test/n3c_route_table_test.cpp" "$ROOT_DIR/lib/Routing/src/RouteTable.cpp" \
  -o "$BUILD_DIR/n3c_route_table_test"

"$BUILD_DIR/n3c_route_table_test"
echo "N3C route table tests: PASS"
