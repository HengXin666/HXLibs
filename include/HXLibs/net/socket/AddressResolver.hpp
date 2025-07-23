#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2024-7-23 18:05:02
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
 * */
#ifndef _HX_ADDRESS_RESOLVER_H_
#define _HX_ADDRESS_RESOLVER_H_

#include <string>
#include <string_view>
#include <system_error>
#include <stdexcept>

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <HXLibs/exception/ErrorHandlingTools.hpp>

namespace HX::net {

#ifdef _WIN32
class WinSockInitializer {
public:
    WinSockInitializer() {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
    }
    ~WinSockInitializer() {
        WSACleanup();
    }
};
#endif

/**
 * @brief 地址注册类
 */
class AddressResolver {
public:
#ifdef _WIN32
    using SocketType = SOCKET;
    static constexpr SocketType invalidSocket = INVALID_SOCKET;
#else
    using SocketType = int;
    static constexpr SocketType invalidSocket = -1;
#endif

    /**
     * @brief 用于保存地址引用
     */
    struct AddressRef {
#ifdef _WIN32
        struct sockaddr* _addr;
        int _addrlen;
#else
        struct sockaddr* _addr;
        socklen_t _addrlen;
#endif
    };

    /**
     * @brief 用于保存地址信息
     */
    struct Address {
        union {
            struct sockaddr _addr;
            struct sockaddr_storage _addr_storage;
        };
#ifdef _WIN32
        int _addrlen = sizeof(sockaddr_storage);
#else
        socklen_t _addrlen = sizeof(sockaddr_storage);
#endif

        operator AddressRef() {
            return { &_addr, _addrlen };
        }
    };

    /**
     * @brief 用于处理 addrinfo 结果
     */
    struct AddressInfo {
#ifdef _WIN32
        ADDRINFOA* _curr = nullptr;
#else
        struct addrinfo* _curr = nullptr;
#endif

        /**
         * @brief 创建套接字、绑定并监听
         * @return 服务器套接字
         */
        SocketType createSocketAndBind() const {
            SocketType serverFd = createSocket();
            AddressRef serveAddr = getAddress();

            int on = 1;
#ifdef _WIN32
            // setsockopt 第二个参数是 SOL_SOCKET, 需要 cast
            setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));
            // SO_REUSEPORT 不一定支持，Windows一般不支持该选项，忽略
#else
            setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
            setsockopt(serverFd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
#endif

            exception::checkSocketError("bind error",
#ifdef _WIN32
                ::bind(serverFd, serveAddr._addr, static_cast<int>(serveAddr._addrlen))
#else
                ::bind(serverFd, serveAddr._addr, serveAddr._addrlen)
#endif
            );

            exception::checkSocketError("listen error",
#ifdef _WIN32
                ::listen(serverFd, SOMAXCONN)
#else
                ::listen(serverFd, SOMAXCONN)
#endif
            );

            return serverFd;
        }

        /**
         * @brief 创建socket套接字
         * @return 服务器套接字
         */
        SocketType createSocket() const {
#ifdef _WIN32
            SOCKET sock = ::socket(_curr->ai_family, _curr->ai_socktype, _curr->ai_protocol);
            if (sock == INVALID_SOCKET) {
                int err = WSAGetLastError();
                throw std::system_error(err, std::system_category(), "socket error");
            }
            return sock;
#else
            int sock = ::socket(_curr->ai_family, _curr->ai_socktype, _curr->ai_protocol);
            if (sock < 0) {
                int err = errno;
                throw std::system_error(err, std::system_category(), "socket error");
            }
            return sock;
#endif
        }

        /**
         * @brief 获取当前 addrinfo 的地址引用
         * @return 当前 addrinfo 的地址引用
         */
        AddressRef getAddress() const {
#ifdef _WIN32
            return { _curr->ai_addr, static_cast<int>(_curr->ai_addrlen) };
#else
            return { _curr->ai_addr, _curr->ai_addrlen };
#endif
        }

        /**
         * @brief 移动到下一个 addrinfo 结构体
         * @return 是否可以继续移动
         */
        [[nodiscard]] bool nextEntry() {
#ifdef _WIN32
            _curr = _curr->ai_next;
            return _curr != nullptr;
#else
            _curr = _curr->ai_next;
            return _curr != nullptr;
#endif
        }
    };

private:
#ifdef _WIN32
    ADDRINFOA* _head = nullptr;
    WinSockInitializer _wsaInit;  // 确保Winsock初始化
#else
    struct addrinfo* _head = nullptr;
#endif

public:
    AddressResolver() = default;

    AddressResolver(AddressResolver&& that) noexcept
#ifdef _WIN32
        : _head(that._head), _wsaInit()
#else
        : _head(that._head)
#endif
    {
        that._head = nullptr;
    }

    ~AddressResolver() noexcept {
        if (_head) {
#ifdef _WIN32
            ::freeaddrinfo(_head);
#else
            ::freeaddrinfo(_head);
#endif
        }
    }

    /**
     * @brief 解析主机名和服务名为 AddressInfo 链表
     * @param name 主机名或地址字符串
     * @param service 服务名或端口号
     * @return AddressInfo
     */
    AddressInfo resolve(std::string_view name, std::string_view service) {
#ifdef _WIN32
        // Windows 默认用 ASCII 版本
        int err = ::getaddrinfo(name.data(), service.data(), nullptr, &_head);
        if (err != 0) {
            // Windows getaddrinfo 返回的错误码与gai_strerror对应, 但不是errno
            throw std::runtime_error{::gai_strerror(err)};
        }
#else
        int err = ::getaddrinfo(name.data(), service.data(), nullptr, &_head);
        if (err != 0) {
            auto ec = std::error_code(err, exception::LinuxErrorHandlingTools::gaiCategory());
            throw std::system_error(ec, std::string{name} + ":" + std::string{service});
        }
#endif
        return {_head};
    }
};

} // namespace HX::net

#endif // _HX_ADDRESS_RESOLVER_H_
