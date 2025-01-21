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
- GCC 编译器 (推荐)
- C++20

## 快速开始
> [!TIP]
> 仍然在开发, 非最终产品
>
> - 其他示例:
>   - [基于轮询的聊天室](examples/ChatServer/ChatServer.cpp)
>   - [WebSocket服务端](examples/WsServer/WsServer.cpp)
>   - [使用`Transfer-Encoding`分块编码传输文件的服务端](examples/FileServer/FileServer.cpp)
>   - [支持`socks5`代理的`Http/Https`客户端](examples/Client/Client.cpp)
>   - [自实现のJson解析、结构体静态反射到Json和Json赋值到反射注册的结构体的示例(只需要一个`宏`即可实现!) / 支持对`聚合类`进行序列化、反序列化! **不需要宏定义!**](tests/JsonTest.cpp)
>   - [LRUCache/LFUCache的示例](tests/CacheTest.cpp)

- 编写端点 (提供了简化使用的 API 宏)

```C
#include <HXWeb/HXApiHelper.h> // 使用简化的api

// 用户定义的控制器
class MyWebController {

    // 定义端点函数
    ENDPOINT_BEGIN(API_GET, "/", root) { // 注册get请求, 接收`/`路径请求
        RESPONSE_DATA( // 响应数据
            200, // 状态码
            co_await HX::STL::utils::FileUtils::asyncGetFileContent("index.html"), // (body数据) 异步(协程)读取文件
            "text/html", "UTF-8" // (响应类型), 以及响应编码
        );
        co_return true; // 注意, 端点函数是协程, 得使用 co_return 而不是return (返回值是 bool)
                        // bool 的意思是是否复用连接 (HTTP/1.1 是推荐复用连接的)
    } ENDPOINT_END;

    ENDPOINT_BEGIN(API_GET, "/favicon.ico", faviconIco) {
        RESPONSE_DATA(
            200, 
            co_await HX::STL::utils::FileUtils::asyncGetFileContent("favicon.ico"),
            "image/x-icon" // 响应编码 可以不写
        );
        co_return true;
    } ENDPOINT_END;

    ENDPOINT_BEGIN(API_GET, "/files/**", files) {
        PARSE_MULTI_LEVEL_PARAM(path);
        RESPONSE_STATUS(200).setContentType("text/html", "UTF-8")
                            .setBodyData("<h1> files URL is " + path + "</h1>"); 
        // 支持直接在端点里面响应 (记得co_await)
        // 响应后, 不会再次在 ConnectionHandler 中再次响应!
        co_await io.sendResponse(); // 立即发送响应
        co_return true;
    } ENDPOINT_END;

    ENDPOINT_BEGIN(API_GET, "/home/{id}/{name}", getIdAndNameByHome) {
        START_PARSE_PATH_PARAMS; // 开始解析请求路径参数
        PARSE_PARAM(0, u_int32_t, id, false); // 解析第一个路径参数{id}, 解析为 u_int32_t类型, 命名为 id
                                              // 并且如果解析失败则不复用连接 (false)

        PARSE_PARAM(1, std::string, name);    // 解析第二个路径参数{name} (不写, 则默认复用连接)

        // 解析查询参数为键值对; ?awa=xxx 这种
        GET_PARSE_QUERY_PARAMETERS(queryMap);

        if (queryMap.count("loli")) // 如果解析到 ?loli=xxx
            std::cout << queryMap["loli"] << '\n'; // xxx 的值

        RESPONSE_DATA(
            200, 
            "<h1> Home id 是 " + std::to_string(*id) + ", 而名字是 " 
            + *name + "</h1><h2> 来自 URL: " 
            + io.getRequest().getRequesPath()  + " 的解析</h2>", // 默认`ENDPOINT_BEGIN`会传入 const HX::web::server::IO<>& io, 您可以对其进行更细致的操作
            "text/html", "UTF-8"
        );
        co_return true;
    } ENDPOINT_END;

public: // 控制器成员函数 (请写成`static`方法)
    // todo...
};
```

- 绑定控制器到全局路由

```C
#include <HXWeb/HXApiHelper.h> // 宏所在头文件

ROUTER_BIND(MyWebController); // 这个类在上面声明过了
```

- 启动服务器, 并且监听 127.0.0.1:28205, 并且设置路由失败时候返回的界面
    - 可选: 可以设置线程数和超时时间 | 每个线程独享一个`uring`, 但是绑定同一个端口, 由操作系统进行负载均衡
```C
#include <HXWeb/HXApiHelper.h> // 宏所在头文件
#include <HXWeb/server/Server.h>

int main() {
    chdir("../static");
    setlocale(LC_ALL, "zh_CN.UTF-8");
    ROUTER_BIND(WSChatController);
    ERROR_ENDPOINT_BEGIN { // 自定义: 设置路由失败时候返回的界面
        RESPONSE_DATA(
            404,
            "<!DOCTYPE html><html><head><meta charset=UTF-8><title>404 Not Found</title><style>body{font-family:Arial,sans-serif;text-align:center;padding:50px;background-color:#f4f4f4}h1{font-size:100px;margin:0;color:#333}p{font-size:24px;color:red}</style><body><h1>404</h1><p>Not Found</p><hr/><p>HXNet</p>",
            "text/html", "UTF-8"
        );
        co_return false;
    } ERROR_ENDPOINT_END;

    // 启动Http服务 [阻塞于此]
    HX::web::server::Server::startHttp("127.0.0.1", "28205", 16 /*可选 线程数(互不相关)*/, 10s /*可选 超时时间*/);

    // 或者, 启动Https服务 [阻塞于此], 需要提供证书和密钥
    HX::web::server::Server::startHttps("127.0.0.1", "28205", "certs/cert.pem", "certs/key.pem");
    return 0;
}
```

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
