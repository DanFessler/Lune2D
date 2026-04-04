#!/usr/bin/env bash
# Run the game briefly and save a composited window PNG (SDL + WKWebView), for viewport debugging.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-$ROOT/artifacts/viewport_test.png}"
case "$OUT" in
  /*) ;;
  *) OUT="$ROOT/${OUT#./}" ;;
esac
mkdir -p "$(dirname "$OUT")"
ARGS=("$ROOT/build/lune2d")
# CAPTURE_NATIVE=1: compute % rect in C++ (still uses JS layout size as %-basis when posted).
[[ "${CAPTURE_NATIVE:-}" == 1 ]] && ARGS+=(--native-game-rect)
ARGS+=(--capture-after-frames 200 --capture-window-png "$OUT")
exec "${ARGS[@]}"
