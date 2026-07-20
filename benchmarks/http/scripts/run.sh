#!/usr/bin/env bash
set -euo pipefail

readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly BENCH_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
readonly REPO_DIR="$(cd -- "${BENCH_DIR}/../.." && pwd)"
readonly BUILD_DIR="${BENCH_BUILD_DIR:-${REPO_DIR}/.benchmark-build}"
readonly BIN_DIR="${BUILD_DIR}/bin"
readonly RESULT_DIR="${BENCH_RESULT_DIR:-${BUILD_DIR}/results}"
readonly ASSET_DIR="${BUILD_DIR}/assets"

readonly PORT="${BENCH_PORT:-18080}"
# 长时间、多轮采样才能让 CPU 频率、连接池和分配器进入稳定状态。
# 本地冒烟测试仍可通过 BENCH_DURATION/BENCH_REPEATS 覆盖这些默认值。
readonly DURATION="${BENCH_DURATION:-60}"
readonly WARMUP="${BENCH_WARMUP:-15}"
readonly REPEATS="${BENCH_REPEATS:-3}"
readonly COOLDOWN="${BENCH_COOLDOWN:-3}"

SERVER_PID=""
SERVER_LABEL=""

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
        for _ in {1..100}; do
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

readarray -t ALLOWED_CPUS < <(python3 - <<'PY'
import os
for cpu in sorted(os.sched_getaffinity(0)):
    print(cpu)
PY
)
if (( ${#ALLOWED_CPUS[@]} == 0 )); then
    printf 'No CPU is available in the current affinity set.\n' >&2
    exit 1
fi

if (( ${#ALLOWED_CPUS[@]} == 1 )); then
    SERVER_CPU_COUNT=1
    SERVER_CPUS="${ALLOWED_CPUS[0]}"
    CLIENT_CPUS="${ALLOWED_CPUS[0]}"
    CLIENT_CPU_COUNT=1
    printf 'Warning: server and wrk share one CPU; results are only suitable for smoke testing.\n' >&2
else
    SERVER_CPU_COUNT="${BENCH_SERVER_CPUS:-$(( ${#ALLOWED_CPUS[@]} / 2 ))}"
    require_positive_integer BENCH_SERVER_CPUS "${SERVER_CPU_COUNT}"
    if (( SERVER_CPU_COUNT >= ${#ALLOWED_CPUS[@]} )); then
        printf 'BENCH_SERVER_CPUS must leave at least one CPU for wrk.\n' >&2
        exit 2
    fi
    SERVER_CPUS="$(IFS=,; printf '%s' "${ALLOWED_CPUS[*]:0:${SERVER_CPU_COUNT}}")"
    CLIENT_CPU_COUNT=$(( ${#ALLOWED_CPUS[@]} - SERVER_CPU_COUNT ))
    CLIENT_CPUS="$(IFS=,; printf '%s' "${ALLOWED_CPUS[*]:${SERVER_CPU_COUNT}:${CLIENT_CPU_COUNT}}")"
fi

readonly SERVER_CPU_COUNT SERVER_CPUS CLIENT_CPU_COUNT CLIENT_CPUS
readonly WORKERS="${BENCH_WORKERS:-${SERVER_CPU_COUNT}}"
readonly WRK_THREADS="${BENCH_WRK_THREADS:-${CLIENT_CPU_COUNT}}"
default_connections=$(( WORKERS * 128 ))
(( default_connections < 256 )) && default_connections=256
readonly CONNECTIONS="${BENCH_CONNECTIONS:-${default_connections}}"
default_low_connections=$(( WORKERS * 8 ))
(( default_low_connections < WRK_THREADS )) && default_low_connections="${WRK_THREADS}"
readonly LOW_CONNECTIONS="${BENCH_LOW_CONNECTIONS:-${default_low_connections}}"
default_high_connections=$(( CONNECTIONS * 2 ))
(( default_high_connections > 8192 )) && default_high_connections=8192
readonly HIGH_CONNECTIONS="${BENCH_HIGH_CONNECTIONS:-${default_high_connections}}"

for pair in \
    "PORT:${PORT}" "WORKERS:${WORKERS}" "WRK_THREADS:${WRK_THREADS}" \
    "CONNECTIONS:${CONNECTIONS}" "LOW_CONNECTIONS:${LOW_CONNECTIONS}" \
    "HIGH_CONNECTIONS:${HIGH_CONNECTIONS}" "DURATION:${DURATION}" \
    "WARMUP:${WARMUP}" "REPEATS:${REPEATS}" "COOLDOWN:${COOLDOWN}"; do
    require_positive_integer "${pair%%:*}" "${pair#*:}"
done
if (( PORT > 65535 )); then
    printf 'PORT must be at most 65535\n' >&2
    exit 2
fi
for connections in "${LOW_CONNECTIONS}" "${CONNECTIONS}" "${HIGH_CONNECTIONS}"; do
    if (( connections < WRK_THREADS )); then
        printf 'Every connection level must be at least WRK_THREADS (%s).\n' "${WRK_THREADS}" >&2
        exit 2
    fi
done

for optimization in O2 O3; do
    for executable in bench-hxlibs bench-asio bench-yalantinglibs; do
        if [[ ! -x "${BIN_DIR}/${optimization}/${executable}" ]]; then
            printf 'Missing %s; run scripts/build.sh first.\n' \
                "${BIN_DIR}/${optimization}/${executable}" >&2
            exit 1
        fi
    done
    if [[ ! -x "${BUILD_DIR}/nginx-${optimization}/sbin/nginx" ]]; then
        printf 'Missing nginx-%s; run scripts/build.sh first.\n' "${optimization}" >&2
        exit 1
    fi
done
if [[ ! -x "${BIN_DIR}/wrk" ]]; then
    printf 'Missing %s; run scripts/build.sh first.\n' "${BIN_DIR}/wrk" >&2
    exit 1
fi
if [[ ! -f "${ASSET_DIR}/page.html" || ! -f "${ASSET_DIR}/payload.bin" ]]; then
    printf 'Missing benchmark assets in %s; run scripts/build.sh first.\n' "${ASSET_DIR}" >&2
    exit 1
fi
readonly PAGE_BYTES="$(stat -c '%s' "${ASSET_DIR}/page.html")"
readonly PAYLOAD_BYTES="$(stat -c '%s' "${ASSET_DIR}/payload.bin")"

ulimit -n 65535 2>/dev/null || true
mkdir -p "${RESULT_DIR}/logs"
: > "${RESULT_DIR}/results.jsonl"

for optimization in O2 O3; do
    nginx_run_prefix="${BUILD_DIR}/nginx-run-${optimization}"
    mkdir -p "${nginx_run_prefix}/logs" "${nginx_run_prefix}/html"
    cp "${ASSET_DIR}/payload.bin" "${nginx_run_prefix}/html/payload.bin"
    cp "${ASSET_DIR}/page.html" "${nginx_run_prefix}/html/page.html"
    sed -e "s|@WORKERS@|${WORKERS}|g" \
        -e "s|@PORT@|${PORT}|g" \
        -e "s|@PREFIX@|${nginx_run_prefix}|g" \
        "${BENCH_DIR}/nginx.conf.in" > "${nginx_run_prefix}/nginx.conf"
done

start_server() {
    local server="$1"
    local optimization="$2"
    SERVER_LABEL="${server}-${optimization}"
    local log_file="${RESULT_DIR}/logs/${SERVER_LABEL}.log"
    case "${server}" in
        HXLibs)
            taskset -c "${SERVER_CPUS}" "${BIN_DIR}/${optimization}/bench-hxlibs" "${PORT}" "${WORKERS}" "${ASSET_DIR}" >"${log_file}" 2>&1 &
            ;;
        Asio)
            taskset -c "${SERVER_CPUS}" "${BIN_DIR}/${optimization}/bench-asio" "${PORT}" "${WORKERS}" "${ASSET_DIR}" >"${log_file}" 2>&1 &
            ;;
        yalantinglibs)
            taskset -c "${SERVER_CPUS}" "${BIN_DIR}/${optimization}/bench-yalantinglibs" "${PORT}" "${WORKERS}" "${ASSET_DIR}" >"${log_file}" 2>&1 &
            ;;
        nginx)
            local prefix="${BUILD_DIR}/nginx-run-${optimization}"
            taskset -c "${SERVER_CPUS}" "${BUILD_DIR}/nginx-${optimization}/sbin/nginx" \
                -p "${prefix}/" -c "${prefix}/nginx.conf" >"${log_file}" 2>&1 &
            ;;
        FastAPI)
            BENCH_ASSET_DIR="${ASSET_DIR}" taskset -c "${SERVER_CPUS}" python3 "${BENCH_DIR}/servers/fastapi_server.py" "${PORT}" "${WORKERS}" >"${log_file}" 2>&1 &
            ;;
        SpringBoot)
            BENCH_ASSET_DIR="${ASSET_DIR}" taskset -c "${SERVER_CPUS}" java -XX:ActiveProcessorCount="${WORKERS}" \
                -jar "${BENCH_DIR}/servers/spring-app/target/spring-bench-1.jar" \
                --server.port="${PORT}" >"${log_file}" 2>&1 &
            ;;
        *)
            printf 'Unknown server: %s\n' "${server}" >&2
            exit 2
            ;;
    esac
    SERVER_PID=$!

    for _ in {1..300}; do
        if ! kill -0 "${SERVER_PID}" 2>/dev/null; then
            printf '%s exited during startup; see %s\n' "${SERVER_LABEL}" "${log_file}" >&2
            if [[ -s "${log_file}" ]]; then
                printf '%s startup log:\n' "${SERVER_LABEL}" >&2
                sed 's/^/  /' "${log_file}" >&2
            fi
            exit 1
        fi
        body="$(curl --silent --show-error --max-time 1 "http://127.0.0.1:${PORT}/" || true)"
        if [[ "${body}" == "Hello World!" ]]; then
            page_bytes="$(curl --silent --show-error --max-time 2 --output /dev/null \
                --write-out '%{size_download}' "http://127.0.0.1:${PORT}/page.html" || true)"
            payload_bytes="$(curl --silent --show-error --max-time 2 --output /dev/null \
                --write-out '%{size_download}' "http://127.0.0.1:${PORT}/payload.bin" || true)"
            route_body="$(curl --silent --show-error --max-time 2 \
                "http://127.0.0.1:${PORT}/api/users/1001/orders/90001?page=2&limit=20&sort=name" || true)"
            if [[ "${page_bytes}" == "${PAGE_BYTES}" \
                && "${payload_bytes}" == "${PAYLOAD_BYTES}" \
                && "${route_body}" == *'90001'* && "${route_body}" == *'name'* ]]; then
                return
            fi
            printf '%s endpoint self-check failed: html=%s/%s payload=%s/%s route=%s\n' \
                "${SERVER_LABEL}" "${page_bytes}" "${PAGE_BYTES}" \
                "${payload_bytes}" "${PAYLOAD_BYTES}" "${route_body}" >&2
            exit 1
        fi
        sleep 0.1
    done
    printf '%s did not become ready; see %s\n' "${SERVER_LABEL}" "${log_file}" >&2
    exit 1
}

run_one() {
    local server="$1"
    local optimization="$2"
    local scenario="$3"
    local profile="$4"
    local paths="$5"
    local connections="$6"
    local repeat="$7"
    printf '轮次=%s 实现=%s 优化=%s 场景=%s 负载=%s 连接=%s 服务端CPU=%s 客户端CPU=%s\n' \
        "${repeat}" "${server}" "${optimization}" "${scenario}" "${profile}" \
        "${connections}" "${SERVER_CPUS}" "${CLIENT_CPUS}"
    start_server "${server}" "${optimization}"

    BENCH_PATHS="${paths}" BENCH_SERVER="${server}" BENCH_OPTIMIZATION="${optimization}" \
        BENCH_SCENARIO="${scenario}" BENCH_PROFILE="${profile}" BENCH_REPEAT="${repeat}" \
        BENCH_CONNECTIONS="${connections}" BENCH_DURATION="${WARMUP}" \
        taskset -c "${CLIENT_CPUS}" "${BIN_DIR}/wrk" \
        -t"${WRK_THREADS}" -c"${connections}" -d"${WARMUP}s" --timeout 5s \
        -s "${SCRIPT_DIR}/wrk_json.lua" "http://127.0.0.1:${PORT}/" >/dev/null

    BENCH_PATHS="${paths}" BENCH_SERVER="${server}" BENCH_OPTIMIZATION="${optimization}" \
        BENCH_SCENARIO="${scenario}" BENCH_PROFILE="${profile}" BENCH_REPEAT="${repeat}" \
        BENCH_CONNECTIONS="${connections}" BENCH_DURATION="${DURATION}" \
        taskset -c "${CLIENT_CPUS}" "${BIN_DIR}/wrk" \
        -t"${WRK_THREADS}" -c"${connections}" -d"${DURATION}s" --timeout 5s --latency \
        -s "${SCRIPT_DIR}/wrk_json.lua" "http://127.0.0.1:${PORT}/" \
        | tee -a "${RESULT_DIR}/results.jsonl"

    stop_server
    sleep "${COOLDOWN}"
}

readonly CONFIGURATIONS=(
    'HXLibs|O2' 'HXLibs|O3'
    'Asio|O2' 'Asio|O3'
    'yalantinglibs|O2' 'yalantinglibs|O3'
    'nginx|O2' 'nginx|O3'
    'FastAPI|运行时' 'SpringBoot|运行时'
)
readonly SCENARIOS=(
    "hello|低并发|/|${LOW_CONNECTIONS}"
    "json-api|标准并发|/api/users|${CONNECTIONS}"
    "route-query|动态路由与参数解析|/api/users/1001/orders/90001?page=2&limit=20&sort=name|${CONNECTIONS}"
    "html-page|标准并发|/page.html|${CONNECTIONS}"
    "payload-64k|带宽负载|/payload.bin|${LOW_CONNECTIONS}"
    "mixed-traffic|高并发|/,/api/users,/api/users/1001/orders/90001?page=2&limit=20&sort=name,/,/page.html,/api/users,/,/payload.bin,/page.html|${HIGH_CONNECTIONS}"
)

for ((repeat = 1; repeat <= REPEATS; ++repeat)); do
    offset=$(( (repeat - 1) % ${#CONFIGURATIONS[@]} ))
    for ((index = 0; index < ${#CONFIGURATIONS[@]}; ++index)); do
        config_index=$(( (index + offset) % ${#CONFIGURATIONS[@]} ))
        IFS='|' read -r server optimization <<< "${CONFIGURATIONS[config_index]}"
        for scenario_config in "${SCENARIOS[@]}"; do
            IFS='|' read -r scenario profile paths connections <<< "${scenario_config}"
            run_one "${server}" "${optimization}" "${scenario}" "${profile}" \
                "${paths}" "${connections}" "${repeat}"
        done
    done
done

if [[ "${BENCH_WS:-0}" == 1 ]]; then
    start_server FastAPI 运行时
    python3 "${SCRIPT_DIR}/ws_bench.py" --uri "ws://127.0.0.1:${PORT}/ws" \
        --server FastAPI --repeats "${REPEATS}" | tee -a "${RESULT_DIR}/results.jsonl"
    stop_server
fi

python3 "${SCRIPT_DIR}/report.py" \
    --input "${RESULT_DIR}/results.jsonl" \
    --output "${RESULT_DIR}/index.html" \
    --versions "${BUILD_DIR}/versions.txt" \
    --workers "${WORKERS}" --wrk-threads "${WRK_THREADS}" \
    --connections "${CONNECTIONS}" --low-connections "${LOW_CONNECTIONS}" \
    --high-connections "${HIGH_CONNECTIONS}" --duration "${DURATION}" \
    --warmup "${WARMUP}" --repeats "${REPEATS}" \
    --server-cpus "${SERVER_CPUS}" --client-cpus "${CLIENT_CPUS}"

printf 'HTML report: %s\n' "${RESULT_DIR}/index.html"
