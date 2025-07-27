<h1 align="center" style="color:yellow">HXLibs</h1>

## 一、概述

> [!TIP]
> main分支正在重构中... 如果需要学习、使用, 可以切换到: [v8.0-old-main](https://github.com/HengXin666/HXLibs/tree/v8.0-old-main) 稳定版本
>
> 新版本完全重构, 目前还有一些以前支持的东西没有迁移过来...

HXLibs 是一个现代 C++ 库. 目前集合了:

- 基于 `io_uring` / `IOCP` 协程的 Http / WebSocket 客户端与服务端, 服务器支持**分块编码**/**断点续传**/**超时机制**! 客户端支持 `Socks5` 代理.

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
#include <HXLibs/net/Api.hpp> // 包含了 端点宏 ENDPOINT
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
        // 支持手动关闭服务器
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

    using namespace utils;
    ser.syncRun(16, 30_s); // 启动服务器, 并且阻塞
                           // 其中第一个参数是线程个数, 每个线程独立维护一个 事件循环
                           // 一个事件循环控制 该线程的所有协程
                           // 第二个参数是超时时间, 超时会自动断开连接 (从离开端点函数时候开始计算)
                           // 注意: 其时间类型定义在 include/HXLibs/utils/TimeNTTP.hpp 中, 并非标准库的时间
}
```

其中, `ENDPOINT` 只是一种简写, 其被定义为:

```cpp
/**
 * @brief 定义标准的端点, 请求使用`req`变量, 响应使用`res`变量
 */
#define ENDPOINT (                           \
    [[maybe_unused]] HX::net::Request& req,  \
    [[maybe_unused]] HX::net::Response& res  \
) -> HX::coroutine::Task<>
```

特别的, 它还支持`面向切面`编程:

```cpp
enum class ServerStatus : uint32_t {
    Error = 0,
    Ok = 1
};

struct TimeLog { // 计时器切面: 端点访问时间间隔
    decltype(std::chrono::steady_clock::now()) t;

    // 进入端点之前
    auto before(Request&, Response&) {
        t = std::chrono::steady_clock::now();
        return ServerStatus::Ok; // 返回值只要可以转换为 bool 类型, 就是合法的
                                 // 如果返回 bool{false}, 那么就会终止连接 (类似于`拦截器`)
    }

    // 端点结束之后
    auto after(Request& req, Response&) {
        auto t1 = std::chrono::steady_clock::now();
        auto dt = t1 - t;
        int64_t us = std::chrono::duration_cast<std::chrono::milliseconds>(dt).count();
        log::hxLog.info("已响应: ", req.getPureReqPath(), "花费: ", us, " us");
        return true;
    }
};

HttpServer ser{"127.0.0.1", "28205"};
ser.addEndpoint<GET>("/", [] ENDPOINT {
    co_await res.setStatusAndContent(
        Status::CODE_200, "<h1>这是 HXLibs::net 服务器</h1>" + utils::DateTimeFormat::formatWithMilli())
                .sendRes();
    co_return;
}, TimeLog{}) // <-- 面向切面编程, 可以编写任意个切面, 只需要在此传参即可
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

#### 3.3.2 使用宏进行反射

在 `HXLibs/reflection/ReflectionMacro.hpp` 中, 我们支持用户使用宏来生成反射代码, 它可以反射私有字段:

> `@todo`
> - 类外反射
> - 字段支持通过宏来重命名a

```cpp
#include <HXLibs/reflection/ReflectionMacro.hpp> // 头文件

struct HXTest {
private:
    int a{};
    std::string b{};
    double c{};
public:
    HX_REFL(a, b) // 指定 反射 a, b, 而可以不反射 c
};

#include <HXLibs/reflection/json/JsonRead.hpp>
#include <HXLibs/reflection/json/JsonWrite.hpp>

TEST_CASE("宏反射内私有成员") {
    using namespace HX;
    [[maybe_unused]] constexpr auto N = reflection::membersCountVal<HXTest>;
    constexpr auto name = reflection::getMembersNames<HXTest>();
    static_assert(name[0] == "a", ""); // 依旧是编译期反射
    
    HXTest t{};
    [[maybe_unused]] auto tr = reflection::internal::getObjTie(t);
    [[maybe_unused]] auto res = HXTest::visit(t);
    [[maybe_unused]] auto cnt = reflection::HasReflectionCount<HXTest const&>;

    // 同样支持 forEach
    reflection::forEach(t, [] <std::size_t Idx> (std::index_sequence<Idx>, auto name, auto& v) {
        if constexpr (Idx == 0) {
            v = 2233;
        } else if constexpr (Idx == 1) {
            v = "666";
        }
        log::hxLog.info(Idx, name, v);
    });

    // 只要注册了, 就可以随意使用 json / log 以反射的实现 (前提是该字段类型是支持的)
    HXTest newT;
    std::string s;
    reflection::toJson(t, s);
    reflection::fromJson(newT, s);
    log::hxLog.info(newT);
}
```

#### 3.3.3 基于反射的Json序列化与反序列化

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

## 五、性能测试 (服务端)
> [!TIP]
> - Arth Linux
> - 13th Gen Intel(R) Core(TM) i9-13980HX
> - RAM: 64GB
> - cmake -> Release (选项 `--config Release`)
> - 测试代码: [benchmarks/01_server.cpp](examples/benchmarks/01_server.cpp)
> - 编译器: Clang 19.1.7 x86_64-pc-linux-gnu

- [断点续传的测试](./documents/UseRangeTransferFile.md)

- http

```sh
# 全程笔记本 CPU 最高温 85度左右, 最高核心频率都在 3.5GHz

# 纯内存回复 Hello World!
╰─ wrk -d10s -t32 -c1000 http://127.0.0.1:28205/
Running 10s test @ http://127.0.0.1:28205/
  32 threads and 1000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.84ms   10.60ms 250.54ms   94.92%
    Req/Sec    73.90k    59.46k  203.26k    63.13%
  22970587 requests in 10.10s, 2.91GB read
  Socket errors: connect 3, read 0, write 0, timeout 0
Requests/sec: 2274737.83
Transfer/sec:    295.03MB

# 涉及I/O读写小文件 (558KB)
╰─ wrk -d10s -t32 -c1000 http://127.0.0.1:28205/html/1
Running 10s test @ http://127.0.0.1:28205/html/1
  32 threads and 1000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    43.97ms   50.57ms 502.64ms   88.57%
    Req/Sec   599.57    282.34     1.47k    64.59%
  189702 requests in 10.10s, 100.39GB read
  Socket errors: connect 3, read 189702, write 0, timeout 0
Requests/sec:  18788.05
Transfer/sec:      9.94GB

# 涉及I/O读写大文件 (574.6 MB) 断点续传, 但是 wrk 并不会测试; 故退化为普通异步文件读写
╰─ wrk -d10s -t32 -c100 http://127.0.0.1:28205/mp4
Running 10s test @ http://127.0.0.1:28205/mp4
  32 threads and 100 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   947.45ms  451.35ms   1.98s    65.85%
    Req/Sec     2.27      2.96    20.00     90.68%
  376 requests in 10.09s, 225.80GB read
  Socket errors: connect 0, read 376, write 0, timeout 171 # 可能: 有些请求未在 wrk 时限内完成 → 属于"还在下载中"
Requests/sec:     37.26   # 此为完成的请求个数, 不能通过此看效果, 因为显然有一些请求还在下载
Transfer/sec:     22.37GB
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