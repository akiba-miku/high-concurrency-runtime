#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-release}"
HOST="${HOST:-127.0.0.1}"
PORT="${PORT:-18081}"
THREADS="${THREADS:-4}"
CONNECTIONS="${CONNECTIONS:-256}"
DURATION="${DURATION:-30s}"
TARGET_PATH="${TARGET_PATH:-/api/health}"
HTTP_METHOD="${HTTP_METHOD:-GET}"
KEEP_ALIVE="${KEEP_ALIVE:-1}"
IO_THREADS="${IO_THREADS:-2}"
IDLE_TIMEOUT_SECONDS="${IDLE_TIMEOUT_SECONDS:-15}"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=ON -DBUILD_TESTS=OFF
cmake --build "$BUILD_DIR" --target simple_http_server -j

HOST="$HOST" PORT="$PORT" IO_THREADS="$IO_THREADS" KEEP_ALIVE="$KEEP_ALIVE" \
IDLE_TIMEOUT_SECONDS="$IDLE_TIMEOUT_SECONDS" STATIC_ROOT="$ROOT_DIR/examples/www" \
  "$BUILD_DIR/examples/simple_http_server" &
SERVER_PID=$!

cleanup() {
  kill "$SERVER_PID" >/dev/null 2>&1 || true
}
trap cleanup EXIT

sleep 1

WRK_ARGS=(-t"$THREADS" -c"$CONNECTIONS" -d"$DURATION" --latency)
if [[ "$HTTP_METHOD" == "POST" ]]; then
  WRK_ARGS+=(-s "$ROOT_DIR/benchmarks/wrk/post_echo.lua")
fi

echo "command: wrk ${WRK_ARGS[*]} http://$HOST:$PORT$TARGET_PATH"
wrk "${WRK_ARGS[@]}" "http://$HOST:$PORT$TARGET_PATH"
