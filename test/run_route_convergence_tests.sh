#!/usr/bin/env bash
# Real routing code on random networks against Dijkstra, with the sanitizers on.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"; BUILD_DIR="$(mktemp -d)"; trap 'rm -rf "$BUILD_DIR"' EXIT
CXX="${CXX:-c++}"
"$CXX" -std=c++17 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined \
  -I"$ROOT_DIR/include" -I"$ROOT_DIR/lib/Protocol/src" -I"$ROOT_DIR/lib/Neighbor/src" -I"$ROOT_DIR/lib/PacketFactory/src" -I"$ROOT_DIR/lib/Node/src" -I"$ROOT_DIR/lib/PacketQueue/src" \
  -I"$ROOT_DIR/lib/EventBus/src" -I"$ROOT_DIR/lib/HAL/src" -I"$ROOT_DIR/lib/Routing/src" \
  "$ROOT_DIR/test/route_convergence_test.cpp" "$ROOT_DIR/lib/Protocol/src/Packet.cpp" "$ROOT_DIR/lib/Neighbor/src/NeighborManager.cpp" "$ROOT_DIR/lib/PacketFactory/src/PacketFactory.cpp" \
  "$ROOT_DIR/lib/Node/src/NodeManager.cpp" "$ROOT_DIR/lib/PacketQueue/src/PacketQueue.cpp" "$ROOT_DIR/lib/EventBus/src/EventBus.cpp" "$ROOT_DIR/lib/Routing/src/RouteTable.cpp" \
  "$ROOT_DIR/lib/Routing/src/RouteAdvertisementService.cpp" "$ROOT_DIR/lib/Utils/src/Crc16.cpp" -o "$BUILD_DIR/route_convergence"
"$BUILD_DIR/route_convergence"
