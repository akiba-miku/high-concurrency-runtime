#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-release}"
HOST="${HOST:-127.0.0.1}"
PORT="${PORT:-18081}"
IO_THREADS="${IO_THREADS:-2}"
DURATION="${DURATION:-15s}"
THREAD_SET="${THREAD_SET:-1 2 4 8}"
CONNECTION_SET="${CONNECTION_SET:-64 128 256 512}"
RESULT_DIR="${RESULT_DIR:-$ROOT_DIR/benchmarks/results}"
STAMP="${STAMP:-$(date +%Y%m%d-%H%M%S)}"
OUT_DIR="${RESULT_DIR}/${STAMP}"
SERVER_LOG="${OUT_DIR}/simple_http_server.log"
SUMMARY_TSV="${OUT_DIR}/summary.tsv"
ENV_TXT="${OUT_DIR}/environment.txt"

SERVER_PID=""

cleanup() {
  if [[ -n "${SERVER_PID}" ]] && kill -0 "${SERVER_PID}" 2>/dev/null; then
    kill "${SERVER_PID}" >/dev/null 2>&1 || true
    wait "${SERVER_PID}" 2>/dev/null || true
  fi
}
trap cleanup EXIT

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "missing required command: $1" >&2
    exit 1
  fi
}

record_env() {
  {
    echo "date=$(date --iso-8601=seconds)"
    echo "host=$(hostname)"
    echo "uname=$(uname -a)"
    echo "compiler_flags=Release (-O3 if CMake default compiler flags are used)"
    echo "build_dir=${BUILD_DIR}"
    echo "server_command=HOST=${HOST} PORT=${PORT} IO_THREADS=${IO_THREADS} KEEP_ALIVE=\$KEEP_ALIVE IDLE_TIMEOUT_SECONDS=\$IDLE_TIMEOUT_SECONDS STATIC_ROOT=${ROOT_DIR}/examples/www ${BUILD_DIR}/examples/simple_http_server"
    echo
    echo "[lscpu]"
    lscpu
    echo
    echo "[free -h]"
    free -h
    echo
    echo "[os-release]"
    cat /etc/os-release || true
  } > "${ENV_TXT}"
}

build_server() {
  cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_EXAMPLES=ON \
    -DBUILD_TESTS=OFF >/dev/null
  cmake --build "${BUILD_DIR}" --target simple_http_server -j >/dev/null
}

wait_for_server() {
  local attempts=40
  local i
  for ((i = 1; i <= attempts; ++i)); do
    if curl -fsS --http1.1 "http://${HOST}:${PORT}/api/health" >/dev/null 2>&1; then
      return 0
    fi
    sleep 0.25
  done
  echo "server did not become ready: http://${HOST}:${PORT}" >&2
  return 1
}

start_server() {
  local keep_alive="$1"
  local idle_timeout="$2"
  : > "${SERVER_LOG}"
  (
    cd "${BUILD_DIR}"
    HOST="${HOST}" \
    PORT="${PORT}" \
    IO_THREADS="${IO_THREADS}" \
    KEEP_ALIVE="${keep_alive}" \
    IDLE_TIMEOUT_SECONDS="${idle_timeout}" \
    STATIC_ROOT="${ROOT_DIR}/examples/www" \
    ./examples/simple_http_server
  ) >> "${SERVER_LOG}" 2>&1 &
  SERVER_PID=$!
  wait_for_server
}

stop_server() {
  if [[ -n "${SERVER_PID}" ]] && kill -0 "${SERVER_PID}" 2>/dev/null; then
    kill "${SERVER_PID}" >/dev/null 2>&1 || true
    wait "${SERVER_PID}" 2>/dev/null || true
  fi
  SERVER_PID=""
}

scenario_url() {
  case "$1" in
    health) echo "http://${HOST}:${PORT}/api/health" ;;
    echo) echo "http://${HOST}:${PORT}/api/echo?src=wrk" ;;
    static) echo "http://${HOST}:${PORT}/static/index.html" ;;
    notfound) echo "http://${HOST}:${PORT}/missing" ;;
    *) echo "unknown scenario: $1" >&2; exit 1 ;;
  esac
}

scenario_script() {
  case "$1" in
    echo) echo "${ROOT_DIR}/benchmarks/wrk/post_echo.lua" ;;
    *) echo "" ;;
  esac
}

run_wrk_once() {
  local scenario="$1"
  local threads="$2"
  local connections="$3"
  local keep_alive="$4"
  local idle_timeout="$5"
  local label="$6"

  local url
  url="$(scenario_url "${scenario}")"
  local script
  script="$(scenario_script "${scenario}")"
  local output_file="${OUT_DIR}/${label}-${scenario}-t${threads}-c${connections}.txt"

  local cmd=(wrk -t"${threads}" -c"${connections}" -d"${DURATION}" --latency)
  if [[ -n "${script}" ]]; then
    cmd+=(-s "${script}")
  fi
  cmd+=("${url}")

  KEEP_ALIVE_HEADER="$([[ "${keep_alive}" == "1" ]] && echo "keep-alive" || echo "close")" \
    "${cmd[@]}" | tee "${output_file}" >/dev/null

  local requests_sec
  requests_sec="$(awk '/Requests\/sec:/ {print $2}' "${output_file}")"
  local transfer_sec
  transfer_sec="$(awk '/Transfer\/sec:/ {print $2 $3}' "${output_file}")"
  local latency_avg
  latency_avg="$(awk '/Latency/ {print $2; exit}' "${output_file}")"
  local latency_stdev
  latency_stdev="$(awk '/Latency/ {print $3; exit}' "${output_file}")"
  local req_line
  req_line="$(awk '/Socket errors:/ {print $0}' "${output_file}")"
  local non2xx
  non2xx="$(awk '/Non-2xx or 3xx responses:/ {print $5}' "${output_file}")"
  local p50
  p50="$(awk '$1=="50%" {print $2 $3}' "${output_file}")"
  local p90
  p90="$(awk '$1=="90%" {print $2 $3}' "${output_file}")"
  local p99
  p99="$(awk '$1=="99%" {print $2 $3}' "${output_file}")"

  printf "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n" \
    "${label}" "${scenario}" "${threads}" "${connections}" "${keep_alive}" "${idle_timeout}" \
    "${requests_sec:-NA}" "${latency_avg:-NA}" "${latency_stdev:-NA}" "${p50:-NA}" "${p90:-NA}" "${p99:-NA}" \
    "${non2xx:-0}" >> "${SUMMARY_TSV}"

  if [[ -n "${req_line}" ]]; then
    echo "${label} ${scenario} t=${threads} c=${connections} ${req_line}" >> "${OUT_DIR}/notes.txt"
  fi
  if [[ -n "${transfer_sec}" ]]; then
    echo "${label} ${scenario} t=${threads} c=${connections} Transfer/sec=${transfer_sec}" >> "${OUT_DIR}/notes.txt"
  fi
}

require_cmd cmake
require_cmd wrk
require_cmd curl
require_cmd awk
require_cmd lscpu
require_cmd free

mkdir -p "${OUT_DIR}"
record_env
build_server

printf "label\tscenario\twrk_threads\tconnections\tkeep_alive\tidle_timeout_s\trps\tavg_latency\tlatency_stdev\tp50\tp90\tp99\tnon2xx\n" > "${SUMMARY_TSV}"

start_server 1 15
for scenario in health echo static notfound; do
  for threads in ${THREAD_SET}; do
    for connections in ${CONNECTION_SET}; do
      run_wrk_once "${scenario}" "${threads}" "${connections}" 1 15 baseline
    done
  done
done
stop_server

start_server 0 15
for scenario in health; do
  for threads in ${THREAD_SET}; do
    for connections in ${CONNECTION_SET}; do
      run_wrk_once "${scenario}" "${threads}" "${connections}" 0 15 keepalive_off
    done
  done
done
stop_server

start_server 1 1
for scenario in health; do
  for threads in ${THREAD_SET}; do
    for connections in ${CONNECTION_SET}; do
      run_wrk_once "${scenario}" "${threads}" "${connections}" 1 1 idle_timeout_1s
    done
  done
done
stop_server

echo "results written to ${OUT_DIR}"
echo "summary: ${SUMMARY_TSV}"
