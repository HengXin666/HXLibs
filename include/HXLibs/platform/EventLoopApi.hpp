#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-08 10:03:46
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

/**
 * @brief 跨平台的事件循环API, 如 io_uring / iocp
 */

#include <HXLibs/macro/NoWarring.hpp>

#if defined(__linux__)
    
    HX_NO_WARNINGS_BEGIN
    #include <liburing.h> // io_uring
    HX_NO_WARNINGS_END

#elif defined(_WIN32)
    /// @todo 下面还需要精简!
    HX_NO_WARNINGS_BEGIN
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #define _WINSOCK_DEPRECATED_NO_WARNINGS
    #include <WinSock2.h>
    #include <MSWSock.h>
    #include <ws2tcpip.h>
    #include <Windows.h>
    HX_NO_WARNINGS_END

    #pragma comment(lib, "Ws2_32.lib")
    #pragma comment(lib, "Mswsock.lib")

    #include <stdexcept>
    #include <string>

    namespace HX::platform::internal {
        
    class ConnectExLoader { // 获取到 ConnectEx 的函数指针
    public:
        using ConnectExType = BOOL (PASCAL*)(SOCKET, const sockaddr*, int, PVOID, DWORD, LPDWORD, LPOVERLAPPED);

        static ConnectExType get(::SOCKET cliSocket) {
            GUID guidConnectEx = WSAID_CONNECTEX;
            ConnectExType result = nullptr;
            DWORD bytesReturned = 0;

            int res = ::WSAIoctl(
                cliSocket,
                SIO_GET_EXTENSION_FUNCTION_POINTER,
                &guidConnectEx,
                sizeof(guidConnectEx),
                &result,
                sizeof(result),
                &bytesReturned,
                nullptr,
                nullptr
            );

            if (res == SOCKET_ERROR || result == nullptr) {
                throw std::runtime_error("WSAIoctl failed to get ConnectEx pointer");
            }

            return result;
        }
    };

    /**
     * @brief 初始化 win32 api
     * @note 此内容在 EventLoop 的构造函数中唯一初始化
     */
    class InitWin32Api {
        InitWin32Api() {
            ::WSADATA data;
            if (::WSAStartup(MAKEWORD(2, 2), &data)) [[unlikely]] {
                throw std::runtime_error{"WSAStartup ERROR: " + std::to_string(::GetLastError())};
            }
        }

        ~InitWin32Api() noexcept {
            ::WSACleanup();
        }
    public:
        /**
         * @brief [[线程安全]] 初始化 InitWin32Api
         */
        inline static void ensure() {
            thread_local static InitWin32Api _{};
        }
    };

    } // namespace HX::platform::internal
#else
    #error "Does not support the current operating system."
#endif

#include <HXLibs/macro/undef/NoWarring.hpp>
