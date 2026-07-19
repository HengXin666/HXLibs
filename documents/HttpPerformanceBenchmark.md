# HTTP 网络压测说明

通过手动触发 `.github/workflows/http-performance.yml` 运行固定环境的 HTTP 压测。工作流在 `ubuntu-24.04` 上使用 Release/O3、固定源码提交、wrk 客户端, 并将服务端与客户端绑定到不同 CPU; 每个服务先预热, 再按相同线程数、连接数、时长重复测试, 结果以 artifact 形式提供 `index.html`。

## 对照库

| 实现 | 类型 | 测试程序 |
|---|---|---|
| HXLibs | 本项目 HTTP 协程服务器 | `bench-hxlibs` |
| Asio | Boost.Asio 的独立头文件版, 异步 I/O 基础库 | `bench-asio` |
| yalantinglibs | Alibaba C++ 协程 HTTP 框架(cinatra) | `bench-yalantinglibs` |
| nginx | 非 C++、成熟事件驱动 HTTP 服务器基线 | nginx |
| Oat++ | C++ Web 框架 | `bench-oatpp` |
| Spring Boot | Java Web 框架 | `spring-app` |
| FastAPI | Python ASGI Web 框架 | `fastapi_server.py` |

HTTP 场景包括 `/`（Hello World）和 `/page.html`（普通 HTML）；WebSocket 使用 `/ws` 回显端点。nginx 作为非 C++ 参考基线, 不代表 API 或功能等价。

## 本地运行

```bash
benchmarks/http/scripts/build.sh
BENCH_DURATION=15 BENCH_REPEATS=3 benchmarks/http/scripts/run.sh
open .benchmark-build/results/index.html
```

不要直接比较不同机器或不同参数产生的数字; 压测结果受 CPU 频率、虚拟化、内核和后台负载影响, 适合用于同一次工作流中的相对比较。
