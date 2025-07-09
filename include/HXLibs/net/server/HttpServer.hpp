#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-07 21:25:09
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
#ifndef _HX_HTTP_SERVER_H_
#define _HX_HTTP_SERVER_H_

#include <HXLibs/net/router/Router.hpp>
#include <HXLibs/net/socket/AddressResolver.hpp>
#include <HXLibs/net/server/Acceptor.hpp>
#include <HXLibs/coroutine/loop/EventLoop.hpp>

namespace HX::net {

class HttpServer {
public:
    /**
     * @brief 创建一个Http服务器
     * @param name 服务器绑定的地址, 如`127.0.0.1`
     * @param port 服务器绑定的端口, 如`28205`
     */
    HttpServer(
        std::string name,
        std::string port
    )
        : _router{}
        , _name{std::move(name)}
        , _port{std::move(port)}
        , _isRun{true}
    {}

    HttpServer& operator=(HttpServer&&) = delete;

    /**
     * @brief 关闭服务器
     */
    void stop() {
        _isRun = false;
        log::hxLog.warning("服务器正在关闭...");
    }

    /**
    * @brief 为服务器添加一个端点
    * @tparam Methods 请求类型, 如`GET`、`POST`
    * @tparam Func 端点函数类型
    * @tparam Interceptors 拦截器类型
    * @param key url, 如"/"、"home/{id}"
    * @param endpoint 端点函数
    * @param interceptors 拦截器
    * @return HttpServer& 可链式调用
    */
    template <HttpMethod... Methods, typename Func, typename... Interceptors>
    HttpServer& addEndpoint(std::string_view path, Func endpoint, Interceptors&&... interceptors) {
        _router.addEndpoint<Methods...>(
            path,
            std::move(endpoint),
            std::forward<Interceptors>(interceptors)...
        );
        return *this;
    }

    /**
     * @brief 同步启动 HttpServer
     * @tparam Timeout 字面常量, 表示超时时间 (单位: 秒(s))
     * @param threadNum 线程数
     */
    template <std::size_t Timeout = 30>
    void sync(std::size_t threadNum = std::thread::hardware_concurrency()) {
        async(threadNum);
        _threads.clear();
    }

    /**
     * @brief 异步启动 HttpServer
     * @warning 本方法不可重入, 并且线程不安全
     * @tparam Timeout 字面常量, 表示超时时间 (单位: 秒(s))
     * @param threadNum 线程数
     */
    template <std::size_t Timeout = 30>
    void async(std::size_t threadNum = std::thread::hardware_concurrency()) {
        if (!_threads.empty()) [[unlikely]] {
            throw std::runtime_error{"The server is already running"};
        }
        for (std::size_t i = 0; i < threadNum; ++i) {
            _threads.emplace_back([this] {
                _sync<Timeout>();
            });
        }
        log::hxLog.info("====== HXServer start: \033[33m\033]8;;http://" 
            + _name 
            + ":" 
            + _port 
            + "/\033\\http://"
            + _name
            + ":"
            + _port
            + "/\033]8;;\033\\\033[0m\033[1;32m ======");
    }

private:
    template <std::size_t Timeout>
    void _sync() {
        AddressResolver addr;
        auto entry = addr.resolve(_name, _port);
        try {
            coroutine::EventLoop _eventLoop;
            Acceptor acceptor{_router, _eventLoop, entry};
            auto mainTask = acceptor.start<Timeout>(_isRun);
            _eventLoop.start(mainTask);
            _eventLoop.run();
        } catch (std::exception const& ec) {
            log::hxLog.error("Server Error:", ec.what());
        }
    }
    
    Router _router;
    std::vector<std::jthread> _threads;
    std::string _name;
    std::string _port;
    std::atomic_bool _isRun;
};

} // namespace HX::net

#endif // !_HX_HTTP_SERVER_H_