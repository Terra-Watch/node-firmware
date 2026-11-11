#!/usr/bin/env bash
# Property and fuzz test of the wire protocol, built with AddressSanitizer and UBSan.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"; BUILD_DIR="$(mktemp -d)"; trap 'rm -rf "$BUILD_DIR"' EXIT
CXX="${CXX:-c++}"
"$CXX" -std=c++17 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined \
  -I"$ROOT_DIR/include" -I"$ROOT_DIR/lib/Protocol/src" -I"$ROOT_DIR/lib/Node/src" -I"$ROOT_DIR/lib/PacketFactory/src" -I"$ROOT_DIR/lib/Utils/src" \
  "$ROOT_DIR/test/fuzz_protocol_test.cpp" "$ROOT_DIR/lib/Protocol/src/Packet.cpp" "$ROOT_DIR/lib/Protocol/src/Parser.cpp" "$ROOT_DIR/lib/Protocol/src/Serializer.cpp" \
  "$ROOT_DIR/lib/Protocol/src/Validator.cpp" "$ROOT_DIR/lib/Utils/src/Crc16.cpp" -o "$BUILD_DIR/fuzz_protocol"
"$BUILD_DIR/fuzz_protocol"
echo "Protocol fuzz and property tests: PASS"
