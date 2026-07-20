# HTTP 网络压测说明

通过手动触发 `.github/workflows/http-performance.yml` 运行固定环境的 HTTP 压测。工作流使用固定源码提交，并把 runner 当前允许使用的 CPU 动态分成两组：一半运行服务端，一半运行 wrk。服务端 worker 和 wrk 线程数随 CPU 配额自动调整，避免固定为 2 个线程而浪费 runner。每轮先预热 15 秒，再正式采样 60 秒（可在手动触发时调整），默认重复 3 次。完整的 10 配置 × 6 场景矩阵预计运行约 4 小时，可在 GitHub 托管 Job 的 6 小时上限内完成；单轮 60 秒足以覆盖 CPU 频率变化、连接复用和缓存热身。

原生实现使用 Clang 分别以 `-O2 -DNDEBUG` 和 `-O3 -DNDEBUG` 构建、运行。FastAPI 与 Spring Boot 没有对应的 Clang 优化等级，因此各自只作为“运行时”基线测试一次。服务端与客户端始终绑定在不重叠的 CPU 集合上；FastAPI worker 和 JVM 也使用相同服务端 CPU 配额。

## 对照库

| 实现 | 类型 | 测试程序 |
|---|---|---|
| HXLibs | 本项目 HTTP 协程服务器 | `bench-hxlibs` |
| Asio | Boost.Asio 的独立头文件版, 异步 I/O 基础库 | `bench-asio` |
| yalantinglibs | Alibaba C++ 协程 HTTP 框架(cinatra) | `bench-yalantinglibs` |
| nginx | 非 C++、成熟事件驱动 HTTP 服务器基线 | nginx |
| Spring Boot | Java Web 框架 | `spring-app` |
| FastAPI | Python ASGI Web 框架 | `fastapi_server.py` |

## 场景与负载范围

| 场景 | 路径 / 流量构成 | 默认负载 | 关注点 |
|---|---|---|---|
| 短响应 | `/`，12 字节纯文本 | 低并发（worker × 8） | 调度、解析和框架固定开销 |
| JSON API | `/api/users` | 标准并发（worker × 128，至少 256） | 常见 API 响应 |
| 动态路由与参数 | `/api/users/{userId}/orders/{orderId}?page=…&limit=…&sort=…` | 标准并发 | 多级路由匹配、两个路径参数与三个查询参数解析 |
| HTML 文件 | `/page.html` | 标准并发 | 从文件系统读取并传输完整 HTML/CSS 页面 |
| 64 KiB 文件 | `/payload.bin` | 低并发 | 从文件系统读取、分块发送与内存复制 |
| 混合流量 | 按固定比例轮换上述路径 | 高并发（标准并发 × 2，最多 8192） | 更接近现实的请求尺寸与路由混合 |

每个实现提供相同路径。HTML 和 64 KiB 响应均来自构建目录中的真实文件；HXLibs 使用协程 `useRangeTransferFile` API，其他实现使用各自的文件响应能力。长时重复访问后操作系统页缓存通常会命中，但每次请求仍执行文件打开/读取/发送路径，这正是静态文件服务的现实热缓存场景。动态路由用真实 path parameter 与 query parameter API 解析参数并构造响应。

报告支持低并发、标准并发、高并发三种统计范围，也可选择“全部负载（分别展示）”快速查看同一场景的完整矩阵。排名及吞吐/P99 差距只在相同场景、负载范围和连接数内计算，不会把不同响应尺寸混成一张总榜。报告包含中位数、P10–P90、最小–最大、均值、标准差、变异系数（CV）、P50/P90/P99/P99.9 延迟、带宽、错误率、相对最优差距，以及 O3 相对 O2 的变化。

nginx 是成熟事件驱动 HTTP 服务器参考基线，不表示 API 或功能完全等价。可选 WebSocket 回显测试仅用于补充观察，因当前没有覆盖所有对照实现，默认关闭，不能用于横向排名。

## 本地运行

```bash
benchmarks/http/scripts/build.sh
BENCH_DURATION=30 BENCH_REPEATS=3 benchmarks/http/scripts/run.sh
open .benchmark-build/results/index.html
```

可通过 `BENCH_WORKERS`、`BENCH_WRK_THREADS`、`BENCH_CONNECTIONS`、`BENCH_LOW_CONNECTIONS`、`BENCH_HIGH_CONNECTIONS`、`BENCH_WARMUP` 和 `BENCH_SERVER_CPUS` 覆盖自动参数。完整默认矩阵耗时较长，这是为了让 JIT、CPU 频率和连接状态充分稳定；本地冒烟时可以临时缩短采样时间和重复次数。

不要直接比较不同机器或不同参数产生的数字。压测结果受 CPU 频率、虚拟化、内核和后台负载影响，适合比较同一次工作流中产生的相对结果。HTML 报告会保留编译器、内核、CPU、内存和亲和性信息，便于确认实验环境。
