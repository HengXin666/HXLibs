#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-08 10:45:58
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
#ifndef _HX_CONNECTION_HANDLER_H_
#define _HX_CONNECTION_HANDLER_H_

#include <HXLibs/coroutine/task/RootTask.hpp>
#include <HXLibs/coroutine/loop/EventLoop.hpp>
#include <HXLibs/net/socket/SocketFd.hpp>
#include <HXLibs/net/router/Router.hpp>
#include <HXLibs/net/socket/IO.hpp>
#include <HXLibs/net/protocol/http/Request.hpp>
#include <HXLibs/net/protocol/http/Response.hpp>

namespace HX::net {

struct ConnectionHandler {

    template <std::size_t Timeout>
    static coroutine::RootTask<> start(
        SocketFdType fd,
        Router const& router,
        coroutine::EventLoop& eventLoop
    ) {
        IO io{fd, eventLoop};
        Request  req{io};
        Response res{io};

        try {
            for (;;) {
                // 读
                if (!co_await req.parserRequest<Timeout>()) [[unlikely]] {
                    break;
                }
                // 路由
                co_await router.getEndpoint(
                    req.getRequesType(), 
                    req.getRequesPath()
                )(req, res);
        
                // 写 (由端点内部完成)

                // 清空
                req.clear();
                res.clear();
            }
        } catch (...) {
            log::hxLog.error("发生未知错误!");
        }
        log::hxLog.debug("连接已断开");

        co_await io.close();

        co_return;
    };

};

} // namespace HX::net

#endif // !_HX_CONNECTION_HANDLER_H_