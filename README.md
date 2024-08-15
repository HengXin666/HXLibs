# HXNet
学习现代Cpp的代码存放库, epoll + 协程的http服务器, 基于压缩前缀树的路由, http解析, Json解析, 万用print等

> 目标是写一个epoll协程的 Web Http 后端

## 构建要求

- Linux 系统
- GCC 编译器
- C++20

## 快速开始
> [!TIP]
> 仍然在开发, 非最终产品

- 编写端点


- 绑定控制器到全局路由
```cpp
#include <HXWeb/HXApiHelper.h> // 宏所在头文件

ROUTER_BIND(MywebController); // 这个类在上面声明过了
```

- 启动服务器, 并且监听 0.0.0.0:28205

## 目录结构

```sh
. # 只说明头文件
|-- include
|   |-- HXJson
|   |   `-- HXJson.h # JSON字符串解析为cpp对象
|   |-- HXSTL
|   |   |-- HXSTL.h
|   |   |-- container
|   |   |   |-- BytesBuffer.h        # 字节数组
|   |   |   |-- Callback.h           # 回调函数(函数对象)
|   |   |   `-- RadixTree.h          # 基数树(压缩前缀树)
|   |   `-- tools
|   |       |-- ErrorHandlingTools.h # Linux 错误处理工具
|   |       |-- MagicEnum.h          # 魔法枚举: 支持反射到枚举值对应的字符串, 和通过字符串得到枚举值
|   |       `-- StringTools.h        # 字符串工具类(切分等) / 当前时间格式化到字符串工具类
|   |-- HXWeb
|   |   |-- HXApiHelper.h            # 为用户提供更好体验的API宏定义
|   |   |-- client                   # 客户端
|   |   |-- protocol                 # 协议
|   |   |   |-- http                 # HTTP协议
|   |   |   |   |-- Request.h        # 请求
|   |   |   |   `-- Response.h       # 响应
|   |   |   `-- websocket
|   |   |-- router
|   |   |   |-- RequestParsing.h     # 请求模版解析 (用于给路由解析初始化)
|   |   |   |-- RouteMapPrefixTree.h # 路由的压缩前缀树
|   |   |   `-- Router.h             # 单例 路由类
|   |   |-- server                   # 服务端
|   |   |   |-- Acceptor.h           # 接受连接类
|   |   |   |-- AsyncFile.h          # 文件异步操作类
|   |   |   |-- ConnectionHandler.h  # 连接消息处理类
|   |   |   `-- context
|   |   |       `-- EpollContext.h   # 单例 Epoll 上下文 + 基于红黑树の定时器
|   |   `-- socket
|   |       |-- AddressResolver.h    # 地址(适配器模式封装的LinuxAPI)
|   |       `-- FileDescriptor.h     # 文件描述符类
|   `-- HXprint
|       `-- HXprint.h                # 可打印 STL 容器 | 日志 函数
```

## 代码规范
> 几乎是谷歌C++规范

但, 类成员命名方式为`_name`(自用函数也是以`_`开头)

- 变量名: 几乎全部都是`驼峰`命名
- 枚举: 首字母大写的驼峰
- 常量: 全大写
- 类的构造函数: 请 **显式** 书写构造函数, 必要可以添加`explicit`关键字!, 而不是依赖`C++20`的自动隐式匹配生成!

> [!TIP]
> 缩进为`4空格`, 并且TAB按键请使用`4空格`而不是`\t`!

## 开发计划/日志

- 阶段性任务:
    - [x] 实现基于红黑树实现定时中断, 超时自动终止任务 (类似于Linux内核的“完全公平调度”)
    - [x] 用户自定义路由 | 控制器
    - [ ] 客户端
    - [x] 重构为基于协程的epoll
    - [x] 修改为使用 io_uring 驱动!!
        - [ ] 支持多线程并且不突出
    - [x] 实现文件的异步读写
    - [ ] 实现协程TimerLoop的对于ABCT任务(其中T是计时任务(?必须吗)), 任意一个完成, 则返回(打断其他), 这个功能

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

- 协程版本: (基准: 别人22w/s的并发的程序在我这里一样的参数也就3w+/s..)

```sh
╰─ wrk -c1000 -d15s http://localhost:28205/ # WSL Arth 测试的 (感觉性能还没有跑到尽头)
Running 15s test @ http://localhost:28205/
  2 threads and 1000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    43.61ms   99.59ms   1.78s    98.21%
    Req/Sec    15.30k     0.96k   26.51k    87.96%
  455707 requests in 15.09s, 1.69GB read # 异步的文件读写 | 目前服务端还只是单线程
Requests/sec:  30190.88
Transfer/sec:    114.62MB
```

---
- [ ] 编写API宏
    - [x] 端点快速定义宏
    - [x] 路径参数解析宏

- [ ] 路由映射的前置任务:
    - [x] 支持解析Http的请求行/头/体
    - [x] 支持解析`Query`参数/`PATH`路径 (未支持解析qwq!)
        - [x] 朴素指定: `/?loli=kawaii&imouto=takusann`
        - [x] 通配指定: `/{name}/{idStr}`, 即支持任意如: `/114514/str` 格式
            - 可解析为`bool`/`(u_)int(16、32、64)_t`/`float`/`(long) double`/`std::string`/`std::string_view`
        - [x] 骂人指定: `/fxxk/**`, 支持所有的如: `/fxxk/csdn`
        - 支持以上内容, 得需要使用字典树(前缀树), 应该是基数树(压缩前缀树) | 哈希表不能完全胜任

    - 解析规则: 
        1. 优先级: `静态URL` > `/{name}` > `/**`
        2. 自动忽略尾部多余的`/`, 如`/home`===`/home/`===`/home////////`
        3. 注: 不会被`GET`参数影响: 如`/?loli=kawaii&imouto=takusann`只会先解析`/`.

    - [ ] 支持解析`Body`请求体
        - [ ] none: 没有请求体
        - [ ] form-data: 格式如下:
```http
Content-Type: multipart/form-data; boundary=--------------------------004625337498508003962705
Content-Length: 167

----------------------------004625337498508003962705
Content-Disposition: form-data; name="awa"

hello Heng_Xin
----------------------------004625337498508003962705--
```
-   -   - [ ] urlencoded: 格式如下
```http
Content-Type: application/x-www-form-urlencoded
Content-Length: 91

awa=%E6%92%92%E5%A4%A7%E5%A3%B0%E5%9C%B0&qwq=%E6%81%AD%E5%96%9C%E5%8F%91%E8%B4%A2&0.0=hello
```
-   -   - [ ] raw: json/xml/html/...格式如下
```http
Content-Length: 186
content-type: application/json

{
    "sites": [
        HX_Github : "https://github.com/HengXin666/HXNet/tree/main"
    ]
}
```
-   - [ ] 解析凭证: (怎么搞? 在哪里? 饼干吗? 在请求头`Authorization`似乎可以)

-   - [x] 响应
        - [x] 响应码
        - [x] 响应头/行
        - [x] 响应体