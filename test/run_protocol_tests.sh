#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT

c++ -std=c++17 \
  -I"$ROOT_DIR/include" \
  -I"$ROOT_DIR/lib/Protocol/src" \
  -I"$ROOT_DIR/lib/Node/src" \
  -I"$ROOT_DIR/lib/PacketFactory/src" \
  -I"$ROOT_DIR/lib/Utils/src" \
  "$ROOT_DIR/test/protocol_test.cpp" \
  "$ROOT_DIR/lib/Protocol/src/Packet.cpp" \
  "$ROOT_DIR/lib/Protocol/src/Parser.cpp" \
  "$ROOT_DIR/lib/Protocol/src/Serializer.cpp" \
  "$ROOT_DIR/lib/Protocol/src/Validator.cpp" \
  "$ROOT_DIR/lib/Node/src/NodeManager.cpp" \
  "$ROOT_DIR/lib/PacketFactory/src/PacketFactory.cpp" "$ROOT_DIR/lib/Utils/src/Crc16.cpp" \
  -o "$BUILD_DIR/protocol_v4_test"

"$BUILD_DIR/protocol_v4_test"
echo "Protocol v4 tests: PASS"
