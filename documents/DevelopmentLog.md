# 开发日志

- [2025-11-01 14:09:33] : 重构服务端构造API, 现在仅需要指定端口即可. 新增 `addController` 方法方便配合 ApiMacro 使用

新API使用如下:

```cpp
#include <HXLibs/net/ApiMacro.hpp>

struct LoliDAO {
    uint64_t select(uint64_t id) { // 示例
        return id;
    }
};

HX_CONTROLLER(LoliController) {
    HX_ENDPOINT_MAIN(std::shared_ptr<LoliDAO> loliDAO) {
        using namespace HX::net;
        addEndpoint<GET>("/", [=] ENDPOINT {
            auto id = loliDAO->select(114514);
            co_await res.setStatusAndContent(Status::CODE_200, std::to_string(id))
                        .sendRes();
        })
        .addEndpoint<POST>("/post", [=] ENDPOINT {
            auto id = loliDAO->select(2233);
            co_await res.setStatusAndContent(Status::CODE_200, std::to_string(id))
                        .sendRes();
        });
    }
};

#include <HXLibs/net/UnApiMacro.hpp>

int main() {
    using namespace HX::net;
    HttpServer server{28205};

    // 依赖注入
    server.addController<LoliController>(std::make_shared<LoliDAO>());
}
```

- [2025-11-01 00:58:20] : 新增 Web 框架使用的宏, 方便定义端点, 可依赖注入:

```cpp
#include <HXLibs/net/ApiMacro.hpp>

HX_CONTROLLER(LoliController) {
    // 可以进行依赖注入, 只需要定义变量
    HX_ENDPOINT_MAIN([[maybe_unused]] int a, [[maybe_unused]] std::string const& b) {
        auto loliPtr = std::make_shared<int>(); // 可以定义成员.
    
        addEndpoint<GET>("/", [=] ENDPOINT {
            *loliPtr = 114514;
            co_return;
        });

        addEndpoint<POST>("/post", [=] ENDPOINT {
            *loliPtr = 2233; // 如果是跨端点共享的话, 应该使用智能指针
                             // 并且注意生命周期~
            co_return;
        });
    }
};

#include <HXLibs/net/UnApiMacro.hpp>

int main() {
    using namespace HX::net;
    HttpServer server{28205};

    addController<LoliController>(server, 1, "1"); // 注册控制器, 并且注入依赖变量
}
```

- [2025-10-31 21:58:42] : 重构 HttpClient / HttpClientPool 支持指定 IO (并且特化为 Http(s)Client)

- [2025-10-31 20:29:24] : 重构 IO, 分为 HttpIO / HttpsIO (需要开启`HXLIBS_ENABLE_SSL`并且依赖openSSL); 为服务端、客户端支持 Https, 并且在 Https 下 支持代理, 新增 Http 代理. 调整了部分代码架构.

- [2025-10-11 12:22:48] : 尝试实现 https://github.com/HengXin666/HXTest/issues/7 的猜想, 但是实际测试下来, 在各个场景下都不如原始表现. 显然这不是瓶颈...; 为 `TickTock` 工具类新增 forEach, 可多次计时求平均值.

- [2025-10-10 23:22:09] : 支持协程`whenAll`以及其`&&`运算符重载, 实现 `auto [a, b, c] = co_await (co1() && co2() && co3())` 等价于 `co_await whenAll(co1(), co2(), co3())` 的效果, 使得使用更加方便、直观。并且修复一个 `||` 重载导致的顺序问题.

- [2025-10-10 01:03:29] : 支持协程的`||`运算符重载, 实现 `co_await (co1() || co2() || co3())` 等价于 `co_await whenAny(co1(), co2(), co3())` 的效果, 使得使用更加方便、直观。

- [2025-10-08 17:10:10] : 修复了 MSVC 编译 Json 模块报错的问题: MSVC上没有导入 `std::span`

- [2025-10-01 21:10:28] : 修复序列化 Json 时候, 字符串 如果含有 `"` 等内容, 没有转义, 导致的bug. 现在全部会转义了, 中文也转义为 `\u????`

- [2025-09-30 21:39:43] : 修复潜在的 客户端上传文件/ws 超时后某些情况下重新建立连接失败: ws 是因为 没有传输 headers, 可能导致服务端校验问题. 上传文件 单纯是没有写...

- [2025-09-28 14:55:53] : windows支持:  为 FutureResult 对接了协程. 现在支持在协程中配合线程池的异步调度了. 即 co_await 线程池的任务, 是可行的.

- [2025-09-27 23:57:16] : 修复客户端 api ws 的握手阶段没有判断状态码是否为 101 的缺陷. 修复 客户端上传文件 如果抛出异常会 close fd, 而不是只是清空 IO 的

- [2025-09-25 20:47:17] : 为 FutureResult 对接了协程. 现在支持在协程中配合线程池的异步调度了. 即 co_await 线程池的任务, 是可行的. (Win待支持)

- [2025-09-25 00:05:57] : 修复了客户端上传文件的bug: 不应该关闭 IO, 而仅 reset 即可.

- [2025-09-24 22:23:49] : 修复客户端上传文件, 可能的 IO fd 没有关闭的情况.

- [2025-09-24 22:00:55] : 统一了 客户端分块编码上传文件 API, 使用新的 EventLoop API 以返回 `Try<T>` 内容.

- [2025-09-22 12:21:04] : 为 FixedString 支持 从编译期的std::string_view构造

- [2025-09-21 23:03:08] : 新增反射: `类成员指针`到`字段名称`的映射

- [2025-09-21 17:34:39] : 优化了 FileUtils 在close异常时候, 依旧保证 _fd 被赋值为 -1, 保证其析构在debug下不会触发异常!

- [2025-09-19 23:36:32] : 统一了数字的反序列化, 并且支持 URL 参数通过 `.to<T>()` 转化为 `数字/枚举`

- [2025-09-18 22:17:46] : 修改请求头的key为大小写不敏感比较. 为 ws 接口新增 headers 参数支持

- [2025-09-16 11:38:30] : 修复 [#8](https://github.com/HengXin666/HXLibs/issues/8) 的 (玄学问题: 普通运行服务端无法退出. 但是 Release / 调试 / cmake, 都没问题... 就 Debug 运行 可以触发...), 为部分协程对象无法 RAII 情况, 添加了 debug 下 未手动释放而抛出异常的检查!

- [2025-09-15 22:10:26] : MSVC 不知道为什么也抽风了... 不过修改为 仅 gcc 屏蔽警告就好啦~

- [2025-09-15 21:03:03] : 爆操 gcc, [https://godbolt.org/z/5q1nKTr3e], 加个 auto _ = ..., 然后显式 void(_) 就好了. (话说, 直接屏蔽警告不更好?)

- [2025-09-15 20:51:13] : gcc 警告抽风? [https://godbolt.org/z/a7jETeG64], 导致牺牲微乎其微但是有心理作用的 `&& ...` 短路求值性能.

- [2025-09-15 20:09:42] : 支持将拦截器声明为协程返回值 `Task<T>`, 其中 T 应该可以转换到 bool.

- [2025-09-12 20:16:48] : 修复 win 部分的一些问题(编译错误/计数问题). 发现新的问题, 客户端创建的链接无效 导致抛出异常. 原因未知. 下次一定

- [2025-09-11 16:54:52] : 修复 客户端 如果触发 断开的管道, 而不会尝试重连的bug. (注意! Win 不一定修复了! `@todo`)

- [2025-09-11 13:45:31] : 修复 端点函数 多`Method` 情况下, `_addEndpoint` 参数是 `Func&&` 导致推导为 `Func&`从而因为内部 `std::move` 而资源被窃取了. 导致安全隐患的Bug.

- [2025-09-10 00:28:38] : 修复 `FutureResult` 在某些情况下的 `thenTry` 的`自提交死锁`问题 ([#15](https://github.com/HengXin666/HXLibs/issues/15)). 并且修复因为模板using名称类似导致类型萃取的bug.

- [2025-09-09 16:43:49] : 修复Try的构造模板不严谨. 导致 thenTry 在传入参数 Try<T>, T = 数字 等情况下, 无法正确约束的bug.

- [2025-09-08 23:09:51] : 升级客户端的 ws 接口. 支持任意返回值类型: `Task<T>` 的 `T`. 而不是只能 `void`

- [2025-09-08 16:08:25] : 新增一个简易的 HttpClientPool ([#14](https://github.com/HengXin666/HXLibs/issues/14)); 修改 TimeNTTP 相关的约束. 新增 `utils::HasTimeNTTP<Timeout>` 作为统一的约束判断.

- [2025-09-08 13:39:01] : 修复: [#14](https://github.com/HengXin666/HXLibs/issues/14) (单个Http客户端对象不应该内设多个线程, 并发请求会共用一个fd, 等. 导致竞争.) 现在一个 HttpClient 仅会对应一个线程, 内部不会竞争. 由任务队列保证.

- [2025-09-08 11:03:10] : 修复bug: `Res`发送文件多发的问题 (应该使用`{buf.data(), size}`限制)

- [2025-09-07 16:35:52] : 为 msvc 编译通过 (#8, #9)

- [2025-09-06 18:21:15] : 支持 thenTry 的链式 返回参数推导优化 ([#12](https://github.com/HengXin666/HXLibs/issues/12))

- [2025-09-05 11:52:49] : Res / Req 解析时候尽可能按需读取. 修复了多读导致的bug. 修改 IO / File buf 大小, 减小系统调用次数, 提升吞吐量:

```bash
# examples/HttpServer/benchmarks_01_server
╭─     ~                                                                                                                  ✔  at 11:51:19   ─╮
╰─ wrk -d10s -t32 -c100 http://127.0.0.1:28205/mp4/1                                                                                                ─╯
Running 10s test @ http://127.0.0.1:28205/mp4/1
  32 threads and 100 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.07s   439.76ms   2.00s    60.86%
    Req/Sec     2.46      2.87    20.00     89.69%
  481 requests in 10.11s, 283.97GB read
  Socket errors: connect 0, read 481, write 0, timeout 131
Requests/sec:     47.58
Transfer/sec:     28.09GB

╭─     ~                                                                                                    ✔  took 10s    at 11:51:44   ─╮
╰─ wrk -d10s -t32 -c100 http://127.0.0.1:28205/html/2                                                                                               ─╯
Running 10s test @ http://127.0.0.1:28205/html/2
  32 threads and 100 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     3.08ms    3.15ms  64.76ms   88.74%
    Req/Sec   615.72    327.64     1.74k    72.14%
  197167 requests in 10.10s, 104.33GB read
  Socket errors: connect 0, read 197167, write 0, timeout 0
Requests/sec:  19527.86
Transfer/sec:     10.33GB
```

- [2025-09-03 16:11:20] : fixbug: `content-length`如果等于0, 那么解析 body 时候会因为 `std::size_t` 导致: `0 - 数字 = (-1) - (数字 - 1)`; 新增 websocket 读取缓存. 可以把 res 的 `_recvBuf` 迁移到 ws 中, 防止多读导致数据丢失. (写入数据的顺序有标准, 是逆序填充(类似于栈), 从而优化性能)

- [2025-09-03 14:27:46] : 新增`EventLoop::trySync`接口, 可以把异常包装到 `Try` 中. 并且修改部分`客户端`的API使用该接口

- [2025-09-03 13:04:01] : 客户端异步接口套上`Try`, 使其可以更好的携带异常信息; 修复一些问题: 为客户端ws加上初次重新连接逻辑 ([#11](https://github.com/HengXin666/HXLibs/issues/11)); 发现 Res 的解析在 ws 快速发生情况, 可能出现粘包, 并且多读了数据, 导致后面解析时候fd缓冲区已经为空. (需要为 Res 重构为懒解析 + 可迁移的`读取缓冲区`)

- [2025-09-02 22:38:36] : 修复字符串make 指定精度时候为非编译期变量字符串问题. 修复 客户端异步ws方法的编译错误: 补上`std::forward<Func>`

- [2025-09-02 21:58:15] : 为 WebSocket 添加 getIO 接口, 供 WebSocketCli 使用

- [2025-09-02 20:40:56] : 优化: AsyncFile 的 写入 现在会保证完全的将数据写入!

- [2025-09-02 16:01:16] : 修复Res解析请求头和请求体的`\r\n\r\n`错误过步问题; 原因是 `chunked` 的发送有问题 (多发了`\r\n`)

- [2025-09-02 10:34:33] : 客户端支持 `chunked` 上传文件; 服务端支持 `chunked` 或者流 `Body` 分块保存文件到本地路径. 新增进制转换类的`strToNum`函数, 无需 `std::stolu` 的 `std::string` 构造. ([#10](https://github.com/HengXin666/HXLibs/issues/10))

- [2025-09-01 16:58:27] : 修复bug: 客户端指定发送body, 可能导致解析出错(忘记写`HEADER_SEPARATOR_SV`); `Res`的`clear()` 修改为协程; 并且会清理 `Body` 防止粘包; (`@todo`: 安全问题: 可能body过大而业务实际上没有用到, 导致浪费性能解析body) (slowloris/大 body DoS 攻击)

- [2025-09-01 14:11:52] : 新增 编译期字符串NTTP; 修改了 TimeNTTP 的模板. 把 `Res` 的 body 改为懒解析. 使用 `parseBody` 获取. ([#10](https://github.com/HengXin666/HXLibs/issues/10))

- [2025-09-01 12:29:50] : 解决: [#9](https://github.com/HengXin666/HXLibs/issues/9) 的端点函数获取`EventLoop`的问题. 为 `Res/Req` 新增 `getIo()` 接口. 然后直接 `utils::AsyncFile file{res.getIO()};` 即可使用.

- [2025-08-31 20:32:47] : 新增协程的 readAll 接口, 修改 `EventLoop::sync` 使得其不会出现奇怪的问题...(触发bug原因未知(知道是赋值失败), 现在直接换实现, 采用封装更好的`Try<Res>`即可)

- [2025-08-30 09:53:46] : 新增自定义打印类 `CustomToString`, 用户可以特化模板进行自定义; 修复 `UninitializedNonVoidVariant` 的 `visit` 的一些bug.

- [2025-08-29 22:49:41] : 移动了 `UninitializedNonVoidVariant` 的全局的 `get` 函数的模板参数顺序, 使得可以 `get<T, exception::ExceptionMode::Nothrow>(v)` 指定是否使用抛出异常版本.

- [2025-08-29 22:18:06] : `Try<T>` 支持重新抛出接口(`rethrow`), 并且补上注释

- [2025-08-29 21:05:47] : `Try<T>` 支持获取`异常字符串`/`异常指针`

- [2025-08-29 20:42:02] : 新增 `Try<T>`/`函数参数Meta模板`/`支持 thenTry 的Future`

- [2025-08-27 19:48:30] : 修改了部分Res/Req的api的签名, 替换setBody为模板实现, 以适配更多情况. 客户端支持断线自动连接 (要求请求的url为带域名的全路径)

- [2025-08-25 11:26:07] : 支持反射 获取名称到索引的映射(map)

- [2025-08-24 21:26:49] : 优化 枚举反射的性能, 运行时几乎 $O(1)$ 并且仅编译期计算

- [2025-08-24 16:17:07] : 支持反射自定义类型(`类、结构体、共用体、枚举类型名称`), 可以从类型反射到字符串编译期常量; 编写了自述文档以介绍 新增的 反射自定义类型 & 反射枚举类型字符串, 和字符串反射回枚举值

- [2025-08-22 14:24:52] : 规范化命名: 为了防止 ub, 把之前的 `__` 开头 / `_` + 大写字母开头 的内容全部重构为 `_hx_` 开头. (如果是私有的话. 并且是全局作用域的化); 去除了头文件的防止重定义宏. 调整了部分命名.

定义命名规范:

  - 宏魔法的公共宏为 `HX_{NAME}` 命名
  - 宏魔法的私有宏为 `_hx_MACRO_{NAME}__` 命名
  - 非宏魔法, 仅作为使用的宏函数的宏, 以 `_hx_{NAME}__` 命名

- [2025-08-17 19:59:40] : 支持枚举的json序列化与反序列化, 修复json解析时候对于数组的 `,]}` 解析的不严谨和 聚合类没有移动`}`

- [2025-08-17 19:17:21] : 支持反射枚举类型字符串, 和字符串反射回枚举值.

- [2025-08-15 23:45:54] : 更新: `EventLoop::sync` 可以捕获到协程内部的异常并且重新抛出!

- [2025-08-14 14:50:32] : 修改 win 部分的编译报错. 重构 win 的 iocp 事件循环的任务判定, 不依赖 hash_set, 而是同 io_uring 一样采用其他计数. 目前存在一个玄学问题: 普通运行服务端无法退出. 但是 Release / 调试 / cmake, 都没问题... 就 Debug 运行 可以触发...

- [2025-08-12 09:54:58] : 支持 string 流传入 格式化字符串类, 支持 toJson 的格式化. 新增简易`计时器工具类`. 为异步文件支持同步的全部读取的方法, 其性能比标准库的可能大 10 ~ 80 倍 (没有严谨测试, 仅测试读取 1MB 左右文件) 并且修复 序列化ToString类的字符串数组指针二义性

```sh
[INFO] "协程的同步" : 1.259914 ms
[INFO] "同步" : 87.192267 ms
```

- [2025-08-11 14:35:38] : 更改一个方法名称, 使得语义更清晰. 支持同步IO (基于异步IO); 给事件循环封装了同步用法, 可以同步的执行完协程.

- [2025-07-30 17:06:13] : 重构了 ToString 模块, 现在更加现代化和规范了. 调整了部分代码, 新增了 ToString 对应的测试

- [2025-07-28 21:20:59] : 修复bug [#7](https://github.com/HengXin666/HXLibs/issues/7), 以避免msvc问题...

- [2025-07-28 15:44:55] : 将json反序列化从基于 tuple引用绑定, 优化为基于指针偏移的零拷贝的实现.

- [2025-07-28 14:17:38] : 支持在全局中声明反射宏, 修复反射宏的一个多参数时候的bug (忘记加for了), 减小递归深度到 256, 以减轻预处理器压力.

- [2025-07-27 22:09:25] : 反射宏支持给变量起别名

- [2025-07-27 17:43:18] : 更新了宏魔法, 新增一个宏库, 提供一些图灵完备的宏, 而不是朴素暴力声明. 定义命名规范:

  - 宏魔法的公共宏为 `HX_{NAME}` 命名
  - 宏魔法的私有宏为 `_HX_MACRO_{NAME}__` 命名
  - 非宏魔法, 仅作为使用的宏函数的宏, 以 `__HX_{NAME}__` 命名

- [2025-07-25 15:57:16] : 支持了宏反射内部字段; `ToString.hpp 需要重构`

- [2025-07-24 23:58:40] : 为面向切面新增更好的支持: 只要返回值可以转化为bool就是合法的返回值. 现在可以轻松使用枚举作为返回值~; 更新了 Linux 侧压力测试(本地版本).

- [2025-07-24 15:58:48] : [Win] 为初始化win32 api提供线程安全的实现 (`thread_local`)

- [2025-07-24 15:27:47] : [Win] 修复 反射字段的问题; 修复bug: MSVC下, `std::from_chars` 参数类型为 `const char*`, 不会从迭代器隐式转换以及隐式比较. 修复 json read 模块 一个 Mask `1u << 32` 的 ub; 修复 `IO::recvLinkTimeout` 接口的问题. 已经全部通过测试!

- [2025-07-23 23:23:44] : 新增 win 部分接口, 基于 iocp 的客户端、服务端. 顶层API无变化. (目前 win 系统的反射对`opt`似乎有问题, 正在尝试修复...)

- [2025-07-22 15:51:11] : 重构部分接口 (添加代理模板), 使用 CRTP 编写代理, 参数通过 客户端配置传递, 支持并且测试通过 `Socks5` 客户端代理.

- [2025-07-22 11:37:41] : 测试了客户端ws接口, 修复了发送数据某些情况会含有`\0`的问题; 提供了同步和协程两种ws接口, 但接口都是运行在一个回调协程里面, 生命周期是独立的. || 修复 FutureResult 在 void 特化的时候的一些问题 (如果需要`wait()`的话会编译错误...) || 调整了部分代码

- [2025-07-21 17:53:31] : 完成了 创建 ws 的客户端接口的编写, 还没有测试; 修复客户端IO发送部分的可能触发的bug; 新增了 origin 解析; 调整了 `Response` 的 tryAddHeaders 接口, 现在可以透明构造了

- [2025-07-21 13:49:39] : 完整的测试了ws服务端的接口, 并且为反射结构体支持 `const&` 版本的重载; 修复ws的bug: 如果此时 之前 recv 的数据到了, 那么继续把这次的 OpCode::Pong 接收; 而不是作为 Pong 的数据. 这会导致抛异常; 提供了 服务端的ws消息群发的示例; 新增接口, 可以让服务端仅生成一次数据, 然后复用多次发送, 减少生成数据生活的拷贝开销. 提高性能.

- [2025-07-21 11:06:35] : 在编译期分开ws的实现, 以进行空基类优化; 并且创建ws工厂类

- [2025-07-21 10:30:08] : 封装了ws的学习了FastApi思想的读取和发送的api, 并且修复解析包的一个bug: 在分片情况下, 会搞混分片内容类型等

- [2025-07-21 00:15:58] : 测试了ws服务端, 功能正常

- [2025-07-20 22:33:01] : 编写了 WebSocket 的解析和发送

- [2025-07-20 15:57:20] : 支持 Json无宏反序列化 从 std::array 或者 std::span; 不支持C风格数组

- [2025-07-20 00:30:24] : 支持 Json无宏反序列化! 调整了部分代码结构; 修复 Log 中 对于空 k-v 的格式化问题

- [2025-07-19 01:02:00] : 简单的实现了编译期完美哈希的哈希表

- [2025-07-18 11:32:10] : 细化工具, 把元模板编程部分, 迁移到 `meta` 模块

- [2025-07-17 17:44:04] : 新增编译期vector, 开始着手 json 反序列化 需要编译期哈希表

- [2025-07-14 16:37:01] : 更新 `UninitializedNonVoidVariant`, 现在可以从多个相似变量构造, 而不需要准确的类型了; 并且加上了检测类型不能为引用的功能

- [2025-07-12 15:35:45] : 新增 格式化带缩进的 字符串格式类, 并且有比较完善的测试用例.

- [2025-07-12 11:16:04] : 迁移 ToString.hpp 放到 log 下, 添加了反射部分的测试用例(针对 `std::initializer_list<T>`)

- [2025-07-11 17:52:32] : 新增 json 序列化部分代码; 打算重构 json 反序列化 以及 有宏的部分; 打算搞一个 格式化 的打印方案

- [2025-07-11 16:47:16] : 为客户端支持 post, 并且封装了通用请求; 修复解析host时候可能导致的bug; 请求时候会自动为请求加上需要的请求头; 重构了 headers 的传入, 现在是请求之间的headers都是独立的了!

- [2025-07-11 14:18:53] : 修复客户端请求的bug, 修复客户端请求中string的协程生命周期的问题, 服务端支持关闭; 修复 `Uninitialized` 的忘记析构bug, 并且替换 `FutureResult` 的内部实现为 `Uninitialized` 作为数据容器; 并且添加了对应的单元测试

- [2025-07-11 00:07:21] : 重新架构了 http-get 接口, 现在集成线程池, 变为线程协程池, 支持协程、异步, 两种调用方式, 为 Task 提供 start 接口, 使得协程可以原地启动(但是要小心, 它可能是未完成的!):

```cpp
#include <HXLibs/net/client/HttpClient.hpp>

#include <HXLibs/log/Log.hpp>

using namespace HX;
using namespace net;

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
    HttpClient cli{};
    auto res = cli.get("http://httpbin.org/get").get(); // 异步请求, .get() 则是阻塞等待获取
    log::hxLog.info("状态码:", res.status);
    log::hxLog.info("拿到了 头:", res.headers);
    log::hxLog.info("拿到了 体:", res.body);
    log::hxLog.debug("end");
    return 0;
}
```

- [2025-07-10 17:45:55] : 初步实现了 http-get 的客户端, 以及初步的架构... 目前还有一些问题:

比如这样的调用方式太不优雅了! 需要这样的传参! 让使用的人头晕的 ... 但是要改的话, 就需要牵动事件循环了 ... 好像有点思路 ...

```cpp
#include <HXLibs/net/client/HttpClient.hpp>

#include <HXLibs/log/Log.hpp>

using namespace HX;
using namespace net;

template <typename Timeout>
coroutine::Task<> coMain(HttpClient<Timeout>& cli) {
    log::hxLog.debug("开始请求");
    ResponseData res = co_await cli.get("http://httpbin.org/get");
    log::hxLog.info("状态码:", res.status);
    log::hxLog.info("拿到了 头:", res.headers);
    log::hxLog.info("拿到了 体:", res.body);
}

int main() {
    HttpClient cli{};
    cli.run(coMain(cli));
    log::hxLog.debug("end");
    return 0;
}
```

- [2025-07-10 14:21:55] : 添加了线程池, 添加了所有权语义的tuple和moveApply, move-only-function(但是不支持运算符()调用(只是一个简单版本)), 添加了线程安全的队列
- [2025-07-09 23:52:31] : 实现了编译期的 STL 时间库的时间包装, 使其作为模板, 行为类似于 `NTTP`, 简化部分函数的字段; 初步架构了客户端...
- [2025-07-09 17:49:14] : 实现了新架构的响应解析, 架构了编译期强类型的超时类型; 完善了一些其他代码
- [2025-07-09 14:57:40] : 支持服务端断点续传接口, 并且简单进行性能测试(如下); 修复 IO::send 接口的问题; 完善 AioFile类 的新实现...

```bash
# 文件大小
─     ~/Loli/code/HXLibs/static/bigFile  on    main !8 ?2                    ✔  at 14:58:51   ─╮
╰─ l                                                                                                       ─╯
总计 716M
drwxr-xr-x 1 loli loli   52  7月 9日 14:56 .
drwxr-xr-x 1 loli loli  134  7月 9日 11:23 ..
-rw------- 1 loli loli 155M  7月 9日 14:04 book.pdf
-rw-r--r-- 1 loli loli  13M  7月 9日 14:10 info.txt
-rwxr-xr-x 1 loli loli 549M  7月 9日 11:24 misaka.mp4

# 13M 文件传输
╭─     ~/Loli                                                       ✔  took 5s    at 14:55:00   ─╮
╰─ wrk -d5s -t32 -c1000 http://127.0.0.1:28205/files/info.txt                                              ─╯
Running 5s test @ http://127.0.0.1:28205/files/info.txt
  32 threads and 1000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   518.14ms  475.56ms   2.00s    74.69%
    Req/Sec    60.58     36.04   190.00     65.42%
  7502 requests in 5.15s, 97.14GB read
  Socket errors: connect 3, read 7502, write 0, timeout 91
Requests/sec:   1456.78
Transfer/sec:     18.86GB

# 155M 文件传输
╭─     ~/Loli                                                       ✔  took 5s    at 14:56:34   ─╮
╰─ wrk -d5s -t32 -c1000 http://127.0.0.1:28205/files/book.pdf                                              ─╯
Running 5s test @ http://127.0.0.1:28205/files/book.pdf
  32 threads and 1000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.29s   348.83ms   1.95s    47.62%
    Req/Sec     7.58      7.38    47.00     71.63%
  469 requests in 5.14s, 124.96GB read
  Socket errors: connect 3, read 469, write 0, timeout 448
Requests/sec:     91.24
Transfer/sec:     24.31GB

# 549M 文件传输
╭─     ~/Loli                                                       ✔  took 5s    at 14:56:48   ─╮
╰─ wrk -d5s -t32 -c1000 http://127.0.0.1:28205/files/misaka.mp4                                            ─╯
Running 5s test @ http://127.0.0.1:28205/files/misaka.mp4
  32 threads and 1000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.84s   137.99ms   1.93s    83.33%
    Req/Sec     0.08      0.28     1.00     91.67%
  48 requests in 5.23s, 151.97GB read
  Socket errors: connect 3, read 48, write 0, timeout 42
Requests/sec:      9.18   # 完整完成的请求数比较少, 或许把时间设置长点会好些?
Transfer/sec:     29.06GB


╭─     ~/Loli/code/HXLibs/static/bigFile  on    main !8 ?2                    ✔  at 14:58:52   ─╮
╰─ wrk -d10s -t32 -c1000 http://127.0.0.1:28205/files/misaka.mp4                                           ─╯
Running 10s test @ http://127.0.0.1:28205/files/misaka.mp4
  32 threads and 1000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.70s   395.57ms   1.98s   100.00%
    Req/Sec     1.78      3.05    22.00     88.31%
  185 requests in 10.13s, 270.90GB read
  Socket errors: connect 3, read 185, write 0, timeout 183
Requests/sec:     18.25   # 实测是差不多?
Transfer/sec:     26.73GB
```

- [2025-07-09 10:19:51] : 提升切面的逻辑完备性 (如果有匹配函数但是返回值不可转化为bool, 则是编译期错误); 初步架构了服务器关闭流程
- [2025-07-08 23:38:43] : 初步完成迁移, 目前测试性能为:

> [!TIP]
> ROG 枪神7 笔记本 Arch Linux x84_64 6.15.5-arch1-1 cmake-Release
>
> CPU: 32 × 13th Gen Intel® Core™ i9-13980HX, RAM: 64GB

```bash
# 以下为初步测试
╰─ wrk -d15s -t32 -c1000 http://127.0.0.1:28205/
Running 15s test @ http://127.0.0.1:28205/
  32 threads and 1000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.85ms   11.30ms 340.14ms   95.23%
    Req/Sec    74.34k    58.70k  201.80k    60.35%
  34571774 requests in 15.10s, 4.28GB read
  Socket errors: connect 3, read 0, write 0, timeout 0
Requests/sec: 2288842.38
Transfer/sec:    290.31MB
```

但是还有 Linux 异步文件读写、断点续传、分块编码还需要再兼容...

- [2025-07-08 17:51:43] : 完善了 req / res 类, 已经初步可用, 但是还有些内存上的问题...
- [2025-07-08 15:25:10] : 编写了 IO 类 和 连接处理类的具体逻辑, 重构了请求解析的部分代码
- [2025-07-08 10:55:07] : 修改部分平台依赖头文件到单独头文件, 并且进行跨平台处理; 新增 连接类 和 连接处理类 等等
- [2025-07-07 23:33:43] : 调整了很多文件结构, 重构了网络部分, 也重新架构了, 编写了`事件循环`和`HttpServer`; 还有很多未完工...
- [2025-07-07 16:14:55] : 添加了 `UninitializedNonVoidVariant` 的 ctest, 并且修复了 `UninitializedNonVoidVariant` 的 `UninitializedNonVoidVariantIndexToType` 的问题; 调整了部分文件的命名空间
- [2025-07-07 14:38:59] : 重构 HXLibs; 调整cmake, 现在更加自动化和现代 | 新增 log 方便打印 | 新增新版本重构的协程等工具

---

- [2025-5-15 14:09:54] : 删除了部分无用代码, 修改切面注入为基于`&&`展开, 以优化微不足道的性能, 但是坏处是, 这个切面类不能把 && 给重载了... 新增代码格式化`.clang-format`
- [2025-4-7 19:58:59] : 删除了一些无用代码, 修复一个大小写的问题, 因为请求头的key是不区分大小写的, 但是unordered_map是find是区分大小写的, 因此需要换成小写...
- [2025-3-14 15:06:03] : 重构项目的异常抛出, 现在全部抛出基于`std::exception`的异常, 而不是`const char*`.
- [2025-3-13 00:13:51] : 成功完整的支持断点续传, 并且修改一些地方为`string_view`, 以节约性能(?)
- [2025-3-12 22:10:19] : 补充断点续传如果没有range头, 则会退化为普通的http传输的逻辑 (即几乎无损耗, 建议所有已知大小的文件传输, 都使用此方案)
- [2025-3-12 00:39:01] : 初步支持简单的断点续传, 但是还需要各种校验...(另外, 重构异常! 不要使用这种垃圾const char*作为抛出了!)
- [2025-3-9 18:39:46] : 支持toString输出wstring内容, 并且支持符合对应概念的第三方库的容器.
- [2025-1-27 23:19:02] : 修复解析路径参数时候会出现的bug, 并且优化其些微性能
- [2025-1-26 23:34:50] : 更新了自述文档, 让示例更加清晰, 并且提供了`undef`API的头文件
- [2025-1-26 23:01:55] : 支持解析路径参数(内部均以`std::string_view`方式实现, 性能好)
- [2025-1-26 16:39:22] : 把响应的分开编码的api进行了迁移, 现在分块编码传输文件支持自动识别文件类型.
- [2025-1-26 16:06:48] : 往`请求类`/`响应类`中内嵌的`io *`, 以便支持在端点内直接进行响应(如升级为`ws`), 以处理一个旧api对接问题.
- [2025-1-26 00:07:31] : 重构了响应码, 简单添加了一些api, 优化了转化大小写的速度. (需要使用`<sys/uio.h>`/`io_uring_prep_writev`重构一下, 以提升性能?! 难搞! 如果后面需要支持iocp的话, 就跟麻烦了.. || 不对, `WSASend 的多缓冲区支持`(WSASend 的 lpBuffers 参数接受 WSABUF 数组，与 writev 或 io_uring_prep_writev 中的 iovec 类似); 可以稍微封装一下!)
- [2025-1-25 12:58:01] : 修改路由树使用`std::string_view`作为`std::unordered_map`的键, 修改`split`为模版, 使其更加通用.
- [2025-1-24 20:02:28] : 初步架构了新的api宏, 并且修改了`ChatServer.cpp`中的代码, 看起来很高级.
- [2025-1-23 23:12:01] : 完成新的支持切片编程的路由, 等待宏封装... (还需要测试`{}`与`**`的匹配, 因为路由部分为非递归编写!)
- [2025-1-22 22:30:48] : 完成简单的新的http服务类架构, 支持面向切片编程.
- [2025-1-21 23:14:07] : 添加了一个前向拦截器的示例... 感觉有点鸡肋... 打算重构一下端点函数部分!!! 简单更新了一下自述文件.
- [2025-1-20 22:44:34] : 初步架构请求(前向)拦截器
- [2025-1-20 20:19:38] : 更新宏json静态反射, 支持零拷贝版本(传入字符串需要为右值); json无宏反射内部支持完全的零拷贝, 添加了缺少的`get<大数字符串>`模版函数重载.
- [2025-1-19 22:54:30] : 更新无宏反序列化为近乎零拷贝(内部使用`std::move`来保证); 支持将数字解析为任意类型(不论int、double、`std::string`)
- [2025-1-18 20:53:09] : 支持从`jsonString`反序列化到结构体, 并且不需要宏定义! (初步测试通过!!)
- [2025-1-17 23:08:53] : 新增`remove_cvref_v = std::remove_cv_t<std::remove_reference_t<T>>`, 而不是使用`std::decay_t<T>`因为会把C数组衰变为指针, 因为有隐患. 目前代码可clang编译, 0警告 (魔法枚举需要重构!)
- [2025-1-17 20:46:02] : 修改`toString`支持传入任意的流对象, 如`std::string`; 然后进行添加, 而不是只能返回`std::string`. (但是内部有依赖`std::string`(数字转换)/`push_back()`/`append()`)
- [2025-1-17 13:53:05] : 修改`toString`的tuple为使用`std::index_sequence<Is...>`的迭代实现, 而不是递归.
- [2025-1-16 23:35:59] : 修复一个计算成员变量个数的bug, 现在可以统计带`std::string_view`的结构体了, 而不会数量异常, 同理无宏`toJson`可以使用`std::string_view`等做为成员了!
- [2025-1-16 22:00:30] : 支持无宏, 结构体`toJson`! 支持嵌套类型、结构体; 但是暂时不支持`const auto& 成员`、`const auto 成员`、`std::string_view`(视图需要一个指向)
- [2025-1-15 19:37:27] : 重构`HXprint`, 使其依赖于`HXSTL/ToString`库, 提高代码复用性.
- [2025-1-15 17:43:42] : 修改Json反射中, `toString`应该为`toJson`, 顺便修改了`ToString.hpp`中的实现, 把通过函数重载修改为通过偏特化区分, 并且支持原生C数组.
- [2025-1-14 00:01:59] : 项目的cmake更加的现代 => 并且支持进行测试(`ctest`), 使用`doctest`作为测试框架. 并且初步修改了部分示例文件的路径.
- [2025-1-13 21:49:56] : 添加了子模块, 以供测试, 让项目尽量的现代~ 并且修改了API头文件, 让一个未使用的`io`变量的警告消失
- [2025-1-13 14:53:02] : 现在可以进行正常编译了, 修复了偶然的无法加载界面的问题(原因是工作路径不对...) => 进而发现 uringLoop 它有些bug, 它把所有可能的错误 (有些是需要抛出的, 也全部当做是野指针忽视了, 导致协程无法被唤醒, => 内存泄漏了)
- [2025-1-12 23:42:08] : 修改了部分cmake目录结构, 但是目前出现了点问题; 修改项目名称为HXLibs, 因为它不仅限于Net
- [2025-1-11 23:36:55] : 修改了部分的`cmake`配置, 使其更加现代和跨平台, (目前只完善了 警告 部分; 还没有进行`ctest`)
- [2025-1-10 23:05:48] : 新增`聚合类`无宏反射到`成员名称arr`编写了对应的代码注释; 支持`clang 18`的编译器, (目前`clang`只会抛出2个警告, 但是不能使用`魔法枚举`, 有空应该重构...)
- [2025-1-9 23:42:37] : 新增`聚合类`无宏反射到`成员名称arr`, 并且发现似乎我的代码不支持`clang`(一堆报错(这里是因为之前的魔法枚举没有支持clang的说...)和警告)
- [2025-1-8 22:06:24] : 新增`聚合类`成员变量个数计算的模版函数
- [2024-10-19 00:43:45] : 封装了一个线程安全的LFUCache, 使用的是读写锁
- [2024-10-19 00:09:52] : 新增LFUCache的`emplace`方法, 以实现原地构造; 并且新增LFUCache/LRUCache部分api, 以支持清除等, 并且支持判断某键的时候使用`透明比较`(C++14)
- [2024-10-18 23:02:24] : 新增自己实现LFUCache数据结构
- [2024-10-18 11:41:18] : 封装了一个线程安全的LRUCache, 使用的是读写锁
- [2024-10-18 00:27:24] : 新增LRUCache的`emplace`方法, 以实现原地构造; 并且调整其部分代码, 使其更加高效
- [2024-10-17 17:31:28] : 新增自己实现LRUCache数据结构
- [2024-9-14 11:16:14] : 在Arch上对服务器进行负载测试, 运行时完全没有问题; 并且新增一个全局的控制器绑定(`examples/HttpsServer.cpp`)访问`/exit?loli=end`即可关闭服务器(用于测试是否产生内存泄漏):

```cpp
// 测试结果如下:
// 如果没有正在处理的连接的情况下关机, 不会出现内存泄漏
// 当且仅当正在处理连接的写入/写出的时候突然关闭, 才会触发内存泄漏(实际上是根本没有来得及处理)
// 解决方案: 在空闲的时候才关闭! 比如关闭后, 不处理请求, 并且所有请求断开 | 计划: 通过回调函数, 从而让用户自定义如何退出/关闭服务器
```

- [2024-9-13 10:34:46] : 新增toString/print对`std::span`的支持! 此外json可以反射到`const auto&`类型(只能使用`REFLECT`宏来反射)
- [2024-9-12 23:25:05] : 修复如果超时在某些情况下, 并没有断开客户端连接; 以及没有取消`读取任务`而导致的内存泄漏/段错误等问题 (已经在`examples`的`ChatServerByJson.cpp`、`WsServer.cpp`、`HttpsServer.cpp`以及`HttpsFileServer.cpp`分别进行`5min`的压力测试和浏览器随机访问(比如等到其超时后, 再操作xxx等..), 目前没有发现任何问题!)
    - 唯一算是问题的是`HttpsFileServer.cpp`在高并发情况下, 如果在浏览器刷新页面, 似乎会打断压力测试的客户端的连接? 目前还不知道是怎么回事qwq...

- [2024-9-12 17:36:13] : 目前如果是超时的话, 会内存泄漏, (任务没有被回收?), 此外存在问题:

```sh
recv 0x50d000000ce0 # 定位到问题
~ 0x50b000006a70
~ 0x50b0000069c0
正在取消了 0x50d0000009a0
任务 (0x50d000000ce0) | res = 1021 | !: (0x50d000000ce8)
~ 0x50d000000ce0 # 析构了
任务 (0x50d000000ce0) | res = 1021 | !: (0x50d000000ce8) # 却被二次访问!
=================================================================
==127961==ERROR: AddressSanitizer: heap-use-after-free on address 0x50d000000ce8 at pc 0x5ba80b5ee5ec bp 0x71c8227fe710 sp 0x71c8227fe700
```

- [2024-9-12 10:08:45] : 新增采用静态反射的Json的聊天室示例, 修复反射Json的小隐患, 现在如果是`const`的进行get/`[]`获取元素/范围索引, 如果非当前类型/获取不到/索引越界, 则会返回空对象, 而不是抛出异常
- [2024-9-11 11:53:13] : 修改Json的获取为安全获取
- [2024-9-8 20:16:42] : 支持从JSON字符串赋值到结构体(类), 并且通过静态反射实现
- [2024-9-7 21:37:29] : 支持结构体(类)静态反射到JSON, 以提供`toString`的序列化
- [2024-9-7 17:21:13] : 提供了对Json的`toString`方法, 重载了`[]`运算符, 使得使用更加方便 | 重构了部分文件以调整文件结构
- [2024-9-6 23:44:45] : 新增进制转化类
- [2024-9-6 20:30:39] : 新增`Transfer-Encoding`分块编码响应的API宏
- [2024-9-6 20:08:19] : 可以粗略的估计, 确实有性能提升 | 但是不知道为什么提示`timeout`, 明明浏览器也可以正常访问のくせに (发现为什么`timeout`了, 你只要设置为`--timeout 5s`就不会啦)
```sh
# 测试环境: [WSL: Arch Linux]
# 文件读写: `static/test/github.html` 大小约: 366 KB

# 分片编码(`transfer-encoding`), 每次只读取最多`4096`字节, 就直接发送
╰─ wrk -c900 -d300s https://localhost:28205/files/awa
Running 5m test @ https://localhost:28205/files/awa
  2 threads and 900 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   637.13ms  267.39ms   2.00s    74.85%
    Req/Sec     1.35k   181.32     2.23k    71.08%
  803134 requests in 5.00m, 140.47GB read
  Socket errors: connect 0, read 0, write 0, timeout 942
Requests/sec:   2676.27   # 可以发现, 并发量翻倍
Transfer/sec:    479.32MB # 每秒读取速率差不多 (可以估计算是硬件速度上限?)

# 单文件全部读取, 再发送 (使用`Content-Length`)
╰─ wrk -c900 -d300s https://localhost:28205/test
Running 5m test @ https://localhost:28205/test
  2 threads and 900 connections
^C  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   661.61ms  298.27ms   2.00s    76.56%
    Req/Sec   668.50    131.92     0.94k    84.11%
  28499 requests in 22.03s, 10.08GB read
  Socket errors: connect 0, read 0, write 0, timeout 331
Requests/sec:   1293.60
Transfer/sec:    468.65MB
```

- [2024-9-6 18:16:09] : 服务端支持分片编码(`transfer-encoding`)以发送大文件
- [2024-9-6 09:34:20] : 重构服务端请求解析, 现在可以解析带有`transfer-encoding`的客户端请求~
- [2024-9-5 15:25:57] : 修改`StringUtil::split`使用C++风格分割, 而不是: C语言风格并且在栈上分配
- [2024-9-5 15:17:39] : 重构客户端读取, 现在完全支持`分块编码`
- [2024-9-5 09:26:23] : 不仅如此, 甚至单独的请求头的一段都可以被分块发送, 并且一`\r\n`结尾...艹
- [2024-9-4 14:09:29] : 发现, 服务端响应几乎是[https://developer.mozilla.org/zh-CN/docs/Web/HTTP/Headers/Transfer-Encoding]这种, 分块编码..
- [2024-9-4 10:47:16] : 成功实现客户端的https请求(支持代理), 但是如果请求头是分片发送的, 则可能会解析失败(bug!)
- [2024-9-3 17:29:53] : 修改客户端/websocket全部为自实现的红黑树计时器+`whenAny`作为超时计时器
- [2024-9-3 17:05:50] : 采用自主实现的红黑树计时器替代io_uring的计时器 (还有websocket没有适配) | 发现问题: `whenAny`如果第一个参数不是计时器, 而是自定义的协程, 有可能会段错误qwq..
- [2024-9-3 14:50:57] : 优化Acceptor部分代码
- [2024-9-2 20:29:25] : 似乎是我无法解决的, 与其修bug, 不如重构, 我们自己实现计时器, 不用io_uring的计时器了!, 反正如果日后支持IOCP的话也是需要自己实现的!!!!
- [2024-9-2 17:41:34] : 修复了之前的bug, 现在新增2个bug, 还有前几天的bug也旧病复发了(或者说潜伏期结束了..)
- [2024-9-2 08:58:34] : 依旧是去掉计时器就完全没有问题, 太奇怪了~, 给需要的计时器协程任务变量变为static也不行...
- [2024-9-1 23:13:23] : 完善Https的封装! 基本全部完成, 但是出现问题: `src/HXWeb/server/IO.cpp`的 111 行, 又出现之前的玄学bug...`it.resume();`为野指针!(一时半会排查不得..)
- [2024-8-31 22:48:24] : 初步实验io_uring+openssl进行异步解析https请求, 成功! 着手架构如何区分http和https以及是否需要提供配置http自动(强制)升级为https
- [2024-8-30 14:00:02] : 支持`socks5`代理
- [2024-8-29 23:26:20] : 初步架构客户端代理的代码框架
- [2024-8-29 16:46:18] : 完成对客户端类的编写 (新增`request`方法, 对客户端`单请求`封装, 如果需要长连接, 则可以使用原始的旧api)
- [2024-8-29 13:02:07] : 修复`TimerTask`内存泄漏问题
- [2024-8-28 20:25:28] : 重构了客户端类, 以及客户端IO, 适配响应/请求类添加对于客户端的api, 发现`TimerTask`存在内存泄漏问题!, 正在修复..
- [2024-8-27 22:02:17] : 初步实现`Socks5`协议的客户端解析的可行性代码; 重构了`IO`类, 使得职责分离
- [2024-8-27 14:31:49] : 修复在高并发时候存在协程的局部变量被释放的问题
- [2024-8-26 23:02:02] : 添加了调试! 发现问题: (似乎定位到了! 就是135行! 如果改为不定时就不会触发了(似乎))
- [2024-8-25 22:59:08] : 可能需要解决的关键问题: `it.resume();`为野指针! 不知何处注册得...
- [2024-8-25 16:43:33] : 排查了一个下午, 依旧解决不了... 艹!(rnm tq! 越来越玄学了, 先是 `it.resume();` 为野指针?!, 再是`Function not implemented`异常, 以及`corrupted double-linked list`; wdf, 能力不足了!)
- [2024-8-24 23:11:51] : 修复了一个uring阻塞等待没有效果的bug | 仍然在排查问题... 因为似乎所有的协程都有可能段错误qwq...(怎么可能被提前释放了? 明明单线程没有的问题!)
- [2024-8-23 23:08:20] : 发现一个多线程高并发时候会触发的bug, 原因不明! (位于`src/HXSTL/coroutine/loop/IoUringLoop.cpp`的`58行`, 原因是协程指针失效了?访问了野指针; 仍在排查qwq)
- [2024-8-22 23:14:13] : 修改为多线程版本(每个线程独享一个`uring`, 并且仍然是协程的!) | 可能有bug, 还不能复现, 原因不明
- [2024-8-22 21:59:34] : 新增可自定义路由失败时候的端点函数
- [2024-8-22 11:59:48] : 完成对端点函数以及api宏的重构
- [2024-8-22 11:37:50] : 完成对websocket的重构
- [2024-8-21 23:35:04] : 正在重构请求类和响应类, 目前还有websocket和端点函数以及api宏没有修改, 其他已经适配 | 采用一种基于协程事件循环`close fd`于析构函数的方法, 避免了无法在析构函数中使用协程这个问题
- [2024-8-20 22:34:25] : 支持`co_await Task`协程抛出异常, 并且可以被捕获!
- [2024-8-19 22:37:29] : 修复WebSocket的bug (1. 不应该使用whenAny来取消uring, 因为还在监听; 2. 写错变量名, debug半天...)
- [2024-8-19 17:25:09] : 初步完成WebSocket, 正在测试... (可以连接, 但是发现无法接收到消息?!)
- [2024-8-18 23:25:46] : 导入`hashlib`项目且配置CMake, 初步设计WebSocket连接解析(未完成), 修改`Response`支持直接异步写回(提供`send`函数)
- [2024-8-18 21:48:26] : 导入`OpenSSL`项目且配置CMake, 并且新增`certs/GenerateCerts.sh`以生成证书和私钥
- [2024-8-17 18:58:31] : 尝试开发 协程返回值 (`Expected<T>`(具体描述见分支`v5.0`)) 未遂

---

> [!TIP]
> 请忽略下面... | 我将以新的格式重新书写 (2024-8-17 15:46:47起)

### 协程epoll服务端BUG汇总
1. [x] 读取数据的时候, 有时候无法读取到正确的数据 (某些值被换成了`\0`)
    - 解决方案: 使用`std::span<char>`和`std::vector<char>`配合, 而不是自制`buf`类, 它他妈居然又读取到?!?
2. [x] 无法正确的断开连接: 明明客户端已经关闭, 而服务端却没有反应 | 实际上`::Accept`已经重新复用那个已经关闭的套接字, 但是`co_await`读取, 它没有反应, 一直卡在那里!
    - 解决方案: `include/HXWeb/server/ConnectionHandler.h`实际上`make`创建的是智能指针, 而我们只是需要其协程, 故不需要其对象的成员, 导致`AsyncFile`无法因协程退出而析构
3. [x] 玄学的`include/HXSTL/coroutine/loop/EpollLoop.h`的`await_suspend`的`fd == -1`的问题, 可能和2有关?!?!
    - 离奇的修复啦?!
---
4. 在`AsyncFile::asyncRead`加入了`try`以处理`[ERROR]: 连接被对方重置`, 是否有非`try`的解决方案?!

5. 依旧不能很好的实现html基于轮询的聊天室, 我都怀疑是html+js的问题了...(明明和基于回调的事件循环差不多, 都是这个问题..)
    - 发现啦: http的请求体是不带`\0`作为终止的, 因此解析的时候使用C语言风格的字符串就导致解析失败(越界了)