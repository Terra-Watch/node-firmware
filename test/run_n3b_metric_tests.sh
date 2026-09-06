#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT

c++ -std=c++17 \
  -I"$ROOT_DIR/include" -I"$ROOT_DIR/lib/BoatProtocol/src" -I"$ROOT_DIR/lib/Neighbor/src" \
  "$ROOT_DIR/test/n3b_metric_test.cpp" "$ROOT_DIR/lib/BoatProtocol/src/Packet.cpp" \
  "$ROOT_DIR/lib/Neighbor/src/NeighborManager.cpp" \
  -o "$BUILD_DIR/n3b_metric_test"

"$BUILD_DIR/n3b_metric_test"
echo "N3B metric tests: PASS"
