# HTTP 网络压测说明

通过手动触发 `.github/workflows/http-performance.yml` 运行固定环境的 HTTP 压测。工作流使用固定源码提交，并把 runner 当前允许使用的 CPU 动态分成两组：一半运行服务端，一半运行 wrk。服务端 worker 和 wrk 线程数随 CPU 配额自动调整，避免固定为 2 个线程而浪费 runner。

每个「实现 × 轮次」只启动一次服务器进程，然后连续压测全部 8 个场景（每个场景切换前预热 5 秒，正式采样 30 秒，均可在手动触发时调整），默认重复 3 轮，轮次间轮转实现顺序以摊平机器随时间的性能漂移。JIT 运行时（Spring Boot / FastAPI）在启动后还会先在全部路由上做一次约 20 秒的整体预热（`BENCH_RUNTIME_WARMUP`），保证 JIT 收敛后才开始计数。由于 O2/O3 结果几乎一致，默认矩阵只跑 O3（可在触发时改为 `O2 O3`），默认配置总耗时约 1.5 小时，远低于旧方案（每场景重启服务器 + 双优化矩阵）的约 4 小时。

原生实现使用 Clang 以 `-O3 -DNDEBUG`（可选加测 `-O2`）构建、运行。FastAPI 与 Spring Boot 没有对应的 Clang 优化等级，因此各自只作为“运行时”基线测试一次。服务端与客户端始终绑定在不重叠的 CPU 集合上；FastAPI worker 和 JVM 也使用相同服务端 CPU 配额。

## 对照库与最佳实践配置

每个实现都按其官方推荐的最佳实践方式参赛，避免因用错 API 而低估任何一方：

| 实现 | 类型 | 最佳实践要点 |
|---|---|---|
| HXLibs | 本项目 HTTP 协程服务器 | 内存变体走统一内存响应路径；磁盘变体用协程 `useRangeTransferFile` |
| Asio | Boost.Asio 的独立头文件版, 异步 I/O 基础库 | 启动时预生成完整 HTTP 响应字节，开启 `TCP_NODELAY` |
| yalantinglibs | Alibaba C++ 协程 HTTP 框架(cinatra) | `set_status_and_content_view` 零拷贝发送，静态资源内存化 |
| nginx | 非 C++、成熟事件驱动 HTTP 服务器基线 | `sendfile` + `tcp_nopush/tcp_nodelay` + `open_file_cache`，access log 关闭 |
| Spring Boot | Java Web 框架 | 静态资源预载为 `byte[]`，`max-keep-alive-requests=-1`（默认 100 会造成持续重连） |
| FastAPI | Python ASGI Web 框架 | `uvicorn[standard]`（uvloop + httptools）、orjson 序列化、关闭 access log、复用预构建 Response |

wrk 侧同样按最佳实践：单路径场景直接改写静态请求缓冲区（请求生成路径零 Lua 开销），混合路径场景在每线程 `init()` 中预编码全部请求。

## 场景与负载范围

| 场景 | 路径 / 流量构成 | 默认负载 | 关注点 |
|---|---|---|---|
| 短响应 | `/`，12 字节纯文本 | 低并发（worker × 8） | 调度、解析和框架固定开销 |
| JSON API | `/api/users` | 标准并发（worker × 128，至少 256） | 常见 API 响应 |
| 动态路由与参数 | `/api/users/{userId}/orders/{orderId}?page=…&limit=…&sort=…` | 标准并发 | 多级路由匹配、两个路径参数与三个查询参数解析 |
| HTML 页面（内存） | `/page.html` | 标准并发 | 完整 HTML/CSS 页面响应，静态内容预载内存 |
| HTML 页面（磁盘 IO） | `/page-file.html` | 标准并发 | 每请求经由各库自己的文件 API 真实读盘 |
| 64 KiB 响应（内存） | `/payload.bin` | 低并发 | 大响应发送与内存复制 |
| 64 KiB 文件（磁盘 IO） | `/payload-file.bin` | 低并发 | 各库文件传输路径：打开、读取、分块发送 |
| 混合流量 | 按固定比例轮换内存版路径 | 高并发（标准并发 × 2，最多 8192） | 更接近现实的请求尺寸与路由混合 |

每个实现提供相同路径。静态内容测两个变体：**内存变体**（`/page.html`、`/payload.bin`）在启动时读入内存，压测 HTTP 框架网络栈本身；**磁盘变体**（`/page-file.html`、`/payload-file.bin`）每请求走各库自己的文件 API——HXLibs 用协程 `useRangeTransferFile`，cinatra 用 `set_static_res_dir`（`coro_file` 异步读盘），FastAPI 用 `FileResponse`，Spring 用 `FileSystemResource`，裸 Asio 每请求同步读文件；nginx 的静态文件本就走 `sendfile` + 页缓存，两个变体同源，可作为文件服务的天花板参考。两个变体字节完全一致，差值即为「文件服务路径」的开销。动态路由用真实 path parameter 与 query parameter API 解析参数并构造响应。

## 报告与透明度

HTML 报告不只展示聚合值，每一轮的原始数据都单独可见：

- **逐轮吞吐曲线**：每个点是一轮完整采样的实际 RPS，每个实现一条线（颜色固定跟随实现，O2 用虚线区分），机器漂移和异常轮次一目了然；
- **延迟分位曲线**：P50 → P99.99 → Max 的完整分位曲线（对数轴），细线为单轮、粗线为跨轮中位，长尾行为直接可比；
- **表格**新增「各轮 RPS」列，聚合统计（中位数、P10–P90、最小–最大、均值 ± 标准差、CV）与原始值可互相印证；
- 图例可点击隐藏 / 显示实现，图上所有点带悬停提示。

排名及吞吐/P99 差距只在相同场景、负载范围和连接数内计算，不会把不同响应尺寸混成一张总榜。

nginx 是成熟事件驱动 HTTP 服务器参考基线，不表示 API 或功能完全等价。可选 WebSocket 回显测试仅用于补充观察，因当前没有覆盖所有对照实现，默认关闭，不能用于横向排名。

## 本地运行

```bash
benchmarks/http/scripts/build.sh
BENCH_DURATION=10 BENCH_REPEATS=2 benchmarks/http/scripts/run.sh
open .benchmark-build/results/index.html
```

可通过 `BENCH_WORKERS`、`BENCH_WRK_THREADS`、`BENCH_CONNECTIONS`、`BENCH_LOW_CONNECTIONS`、`BENCH_HIGH_CONNECTIONS`、`BENCH_WARMUP`、`BENCH_RUNTIME_WARMUP`、`BENCH_OPTIMIZATIONS`（默认 `O3`，构建与运行需一致）和 `BENCH_SERVER_CPUS` 覆盖自动参数。本地冒烟时可以临时缩短采样时间和重复次数。

不要直接比较不同机器或不同参数产生的数字。压测结果受 CPU 频率、虚拟化、内核和后台负载影响，适合比较同一次工作流中产生的相对结果。HTML 报告会保留编译器、内核、CPU、内存和亲和性信息，便于确认实验环境。
