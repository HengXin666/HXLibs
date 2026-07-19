#!/usr/bin/env bash
set -euo pipefail

readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly BENCH_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
readonly REPO_DIR="$(cd -- "${BENCH_DIR}/../.." && pwd)"
readonly BUILD_DIR="${BENCH_BUILD_DIR:-${REPO_DIR}/.benchmark-build}"
readonly BIN_DIR="${BUILD_DIR}/bin"
readonly RESULT_DIR="${BENCH_RESULT_DIR:-${BUILD_DIR}/results}"

readonly PORT="${BENCH_PORT:-18080}"
readonly WORKERS="${BENCH_WORKERS:-2}"
readonly WRK_THREADS="${BENCH_WRK_THREADS:-2}"
readonly CONNECTIONS="${BENCH_CONNECTIONS:-256}"
readonly DURATION="${BENCH_DURATION:-15}"
readonly WARMUP="${BENCH_WARMUP:-3}"
readonly REPEATS="${BENCH_REPEATS:-3}"

SERVER_PID=""
SERVER_NAME=""

require_positive_integer() {
    local name="$1"
    local value="$2"
    if [[ ! "${value}" =~ ^[1-9][0-9]*$ ]]; then
        printf '%s must be a positive integer, got %s\n' "${name}" "${value}" >&2
        exit 2
    fi
}

stop_server() {
    if [[ -n "${SERVER_PID}" ]] && kill -0 "${SERVER_PID}" 2>/dev/null; then
        kill -TERM "${SERVER_PID}" 2>/dev/null || true
        for _ in {1..50}; do
            if ! kill -0 "${SERVER_PID}" 2>/dev/null; then
                wait "${SERVER_PID}" 2>/dev/null || true
                SERVER_PID=""
                return
            fi
            sleep 0.1
        done
        kill -KILL "${SERVER_PID}" 2>/dev/null || true
        wait "${SERVER_PID}" 2>/dev/null || true
        SERVER_PID=""
    fi
}

trap stop_server EXIT INT TERM

for pair in \
    "PORT:${PORT}" "WORKERS:${WORKERS}" "WRK_THREADS:${WRK_THREADS}" \
    "CONNECTIONS:${CONNECTIONS}" "DURATION:${DURATION}" \
    "WARMUP:${WARMUP}" "REPEATS:${REPEATS}"; do
    require_positive_integer "${pair%%:*}" "${pair#*:}"
done
if (( PORT > 65535 )); then
    printf 'PORT must be at most 65535\n' >&2
    exit 2
fi
if (( CONNECTIONS < WRK_THREADS )); then
    printf 'CONNECTIONS must be greater than or equal to WRK_THREADS\n' >&2
    exit 2
fi

for executable in bench-hxlibs bench-asio bench-yalantinglibs wrk; do
    if [[ ! -x "${BIN_DIR}/${executable}" ]]; then
        printf 'Missing %s; run scripts/build.sh first.\n' "${BIN_DIR}/${executable}" >&2
        exit 1
    fi
done
if [[ ! -x "${BUILD_DIR}/nginx/sbin/nginx" ]]; then
    printf 'Missing nginx; run scripts/build.sh first.\n' >&2
    exit 1
fi

mkdir -p "${RESULT_DIR}/logs" "${BUILD_DIR}/nginx-run"
: > "${RESULT_DIR}/results.jsonl"

readarray -t ALLOWED_CPUS < <(python3 - <<'PY'
import os
for cpu in sorted(os.sched_getaffinity(0)):
    print(cpu)
PY
)
if (( ${#ALLOWED_CPUS[@]} >= 2 )); then
    readonly CLIENT_CPUS="${ALLOWED_CPUS[-1]}"
    server_cpu_count=$(( ${#ALLOWED_CPUS[@]} - 1 ))
    server_cpu_count=$(( server_cpu_count < WORKERS ? server_cpu_count : WORKERS ))
    readonly SERVER_CPUS="$(IFS=,; printf '%s' "${ALLOWED_CPUS[*]:0:${server_cpu_count}}")"
else
    readonly CLIENT_CPUS="${ALLOWED_CPUS[0]}"
    readonly SERVER_CPUS="${ALLOWED_CPUS[0]}"
fi

readonly NGINX_PREFIX="${BUILD_DIR}/nginx-run"
sed -e "s|@WORKERS@|${WORKERS}|g" \
    -e "s|@PORT@|${PORT}|g" \
    -e "s|@PREFIX@|${NGINX_PREFIX}|g" \
    "${BENCH_DIR}/nginx.conf.in" > "${NGINX_PREFIX}/nginx.conf"

start_server() {
    SERVER_NAME="$1"
    local log_file="${RESULT_DIR}/logs/${SERVER_NAME}.log"
    case "${SERVER_NAME}" in
        HXLibs)
            taskset -c "${SERVER_CPUS}" "${BIN_DIR}/bench-hxlibs" \
                "${PORT}" "${WORKERS}" >"${log_file}" 2>&1 &
            ;;
        Asio)
            taskset -c "${SERVER_CPUS}" "${BIN_DIR}/bench-asio" \
                "${PORT}" "${WORKERS}" >"${log_file}" 2>&1 &
            ;;
        yalantinglibs)
            taskset -c "${SERVER_CPUS}" "${BIN_DIR}/bench-yalantinglibs" \
                "${PORT}" "${WORKERS}" >"${log_file}" 2>&1 &
            ;;
        nginx)
            taskset -c "${SERVER_CPUS}" "${BUILD_DIR}/nginx/sbin/nginx" \
                -p "${NGINX_PREFIX}/" -c "${NGINX_PREFIX}/nginx.conf" \
                >"${log_file}" 2>&1 &
            ;;
        FastAPI)
            python3 "${BENCH_DIR}/servers/fastapi_server.py" "${PORT}" >"${log_file}" 2>&1 &
            ;;
        SpringBoot)
            java -jar "${BENCH_DIR}/servers/spring-app/target/spring-bench-1.jar" --server.port="${PORT}" >"${log_file}" 2>&1 &
            ;;
        *)
            printf 'Unknown server: %s\n' "${SERVER_NAME}" >&2
            exit 2
            ;;
    esac
    SERVER_PID=$!

    for _ in {1..100}; do
        if ! kill -0 "${SERVER_PID}" 2>/dev/null; then
            printf '%s exited during startup; see %s\n' "${SERVER_NAME}" "${log_file}" >&2
            exit 1
        fi
        body="$(curl --silent --show-error --max-time 1 "http://127.0.0.1:${PORT}/" || true)"
        if [[ "${body}" == "Hello World!" ]]; then
            return
        fi
        sleep 0.1
    done
    printf '%s did not become ready; see %s\n' "${SERVER_NAME}" "${log_file}" >&2
    exit 1
}

run_one() {
    local server="$1"
    local scenario="$2"
    local repeat="$3"
    local path="${4:-/}"
    local name="${server}-${scenario}"
    printf 'repeat=%s server=%s server_cpus=%s client_cpus=%s\n' \
        "${repeat}" "${name}" "${SERVER_CPUS}" "${CLIENT_CPUS}"
    start_server "${server}"

    taskset -c "${CLIENT_CPUS}" "${BIN_DIR}/wrk" \
        -t"${WRK_THREADS}" -c"${CONNECTIONS}" -d"${WARMUP}s" \
        --timeout 2s "http://127.0.0.1:${PORT}${path}" >/dev/null

    BENCH_SERVER="${name}" BENCH_REPEAT="${repeat}" \
        taskset -c "${CLIENT_CPUS}" "${BIN_DIR}/wrk" \
        -t"${WRK_THREADS}" -c"${CONNECTIONS}" -d"${DURATION}s" \
        --timeout 2s --latency -s "${SCRIPT_DIR}/wrk_json.lua" \
        "http://127.0.0.1:${PORT}${path}" \
        | tee -a "${RESULT_DIR}/results.jsonl"

    stop_server
    sleep 1
}

readonly SERVERS=(HXLibs Asio yalantinglibs nginx FastAPI SpringBoot)
readonly SCENARIOS=(hello html)
for ((repeat = 1; repeat <= REPEATS; ++repeat)); do
    offset=$(( (repeat - 1) % ${#SERVERS[@]} ))
    for ((index = 0; index < ${#SERVERS[@]}; ++index)); do
        server_index=$(( (index + offset) % ${#SERVERS[@]} ))
        for scenario in "${SCENARIOS[@]}"; do
            [[ "${scenario}" == html ]] && path=/page.html || path=/
            run_one "${SERVERS[server_index]}" "${scenario}" "${repeat}" "${path}"
        done
    done
done

if [[ "${BENCH_WS:-1}" == 1 ]]; then
    start_server FastAPI
    python3 "${SCRIPT_DIR}/ws_bench.py" --uri "ws://127.0.0.1:${PORT}/ws" --server FastAPI --repeats "${REPEATS}" \
        | tee -a "${RESULT_DIR}/results.jsonl"
    stop_server
fi

python3 "${SCRIPT_DIR}/report.py" \
    --input "${RESULT_DIR}/results.jsonl" \
    --output "${RESULT_DIR}/index.html" \
    --versions "${BUILD_DIR}/versions.txt" \
    --workers "${WORKERS}" --wrk-threads "${WRK_THREADS}" \
    --connections "${CONNECTIONS}" --duration "${DURATION}" \
    --warmup "${WARMUP}" --repeats "${REPEATS}" \
    --server-cpus "${SERVER_CPUS}" --client-cpus "${CLIENT_CPUS}"

printf 'HTML report: %s\n' "${RESULT_DIR}/index.html"
