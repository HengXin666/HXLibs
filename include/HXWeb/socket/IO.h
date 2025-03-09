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
#ifndef _HX_SOCKET_IO_H_
#define _HX_SOCKET_IO_H_

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

#include <liburing.h>

#pragma GCC diagnostic pop

#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <HXSTL/coroutine/task/Task.hpp>

#ifdef __GNUC__
#define HOT_FUNCTION [[gnu::hot]]
#else
#define HOT_FUNCTION
#endif

namespace HX { namespace web { namespace protocol { namespace websocket {

class WebSocket;

}}}} // HX::web::protocol::websocket

namespace HX { namespace web { namespace protocol { namespace http {

class Request;
class Response;

}}}} // namespace HX::web::protocol::http

namespace HX { namespace web { namespace socket {

/**
 * @brief 套接字io基类
 */
class IO {
public:
    explicit IO(int fd);

    virtual ~IO() noexcept;

    HOT_FUNCTION HX::web::protocol::http::Request& getRequest() const noexcept {
        return *_request;
    }

    HOT_FUNCTION HX::web::protocol::http::Response& getResponse() const noexcept {
        return *_response;
    }

    // 父类禁止, 子类也继承禁止, 除非重新声明
    IO& operator=(IO&&) = delete;
protected:
    // === start === 读取相关的函数 === start ===
    /**
     * @brief 尝试读取 n 个字符
     * @param n 读取字符个数
     * @return std::optional<std::string>, 超时或者断开连接, 则为 std::nullpot
     */
    HX::STL::coroutine::task::Task<std::optional<std::string>> recvN(
        std::size_t n
    ) const;

    /**
     * @brief 尝试读取 n 个字符
     * @param buf 存储读取字符的字符数组视图
     * @param n 读取字符个数 (buf.size() >= n)
     * @return 读取的字节数
     */
    HX::STL::coroutine::task::Task<std::size_t> recvN(
        std::span<char> buf,
        std::size_t n
    ) const;

    /**
     * @brief 读取一个大小为`sizeof(T)`字节的内容, 并且构造为T类型的数据
     * @tparam T 读取的类型
     * @param res [in, out] 原地在`res`的内存上重新构造对象
     * @return T
     */
    template <class T>
    HX::STL::coroutine::task::Task<T> recvStruct(
        T& res
    ) const {
        co_return co_await _recvSpan(
            std::span<char>(
                reinterpret_cast<char *>(&res), 
                sizeof(T)
            )
        );
    }

    /**
     * @brief 读取一个大小为`sizeof(T)`字节的内容, 并且构造为T类型的数据
     * @tparam T 读取的类型
     * @return T 读取的类型
     */
    template <class T>
    HX::STL::coroutine::task::Task<T> recvStruct() const {
        T res;
        co_await recvStruct<T>(res);
        co_return res;
    }

    HX::STL::coroutine::task::Task<int> _recvSpan(std::span<char> buf) const;
    // === end === 读取相关的函数 === end ===

    // === start === 写入相关的函数 === start ===
    /**
     * @brief 写入数据到套接字
     * @param buf 需要写入的内容
     */
    HX::STL::coroutine::task::Task<> _sendSpan(std::span<char> buf) const;
    // === end === 写入相关的函数 === end ===

    /**
     * @brief 监测fd的事件
     * @param pollMask 事件
     * @return int 什么事件
     */
    HX::STL::coroutine::task::Task<int> _pollAdd(
        unsigned int pollMask
    ) const;

    int _fd = -1;
    std::unique_ptr<HX::web::protocol::http::Request> _request{};
    std::unique_ptr<HX::web::protocol::http::Response> _response{};

    /// @brief 请在子类中使用
    std::vector<char> _recvBuf;
};

}}} // namespace HX::web::socket

#undef HOT_FUNCTION

#endif // !_HX_SOCKET_IO_H_