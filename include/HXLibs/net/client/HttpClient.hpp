#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-09 17:42:18
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	  https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <HXLibs/net/client/HttpClientOptions.hpp>
#include <HXLibs/net/protocol/http/Request.hpp>
#include <HXLibs/net/protocol/http/Response.hpp>
#include <HXLibs/net/socket/IO.hpp>
#include <HXLibs/net/socket/AddressResolver.hpp>
#include <HXLibs/net/protocol/url/UrlParse.hpp>
#include <HXLibs/net/protocol/websocket/WebSocket.hpp>
#include <HXLibs/coroutine/loop/EventLoop.hpp>
#include <HXLibs/container/ThreadPool.hpp>
#include <HXLibs/meta/ContainerConcepts.hpp>
#include <HXLibs/exception/ErrorHandlingTools.hpp>

#include <HXLibs/log/Log.hpp> // debug

/**
 * @todo 还有问题: 如果客户端断线了, 我怎么检测到他是断线了?
 * @note 无法通过通知检测, 只能在读/写的时候发现
 * @note 实际上, 可以写一个读取, 在空闲的时候挂后台做哨兵
 * @note 但是iocp不能取消; 那就只能复用这个读, 但是如果是 https, 我们读取的内容要给openssl啊
 * @warning 感觉技术实现比较难... 而且用处不大, 不如直接判断是否出问题了来得快...
 */

namespace HX::net {

template <typename Timeout, 
          typename Proxy,
          typename RequestType,
          typename ResponseType
>
    requires(utils::HasTimeNTTP<Timeout>)
class HttpBaseClient {
public:
    /**
     * @brief 构造一个 HTTP 客户端
     * @param options 选项
     * @param loop 事件循环, 如果传入 nullptr 则是内部独享. 否则就是用户管理的共享.
     * @warning 当使用共享事件循环时候, 仅可使用协程函数, 使用其他函数可能因为线程竞争导致ub
     */
    HttpBaseClient(
        HttpClientOptions<Timeout, Proxy> options = HttpClientOptions{},
        std::shared_ptr<coroutine::EventLoop> loop = nullptr
    ) 
        : _options{std::move(options)}
        , _eventLoop{nullptr}
        , _cliFd{kInvalidSocket}
        , _pool{}
        , _host{}
        , _isAutoReconnect{true}
        , _isUniqueEventLoop{loop == nullptr}
    {
        _eventLoop = _isUniqueEventLoop
            ? std::make_shared<coroutine::EventLoop>()
            : std::move(loop);
        // https://github.com/HengXin666/HXLibs/issues/14
        // 并发时候可能会对fd并发. 多个不同任务不可能共用流式缓冲区.
        // 所以, 使用线程池仅需要的是一个任务队列, 和一个任务线程.
        _pool.setFixedThreadNum(1);
        _pool.run<container::ThreadPool::Model::FixedSizeAndNoCheck>();
    }

    /**
     * @brief 设置是否自动重连
     * @param isAutoReconnect true (默认) 自动重连
     */
    void setAutoReconnect(bool isAutoReconnect) noexcept {
        _isAutoReconnect = isAutoReconnect;
    }

    /**
     * @brief 判断是否需要重新建立连接
     * @return true 
     * @return false 
     */
    bool needConnect() const noexcept {
        return _cliFd == kInvalidSocket;
    }

    /**
     * @brief 分块编码上传文件
     * @tparam Method 请求方式
     * @param url 请求 URL
     * @param path 需要上传的文件路径
     * @param contentType 正文类型
     * @param headers 请求头
     * @return container::FutureResult<>
     */
    template <HttpMethod Method>
    container::FutureResult<container::Try<ResponseData>> uploadChunked(
        std::string url,
        std::string path,
        HttpContentType contentType = HttpContentType::Text,
        HeaderHashMap headers = {}
    ) {
#ifndef NDEBUG
        if (!_isUniqueEventLoop) [[unlikely]] {
            // 当使用共享事件循环时候, 仅可使用协程函数
            throw std::runtime_error{
                "When using a shared event loop, "
                "only coroutine functions are allowed"};
        }
#endif // !NDEBUG
        return _pool.addTask([this,
                              _url = std::move(url),
                              _path = std::move(path),
                              _contentType = contentType,
                              _headers = std::move(headers)]() {
            return coUploadChunked<Method>(
                std::move(_url),
                std::move(_path),
                _contentType,
                std::move(_headers)
            ).runSync();
        });
    }

    /**
     * @brief 分块编码上传文件
     * @tparam Method 请求方式
     * @param url 请求 URL
     * @param path 需要上传的文件路径
     * @param contentType 正文类型
     * @param headers 请求头
     * @return coroutine::Task<> 
     */
    template <HttpMethod Method>
    coroutine::Task<container::Try<ResponseData>> coUploadChunked(
        std::string url,
        std::string path,
        HttpContentType contentType = HttpContentType::Text,
        HeaderHashMap headers = {}
    ) {
        auto task = [&]() -> coroutine::Task<ResponseData> {
            if (needConnect()) {
                co_await makeSocket(url);
            }
            container::Try<> err{};
            RequestType req{_io.get()};
            req.template setReqLine<Method>(UrlParse::extractPath(url));
            preprocessHeaders(url, contentType, req);
            req.addHeaders(std::move(headers));
            try {
                co_await req.template sendChunkedReq<Timeout>(path);
                ResponseType res{_io.get()};
                if (!co_await res.template parserRes<Timeout>()) [[unlikely]] {
                    throw std::runtime_error{"Recv Timed Out"};
                }
                co_return res.makeResponseData();
            } catch (...) {
                ;
            }
            // 二次尝试
            try {
                co_await makeSocket(url);
                // 再次发送
                co_await req.template sendChunkedReq<Timeout>(path);
                ResponseType res{_io.get()};
                if (!co_await res.template parserRes<Timeout>()) [[unlikely]] {
                    throw std::runtime_error{"Recv Timed Out"};
                }
                co_return res.makeResponseData();
            } catch (...) {
                err.setException(std::current_exception());
            }
            // 断开连接
            co_await _io.get().close();
            _cliFd = kInvalidSocket;
            err.rethrow();
            co_return {};
        };
        co_return _isUniqueEventLoop
            ? _eventLoop->trySync(task())
            : co_await _eventLoop->tryAsync(task());
    }

    /**
     * @brief 发送一个 GET 请求, 其会在后台线程协程池中执行
     * @param url 请求的 URL
     * @return container::FutureResult<container::Try<ResponseData>> 
     */
    container::FutureResult<container::Try<ResponseData>> get(
        std::string url, 
        HeaderHashMap headers = {}
    ) {
        return requst<GET>(std::move(url), std::move(headers));
    }

    /**
     * @brief 发送一个 GET 请求, 其以协程的方式运行
     * @param url 请求的 URL
     * @return coroutine::Task<ResponseData> 
     */
    coroutine::Task<container::Try<ResponseData>> coGet(
        std::string url,
        HeaderHashMap headers = {}
    ) {
        co_return co_await coRequst<GET>(std::move(url), std::move(headers));
    }

    /**
     * @brief 发送一个 POST 请求, 其会在后台线程协程池中执行
     * @param url 请求的 URL
     * @param body 请求正文
     * @param contentType 请求正文类型
     * @return container::FutureResult<container::Try<ResponseData>> 
     */
    container::FutureResult<container::Try<ResponseData>> post(
        std::string url,
        std::string body,
        HttpContentType contentType,
        HeaderHashMap headers = {}
    ) {
        return requst<POST>(
            std::move(url), std::move(headers), 
            std::move(body), contentType);
    }

    /**
     * @brief 发送一个 POST 请求, 其以协程的方式运行
     * @param url 请求的 URL
     * @param body 请求正文
     * @param contentType 请求正文类型
     * @return coroutine::Task<ResponseData> 
     */
    coroutine::Task<container::Try<ResponseData>> coPost(
        std::string url,
        std::string body,
        HttpContentType contentType,
        HeaderHashMap headers = {}
    ) {
        co_return co_await coRequst<POST>(
            std::move(url), std::move(headers),
            std::move(body), contentType);
    }

    /**
     * @brief 异步的发送一个请求
     * @tparam Method 请求类型
     * @tparam Str 正文字符串类型
     * @param url url 或者 path (以连接的情况下)
     * @param body 正文
     * @param contentType 正文类型 
     * @return container::FutureResult<container::Try<ResponseData>> 响应数据
     */
    template <HttpMethod Method, meta::StringType Str = std::string>
    container::FutureResult<container::Try<ResponseData>> requst(
        std::string url,
        HeaderHashMap headers = {},
        Str&& body = {},
        HttpContentType contentType = HttpContentType::None
    ) {
#ifndef NDEBUG
        if (!_isUniqueEventLoop) [[unlikely]] {
            // 当使用共享事件循环时候, 仅可使用协程函数
            throw std::runtime_error{
                "When using a shared event loop, "
                "only coroutine functions are allowed"};
        }
#endif // !NDEBUG
        return _pool.addTask([this, _url = std::move(url),
                              _body = std::move(body), _headers = std::move(headers), 
                              contentType] {
            return coRequst<Method>(
                std::move(_url), std::move(_headers),
                std::move(_body), contentType
            ).runSync();
        });
    }

    /**
     * @brief 协程的发送一个请求
     * @tparam Method 请求类型
     * @tparam Str 正文字符串类型
     * @param url url 或者 path (以连接的情况下)
     * @param body 正文
     * @param contentType 正文类型 
     * @return coroutine::Task<ResponseData> 响应数据
     */
    template <HttpMethod Method, meta::StringType Str = std::string>
    coroutine::Task<container::Try<ResponseData>> coRequst(
        std::string url,
        HeaderHashMap headers = {},
        Str&& body = {},
        HttpContentType contentType = HttpContentType::None
    ) {
        auto task = [&]() -> coroutine::Task<ResponseData> {
            if (needConnect()) {
                co_await makeSocket(url);
            }
            co_return co_await sendReq<Method>(
                std::move(url),
                std::move(body),
                contentType,
                std::move(headers)
            );
        };
        co_return _isUniqueEventLoop 
            ? _eventLoop->trySync(task())
            : co_await _eventLoop->tryAsync(task());
    }

    /**
     * @brief 建立连接
     * @param url 
     * @return coroutine::Task<container::Try<>> 
     */
    coroutine::Task<container::Try<>> coConnect(std::string_view url) {
        auto task = [&]() -> coroutine::Task<> {
            if (needConnect()) {
                co_await coClose();
            }
            co_await makeSocket(url);
        };
        co_return _isUniqueEventLoop 
            ? _eventLoop->trySync(task())
            : co_await _eventLoop->tryAsync(task());
    }

    /**
     * @brief 建立连接
     * @param url 
     */
    container::FutureResult<container::Try<>> connect(std::string url) {
#ifndef NDEBUG
        if (!_isUniqueEventLoop) [[unlikely]] {
            // 当使用共享事件循环时候, 仅可使用协程函数
            throw std::runtime_error{
                "When using a shared event loop, "
                "only coroutine functions are allowed"};
        }
#endif // !NDEBUG
        return _pool.addTask([this, _url = std::move(url)](){
            return coConnect(_url).runSync();
        });
    }

    /**
     * @brief 断开连接
     */
    void close() {
        if (_isUniqueEventLoop) {
            _eventLoop->trySync(coClose());
            return;
        }
#ifndef NDEBUG
        else if (_cliFd != kInvalidSocket) [[unlikely]] {
            // 请先调用 coClose() 函数; 对于共享 EventLoop 情况, 必须手动 co_await coClose()
            throw std::runtime_error{"Please call coClose() first"};
        }
#endif // !NDEBUG
        _io.get().reset();
    }

    /**
     * @brief 断开连接
     * @tparam NowOsType 
     * @return coroutine::Task<> 
     */
    coroutine::Task<> coClose() noexcept {
        if (_cliFd == kInvalidSocket) {
            _io.get().reset();
            co_return;
        }
        co_await _io.get().close();
        _cliFd = kInvalidSocket;
    }

    /**
     * @brief 创建一个 WebSocket 循环, 其以线程池的方式独立运行
     * @tparam Func 
     * @param url  ws 的 url, 如 ws://127.0.0.1:28205/ws (如果不对则抛异常)
     * @param func 该声明为 [](WebSocketClient ws) -> coroutine::Task<> { }
     * @param headers 请求头
     * @return container::FutureResult<container::Try<Res>>
     */
    template <
        typename Func, 
        typename Res = coroutine::AwaiterReturnType<std::invoke_result_t<Func, WebSocketClient>>
    >
        requires(std::is_same_v<std::invoke_result_t<Func, WebSocketClient>, coroutine::Task<Res>>)
    container::FutureResult<container::Try<Res>> wsLoop(
        std::string url,
        Func&& func,
        HeaderHashMap headers = {}
    ) {
#ifndef NDEBUG
        if (!_isUniqueEventLoop) [[unlikely]] {
            // 当使用共享事件循环时候, 仅可使用协程函数
            throw std::runtime_error{
                "When using a shared event loop, "
                "only coroutine functions are allowed"};
        }
#endif // !NDEBUG
        return _pool.addTask([this, _url = std::move(url),
                              _func = std::forward<Func>(func), _headers = std::move(headers)]() mutable {
            return coWsLoop(
                std::move(_url), std::forward<Func>(_func), std::move(_headers)
            ).runSync();
        });
    }

    /**
     * @brief 一个独立的 WebSocket 循环
     * @tparam Func 
     * @param url  ws 的 url, 如 ws://127.0.0.1:28205/ws (如果不对则抛异常)
     * @param func 该声明为 [](WebSocketClient ws) -> coroutine::Task<> { }
     * @param headers 请求头
     * @return coroutine::Task<container::Try<Res>> 
     */
    template <
        typename Func, 
        typename Res = coroutine::AwaiterReturnType<std::invoke_result_t<Func, WebSocketClient>>
    >
        requires(std::is_same_v<std::invoke_result_t<Func, WebSocketClient>, coroutine::Task<Res>>)
    coroutine::Task<container::Try<Res>> coWsLoop(std::string url, Func&& func, HeaderHashMap headers = {}) {
        container::Try<Res> res;
        auto taskObj = [&]() -> coroutine::Task<> {
            if (needConnect()) {
                co_await makeSocket(url);
            }
            container::Uninitialized<WebSocketClient> ws;
            try {
                ws.set(co_await WebSocketFactory::connect<Timeout>(
                    url, _io.get(), headers
                ));
            } catch (...) {
                res.setException(std::current_exception());
            }
            if (!ws.isAvailable()) [[unlikely]] {
                // 之前的连接断开了, 再次尝试连接
                if (_isAutoReconnect) [[likely]] {
                    // 重新建立连接
                    co_await makeSocket(url);
                    try {
                        ws.set(co_await WebSocketFactory::connect<Timeout>(url, _io.get(), headers));
                        res.reset();
                    } catch(...) {
                        res.setException(std::current_exception());
                    }
                }
            }
            if (ws.isAvailable()) [[likely]] {
                try {
                    if constexpr (!std::is_void_v<Res>) {
                        res.setVal(co_await func(ws.move()));
                    } else {
                        co_await func(ws.move());
                        res.setVal(container::NonVoidType<>{});
                    }
                } catch (...) {
                    // 如果内部没有捕获异常, 就重抛给外部
                    res.setException(std::current_exception());
                }
            }
            // 断开连接
            co_await _io.get().close();
            _cliFd = kInvalidSocket;
        };
        auto taskMain = taskObj();
        if (_isUniqueEventLoop) {
            _eventLoop->start(taskMain);
            _eventLoop->run();
        } else {
            co_await taskMain;
        }
        co_return res;
    }

    HttpBaseClient& operator=(HttpBaseClient&&) noexcept = delete;

#ifdef NDEBUG
    ~HttpBaseClient() noexcept {
        close();
    }
#else
    ~HttpBaseClient() noexcept(false) {
        close();
    }
#endif // !NDEBUG
private:
    /**
     * @brief 建立 TCP 连接
     * @param url 
     * @return coroutine::Task<> 
     */
    coroutine::Task<> makeSocket(std::string_view url) {
        AddressResolver resolver;
        UrlInfoExtractor parser{_options.proxy.get().size() ? _options.proxy.get() : url};
        auto entry = resolver.resolve(parser.getHostname(), parser.getService());
        try {
            _cliFd = HXLIBS_CHECK_EVENT_LOOP((
                co_await _eventLoop->makeAioTask().prepSocket(
                    entry._curr->ai_family,
                    entry._curr->ai_socktype,
                    entry._curr->ai_protocol,
                    0
                )
            ));
            try {
                auto sockaddr = entry.getAddress();
                co_await _eventLoop->makeAioTask().prepConnect(
                    _cliFd,
                    sockaddr._addr,
                    sockaddr._addrlen
                );
                auto isInit = _io.isAvailable();
                if (!isInit) {                
                    _io.set(_cliFd, *_eventLoop);
                } else {
                    co_await _io.get().bindNewFd(_cliFd);
                }
                if constexpr (!std::is_same_v<Proxy, NoneProxy>) {
                    // 初始化代理
                    if (_options.proxy.get().size()) {
                        Proxy proxy{_io.get()};
                        co_await proxy.connect(_options.proxy.get(), url);
                    }
                }
#ifdef HXLIBS_ENABLE_SSL
                if (!isInit) {
                    _io.get().initSslBio(SslContext::SslType::Client);
                } else {
                    _io.get().resetSslBio();
                }
                // 初始化连接 (Https 握手)
                co_await _io.get().template initSsl<Timeout>(false);
#endif // !HXLIBS_ENABLE_SSL
                co_return;
            } catch (...) {
                log::hxLog.error("连接失败");
            }
        } catch (std::exception const& e) {
            log::hxLog.error("创建套接字失败:", e.what());
        }
        // 总之得关闭
        co_await _eventLoop->makeAioTask().prepClose(_cliFd);
        _cliFd = kInvalidSocket;
    }

    /**
     * @brief 发送 Http 请求
     * @tparam Method 请求类型
     * @tparam Str 
     * @param url 请求 url
     * @param body 请求体
     * @param contentType 正文类型, 如 HTML / JSON / TEXT 等等
     * @param headers 请求头
     * @return coroutine::Task<ResponseData> 
     */
    template <HttpMethod Method, meta::StringType Str = std::string_view>
    coroutine::Task<ResponseData> sendReq(
        std::string const& url,
        Str&& body = std::string_view{},
        HttpContentType contentType = HttpContentType::None,
        HeaderHashMap headers = {}
    ) {
        RequestType req{_io.get()};
        req.template setReqLine<Method>(UrlParse::extractPath(url));
        preprocessHeaders(url, contentType, req);
        req.addHeaders(std::move(headers));
        if (body.size()) {
            // @todo 请求体还需要支持一些格式!
            req.setBody(std::forward<Str>(body));
        }
        std::exception_ptr exceptionPtr{};
        try {
            bool isOkFd = true;
            try {
                co_await req.template sendHttpReq<Timeout>();
            } catch (...) {
                // e: 大概率是 断开的管道, 直接重连
                isOkFd = false;
            }
            ResponseType res{_io.get()};
            do {
                try {
                    // 可能会抛异常...
                    if (isOkFd && co_await res.template parserRes<Timeout>()) {
                        break;
                    }
                } catch (...) {
                    ;
                }
                // 读取超时
                if (_isAutoReconnect) [[likely]] {
                    // 重新建立连接
                    co_await makeSocket(url);
                    // 重新发送一次请求
                    co_await req.template sendHttpReq<Timeout>();
                    // 再次解析请求
                    if (co_await res.template parserRes<Timeout>()) [[likely]] {
                        break;
                    }
                }
                [[unlikely]] throw std::runtime_error{"Send Timed Out"};
            } while (false);
            if (auto it = req.getHeaders().find("Connection");
                it != req.getHeaders().end() && it->second == "close"
            ) {
                co_await _io.get().close();
                _cliFd = kInvalidSocket;
            }
            co_return res.makeResponseData();
        } catch (...) {
            exceptionPtr = std::current_exception();
        }
        
        log::hxLog.error("解析出错"); // debug

        co_await _io.get().close();
        _cliFd = kInvalidSocket;
        std::rethrow_exception(exceptionPtr);
    }

    /**
     * @brief 预处理请求头
     * @param url 
     * @param req 
     */
    void preprocessHeaders(std::string const& url, HttpContentType contentType, RequestType& req) { 
        try {
            auto host = UrlParse::extractDomainName(url);
            req.tryAddHeaders("Host", host);
            _host = std::move(host);
        } catch ([[maybe_unused]] std::exception const& e) {
            if (_host.size()) [[likely]] {
                req.tryAddHeaders("Host", _host);
            }
        }
        req.tryAddHeaders("Accept", "*/*");
        req.tryAddHeaders("Connection", "keep-alive");
        req.tryAddHeaders("User-Agent", "HXLibs/1.0");
        req.tryAddHeaders("Content-Type", getContentTypeStrView(contentType));
        req.tryAddHeaders("Date", utils::DateTimeFormat::makeHttpDate());
    }

    HttpClientOptions<Timeout, Proxy> _options;
    std::shared_ptr<coroutine::EventLoop> _eventLoop;
    SocketFdType _cliFd;
    container::ThreadPool _pool;
    container::Uninitialized<IO> _io;

    // 上一次解析的 Host
    std::string _host;

    // 是否自动重连
    bool _isAutoReconnect;

    // 是否为独享事件循环
    bool _isUniqueEventLoop;
};

template <typename Timeout, typename Proxy>
    requires(utils::HasTimeNTTP<Timeout>)
class HttpClient : public HttpBaseClient<Timeout, Proxy, HttpRequest<HttpIO>, HttpResponse<HttpIO>> {
    using Base = HttpBaseClient<Timeout, Proxy, HttpRequest<HttpIO>, HttpResponse<HttpIO>>;
public:
    using Base::Base;

    HttpClient(
        HttpClientOptions<Timeout, Proxy> options = HttpClientOptions{},
        std::shared_ptr<coroutine::EventLoop> loop = nullptr
    )
        : Base(std::move(options), std::move(loop))
    {}
};

HttpClient() -> HttpClient<decltype(utils::operator""_ms<"5000">()), NoneProxy>;

#if HXLIBS_ENABLE_SSL

template <typename Timeout, typename Proxy>
    requires(utils::HasTimeNTTP<Timeout>)
class HttpsClient : public HttpBaseClient<Timeout, Proxy, HttpRequest<HttpsIO>, HttpResponse<HttpsIO>> {
    using Base = HttpBaseClient<Timeout, Proxy, HttpRequest<HttpsIO>, HttpResponse<HttpsIO>>;
public:
    using Base::Base;
    
    HttpsClient(
        HttpClientOptions<Timeout, Proxy> options = HttpClientOptions{},
        std::shared_ptr<coroutine::EventLoop> loop = nullptr
    )
        : Base(std::move(options), std::move(loop))
    {}

    /**
     * @brief 初始化 SSL, 此为全局单例. 仅以第一次调用为准.
     * @param config 
     * @return void
     */
    static void initSsl(SslConfig config) {
        SslContext::get().init<SslContext::SslType::Client>(std::move(config));
    }
};

HttpsClient() -> HttpsClient<decltype(utils::operator""_ms<"5000">()), NoneProxy>;

#endif // !HXLIBS_ENABLE_SSL

} // namespace HX::net
