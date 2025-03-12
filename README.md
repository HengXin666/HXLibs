<h1 align="center" style="color:yellow">HXLibs</h1>

- 基于`io_uring`+协程的`http/https`服务器, 基于压缩前缀树的路由, 支持`http/https`解析, `WebSocket`协议, 支持`Transfer-Encoding`分块编码传输文件.

- 客户端提供了简单的协程的`request`方法(API), 并且支持使用`socks5`代理. 支持`http/https`, 以及解析`Transfer-Encoding`分块编码的响应体

- 封装了[LFUCache](include/HXSTL/cache/LFUCache.hpp)和[LRUCache](include/HXSTL/cache/LRUCache.hpp), 并且提供有线程安全的版本, 支持透明查找(C++14), 支持原地构造`emplace`. (支持C++11的版本: [HXCache](https://github.com/HengXin666/HXCache))

- `Json`解析: 
    - 支持静态反射注册到结构体, 实现`toString`生成json字符串和自动生成的构造函数实现`jsonString`/`jsonObj`赋值到结构体, 只需要一个宏即可实现!

    - 支持对`聚合类`进行序列化、反序列化! **不需要宏定义!** 直接使用即可 (示例: [JsonTest.cpp](tests/JsonTest.cpp))

- 万能`print`/`toString`等工具类; 

## 构建要求

- Linux 5.1+ 
- GCC / Clang 编译器
- C++20

## 快速开始
> [!TIP]
> 仍然在开发, 非最终产品
>
> - 其他示例:
>   - [基于轮询的聊天室](examples/ChatServer/ChatServer.cpp)
>   - [WebSocket服务端](examples/WsServer/WsServer.cpp)
>   - [使用`Transfer-Encoding`分块编码/`断点续传`传输文件的服务端](examples/FileServer/FileServer.cpp)
>   - [服务端`面向切面`编程(拦截器)](examples/ServerAddInterceptor/ServerAddInterceptor.cpp)
>   - [支持`socks5`代理的`Http/Https`客户端](examples/Client/Client.cpp)
>   - [自实现のJson解析、结构体静态反射到Json和Json赋值到反射注册的结构体的示例(只需要一个`宏`即可实现!) / 支持对`聚合类`进行序列化、反序列化! **不需要宏定义!**](tests/JsonTest.cpp)
>   - [LRUCache/LFUCache的示例](tests/CacheTest.cpp)

- 编写端点 (提供了简化使用的 API 宏)

```C++
#include <HXWeb/HXApi.hpp> // 使用简化的api

// 用户定义的控制器
class MyWebController {
    ROUTER
        .on<GET, POST /*一个端点函数 挂载多种请求方式(GET, DEL ...) (这是一个`可变参数`)*/>("/", [] ENDPOINT {
            co_return res.setStatusAndContent(
                Status::CODE_200, "<h1>Hello! this Heng_Xin 自主研发的 Http 服务器!</h1>");
        })
        .on<GET>("/home/{id}/{name}/**", [] ENDPOINT {
            // 获取第一个(index = 0)路径参数 {id}, 并且解析为`u_int32_t`类型, 局部变量命名为:`id`
            PARSE_PARAM(0, u_int32_t, id);

            // 获取第一个(index = 1)路径参数 {name}, 并且解析为`std::string`类型, 局部变量命名为:`name`
            PARSE_PARAM(1, std::string, name);

            // 解析通配符**路径, 保存到 std::string uwp 这个局部变量中
            PARSE_MULTI_LEVEL_PARAM(uwp);
            // 注意, 一般的url对于尾部是否有`/`是全部当做没有的
            // 如: /home/id == /home/id/
            // 而通配符**路径是需要当做有的:
            // 如: /files/**
            // 不解析 /files
            // 但是解析 /files/ 为 ""

            // 解析查询参数为键值对; url?awa=xxx 这种
            GET_PARSE_QUERY_PARAMETERS(queryMap);

            if (queryMap.count("loli")) // 如果解析到 ?loli=xxx
                std::cout << queryMap["loli"] << '\n'; // xxx 的值

            RESPONSE_DATA(
                200,
                html,
                "<h1> Home id 是 " + std::to_string(*id) + ", "
                "而名字是 " + *name + "</h1>"
                "<h2> 来自 URL: " + req.getRequesPath() + " 的解析</h2>"
                "<h3>[通配符]参数为: " + uwp + "</h3>"
            );
        })
        .on<GET>("/favicon.ico", [] ENDPOINT {
            // 支持分块编码 (内部自动识别文件拓展名, 以选择对应的`Content-Type`)
            co_return co_await res.useChunkedEncodingTransferFile("favicon.ico");
        })
    ROUTER_END;
};
```

- 绑定控制器到全局路由

```C++
#include <HXWeb/HXApi.hpp> // 宏所在头文件

ROUTER_BIND(MyWebController); // 这个类在上面声明过了
```

- 启动服务器, 并且监听 127.0.0.1:28205, 并且设置路由失败时候返回的界面
    - 可选: 可以设置线程数和超时时间 | 每个线程独享一个`uring`, 但是绑定同一个端口, 由操作系统进行负载均衡

```C++
#include <HXWeb/HXApi.hpp> // 宏所在头文件
#include <HXWeb/server/Server.h>

int main() {
    chdir("../static");
    setlocale(LC_ALL, "zh_CN.UTF-8");
    ROUTER_BIND(WSChatController);
    // 设置路由失败时候的端点
    ROUTER_ERROR_ENDPOINT([] ENDPOINT {
        static_cast<void>(req);
        res.setResponseLine(HX::web::protocol::http::Status::CODE_404)
           .setContentType(HX::web::protocol::http::ResContentType::html)
           .setBodyData("<!DOCTYPE html><html><head><meta charset=UTF-8><title>404 Not Found</title><style>body{font-family:Arial,sans-serif;text-align:center;padding:50px;background-color:#f4f4f4}h1{font-size:100px;margin:0;color:#990099}p{font-size:24px;color:gold}</style><body><h1>404</h1><p>Not Found</p><hr/><p>HXLibs</p>");
        co_return;
    });

    // 启动Http服务 [阻塞于此]
    HX::web::server::Server::startHttp("127.0.0.1", "28205", 16 /*可选 线程数(互不相关)*/, 10s /*可选 超时时间*/);

    // 或者, 启动Https服务 [阻塞于此], 需要提供证书和密钥
    HX::web::server::Server::startHttps("127.0.0.1", "28205", "certs/cert.pem", "certs/key.pem");
    return 0;
}
```

---

进阶的, 实际上端点的`ENDPOINT`宏, 是:

```cpp
#define ENDPOINT  \
(                 \
    Request& req, \
    Response& res \
) -> Task<>
```

也就是`lambda`表达式, 因此您可以捕获一些变量, 例如:

```cpp
class ChatController {
    struct LogPrint {
        void print() const {
            HX::print::println("欢迎光临~");
        }
    };

    ROUTER
        .on<GET, POST>("/", [log = LogPrint{}] ENDPOINT {
            RESPONSE_DATA(
                200,
                html,
                co_await HX::STL::utils::FileUtils::asyncGetFileContent("indexByJson.html")
            );
            log.print();
        })
    ROUTER_END;
};
```

(一般场景下没啥用), 但是下面的功能可以让您`面向切面`编程`端点`(类似于`拦截器`):

*下面并没有使用`ENDPOINT`, 可以发现, 代码略有复杂和重复.*

```cpp
class ServerAddInterceptorTestController {
public:
    struct Log {
        decltype(std::chrono::steady_clock::now()) t;

        bool before(HX::web::protocol::http::Request& req, HX::web::protocol::http::Response& res) {
            HX::print::println("请求了: ", req.getPureRequesPath());
            static_cast<void>(res);
            t = std::chrono::steady_clock::now();
            return true;
        }

        bool after(HX::web::protocol::http::Request& req, HX::web::protocol::http::Response& res) {
            auto t1 = std::chrono::steady_clock::now();
            auto dt = t1 - t;
            int64_t us = std::chrono::duration_cast<std::chrono::milliseconds>(dt).count();
            HX::print::println("已响应: ", req.getPureRequesPath(), "花费: ", us, " us");
            static_cast<void>(res);
            return true;
        }
    };

    ROUTER
        .on<GET, POST>("/", [](
            Request& req,
            Response& res
        ) -> Task<> {
            auto map = req.getParseQueryParameters();
            if (map.find("loli") == map.end()) {
                res.setResponseLine(Status::CODE_200)
                .setContentType(HX::web::protocol::http::ResContentType::html)
                .setBodyData("<h1>You is no good!</h1>");
                co_return;
            }
            res.setResponseLine(Status::CODE_200)
            .setContentType(HX::web::protocol::http::ResContentType::html)
            .setBodyData("<h1>yo si yo si!</h1>");
        }, Log{})
        .on<GET, POST>("/home/{id}/**", [](
            Request& req,
            Response& res
        ) -> Task<> {
            static_cast<void>(req);
            res.setResponseLine(Status::CODE_200)
            .setContentType(HX::web::protocol::http::ResContentType::html)
            .setBodyData("<h1>This is Home</h1>");
            co_return;
        })
    ROUTER_END;
};
```

> [!TIP]
> 为了防止api宏带来的命名空间污染, 特别是在头文件中声明`控制器端点`的, 建议在尾部添加`#include <HXWeb/UnHXApi.hpp>`以`undef`那些宏.

- 断点续传api

```cpp
class RangeController {
    ROUTER
        .on<HEAD, GET>("/test/range", [] ENDPOINT {
            try {
                // 使用断点续传, 如果客户端请求头没有要求使用断点续传, 那么只是退化为普通的http传输
                // 无其他额外的性能影响
                co_return co_await res.useRangeTransferFile("static/test/github.html");
            } catch (...) {
                co_return res.setStatusAndContent(
                    Status::CODE_500, "<h1>没有这个文件</h1>");
            }
        })
    ROUTER_END;
};
```

- [断点续传的测试](./documents/UseRangeTransferFile.md)

## 相关依赖

|依赖库|说明|备注|
|---|---|---|
|liburing|io_uring的封装|https://github.com/axboe/liburing|
|hashlib|用于`WebSocket`构造`SHA-1`信息摘要; 以及进行`Base64`编码|https://create.stephan-brumme.com/hash-library/|
|OpenSSL 3.3.1+|用于https的证书/握手|https://github.com/openssl/openssl|

## 性能测试
> [!TIP]
> - Arth Linux
> - 13th Gen Intel(R) Core(TM) i9-13980HX
> - RAM: 64GB
> - cmake -> Release

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

## 代码规范
> --> [C++ 编码规范](documents/CodingStandards/CppStyle.md)

## 开发计划
> --> [开发计划](documents/DevelopmentPlan.md)

## 开发日志
> --> [开发日志](documents/DevelopmentLog.md)

## 特别感谢
### Stargazers
[![Stargazers repo roster for @HengXin666/HXNet](https://reporoster.com/stars/dark/HengXin666/HXNet)](https://github.com/HengXin666/HXNet/stargazers)

### Forkers
[![Forkers repo roster for @HengXin666/HXNet](https://reporoster.com/forks/dark/HengXin666/HXNet)](https://github.com/HengXin666/HXNet/network/members)
