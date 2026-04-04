#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-release}"
HOST="${HOST:-127.0.0.1}"
PORT="${PORT:-18081}"
THREADS="${THREADS:-4}"
CONNECTIONS="${CONNECTIONS:-256}"
DURATION="${DURATION:-30s}"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=ON -DBUILD_TESTS=OFF
cmake --build "$BUILD_DIR" --target simple_http_server -j

HOST="$HOST" PORT="$PORT" "$BUILD_DIR/examples/simple_http_server" &
SERVER_PID=$!

cleanup() {
  kill "$SERVER_PID" >/dev/null 2>&1 || true
}
trap cleanup EXIT

sleep 1

wrk -t"$THREADS" -c"$CONNECTIONS" -d"$DURATION" "http://$HOST:$PORT/"
