#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT
c++ -std=c++17 \
  -I"$ROOT_DIR/include" -I"$ROOT_DIR/lib/BoatProtocol/src" -I"$ROOT_DIR/lib/Forwarding/src" \
  -I"$ROOT_DIR/lib/Routing/src" -I"$ROOT_DIR/lib/Services/src" -I"$ROOT_DIR/lib/Node/src" \
  -I"$ROOT_DIR/lib/Neighbor/src" -I"$ROOT_DIR/lib/PacketQueue/src" -I"$ROOT_DIR/lib/EventBus/src" \
  -I"$ROOT_DIR/lib/HAL/src" \
  "$ROOT_DIR/test/dtn_store_forward_test.cpp" \
  "$ROOT_DIR/lib/BoatProtocol/src/Packet.cpp" "$ROOT_DIR/lib/Forwarding/src/DynamicForwardingStrategy.cpp" \
  "$ROOT_DIR/lib/Routing/src/RouteTable.cpp" "$ROOT_DIR/lib/Services/src/ForwardingService.cpp" \
  "$ROOT_DIR/lib/Services/src/DtnStoreForwardService.cpp" "$ROOT_DIR/lib/Node/src/NodeManager.cpp" \
  "$ROOT_DIR/lib/PacketQueue/src/PacketQueue.cpp" "$ROOT_DIR/lib/EventBus/src/EventBus.cpp" \
  -o "$BUILD_DIR/dtn_store_forward_test"
"$BUILD_DIR/dtn_store_forward_test"
echo "DTN store-and-forward tests: PASS"
