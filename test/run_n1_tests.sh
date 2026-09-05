#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${TMPDIR:-/tmp}/jalari-n1-tests"
mkdir -p "$BUILD_DIR"
c++ -std=c++17 \
  -I"$ROOT_DIR/include" -I"$ROOT_DIR/lib/BoatProtocol/src" -I"$ROOT_DIR/lib/PacketFactory/src" \
  -I"$ROOT_DIR/lib/Node/src" -I"$ROOT_DIR/lib/PacketQueue/src" -I"$ROOT_DIR/lib/EventBus/src" \
  -I"$ROOT_DIR/lib/ReliableLink/src" -I"$ROOT_DIR/lib/Forwarding/src" -I"$ROOT_DIR/lib/Services/src" \
  -I"$ROOT_DIR/lib/HAL/src" -I"$ROOT_DIR/lib/Utils/src" \
  "$ROOT_DIR/test/n1_reliability_test.cpp" \
  "$ROOT_DIR/lib/BoatProtocol/src/Packet.cpp" "$ROOT_DIR/lib/BoatProtocol/src/Parser.cpp" \
  "$ROOT_DIR/lib/BoatProtocol/src/Serializer.cpp" "$ROOT_DIR/lib/BoatProtocol/src/Validator.cpp" \
  "$ROOT_DIR/lib/PacketFactory/src/PacketFactory.cpp" "$ROOT_DIR/lib/Node/src/NodeManager.cpp" \
  "$ROOT_DIR/lib/PacketQueue/src/PacketQueue.cpp" "$ROOT_DIR/lib/EventBus/src/EventBus.cpp" \
  "$ROOT_DIR/lib/ReliableLink/src/AckManager.cpp" "$ROOT_DIR/lib/Forwarding/src/NullForwardingStrategy.cpp" \
  "$ROOT_DIR/lib/Services/src/ForwardingService.cpp" "$ROOT_DIR/lib/Utils/src/Crc16.cpp" \
  -o "$BUILD_DIR/n1_reliability_test"
"$BUILD_DIR/n1_reliability_test"
echo "N1 host tests: PASS"
