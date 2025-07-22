# 开发计划

- 阶段性任务:
    - [x] 实现基于红黑树实现定时中断, 超时自动终止任务 (类似于Linux内核的“完全公平调度”)
    - [x] 用户自定义路由 | 控制器
    - [x] 客户端
    - [x] 支持`https`
    - [x] 支持代理(`socks5`)
    - [x] 支持`WebSocket`
    - [x] 重构为基于协程的epoll
    - [x] 修改为使用 io_uring 驱动!!
        - [x] 支持多线程并且不突出
    - [ ] 支持跨平台(IOCP)
    - [x] 实现文件的异步读写
    - [x] 实现协程TimerLoop的对于ABCT任务(其中T是计时任务(?必须吗)), 任意一个完成, 则返回(打断其他), 这个功能
    - [x] 编写可以静态反射的Json
        - [x] 支持静态反射`const auto&`类型
    - [x] 服务端支持分块编码的收发
    - [ ] 客户端支持分块编码的收发 (发送还不支持)
    - [x] 支持断点续传
    - [ ] 客户端支持`WebSocket`, 以及`wss://`
    - [x] 服务端优雅的退出

- **越发繁重**:
    - [x] 重构: 请求类和响应类, 目前太乱了!
    - 二选一的:
        - [x] 调整: 协程内`throw`, 可以抛出到父协程, 并且被捕获 (不捕获则报错!) | 一般用于co_await的协程内, 可以抛出异常
        - [pass] 老生常谈, 一劳永逸?: ~~实现`Expected<T>`协程~~ (资源释放怎么办?!) <-- RAII啊baka!
            > 我更倾向于这个只是简化协程写法罢了

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