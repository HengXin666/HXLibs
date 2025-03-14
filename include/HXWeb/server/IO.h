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
#ifndef _HX_SERVER_IO_H_
#define _HX_SERVER_IO_H_

#include <HXSTL/coroutine/task/Task.hpp>
#include <HXWeb/protocol/http/Http.hpp>
#include <HXWeb/protocol/https/Https.hpp>
#include <HXWeb/socket/IO.h>

// 前置声明
typedef struct ssl_st SSL;

namespace HX { namespace web { namespace server {

template <class T>
struct ConnectionHandler;

template <class T = void>
class IO {
    // 静态断言: 不允许其他非void的实现 (这个还不起作用...)
    static_assert(std::is_same<T, void>::value, "Not supported for instantiation");
};

/**
 * @brief 服务连接时候的io基类
 */
template <>
class IO<void> : public HX::web::socket::IO {
public:
    explicit IO(int fd);

    virtual ~IO() noexcept = default;

    /**
     * @brief 立即发送响应
     */
    HX::STL::coroutine::task::Task<> sendResponse() const;
    HX::STL::coroutine::task::Task<> sendResponse(HX::STL::container::NonVoidHelper<>) const;

    /**
     * @brief 设置响应体使用`TransferEncoding`分块编码, 以传输读取的文件
     * @param path 需要读取的文件的路径
     * @warning 在此之前都不需要使用`setBodyData`
     */
    HX::STL::coroutine::task::Task<> sendResponseWithChunkedEncoding(
        const std::string& path
    ) const;

    /**
     * @brief 使用`断点续传`, 以传输读取的文件. 内部会自动处理.
     * @param path 需要读取的文件的路径
     * @warning 在此之前都不需要使用`setBodyData` 
     */
    HX::STL::coroutine::task::Task<> sendResponseWithRange(
        const std::string& path
    ) const;

    /**
     * @brief 是否复用连接
     * @return true 复用
     * @return false 不复用
     */
    bool isReuseConnection() const noexcept {
        return _isReuseConnection;
    }

    /**
     * @brief 从请求头更新是否复用连接
     * @param isReuseConnection 
     */
    void updateReuseConnection() noexcept;
protected:
    /**
     * @brief 解析一条完整的客户端请求
     * @return bool 是否断开连接
     */
    virtual HX::STL::coroutine::task::Task<bool> _recvRequest() = 0;

    /**
     * @brief 写入响应到套接字
     * @param buf 需要写入的数据
     */
    virtual HX::STL::coroutine::task::Task<> _sendResponse(
        std::span<char const> buf
    ) const = 0;

    friend HX::web::protocol::websocket::WebSocket;
    friend HX::web::protocol::http::Request;
    friend HX::web::protocol::http::Response;
private:
    HX::STL::coroutine::task::Task<> __sendResponse() const;
    bool _isReuseConnection;
};

template <>
class IO<HX::web::protocol::http::Http> : public HX::web::server::IO<> {
public:
    explicit IO(int fd)
        : HX::web::server::IO<>(fd)
    {}

    ~IO() noexcept = default;
protected:
    virtual HX::STL::coroutine::task::Task<bool> _recvRequest() override;

    virtual HX::STL::coroutine::task::Task<> _sendResponse(
        std::span<char const> buf
    ) const override;

    friend ConnectionHandler<HX::web::protocol::http::Http>;
};

template <>
class IO<HX::web::protocol::https::Https> : public HX::web::server::IO<> {
public:
    explicit IO(int fd)
        : HX::web::server::IO<>(fd)
    {}

    ~IO() noexcept;
protected:
    /**
     * @brief 进行SSL(https)握手
     * @return bool 是否成功
     */
    HX::STL::coroutine::task::Task<bool> handshake();

    virtual HX::STL::coroutine::task::Task<bool> _recvRequest() override;

    virtual HX::STL::coroutine::task::Task<> _sendResponse(
        std::span<char const> buf
    ) const override;

    friend ConnectionHandler<HX::web::protocol::https::Https>;

    SSL* _ssl = nullptr;
};

}}} // namespace HX::web::server

#endif // !_HX_SERVER_IO_H_