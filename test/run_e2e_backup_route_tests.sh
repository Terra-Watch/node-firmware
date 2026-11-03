#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT
c++ -std=c++17 -I"$ROOT_DIR/include" -I"$ROOT_DIR/lib/Protocol/src" -I"$ROOT_DIR/lib/Routing/src" -I"$ROOT_DIR/lib/Neighbor/src" \
  -I"$ROOT_DIR/lib/ReliableLink/src" -I"$ROOT_DIR/lib/EventBus/src" -I"$ROOT_DIR/lib/HAL/src" \
  "$ROOT_DIR/test/e2e_backup_route_test.cpp" "$ROOT_DIR/lib/Routing/src/RouteTable.cpp" "$ROOT_DIR/lib/ReliableLink/src/EndToEndTracker.cpp" \
  "$ROOT_DIR/lib/Protocol/src/Validator.cpp" "$ROOT_DIR/lib/Protocol/src/Packet.cpp" -o "$BUILD_DIR/e2e_backup_route_test"
"$BUILD_DIR/e2e_backup_route_test"
echo "E2E and backup-route tests: PASS"
