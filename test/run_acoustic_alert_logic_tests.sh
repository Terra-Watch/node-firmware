#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT
c++ -std=c++17 -I"$ROOT_DIR/lib/Acoustic/src" "$ROOT_DIR/test/acoustic_alert_logic_test.cpp" "$ROOT_DIR/lib/Acoustic/src/AlertLogic.cpp" -o "$BUILD_DIR/test"
"$BUILD_DIR/test"
