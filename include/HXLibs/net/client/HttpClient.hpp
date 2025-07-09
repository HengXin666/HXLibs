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
#include <HXLibs/coroutine/loop/EventLoop.hpp>

namespace HX::net {

template <typename Timeout>
    requires(requires { Timeout::Val; })
class HttpClient {
public:
    HttpClient(HttpClientOptions<Timeout>&& options) 
        : _options{std::move(options)}
    {
        _req.addHeaders(_options.reqHead);
    }

    void get([[maybe_unused]] std::string_view url) {
        coroutine::EventLoop _eventLoop;
        _req.setReqLine<GET>(url.empty() ? _options.url : url);
        auto mainTask = [&]() -> coroutine::Task<> {
            // 需要编写连接者 ...
        }();
        _eventLoop.start(mainTask);
    }

    HttpClient& operator=(HttpClient&&) noexcept = delete;
private:
    HttpClientOptions<Timeout> _options;
    Request _req;
    Response _res;
};

} // namespace HX::net

#endif // !_HX_HTTP_CLIENT_H_