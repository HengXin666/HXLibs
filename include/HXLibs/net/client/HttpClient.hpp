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
#ifndef _HX_HTTP_CLIENT_H_
#define _HX_HTTP_CLIENT_H_

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

template <typename Timeout, typename Proxy>
    requires(requires { Timeout::Val; })
class HttpClient {
public:
    /**
     * @brief 构造一个 HTTP 客户端
     * @param options 选项
     * @param threadNum 线程数
     */
    HttpClient(HttpClientOptions<Timeout, Proxy>&& options = HttpClientOptions{}, uint32_t threadNum = 1) 
        : _options{std::move(options)}
        , _eventLoop{}
        , _cliFd{kInvalidSocket}
        , _pool{}
        , _host{}
        , _headers{}
    {
        _pool.setFixedThreadNum(threadNum);
        _pool.run<container::ThreadPool::Model::FixedSizeAndNoCheck>();
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
     * @brief 发送一个 GET 请求, 其会在后台线程协程池中执行
     * @param url 请求的 URL
     * @return container::FutureResult<ResponseData> 
     */
    container::FutureResult<ResponseData> get(
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
    coroutine::Task<ResponseData> coGet(
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
     * @return container::FutureResult<ResponseData> 
     */
    container::FutureResult<ResponseData> post(
        std::string url,
        HeaderHashMap headers,
        std::string body,
        HttpContentType contentType
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
    coroutine::Task<ResponseData> coPost(
        std::string url,
        HeaderHashMap headers,
        std::string body,
        HttpContentType contentType
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
     * @return container::FutureResult<ResponseData> 响应数据
     */
    template <HttpMethod Method, meta::StringType Str = std::string>
    container::FutureResult<ResponseData> requst(
        std::string url,
        HeaderHashMap headers = {},
        Str&& body = {},
        HttpContentType contentType = HttpContentType::None
    ) {
        return _pool.addTask([this, _url = std::move(url),
                              _body = std::move(body), _headers = std::move(headers), 
                              contentType] {
            return coRequst<Method>(
                std::move(_url), std::move(_headers),
                std::move(_body), contentType
            ).start();
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
    coroutine::Task<ResponseData> coRequst(
        std::string url,
        HeaderHashMap headers = {},
        Str&& body = {},
        HttpContentType contentType = HttpContentType::None
    ) {
        container::FutureResult<ResponseData> res;
        _headers = std::move(headers);
        auto task = [&, ans = res.getFutureResult()]() -> coroutine::Task<> {
            try {
                if (needConnect()) {
                    co_await makeSocket(url);
                }
                ans->setData(co_await sendReq<Method>(
                    url, std::move(body), contentType)
                );
#if defined(_WIN32)
                    // 主动泄漏 fd, 以退出事件循环 (仅 IOCP)
                    _eventLoop.getEventDrive().leak(_cliFd);
#endif // !defined(_WIN32)
            } catch (...) {
                ans->unhandledException();
            }
            co_return;
        };
        auto taskMain = task();
        _eventLoop.start(taskMain);
        _eventLoop.run();
        co_return res.get();
    }

    /**
     * @brief 建立连接
     * @param url 
     * @return coroutine::Task<> 
     */
    coroutine::Task<> coConnect(std::string url) {
        co_await makeSocket(url);
        co_await sendReq<GET>(url);
    }

    /**
     * @brief 断开连接
     * @return container::FutureResult<> 
     */
    container::FutureResult<> close() {
        return _pool.addTask([this] {
            coClose().start();
        });
    }

    /**
     * @brief 断开连接
     * @tparam NowOsType 
     * @return coroutine::Task<> 
     */
    coroutine::Task<> coClose() {
        if (_cliFd == kInvalidSocket) {
            co_return;
        }
        auto task = [this] () -> coroutine::Task<> {
            co_await _eventLoop.makeAioTask().prepClose(_cliFd);
            _cliFd = kInvalidSocket;
        };
#if defined(_WIN32)
            // 主动回复 fd, 以维持事件循环 (仅 IOCP)
            _eventLoop.getEventDrive().heal(_cliFd);
#endif // !defined(_WIN32)
        auto taskMain = task();
        _eventLoop.start(taskMain);
        _eventLoop.run();
        co_return;
    }

    /**
     * @brief 创建一个 WebSocket 循环, 其以线程池的方式独立运行
     * @tparam Func 
     * @param url  ws 的 url, 如 ws://127.0.0.1:28205/ws (如果不对则抛异常)
     * @param func 该声明为 [](WebSocketClient ws) -> coroutine::Task<> { }
     * @return container::FutureResult<>
     */
    template <typename Func>
        requires(std::is_same_v<std::invoke_result_t<Func, WebSocketClient>, coroutine::Task<>>)
    container::FutureResult<> wsLoop(std::string url, Func&& func) {
        return _pool.addTask([this, _url = std::move(url),
                              _func = std::forward<Func>(func)] {
            return coWsLoop(std::move(_url), _func).start();
        });
    }

    /**
     * @brief 一个独立的 WebSocket 循环
     * @tparam Func 
     * @param url  ws 的 url, 如 ws://127.0.0.1:28205/ws (如果不对则抛异常)
     * @param func 该声明为 [](WebSocketClient ws) -> coroutine::Task<> { }
     * @return coroutine::Task<> 
     */
    template <typename Func>
        requires(std::is_same_v<std::invoke_result_t<Func, WebSocketClient>, coroutine::Task<>>)
    coroutine::Task<> coWsLoop(std::string url, Func&& func) {
        std::exception_ptr exceptionPtr{};
        auto taskObj = [&]() -> coroutine::Task<> {
            if (needConnect()) {
                co_await makeSocket(url);
            }
            IO io{_cliFd, _eventLoop};
            try {            
                co_await func(
                    co_await WebSocketFactory::connect<Timeout>(url, io)
                );
            } catch (...) {
                // 如果内部没有捕获异常, 就重抛给外部
                exceptionPtr = std::current_exception();
            }
            // 断开连接
            co_await io.close();
            _cliFd = kInvalidSocket;
        };
        auto taskMain = taskObj();
        _eventLoop.start(taskMain);
        _eventLoop.run();
        if (exceptionPtr) [[unlikely]] {
            std::rethrow_exception(exceptionPtr);
        }
        co_return;
    }

    HttpClient& operator=(HttpClient&&) noexcept = delete;

    ~HttpClient() noexcept {
        close().wait();
    }
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
        _cliFd = HXLIBS_CHECK_EVENT_LOOP((
            co_await _eventLoop.makeAioTask().prepSocket(
                entry._curr->ai_family,
                entry._curr->ai_socktype,
                entry._curr->ai_protocol,
                0
            )
        ));
        try {
            auto sockaddr = entry.getAddress();
            co_await _eventLoop.makeAioTask().prepConnect(
                _cliFd,
                sockaddr._addr,
                sockaddr._addrlen
            );
            if (_options.proxy.get().size()) {
                // 初始化代理
                IO io{_cliFd, _eventLoop};
                Proxy proxy{io};
                co_await proxy.connect(_options.proxy.get(), url);
                io.reset();
            }
            // 初始化连接 (如 Https 握手)
            co_return;
        } catch (...) {
            ;
        }
        // 总之得关闭
        co_await _eventLoop.makeAioTask().prepClose(_cliFd);
        _cliFd = kInvalidSocket;
    }

    /**
     * @brief 发送 Http 请求
     * @tparam Method 请求类型
     * @tparam Str 
     * @param url 请求 url
     * @param body 请求体
     * @param contentType 正文类型, 如 HTML / JSON / TEXT 等等
     * @return coroutine::Task<ResponseData> 
     */
    template <HttpMethod Method, meta::StringType Str = std::string_view>
    coroutine::Task<ResponseData> sendReq(
        std::string const& url,
        Str&& body = std::string_view{},
        HttpContentType contentType = HttpContentType::None
    ) {
        IO io{_cliFd, _eventLoop};
        Request req{io};
        req.setReqLine<Method>(UrlParse::extractPath(url));
        preprocessHeaders(url, contentType, req);
        req._requestHeaders = std::move(_headers);
        if (body.size()) {
            // @todo 请求体还需要支持一些格式!
            req.setBody(std::forward<Str>(body));
        }
        std::exception_ptr exceptionPtr{};
        try {
            co_await req.sendHttpReq<Timeout>();
            Response res{io};
            if (co_await res.parserRes<Timeout>() == false) [[unlikely]] {
                // 读取超时
                throw std::runtime_error{"Send Timed Out"};
            }
            io.reset();
            co_return res.makeResponseData();
        } catch (...) {
            exceptionPtr = std::current_exception();
        }
        
        log::hxLog.error("解析出错"); // debug
        co_await io.close();
        _cliFd = kInvalidSocket;
        std::rethrow_exception(exceptionPtr);
    }

    /**
     * @brief 预处理请求头
     * @param url 
     * @param req 
     */
    void preprocessHeaders(std::string const& url, HttpContentType contentType, Request& req) { 
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
    coroutine::EventLoop _eventLoop;
    SocketFdType _cliFd;
    container::ThreadPool _pool;

    // 上一次解析的 Host
    std::string _host;

    // 请求头
    HeaderHashMap _headers;
};

HttpClient() -> HttpClient<decltype(utils::operator""_ms<'5', '0', '0', '0'>()), Socks5Proxy>;

} // namespace HX::net

#endif // !_HX_HTTP_CLIENT_H_