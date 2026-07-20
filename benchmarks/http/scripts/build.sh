#!/usr/bin/env bash
set -euo pipefail

readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly BENCH_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
readonly REPO_DIR="$(cd -- "${BENCH_DIR}/../.." && pwd)"
readonly BUILD_DIR="${BENCH_BUILD_DIR:-${REPO_DIR}/.benchmark-build}"
readonly SOURCE_DIR="${BUILD_DIR}/sources"
readonly BIN_DIR="${BUILD_DIR}/bin"
readonly OPTIMIZATIONS=(O2 O3)

readonly ASIO_REPOSITORY="https://github.com/chriskohlhoff/asio.git"
readonly ASIO_COMMIT="231cb29bab30f82712fcd54faaea42424cc6e710"
readonly YLT_REPOSITORY="https://github.com/alibaba/yalantinglibs.git"
readonly YLT_COMMIT="225f4d4805145b9bba73a618edd16312e1339369"
readonly NGINX_REPOSITORY="https://github.com/nginx/nginx.git"
readonly NGINX_COMMIT="481d28cb4e04c8096b9b6134856891dc52ecc68f"
readonly WRK_REPOSITORY="https://github.com/wg/wrk.git"
readonly WRK_COMMIT="a211dd5a7050b1f9e8a9870b95513060e72ac4a0"

fetch_source() {
    local name="$1"
    local repository="$2"
    local commit="$3"
    local destination="${SOURCE_DIR}/${name}"

    if [[ ! -d "${destination}/.git" ]]; then
        git clone --filter=blob:none --no-checkout "${repository}" "${destination}"
    fi
    git -C "${destination}" fetch --depth 1 origin "${commit}"
    git -C "${destination}" checkout --detach --force "${commit}"
}

mkdir -p "${SOURCE_DIR}" "${BIN_DIR}"
mkdir -p "${BUILD_DIR}/assets"
cp "${BENCH_DIR}/assets/page.html" "${BUILD_DIR}/assets/page.html"
python3 - "${BUILD_DIR}/assets/payload.bin" <<'PY'
import pathlib, sys
pathlib.Path(sys.argv[1]).write_bytes(b'x' * (64 * 1024))
PY

for command in clang clang++ cmake ninja; do
    if ! command -v "${command}" >/dev/null 2>&1; then
        printf 'Missing required command: %s\n' "${command}" >&2
        exit 1
    fi
done

fetch_source asio "${ASIO_REPOSITORY}" "${ASIO_COMMIT}"
fetch_source yalantinglibs "${YLT_REPOSITORY}" "${YLT_COMMIT}"
fetch_source nginx "${NGINX_REPOSITORY}" "${NGINX_COMMIT}"
fetch_source wrk "${WRK_REPOSITORY}" "${WRK_COMMIT}"

for optimization in "${OPTIMIZATIONS[@]}"; do
    cmake -S "${BENCH_DIR}" -B "${BUILD_DIR}/cmake-${optimization}" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER=clang++ \
        -DCMAKE_CXX_FLAGS_RELEASE="-${optimization} -DNDEBUG" \
        -DASIO_SOURCE_DIR="${SOURCE_DIR}/asio" \
        -DYLT_SOURCE_DIR="${SOURCE_DIR}/yalantinglibs"
    cmake --build "${BUILD_DIR}/cmake-${optimization}" --parallel --target \
        bench-hxlibs bench-asio bench-yalantinglibs

    mkdir -p "${BIN_DIR}/${optimization}"
    for executable in bench-hxlibs bench-asio bench-yalantinglibs; do
        cp "${BUILD_DIR}/cmake-${optimization}/bin/${executable}" \
            "${BIN_DIR}/${optimization}/${executable}"
    done
done

make -C "${SOURCE_DIR}/wrk" clean >/dev/null 2>&1 || true
make -C "${SOURCE_DIR}/wrk" -j"$(nproc)" CC=clang CFLAGS="-O3 -DNDEBUG"
cp "${SOURCE_DIR}/wrk/wrk" "${BIN_DIR}/wrk"

(cd "${BENCH_DIR}/servers/spring-app" && mvn -q -DskipTests package)

for optimization in "${OPTIMIZATIONS[@]}"; do
    nginx_prefix="${BUILD_DIR}/nginx-${optimization}"
    (
        cd "${SOURCE_DIR}/nginx"
        make clean >/dev/null 2>&1 || true
        ./auto/configure \
            --prefix="${nginx_prefix}" \
            --without-http_gzip_module \
            --with-cc=clang \
            --with-cc-opt="-${optimization} -DNDEBUG"
        make -j"$(nproc)"
        make install
    )
done

{
    printf 'HXLibs=%s\n' "$(git -C "${REPO_DIR}" rev-parse HEAD 2>/dev/null || printf working-tree)"
    printf 'Asio=%s (asio-1-36-0)\n' "${ASIO_COMMIT}"
    printf 'yalantinglibs=%s (0.5.0)\n' "${YLT_COMMIT}"
    printf 'nginx=%s (release-1.28.0)\n' "${NGINX_COMMIT}"
    printf 'wrk=%s (4.2.0)\n' "${WRK_COMMIT}"
    clang --version | head -n 1
    clang++ --version | head -n 1
    cmake --version | head -n 1
    "${BUILD_DIR}/nginx-O2/sbin/nginx" -v 2>&1
    printf '\n--- 运行环境 ---\n'
    uname -srmo
    lscpu
    printf '可用 CPU: '
    python3 - <<'PY'
import os
print(','.join(map(str, sorted(os.sched_getaffinity(0)))))
PY
    awk '/MemTotal/ {printf "内存: %.2f GiB\\n", $2 / 1024 / 1024}' /proc/meminfo
} > "${BUILD_DIR}/versions.txt"

printf 'Benchmark binaries are in %s\n' "${BIN_DIR}"
