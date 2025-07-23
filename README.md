<h1 align="center" style="color:yellow">HXLibs</h1>

## 一、概述

> [!TIP]
> main分支正在重构中... 如果需要学习、使用, 可以切换到: [v8.0-old-main](https://github.com/HengXin666/HXLibs/tree/v8.0-old-main) 稳定版本
>
> 新版本完全重构, 目前还有一些以前支持的东西没有迁移过来...

HXLibs 是一个现代 C++ 库. 目前集合了:

- 基于 `io_uring` / `IOCP` 协程的 Http / WebSocket 客户端与服务端, 服务器支持**分块编码**/**断点续传**! 客户端支持 `Socks5` 代理.

> 本地测试 500MB 文件传输吞吐量高达 20 GB/s; wrk 压测 Http 请求并发高达 200w+ Requests/sec

- WebSocket 服务端接口学习了 FastApi 思想, 使用简单.

- 支持聚合类反射, 轻松支持 Json 的序列化与反序列化!

## 二、构建要求

- Linux 5.1+ 
- GCC / Clang 编译器
- C++20

## 三、快速开始
> [!TIP]
> 仍然在开发, 非最终产品

### 3.1 HX::net (网络模块)
#### 3.1.1 编写服务端

下面是一个简单的服务端示例: ([examples/HttpServer/01_http_server.cpp](examples/HttpServer/01_http_server.cpp))

```cpp
#include <HXLibs/net/Api.hpp>
#include <HXLibs/log/Log.hpp>

using namespace HX;
using namespace net;

int main() {
    HttpServer ser{"127.0.0.1", "28205"}; // 定义服务器
    ser.addEndpoint<GET>("/user/{id}", [] ENDPOINT {
        co_await res.setStatusAndContent(
            Status::CODE_200,
            "<h1>Hello" 
            + req.getPathParam(0) // 获取 {id} 路径参数
            + "</h1>"
            + utils::DateTimeFormat::formatWithMilli()
        ).sendRes();
        co_return;
    }, TimeLog{})
    .addEndpoint<GET>("/stop", [&] ENDPOINT {
        // 关闭服务器
        co_await res.setStatusAndContent(
            Status::CODE_200, "<h1>stop server!</h1>" + utils::DateTimeFormat::formatWithMilli())
                    .sendRes();
        req.addHeaders("Connection", "close");
        ser.asyncStop();
        co_return;
    })
    .addEndpoint<GET>("/favicon.ico", [] ENDPOINT {
        log::hxLog.debug("get .ico");
        co_return co_await res.useRangeTransferFile( // 传输单个文件
            req.getRangeRequestView(),
            "static/favicon.ico"
        );
    })
    .addEndpoint<GET, HEAD>("/files/**", [] ENDPOINT {
        using namespace std::string_literals;
        using namespace std::string_view_literals;
        auto path = req.getUniversalWildcardPath();
        log::hxLog.debug("请求:", path, "| 断点续传:", req.getReqType() == "HEAD"sv || req.getHeaders().contains("range"));
        try {
            // 使用断点续传传输文件 (匹配到 `static/bigFile/**` 路径)
            co_await res.useRangeTransferFile(
                req.getRangeRequestView(),
                "static/bigFile/"s + std::string{path.data(), path.size()}
            );
            log::hxLog.debug("发送完毕!");
        } catch (std::exception const& ec) {
            // 协程支持异常
            log::hxLog.error(__FILE__, __LINE__, ":", ec.what());
        }
        co_return ;
    });
    ser.syncRun(); // 启动服务器, 并且阻塞
}
```

#### 3.1.2 编写客户端

下面是一个简单的客户端示例: ([tests/client/01_http_client.cpp](tests/client/01_http_client.cpp))

> [!TIP]
> 客户端的 http 请求都提供了`协程`和`异步`两种接口, 后者可以通过 `.get()` 阻塞等待获取 `返回值`, 以变为同步接口 (如下示例)

```cpp
#include <HXLibs/net/client/HttpClient.hpp>

#include <HXLibs/log/Log.hpp>

using namespace HX;
using namespace net;

// 协程接口
coroutine::Task<> coMain() {
    HttpClient cli{};
    log::hxLog.debug("开始请求");
    ResponseData res = co_await cli.coGet("http://httpbin.org/get");
    log::hxLog.info("状态码:", res.status);
    log::hxLog.info("拿到了 头:", res.headers);
    log::hxLog.info("拿到了 体:", res.body);
}

int main() {
    coMain().start();

    // 同步接口
    HttpClient cli{};
    auto res = cli.get("http://httpbin.org/get").get(); // 后面的 .get() 是阻塞获取返回值
                                                        // 如果不 .get(), 那么它实际上是异步的 (内部有线程执行)
    log::hxLog.info("状态码:", res.status);
    log::hxLog.info("拿到了 头:", res.headers);
    log::hxLog.info("拿到了 体:", res.body);
    return 0;
}

void test() {
    // 使用代理
    HttpClient cli{HttpClientOptions{{"socks5://127.0.0.1:2334"}}};
    auto res = cli.get("http://httpbin.org/get").get();
    log::hxLog.info("状态码:", res.status);
    log::hxLog.info("拿到了 头:", res.headers);
    log::hxLog.info("拿到了 体:", res.body);
    return 0;
}
```

#### 3.1.3 WebSocket 客户端与服务端

在 **服务端**, 我们提供了类似于 FastApi 的 WebSocket 接口, 使用非常方便!

> 详细内容请看: [tests/server/02_ws_server.cpp](tests/server/02_ws_server.cpp), 里面还提供了 WebSocket **消息群发** 的代码示例.

```cpp
HttpServer serv{"127.0.0.1", "28205"};
serv.addEndpoint<GET>("/ws", [] ENDPOINT {
    auto ws = co_await WebSocketFactory::accept(req, res); // 升级为 WebSocket
    struct JsonDataVo {
        std::string msg;
        int code;
    };
    JsonDataVo const vo{"Hello 客户端, 我只能通信3次!", 200};
    co_await ws.sendJson(vo); // WebSocket 发送 Json 数据 (基于聚合类反射)
    for (int i = 0; i < 3; ++i) {
        auto res = co_await ws.recvText(); // 等待读取文本
                                           // [!] 特别的: 如果客户端发送了 ping, 其内部会自动回应 pong
                                           //     并且不会阻断 recvText (也就是无感知的)
                                           //     同理, 如果客户端发送 close, 则内部也会响应 close
                                           //     关闭 ws 后, 会抛出 {已经安全关闭连接} 的异常
                                           // [!] 如果读取的数据格式不对, 也会抛异常: 比如期望是 Test, 但是读取到二进制
        log::hxLog.info(res);
        co_await ws.sendText("Hello! " + res); // 发送文本
    }
    co_await ws.close(); // 主动关闭 WebSocket 连接
    log::hxLog.info("断开ws");
    co_return ;
});
```

在 **客户端**, 我们提供的是协程回调的 WebSocket, 并且同样提供了`协程`和`异步`两种接口: ([tests/client/02_ws_client.cpp](tests/client/02_ws_client.cpp))

> [!TIP]
> 为什么是 **协程回调**?
>
> 实际上重点是`回调`, 内部会传入一个 `WebSocketClient` 供用户使用, 而将它作为回调是为了安全的控制 WebSocket 的生命周期. 以防止它被用户滥用.

```cpp
coroutine::Task<> coMain() {
    HttpClient cli{};
    co_await cli.coWsLoop("ws://127.0.0.1:28205/ws",
        [](WebSocketClient ws) -> coroutine::Task<> {
            co_await ws.sendText("hello");
            auto msg = co_await ws.recvText();
            log::hxLog.info("收到: ", msg);
            co_await ws.close();
        }
    );
}

int main() {
    // 协程
    coMain().start();

    // 同步
    HttpClient cli{};
    cli.wsLoop("ws://127.0.0.1:28205/ws",
        [](WebSocketClient ws) -> coroutine::Task<> {
            co_await ws.sendText("hello 我是同步接口的协程回调");
            auto msg = co_await ws.recvText();
            log::hxLog.info("收到: ", msg);
            co_await ws.close();
        }
    ).wait();
    return 0;
}
```

### 3.2 HX::coroutine (协程模块)
#### 3.2.1 Task (默认协程)

- `T`: 返回值类型
- `P`: 协程控制者 (默认的`Promise<T>`是懒启动, 以及 `co_return` 可以恢复到之前的协程调用者)
- `Awaiter`: 则是指定 `co_await` 的功能

其中的 `start` 方法很危险, **除非你可以保证它可以执行到 co_return 并且在此之前不会返回, 否则不要调用该方法**.

```cpp
template <
    typename T = void,
    typename P = Promise<T>,
    typename Awaiter = ExitAwaiter<T, P>
>
struct [[nodiscard]] Task {
    /**
     * @brief 立即执行协程
     * @warning 除非你可以保证它可以执行到 co_return 并且在此之前不会返回, 否则不要调用该方法
     * @return constexpr auto 
     */
    constexpr auto start() const {
        _handle.resume();
        if constexpr (requires {
            _handle.promise().result(); // Promise<T> 存在的
        }) {
            if (_handle.done()) [[likely]] {
                return _handle.promise().result();
            } else {
                // 不是期望的! 协程还没有执行完毕
                throw std::runtime_error{"The collaborative process has not been completed yet"};
            }
        }
    }
};
```

#### 3.2.2 RootTask (根协程)

它可以让自己从当前协程中分离出来, 变成一个独立的协程 "根协程" 被调度.

```cpp
// 使用示例: include/HXLibs/net/server/ConnectionHandler.hpp
coroutine::Task<> start(std::atomic_bool const& isRun) {
    auto serverFd = co_await makeServerFd();
    for (;;) [[likely]] {
        auto fd = exception::IoUringErrorHandlingTools::check(
            co_await _eventLoop.makeAioTask().prepAccept(
                serverFd,
                nullptr,
                nullptr,
                0
            )
        );
        log::hxLog.debug("有新的连接:", fd);

        // 直接从当前协程分离
        ConnectionHandler::start<Timeout>(fd, isRun, _router, _eventLoop).detach();
    }
}
```

#### 3.2.3 WhenAny (Awaiter)

它可以同时启动所有的协程, 并且返回第一个完成的那个

```cpp
// 常见使用场景: 超时
auto res = co_await whenAny(协程_01(), 定时器());
if (res.index() == 1)
    log::hxLog.debug("超时:");
```

#### 3.2.4 协程调度器

见 [EventLoop.hpp](include/HXLibs/coroutine/loop/EventLoop.hpp) (Linux基于`io_uring`实现, Win基于`IOCP`实现)

以及协程定时器 (基于红黑树实现快速删除 (析构时候直接通过迭代器删除, 无需查找)) [TimerLoop.hpp](include/HXLibs/coroutine/loop/TimerLoop.hpp)

### 3.3 HX::reflection (反射模块)
#### 3.3.1 反射模板

```cpp
#include <HXLibs/reflection/MemberName.hpp>

struct Test {
    int a;
    std::string name;
};

constexpr auto Cnt = reflection::membersCountVal<Test>;
static_assert(Cnt == 2, ""); // 编译期获取 聚合类 成员个数

constexpr auto Name = reflection::getMembersNames<Test>();
static_assert(Name[0] == "a", "");
static_assert(Name[1] == "name", ""); // 编译期获取 聚合类 成员字段

// (部分)编译期 for 循环
Test obj;
reflection::forEach(obj, [&] <std::size_t I> (
    std::index_sequence<I>, // 字段索引 I (编译器字面量)
    auto name,              // 字段名称 std::string_view
    auto /*const*/& val     // 字段引用 (如果传入的是 Test const&, 那么对应的 val 也是 auto const&)
) {
    if constexpr (I == 0)
        val = 0;
    if constexpr (I == 1)
        val = "index = 1";
});
```

#### 3.3.2 基于反射的Json序列化与反序列化

```cpp
#include <HXLibs/reflection/json/JsonRead.hpp>
#include <HXLibs/reflection/json/JsonWrite.hpp>
#include <HXLibs/log/Log.hpp>

using namespace HX;

TEST_CASE("json 序列化") {
    struct Cat {
        std::string name;
        int num;
        std::vector<Cat> sub;
    };
    Cat cat = {
        "咪咪",
        2233,
        {{
            "明卡",
            233,
            {{
                "大猫",
                666,
                {}
            }}
        }, {
            "喵喵",
            114514,
            {{}}
        }}
    };
    std::string catStr;
    reflection::toJson(cat, catStr);
    log::hxLog.info("元素:", cat);
    log::hxLog.info("序列化字符串:", catStr);
    CHECK(catStr == R"({"name":"咪咪","num":2233,"sub":[{"name":"明卡","num":233,"sub":[{"name":"大猫","num":666,"sub":[]}]},{"name":"喵喵","num":114514,"sub":[{"name":"","num":0,"sub":[]}]}]})");
}

TEST_CASE("json 反序列化") {
    struct A {
        bool a;
        std::vector<std::vector<std::string>> b;
        std::unordered_map<std::string, std::string> c;
        std::optional<std::string> d;
        std::shared_ptr<std::string> e;
        double f;
    } t{};

    reflection::fromJson(t, R"({
        "f": -3.1415926E+10,
        "d": null,
        "a": 0,
        "c": {
            "": "",
            "中文Key": "中文Value",
            "特殊字符!@#$%^&*()_+": "特殊Value",
            "换行": "第一行\n第二行",
            "制表符": "\tTabTest"
        },
        "b": [
            [],
            ["单独一个"],
            ["多行\n字符串", "特殊字符\"\\\'", "空字符串", "\u4E2D\u6587"]
        ],
        "e": "HelloPtr"
    })");

    // 断言检查
    REQUIRE(t.a == false);
    REQUIRE(t.f == -3.1415926E+10);
    REQUIRE(!t.d.has_value());
    REQUIRE(t.e != nullptr);
    REQUIRE(*t.e == "HelloPtr");

    REQUIRE(t.c.at("") == "");
    REQUIRE(t.c.at("中文Key") == "中文Value");
    REQUIRE(t.c.at("特殊字符!@#$%^&*()_+") == "特殊Value");
    REQUIRE(t.c.at("换行") == "第一行\n第二行");
    REQUIRE(t.c.at("制表符") == "\tTabTest");

    REQUIRE(t.b.size() == 3);
    REQUIRE(t.b[0].empty());
    REQUIRE(t.b[1].size() == 1);
    REQUIRE(t.b[1][0] == "单独一个");
    REQUIRE(t.b[2].size() == 4);
    REQUIRE(t.b[2][0] == "多行\n字符串");
    REQUIRE(t.b[2][1] == "特殊字符\"\\\'");
    REQUIRE(t.b[2][2] != "");
    REQUIRE(t.b[2][3] == "中文");
}
```

### 3.4 HX::log (日志模块)

> 目前实现比较简单, 只是提供了颜色; 日后会支持协程、分线程的异步模式..
>
> 输出自带格式化, 特别是对于键值对和聚合类, 默认带空格 (类似于 `QDebug()`)

@todo

## 四、相关依赖

|依赖库|说明|备注|
|---|---|---|
|liburing|io_uring的封装|https://github.com/axboe/liburing|
|OpenSSL 3.3.1+|用于https的证书/握手|https://github.com/openssl/openssl|

## 五、性能测试
> `@todo` 应该更新性能测试

> [!TIP]
> - Arth Linux
> - 13th Gen Intel(R) Core(TM) i9-13980HX
> - RAM: 64GB
> - cmake -> Release

- [断点续传的测试](./documents/UseRangeTransferFile.md)

- http
```sh
# build: add_definitions(-DCOMPILE_WEB_SOCKET_SERVER_MAIN)  # websocket服务端

# http 没有文件读写的情况下:
➜ wrk -t32 -c1100 http://127.0.0.1:28205/home/123/123
Running 10s test @ http://127.0.0.1:28205/home/123/123
  32 threads and 1100 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.48ms    2.52ms  36.60ms   86.18%
    Req/Sec    77.98k    16.30k  241.91k    74.07%
  23512408 requests in 10.10s, 4.51GB read
  Socket errors: connect 99, read 0, write 0, timeout 0
Requests/sec: 2327117.67
Transfer/sec:    457.18MB

# http 读写 static/WebSocketIndex.html
➜ wrk -t32 -c1100 http://127.0.0.1:28205/            
Running 10s test @ http://127.0.0.1:28205/
  32 threads and 1100 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.18ms    1.63ms  23.96ms   86.39%
    Req/Sec    45.53k    13.36k  263.46k    89.82%
  14176629 requests in 10.09s, 43.81GB read
  Socket errors: connect 99, read 0, write 0, timeout 0
Requests/sec: 1405535.24
Transfer/sec:      4.34GB
```

- https
```sh
# build: add_definitions(-DHTTPS_FILE_SERVER_MAIN)  # https简单的文件服务器
# 无文件读写, 仅仅https连接和通过会话密钥加解密通信
➜ wrk -d10s -t32 -c1000 --timeout 5s https://127.0.0.1:28205
Running 10s test @ https://127.0.0.1:28205
  32 threads and 1000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.04ms    3.21ms  70.65ms   86.96%
    Req/Sec    35.71k     6.56k   77.68k    70.85%
  11447330 requests in 10.10s, 1.97GB read
Requests/sec: 1133384.87
Transfer/sec:    199.96MB

# (100s) 小文件读写 (static/test/github.html) 563.9kb 采用分块编码, 但是需要https加密
➜ wrk -d100s -t32 -c1000 --timeout 5s https://127.0.0.1:28205/files/a
Running 2m test @ https://127.0.0.1:28205/files/a
  32 threads and 1000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    62.20ms   35.25ms 529.39ms   72.92%
    Req/Sec     1.01k   358.38     3.09k    65.19%
  3203315 requests in 1.67m, 843.28GB read
Requests/sec:  32001.06
Transfer/sec:      8.42GB

# (10s) 小文件读写 (static/test/github.html) 563.9kb 采用分块编码, 但是需要https加密
# 并且修改了 include/HXSTL/utils/FileUtils.h 定义的:
    /// @brief 读取文件buf数组的缓冲区大小
    static constexpr std::size_t kBufMaxSize = 4096U * 2; # 进行了 * 2
# 注: 这个是后来加上的测试, 也就是说其他的测试都是在`kBufMaxSize = 4096U`时候测试的, 因此这个只能和上面的形成对比
➜ wrk -d10s -t32 -c1000 https://127.0.0.1:28205/files/a
Running 10s test @ https://127.0.0.1:28205/files/a
  32 threads and 1000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    39.14ms   23.01ms 155.74ms   67.09%
    Req/Sec     1.60k   374.46     3.74k    69.51%
  516854 requests in 10.10s, 136.26GB read
Requests/sec:  51182.79
Transfer/sec:     13.49GB

# 对比 小文件读写 (static/test/github.html) 563.9kb `没有`采用分块编码, 但是需要https加密
➜ wrk -d10s -t32 -c1000 https://127.0.0.1:28205/test
Running 10s test @ https://127.0.0.1:28205/test
  32 threads and 1000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   105.38ms   41.80ms 427.97ms   69.93%
    Req/Sec   294.52     43.68   425.00     74.12%
  94528 requests in 10.10s, 49.77GB read
Requests/sec:   9360.72
Transfer/sec:      4.93GB
```

## 六、特别感谢
### 6.1 Stargazers
[![Stargazers repo roster for @HengXin666/HXLibs](https://reporoster.com/stars/dark/HengXin666/HXLibs)](https://github.com/HengXin666/HXLibs/stargazers)

### 6.2 Forkers
[![Forkers repo roster for @HengXin666/HXLibs](https://reporoster.com/forks/dark/HengXin666/HXLibs)](https://github.com/HengXin666/HXLibs/network/members)

## 七、杂项
### 7.1 代码规范
> --> [C++ 编码规范](documents/CodingStandards/CppStyle.md)

### 7.2 开发计划
> --> [开发计划](documents/DevelopmentPlan.md)

### 7.3 开发日志
> --> [开发日志](documents/DevelopmentLog.md)