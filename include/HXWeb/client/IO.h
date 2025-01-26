#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2024-08-21 21:01:37
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
#ifndef _HX_CLIENT_IO_H_
#define _HX_CLIENT_IO_H_

#include <chrono>

#include <HXSTL/coroutine/task/Task.hpp>
#include <HXWeb/protocol/http/Http.hpp>
#include <HXWeb/protocol/https/Https.hpp>
#include <HXWeb/socket/IO.h>

// 前置声明

typedef struct ssl_st SSL;

namespace HX { namespace web { namespace protocol { namespace proxy {

class Socks5Proxy;

}}}} // namespace::HX::web::protocol::proxy

namespace HX { namespace web { namespace client {

template <class T = void>
class IO {
    // 静态断言: 不允许其他非void的实现
    static_assert(true, "Not supported for instantiation");
};

/**
 * @brief 客户端连接基类  
 */
template <>
class IO<void> : public HX::web::socket::IO {
public:
    explicit IO(int fd);

    virtual ~IO() noexcept
    {}

    IO& operator=(IO&&) = delete;

    /**
     * @brief 解析一条完整的服务端响应
     * @param timeout 超时时间
     * @return bool 是否断开连接
     */
    HX::STL::coroutine::task::Task<bool> recvResponse(
        std::chrono::milliseconds timeout
    );

    /**
     * @brief 写入请求到套接字
     */
    HX::STL::coroutine::task::Task<> sendRequest() const;

    /**
     * @brief 初始化连接
     * @param timeout 超时时间
     * @return bool 是否成功
     */
    virtual HX::STL::coroutine::task::Task<bool> init(
        std::chrono::milliseconds timeout
    ) = 0;
protected:
    /**
     * @brief 解析一条完整的服务端响应
     * @return bool 是否断开连接
     */
    virtual HX::STL::coroutine::task::Task<bool> _recvResponse() = 0;

    /**
     * @brief 写入请求到套接字
     * @param buf 需要写入的数据
     */
    virtual HX::STL::coroutine::task::Task<> _sendRequest(
        std::span<char> buf
    ) const = 0;

    friend HX::web::protocol::websocket::WebSocket;
    friend HX::web::protocol::http::Request;
    friend HX::web::protocol::http::Response;
    friend HX::web::protocol::proxy::Socks5Proxy;
    friend class Client;
};

template <>
class IO<HX::web::protocol::http::Http> : public IO<void> {
public:
    explicit IO(int fd) : IO<void>(fd)
    {}

    ~IO() noexcept = default;

    HX::STL::coroutine::task::Task<bool> init(
        std::chrono::milliseconds
    ) override {
        co_return true;
    }
protected:
    /**
     * @brief 解析一条完整的服务端响应
     * @return bool 是否断开连接
     */
    HX::STL::coroutine::task::Task<bool> _recvResponse() override;

    /**
     * @brief 写入请求到套接字
     */
    HX::STL::coroutine::task::Task<> _sendRequest(
        std::span<char> buf
    ) const override;
};

template <>
class IO<HX::web::protocol::https::Https> : public IO<void> {
public:
    explicit IO(int fd) : IO<void>(fd)
    {}

    ~IO() noexcept;

    HX::STL::coroutine::task::Task<bool> init(
        std::chrono::milliseconds timeout
    ) override;
protected:
    /**
     * @brief 进行SSL(https)握手
     * @return bool 是否成功
     */
    HX::STL::coroutine::task::Task<bool> handshake();

    /**
     * @brief 解析一条完整的服务端响应
     * @return bool 是否断开连接
     */
    HX::STL::coroutine::task::Task<bool> _recvResponse() override;

    /**
     * @brief 写入请求到套接字
     */
    HX::STL::coroutine::task::Task<> _sendRequest(
        std::span<char> buf
    ) const override;

    SSL* _ssl = nullptr;
};

}}} // namespace HX::web::server

#endif // !_HX_CLIENT_IO_H_