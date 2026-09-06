#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT
c++ -std=c++17 -I"$ROOT_DIR/include" -I"$ROOT_DIR/lib/Services/src" \
  "$ROOT_DIR/test/test_command_parser.cpp" "$ROOT_DIR/lib/Services/src/TestCommandParser.cpp" \
  -o "$BUILD_DIR/test_command_parser"
"$BUILD_DIR/test_command_parser"
echo "test command parser tests: PASS"
