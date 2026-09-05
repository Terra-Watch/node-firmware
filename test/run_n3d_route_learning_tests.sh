#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT

c++ -std=c++17 \
  -I"$ROOT_DIR/include" -I"$ROOT_DIR/lib/BoatProtocol/src" -I"$ROOT_DIR/lib/Neighbor/src" \
  -I"$ROOT_DIR/lib/PacketFactory/src" -I"$ROOT_DIR/lib/Node/src" -I"$ROOT_DIR/lib/PacketQueue/src" \
  -I"$ROOT_DIR/lib/EventBus/src" -I"$ROOT_DIR/lib/HAL/src" -I"$ROOT_DIR/lib/Routing/src" \
  "$ROOT_DIR/test/n3d_route_learning_test.cpp" "$ROOT_DIR/lib/BoatProtocol/src/Packet.cpp" \
  "$ROOT_DIR/lib/Neighbor/src/NeighborManager.cpp" "$ROOT_DIR/lib/PacketFactory/src/PacketFactory.cpp" \
  "$ROOT_DIR/lib/Node/src/NodeManager.cpp" "$ROOT_DIR/lib/PacketQueue/src/PacketQueue.cpp" \
  "$ROOT_DIR/lib/EventBus/src/EventBus.cpp" "$ROOT_DIR/lib/Routing/src/RouteTable.cpp" \
  "$ROOT_DIR/lib/Routing/src/RouteAdvertisementService.cpp" \
  -o "$BUILD_DIR/n3d_route_learning_test"

"$BUILD_DIR/n3d_route_learning_test"
echo "N3D route learning tests: PASS"
