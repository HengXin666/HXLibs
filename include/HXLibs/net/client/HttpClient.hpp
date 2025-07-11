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
#include <HXLibs/coroutine/loop/EventLoop.hpp>
#include <HXLibs/container/ThreadPool.hpp>
#include <HXLibs/utils/ContainerConcepts.hpp>
#include <HXLibs/exception/ErrorHandlingTools.hpp>

#include <HXLibs/log/Log.hpp>

namespace HX::net {

template <typename Timeout>
    requires(requires { Timeout::Val; })
class HttpClient {
public:
    /**
     * @brief 构造一个 HTTP 客户端
     * @param options 选项
     * @param threadNum 线程数
     */
    HttpClient(HttpClientOptions<Timeout>&& options = HttpClientOptions{}, uint32_t threadNum = 1) 
        : _options{std::move(options)}
        , _eventLoop{}
        , _cliFd{kInvalidSocket}
        , _pool{}
    {
        _pool.setFixedThreadNum(threadNum);
        _pool.run<container::ThreadPool::Model::FixedSizeAndNoCheck>();
    }

    void run(coroutine::Task<> const& coMain) {
        _eventLoop.start(coMain);
        _eventLoop.run();
    }

    /**
     * @brief 判断是否需要重新建立连接
     * @return true 
     * @return false 
     */
    bool needConnect() const {
        return _cliFd == kInvalidSocket;
    }

    /**
     * @brief 发送一个 GET 请求, 其会在后台线程协程池中执行
     * @param url 请求的 URL
     * @return container::FutureResult<ResponseData> 
     */
    container::FutureResult<ResponseData> get(std::string url) {
        return _pool.addTask([this, _url = std::move(url)] {
            return coGet(std::move(_url)).start();
        });
    }

    /**
     * @brief 发送一个 GET 请求, 其以协程的方式运行
     * @param url 请求的 URL
     * @return coroutine::Task<ResponseData> 
     */
    coroutine::Task<ResponseData> coGet(std::string url) {
        container::FutureResult<ResponseData> res;
        auto task = [this, ans = res.getFutureResult()](std::string _url) -> coroutine::Task<> {
            if (needConnect()) {
                co_await makeSocket(_url);
            }
            ans->setData(co_await sendReq<GET>(_url));
            co_return;
        }(std::move(url));
        _eventLoop.start(task);
        _eventLoop.run();
        co_return res.get();
    }

    /**
     * @brief 建立连接
     * @param url 
     * @return coroutine::Task<> 
     */
    coroutine::Task<> connect(std::string url) {
        co_await makeSocket(url);
        co_await sendReq<GET>(url);
    }

    /**
     * @brief 清空请求头
     */
    void clearHeader() {
        _options.reqHead.clear();
    }

    /**
     * @brief 添加请求头项
     * @param key 
     * @param val 
     * @return true 添加成功
     * @return false 添加失败: key 不能为空
     */
    bool addHeader(std::string const& key, std::string val) {
        if (key.empty()) [[unlikely]] {
            return false;
        }
        _options.reqHead[key] = std::move(val);
        return true;
    }

    /**
     * @brief 设置请求头
     * @param header 
     */
    void setHeader(std::unordered_map<std::string, std::string> header) {
        clearHeader();
        _options.reqHead = std::move(header);
    }

    HttpClient& operator=(HttpClient&&) noexcept = delete;
private:
    /**
     * @brief 建立 TCP 连接
     * @param url 
     * @return coroutine::Task<> 
     */
    coroutine::Task<> makeSocket(std::string_view url) {
        AddressResolver resolver;
        UrlInfoExtractor parser{_options.proxy.size() ? _options.proxy : url};
        auto entry = resolver.resolve(parser.getHostname(), parser.getService());
        _cliFd = exception::IoUringErrorHandlingTools::check(
            co_await _eventLoop.makeAioTask().prepSocket(
                entry._curr->ai_family,
                entry._curr->ai_socktype,
                entry._curr->ai_protocol,
                0
            )
        );
        auto sockaddr = entry.getAddress();
        co_await _eventLoop.makeAioTask().prepConnect(
            _cliFd,
            sockaddr._addr,
            sockaddr._addrlen
        );
    }

    template <HttpMethod Method, utils::StringType Str = std::string_view>
    coroutine::Task<ResponseData> sendReq(std::string_view url, Str&& body = std::string_view{}) {
        IO io{_cliFd, _eventLoop};
        Request req{io};
        req.setReqLine<Method>(UrlParse::extractPath(url));
        req.addHeaders(_options.reqHead);
        if (body.size()) {
            req.setBody(std::forward<Str>(body));
        }
        do {
            try {
                co_await req.sendHttpReq<Timeout>();
                Response res{io};
                if (co_await res.parserRes<Timeout>() == false) [[unlikely]] {
                    break;
                }
                io.reset();
                co_return res.makeResponseData();
            } catch (std::exception const& e) {
                break;
            }
        } while (false);
        
        log::hxLog.error("解析出错");
        co_await io.close();
        _cliFd = kInvalidSocket;
        co_return {};
    }

    HttpClientOptions<Timeout> _options;
    coroutine::EventLoop _eventLoop;
    SocketFdType _cliFd;
    container::ThreadPool _pool;
};

HttpClient() -> HttpClient<decltype(utils::operator""_ms<'5', '0', '0', '0'>())>;

} // namespace HX::net

#endif // !_HX_HTTP_CLIENT_H_