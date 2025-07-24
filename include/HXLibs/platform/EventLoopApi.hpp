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
#ifndef _HX_EVENT_LOOP_API_H_
#define _HX_EVENT_LOOP_API_H_

/**
 * @brief 跨平台的事件循环API, 如 io_uring / iocp
 */

#if defined(__linux__)
    #if defined(__GNUC__) || defined(__clang__)
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wpedantic"
    #elif defined(_MSC_VER)
        // 没事誰会msvc编译linux的它?
        #pragma warning(push)
        #pragma warning(disable : 4100 4101)
    #endif
    
    #include <liburing.h> // io_uring

    #if defined(__GNUC__) || defined(__clang__)
        #pragma GCC diagnostic pop
    #elif defined(_MSC_VER)
        #pragma warning(pop)
        #pragma warning(pop)
    #endif
#elif defined(_WIN32)
    /// @todo 下面还需要精简!
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #define _WINSOCK_DEPRECATED_NO_WARNINGS
    #include <WinSock2.h>
    #include <MSWSock.h>
    #include <ws2tcpip.h>
    #include <Windows.h>

    #pragma comment(lib, "Ws2_32.lib")
    #pragma comment(lib, "Mswsock.lib")

    #include <stdexcept>
    #include <string>

    namespace HX::platform::internal {
        
    class ConnectExLoader { // 获取到 ConnectEx 的函数指针
    public:
        using ConnectExType = BOOL (PASCAL*)(SOCKET, const sockaddr*, int, PVOID, DWORD, LPDWORD, LPOVERLAPPED);

        static ConnectExType get() {
            static ConnectExType ptr = []() -> ConnectExType {
                GUID guidConnectEx = WSAID_CONNECTEX;
                ConnectExType result = nullptr;
                DWORD bytesReturned = 0;

                SOCKET tempSocket = ::WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
                if (tempSocket == INVALID_SOCKET) {
                    throw std::runtime_error("WSASocketW failed");
                }

                int res = ::WSAIoctl(
                    tempSocket,
                    SIO_GET_EXTENSION_FUNCTION_POINTER,
                    &guidConnectEx,
                    sizeof(guidConnectEx),
                    &result,
                    sizeof(result),
                    &bytesReturned,
                    nullptr,
                    nullptr
                );

                ::closesocket(tempSocket);

                if (res == SOCKET_ERROR || result == nullptr) {
                    throw std::runtime_error("WSAIoctl failed to get ConnectEx pointer");
                }

                return result;
            }();

            return ptr;
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

#endif // !_HX_EVENT_LOOP_API_H_