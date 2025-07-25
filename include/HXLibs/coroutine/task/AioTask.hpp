#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-07 22:24:56
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
#ifndef _HX_AIO_TASK_H_
#define _HX_AIO_TASK_H_

#include <span>

#include <HXLibs/platform/EventLoopApi.hpp>
#include <HXLibs/platform/LocalFdApi.hpp>
#include <HXLibs/coroutine/awaiter/WhenAny.hpp>

#if defined(__linux__)

namespace HX::coroutine {

namespace internal {

struct IoUring;

} // namespace internal

struct AioTask {
    AioTask(::io_uring_sqe* sqe) noexcept
        : _sqe{sqe}
    {
        ::io_uring_sqe_set_data(_sqe, this);
    }

#if 0 // 注意: 不能存在`移动`, 否则 IoUring::makeAioTask 返回就是 构造的新对象; 屏蔽了移动, 反而是编译器优化!
    AioTask& operator=(AioTask const&) noexcept = delete;
    AioTask(AioTask const&) noexcept = delete;

    AioTask(AioTask&&) noexcept = default;
    AioTask& operator=(AioTask&&) noexcept = default;
#else
    AioTask& operator=(AioTask&&) noexcept = delete;
#endif

    struct AioAwaiter {
        constexpr bool await_ready() const noexcept { return false; }
        constexpr void await_suspend(std::coroutine_handle<> coroutine) const noexcept {
            _task->_previous = coroutine;
            _task->_res = -ENOSYS;
        }
        constexpr int await_resume() const noexcept {
            return _task->_res;
        }
        AioTask* _task;
    };

    AioAwaiter operator co_await() {
        return {this};
    }

private:
    friend internal::IoUring;

    union {
        int _res;
        ::io_uring_sqe* _sqe;
    };
    std::coroutine_handle<> _previous;

public:
    /**
     * @brief 异步打开文件
     * @param dirfd 目录文件描述符, 它表示相对路径的基目录; `AT_FDCWD`, 则表示相对于当前工作目录
     * @param path 文件路径
     * @param flags 指定文件打开的方式, 比如 `O_RDONLY`
     * @param mode 文件权限模式, 仅在文件创建时有效 (一般写`0644`)
     * @return AioTask&& 
     */
    [[nodiscard]] AioTask&& prepOpenat(
        int dirfd, 
        char const* path, 
        int flags,
        platform::ModeType mode
    ) && {
        ::io_uring_prep_openat(_sqe, dirfd, path, flags, mode);
        return std::move(*this);
    }

    /**
     * @brief 异步创建一个套接字
     * @param domain 指定 socket 的协议族 (AF_INET(ipv4)/AF_INET6(ipv6)/AF_UNIX/AF_LOCAL(本地))
     * @param type 套接字类型 SOCK_STREAM(tcp)/SOCK_DGRAM(udp)/SOCK_RAW(原始)
     * @param protocol 使用的协议, 通常为 0 (默认协议), 或者指定具体协议(如 IPPROTO_TCP、IPPROTO_UDP 等)
     * @param flags 
     * @return AioTask&& 
     */
    [[nodiscard]] AioTask&& prepSocket(
        int domain, 
        int type, 
        int protocol,
        unsigned int flags
    ) && {
        ::io_uring_prep_socket(_sqe, domain, type, protocol, flags);
        return std::move(*this);
    }

    /**
     * @brief 异步建立连接
     * @param fd 服务端套接字
     * @param addr [out] 客户端信息
     * @param addrlen [out] 客户端信息长度指针
     * @param flags 
     * @return AioTask&& 
     */
    [[nodiscard]] AioTask&& prepAccept(
        int fd, 
        struct ::sockaddr *addr, 
        ::socklen_t *addrlen,
        int flags
    ) && {
        ::io_uring_prep_accept(_sqe, fd, addr, addrlen, flags);
        return std::move(*this);
    }

    /**
     * @brief 异步的向服务端创建连接
     * @param fd 客户端套接字
     * @param addr [out] 服务端信息
     * @param addrlen 服务端信息长度指针
     * @return AioTask&& 
     */
    [[nodiscard]] AioTask&& prepConnect(
        int fd, 
        const struct sockaddr *addr,
        socklen_t addrlen
    ) && {
        ::io_uring_prep_connect(_sqe, fd, addr, addrlen);
        return std::move(*this);
    }

    /**
     * @brief 异步读取文件
     * @param fd 文件描述符
     * @param buf [out] 读取到的数据
     * @param offset 文件偏移量
     * @return AioTask&& 
     */
    [[nodiscard]] AioTask&& prepRead(
        int fd,
        std::span<char> buf,
        std::uint64_t offset
    ) && {
        ::io_uring_prep_read(_sqe, fd, buf.data(), static_cast<unsigned int>(buf.size()), offset);
        return std::move(*this);
    }

    /**
     * @brief 异步读取文件
     * @param fd 文件描述符
     * @param buf [out] 读取到的数据
     * @param size 读取的长度
     * @param offset 文件偏移量
     * @return AioTask&& 
     */
    [[nodiscard]] AioTask&& prepRead(
        int fd,
        std::span<char> buf,
        unsigned int size,
        std::uint64_t offset
    ) && {
        ::io_uring_prep_read(_sqe, fd, buf.data(), size, offset);
        return std::move(*this);
    }

    /**
     * @brief 异步写入文件
     * @param fd 文件描述符
     * @param buf [in] 写入的数据
     * @param offset 文件偏移量
     * @return AioTask&& 
     */
    [[nodiscard]] AioTask&& prepWrite(
        int fd, 
        std::span<char const> buf,
        std::uint64_t offset
    ) && {
        ::io_uring_prep_write(_sqe, fd, buf.data(), static_cast<unsigned int>(buf.size()), offset);
        return std::move(*this);
    }

    /**
     * @brief 异步读取网络套接字文件
     * @param fd 文件描述符
     * @param buf [out] 读取到的数据
     * @param flags 
     * @return AioTask&& 
     */
    [[nodiscard]] AioTask&& prepRecv(
        int fd,
        std::span<char> buf,
        int flags
    ) && {
        ::io_uring_prep_recv(_sqe, fd, buf.data(), buf.size(), flags);
        // printf("recv %p\n", (void *)this);
        return std::move(*this);
    }

    /**
     * @brief 异步写入网络套接字文件
     * @param fd 文件描述符
     * @param buf [in] 写入的数据
     * @param flags 
     * @return AioTask&& 
     */
    [[nodiscard]] AioTask&& prepSend(
        int fd, 
        std::span<char const> buf, 
        int flags
    ) && {
        ::io_uring_prep_send(_sqe, fd, buf.data(), buf.size(), flags);
        return std::move(*this);
    }

    /**
     * @brief 异步关闭文件
     * @param fd 文件描述符
     * @return AioTask&& 
     */
    [[nodiscard]] AioTask&& prepClose(int fd) && {
        ::io_uring_prep_close(_sqe, fd);
        return std::move(*this);
    }

    /**
     * @brief 监测一个fd的pool事件
     * @param fd 需要监测的fd
     * @param pollMask 需要监测的poll事件 (如:`POLLIN`)
     * @return AioTask&& 
     */
    [[nodiscard]] AioTask&& prepPollAdd(
        int fd, 
        unsigned int pollMask
    ) && {
        ::io_uring_prep_poll_add(_sqe, fd, pollMask);
        return std::move(*this);
    }

    /**
     * @brief 创建未链接的超时操作
     * @param ts 超时时间
     * @param flags 
     * @return AioTask&& 
     */
    [[nodiscard]] AioTask&& prepLinkTimeout(
        struct __kernel_timespec* ts,
        unsigned int flags
    ) && {
        ::io_uring_prep_link_timeout(_sqe, ts, flags);
        return std::move(*this);
    }

    /**
     * @brief 链接超时操作
     * @param task 任务
     * @param timeoutTask `prepLinkTimeout`的返回值
     * @return auto 
     */
    [[nodiscard]] inline static auto linkTimeout(
        AioTask&& task, AioTask&& timeoutTask
    ) {
        task._sqe->flags |= IOSQE_IO_LINK;
        return whenAny(std::move(task), std::move(timeoutTask));
    }

    ~AioTask() noexcept = default;
};

} // namespace HX::coroutine

#elif defined(_WIN32)

#include <string>
#include <unordered_set>

#include <HXLibs/coroutine/loop/TimerLoop.hpp>
#include <HXLibs/exception/ExceptionMode.hpp>

namespace HX::coroutine {

namespace internal {

struct Iocp;

} // namespace internal

struct AioTask {
    AioTask(::HANDLE iocpHandle, std::unordered_set<::HANDLE>& runingHandle)
        : _iocpHandle{iocpHandle}
        , _runingHandle{runingHandle}
        , _data{new _AioIocpData{*this}}
    {}
#if 1 // 注意: 不能存在`移动`, 否则 IoUring::makeAioTask 返回就是 构造的新对象; 屏蔽了移动, 反而是编译器优化!
      // 得写移动, 不然无法在 MSVC 上通过编译 对于 HX::whenAny
    AioTask& operator=(AioTask const&) = delete;
    AioTask(AioTask const&) noexcept = delete;

    AioTask(AioTask&&) = default;
    AioTask& operator=(AioTask&&) noexcept = default;
#else
    AioTask& operator=(AioTask&&) noexcept = delete;
#endif

    struct AioAwaiter {
        constexpr bool await_ready() const noexcept { return false; }
        constexpr void await_suspend(std::coroutine_handle<> coroutine) const noexcept {
            _task->_previous = coroutine;
            _task->_res = -ENOSYS;
        }
        constexpr uint64_t await_resume() const noexcept {
            return _task->_res;
        }
        AioTask* _task;
    };

    AioAwaiter operator co_await() {
        return {this};
    }

private:
    /**
     * @brief 非 RAII 的, 存放 OVERLAPPED 的类
     */
    struct _AioIocpData : public ::OVERLAPPED {
        _AioIocpData(AioTask& self)
            : _self{self}
            , _isCancel{false}
        {
            ::memset(this, 0, sizeof(::OVERLAPPED)); // 必须初始化 OVERLAPPED
        }

        _AioIocpData& operator=(_AioIocpData&&) noexcept = delete;

        void setCancel() noexcept {
            _isCancel = true;
        }

        bool isCancel() const noexcept {
            return _isCancel;
        }

        AioTask& _self; // 注意! 当 res = err 或者 _iocpState & 1 == Cancel 时候,
                        // _self 为悬挂引用! (野指针)
        bool _isCancel;
    };

    friend internal::Iocp;

    union {
        uint64_t _res;
        ::HANDLE _iocpHandle;
    };
    container::UninitializedNonVoidVariant<::HANDLE, ::SOCKET> _fd;
    std::reference_wrapper<std::unordered_set<::HANDLE>> _runingHandle;
    std::coroutine_handle<> _previous;
    _AioIocpData* _data; // 其实际所有权会被转移到 Loop::run() 中

    void _associateHandle(::HANDLE h) & {
        auto&& runingHandleRef = _runingHandle.get();
        if (runingHandleRef.count(h))
            return;
        if (!::CreateIoCompletionPort(
            h, _iocpHandle, 0, 0) 
            && ::GetLastError() != ERROR_INVALID_PARAMETER
        ) {
            throw std::runtime_error{std::to_string(::GetLastError())};
        }
        runingHandleRef.insert(h);
    }

    void associateHandle(::HANDLE h) & {
        _fd.emplace<::HANDLE>(h);
        _associateHandle(h);
    }

    void associateHandle(::SOCKET h) & {
        _fd.emplace<::SOCKET>(h);
        _associateHandle(reinterpret_cast<::HANDLE>(h));
    }

    struct _AioTimeoutTask {
        _AioTimeoutTask(TimerLoop::TimerAwaiter&& timerTask)
            : _self{}
            , _timerTask{std::move(timerTask)}
        {}

        _AioTimeoutTask(_AioTimeoutTask&&) = default;
        _AioTimeoutTask& operator=(_AioTimeoutTask&&) noexcept = default;

        Task<> co() noexcept {
            co_await _timerTask;
            _self->_data->setCancel();
            switch (_self->_fd.index()) {
                case 0: { // HANDLE
                    co_await std::move(*_self).prepClose(
                        _self->_fd.get<::HANDLE, exception::ExceptionMode::Nothrow>());
                    break;
                }
                [[likely]] case 1: { // SOCKET
                    co_await std::move(*_self).prepClose(
                        _self->_fd.get<::SOCKET, exception::ExceptionMode::Nothrow>());
                    break;
                }
                [[unlikely]] case container::UninitializedNonVoidVariantNpos: {
                    // 库内部错误
                    std::terminate();
                }
            }
            co_return;
        }
    private:
        friend AioTask;
        AioTask* _self; // 无所有权! 为链接的只读
        TimerLoop::TimerAwaiter _timerTask;
    };
public:
    // IO 操作 ...
    // https://learn.microsoft.com/zh-cn/windows/win32/fileio/i-o-completion-ports

/*
支持的 I/O 函数

AcceptEx: 接受一个新的传入连接, 并可选地接收对方地址和初始数据, 常用于异步套接字服务器。
ConnectNamedPipe: 等待客户端连接到指定的命名管道实例, 用于实现服务端命名管道连接。
DeviceIoControl: 向设备驱动程序发送控制码, 用于执行自定义的设备操作, 如读写、获取状态等。
LockFileEx: 对文件的指定区域加锁, 支持共享锁或独占锁, 适用于进程间文件访问同步。
ReadDirectoryChangesW: 监视目录中的文件系统更改, 如文件创建、删除、重命名或修改等。
ReadFile: 从文件、管道、串口等读取数据, 支持 OVERLAPPED 结构实现异步读取。
TransactNamedPipe: 向命名管道写入请求并立即读取响应, 适用于单一事务的客户端-服务端通信。
WaitCommEvent: 等待串口通信事件发生, 如接收字符、状态位变化等, 用于串口异步事件通知。
WriteFile: 向文件、管道、串口等写入数据, 支持 OVERLAPPED 结构实现异步写入。
WSASendMsg: 向套接字发送带辅助控制信息的消息, 常用于实现高级协议特性如多播、IP 选项。
WSASendTo: 向指定地址的套接字发送数据, 适用于 UDP 等无连接协议的发送操作。
WSASend: 向已连接的套接字发送数据, 支持 OVERLAPPED 结构实现异步写入。
WSARecvFrom: 从套接字接收数据, 并获取发送方地址, 适用于 UDP 等无连接协议的接收操作。
LPFN_WSARECVMSG（WSARecvMsg）: 接收带有辅助控制信息的数据报, 用于处理如 IP_PKTINFO 等底层数据。
WSARecv: 从已连接的套接字接收数据, 支持 OVERLAPPED 结构实现异步读取。
*/

    /**
     * @brief 异步打开文件
     * @param dirfd 目录文件描述符, 它表示相对路径的基目录; `AT_FDCWD`, 则表示相对于当前工作目录
     * @param path 文件路径
     * @param flags 指定文件打开的方式, 比如 `O_RDONLY`
     * @param mode 文件权限模式, 仅在文件创建时有效 (一般写`0644`)
     * @return AioTask&& 
     */
    // [[nodiscard]] AioTask&& prepOpenat(
    //     int dirfd, 
    //     char const* path, 
    //     int flags,
    //     platform::ModeType mode
    // ) && {
    //     // ::io_uring_prep_openat(_sqe, dirfd, path, flags, mode);
    //     return std::move(*this);
    // }

    /**
     * @brief 异步创建一个套接字
     * @param domain 指定 socket 的协议族 (AF_INET(ipv4)/AF_INET6(ipv6)/AF_UNIX/AF_LOCAL(本地))
     * @param type 套接字类型 SOCK_STREAM(tcp)/SOCK_DGRAM(udp)/SOCK_RAW(原始)
     * @param protocol 使用的协议, 通常为 0 (默认协议), 或者指定具体协议(如 IPPROTO_TCP、IPPROTO_UDP 等)
     * @param flags 
     * @return Task<::SOCKET>
     */
    [[nodiscard]] Task<::SOCKET> prepSocket(
        int domain, 
        int type, 
        int protocol,
        unsigned int flags
    ) && {
        // ::io_uring_prep_socket(_sqe, domain, type, protocol, flags);
        flags |= WSA_FLAG_OVERLAPPED;
        auto socket = ::WSASocket(domain, type, protocol, NULL, 0, flags);
        if (socket == INVALID_SOCKET) [[unlikely]] {
            throw std::runtime_error{"socket ERROR: " + std::to_string(::WSAGetLastError())};
        }
        associateHandle(socket);
        co_return socket;
    }

    /**
     * @brief 异步建立连接
     * @param fd 服务端套接字
     * @param addr [out] 客户端信息
     * @param addrlen 必须传入 nullptr
     * @param flags 
     * @return Task<::SOCKET>
     */
    [[nodiscard]] Task<::SOCKET> prepAccept(
        ::SOCKET serSocket,
        ::sockaddr* addr,
        [[maybe_unused]] std::nullptr_t addrlen,
        int flags
    ) && {
        // ::io_uring_prep_accept(_sqe, fd, addr, addrlen, flags);
        /*
BOOL AcceptEx(
  [in]  SOCKET sListenSocket,         // 监听的socket <服务端套接字>
  [in]  SOCKET sAcceptSocket,         // 客户端套接字, 必须是通过socket函数创建的，未绑定的套接字
  [out] PVOID lpOutputBuffer,         // 用来接收首批数据及存储两个sockaddr结构的缓冲区
  [in]  DWORD dwReceiveDataLength,    // 在首次接受的数据中期望读取的字节数, 如果为0, 则表示不接收数据
  [in]  DWORD dwLocalAddressLength,   // 本地地址sockaddr结构的大小, 此值必须至少比正在使用的传输协议的最大地址长度多16个字节
  [in]  DWORD dwRemoteAddressLength,  // 远程地址sockaddr结构的大小, 此值必须至少比正在使用的传输协议的最大地址长度多16个字节
  [out] LPDWORD lpdwBytesReceived,    // 实际接收到的字节数
  [in, out] LPOVERLAPPED lpOverlapped // 指向OVERLAPPED结构的指针, 用于异步操作
);
        */
        // @todo 此处似乎仅支持 ipv4, ipv6 得想其他办法, 比如用宏
        // √8 iocp 和 AcceptEx 的设计... 居然要我们自己传入 new 的客户端套接字...
        auto cliSocket = 
            ::WSASocket(AF_INET, SOCK_STREAM, 0,
                        nullptr, 0, WSA_FLAG_OVERLAPPED);
        
        if (cliSocket == INVALID_SOCKET) [[unlikely]] {
            throw std::runtime_error{
                "socket ERROR: " + std::to_string(::WSAGetLastError())};
        }
        associateHandle(cliSocket);
        
        char addrBuf[2 * (sizeof(::sockaddr) + 16)] {};
        ::DWORD bytes = 0;

        bool ok = ::AcceptEx(
            serSocket,
            cliSocket,
            addrBuf,
            0,
            sizeof(::sockaddr) + 16,
            sizeof(::sockaddr) + 16,
            &bytes,
            static_cast<::OVERLAPPED*>(_data)
        );
        if (!ok && ::WSAGetLastError() != ERROR_IO_PENDING) [[unlikely]] {
            throw std::runtime_error{
                "AcceptEx ERROR: " + std::to_string(::GetLastError())};
        }
        return [](AioTask&& task, ::SOCKET _cliSocket) -> Task<::SOCKET> {
            co_await task;
            co_return _cliSocket;
        }(std::move(*this), cliSocket);
    }

    /**
     * @brief 异步的向服务端创建连接
     * @param fd 客户端套接字
     * @param addr [out] 服务端信息
     * @param addrlen 服务端信息长度指针
     * @return Task<::SOCKET>
     */
    [[nodiscard]] Task<::SOCKET> prepConnect(
        ::SOCKET fd, 
        const ::sockaddr* addr,
        [[maybe_unused]] int addrlen
    ) && {
        // ::io_uring_prep_connect(_sqe, fd, addr, addrlen);
        associateHandle(fd);
        
        // bind 是必须的, 即使是客户端
        ::sockaddr_in localAddr {};
        localAddr.sin_family = AF_INET;
        localAddr.sin_addr.s_addr = INADDR_ANY;
        localAddr.sin_port = 0;

        if (::bind(fd, reinterpret_cast<const ::sockaddr*>(&localAddr), sizeof(localAddr)) == SOCKET_ERROR) [[unlikely]] {
            throw std::runtime_error("bind failed: " + std::to_string(::WSAGetLastError()));
        }

        ::DWORD bytes = 0;

        bool ok = platform::internal::ConnectExLoader::get()(
            fd,
            addr,
            addrlen,
            nullptr,      // 可选发送缓冲区
            0,            // 可选发送的长度
            &bytes,
            static_cast<::OVERLAPPED*>(_data)
        );
        if (!ok && ::WSAGetLastError() != ERROR_IO_PENDING) [[unlikely]] {
            throw std::runtime_error{
                "ConnectEx ERROR: " + std::to_string(::GetLastError())};
        }
        return [](AioTask&& task, ::SOCKET _fd) -> Task<::SOCKET> {
            co_await task;
            co_return _fd;
        }(std::move(*this), fd);
    }

    /**
     * @brief 异步读取文件
     * @param fd 文件句柄
     * @param buf [out] 读取到的数据
     * @param offset 文件偏移量
     * @return AioTask&& 
     */
    [[nodiscard]] AioTask&& prepRead(
        ::HANDLE fd,
        std::span<char> buf,
        std::uint64_t offset
    ) && {
        // ::io_uring_prep_read(_sqe, fd, buf.data(), static_cast<unsigned int>(buf.size()), offset);
        /*
BOOL ReadFile(
  HANDLE       hFile,                // 文件句柄（可为文件、管道、串口、Socket 等）
  LPVOID       lpBuffer,            // 数据读入的缓冲区指针（你准备好的内存）
  DWORD        nNumberOfBytesToRead,// 想要读取的字节数
  LPDWORD      lpNumberOfBytesRead, // 实际读取的字节数（同步时非 NULL, 异步时设为 NULL）
  LPOVERLAPPED lpOverlapped         // OVERLAPPED 结构指针（异步时必填, 同步时为 NULL）
);
        */
        /*
typedef struct _OVERLAPPED {
  ULONG_PTR Internal;
  ULONG_PTR InternalHigh;
  union {
    struct {
      DWORD Offset;      // 低 32 位文件偏移量
      DWORD OffsetHigh;  // 高 32 位文件偏移量
                         // 两个组成 64 为的偏移量, 破win为了远古兼容, 就这样搞了...
    };
    PVOID Pointer;
  };
  HANDLE    hEvent;
} OVERLAPPED, *LPOVERLAPPED;
        */
        associateHandle(fd);
        // 设置偏移量
        _data->Offset = static_cast<::DWORD>(offset & 0xFFFFFFFF);
        _data->OffsetHigh = static_cast<::DWORD>((offset >> 32) & 0xFFFFFFFF);
        bool ok = ::ReadFile(
            fd,
            buf.data(),
            static_cast<::DWORD>(buf.size()),
            nullptr,
            static_cast<::OVERLAPPED*>(_data)
        );
        if (!ok && ::GetLastError() != ERROR_IO_PENDING) [[unlikely]] {
            throw std::runtime_error{
                "ReadFile ERROR: " + std::to_string(::GetLastError())};
        }
        return std::move(*this);
    }

    /**
     * @brief 异步读取文件
     * @param fd 文件描述符
     * @param buf [out] 读取到的数据
     * @param size 读取的长度
     * @param offset 文件偏移量
     * @return AioTask&& 
     */
    [[nodiscard]] AioTask&& prepRead(
        ::HANDLE fd,
        std::span<char> buf,
        unsigned int size,
        std::uint64_t offset
    ) && {
        // ::io_uring_prep_read(_sqe, fd, buf.data(), size, offset);
        associateHandle(fd);
        // 设置偏移量
        _data->Offset = static_cast<::DWORD>(offset & 0xFFFFFFFF);
        _data->OffsetHigh = static_cast<::DWORD>((offset >> 32) & 0xFFFFFFFF);
        bool ok = ::ReadFile(
            fd,
            buf.data(),
            static_cast<::DWORD>(size),
            nullptr,
            static_cast<::OVERLAPPED*>(_data)
        );
        if (!ok && ::GetLastError() != ERROR_IO_PENDING) [[unlikely]] {
            throw std::runtime_error{
                "ReadFile ERROR: " + std::to_string(::GetLastError())};
        }
        return std::move(*this);
    }

    /**
     * @brief 异步写入文件
     * @param fd 文件描述符
     * @param buf [in] 写入的数据
     * @param offset 文件偏移量
     * @return AioTask&& 
     */
    [[nodiscard]] AioTask&& prepWrite(
        HANDLE fd, 
        std::span<char const> buf,
        std::uint64_t offset
    ) && {
        // ::io_uring_prep_write(_sqe, fd, buf.data(), static_cast<unsigned int>(buf.size()), offset);
        associateHandle(fd);
        _data->Offset = static_cast<::DWORD>(offset & 0xFFFFFFFF);
        _data->OffsetHigh = static_cast<::DWORD>((offset >> 32) & 0xFFFFFFFF);
        bool ok = ::WriteFile(
            fd,
            buf.data(),
            static_cast<::DWORD>(buf.size()),
            nullptr,
            static_cast<::OVERLAPPED*>(_data)
        );
        if (!ok && ::GetLastError() != ERROR_IO_PENDING) [[unlikely]] {
            throw std::runtime_error{
                "WriteFile ERROR: " + std::to_string(::GetLastError())};
        }
        return std::move(*this);
    }

    /**
     * @brief 异步读取网络套接字文件
     * @param fd 文件描述符
     * @param buf [out] 读取到的数据
     * @param flags 
     * @return AioTask&& 
     */
    [[nodiscard]] AioTask&& prepRecv(
        ::SOCKET fd,
        std::span<char> buf,
        ::DWORD flags
    ) && {
        // ::io_uring_prep_recv(_sqe, fd, buf.data(), buf.size(), flags);
/*
int WSARecv(
  [in]  SOCKET                             s,                    // 目标套接字
  [in, out] LPWSABUF                       lpBuffers,            // 数据缓冲区数组
  [in]  DWORD                              dwBufferCount,        // 缓冲区数组中的元素数量
  [out] LPDWORD                            lpNumberOfBytesRecvd, // 实际接收的字节数 (IOCP时候为 nullptr)
  [in, out] LPDWORD                        lpFlags,              // 控制接收行为的标志
  [in, out] LPWSAOVERLAPPED                lpOverlapped,         // 指向OVERLAPPED结构的指针, 用于异步操作
  [in]  LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine   // 完成例程的回调函数, 当I/O操作完成时被调用
);
*/
        associateHandle(fd);
        ::WSABUF wsabuf {
            static_cast<::ULONG>(buf.size()),
            buf.data(),
        };
        int ok = ::WSARecv(
            fd,
            &wsabuf,
            1,
            nullptr,
            &flags,
            static_cast<::OVERLAPPED*>(_data),
            nullptr
        );
        if (ok == SOCKET_ERROR && ::WSAGetLastError() != ERROR_IO_PENDING) [[unlikely]] {
            throw std::runtime_error{
                "WSARecv ERROR: " + std::to_string(::WSAGetLastError())};
        }
        return std::move(*this);
    }

    /**
     * @brief 异步写入网络套接字文件
     * @param fd 文件描述符
     * @param buf [in] 写入的数据
     * @param flags 
     * @return AioTask&& 
     */
    [[nodiscard]] AioTask&& prepSend(
        ::SOCKET fd, 
        std::span<char const> buf, 
        ::DWORD flags
    ) && {
        // ::io_uring_prep_send(_sqe, fd, buf.data(), buf.size(), flags);
/*
int WSASend(
  SOCKET                             s,                     // 套接字
  LPWSABUF                           lpBuffers,             // WSABUF
  DWORD                              dwBufferCount,         // lpBuffers数组中的元素数目
  LPDWORD                            lpNumberOfBytesSent,   // 返回实际发送的字节数
  DWORD                              dwFlags,               // 标志来控制发送操作的行为, 一般写 0
  LPWSAOVERLAPPED                    lpOverlapped,
  LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine    // 回调函数的指针
);
*/
        associateHandle(fd);
        ::WSABUF wsabuf{
            static_cast<::ULONG>(buf.size()),
            const_cast<char*>(buf.data()),
        };
        int ok = ::WSASend(
            fd,
            &wsabuf,
            1,
            nullptr,
            flags,
            static_cast<::OVERLAPPED*>(_data),
            nullptr
        );
        if (ok == SOCKET_ERROR && ::WSAGetLastError() != ERROR_IO_PENDING) [[unlikely]] {
            throw std::runtime_error{
                "WSASend ERROR: " + std::to_string(::WSAGetLastError())};
        }
        return std::move(*this);
    }

    /**
     * @brief 异步关闭文件
     * @param fd 文件描述符
     * @return std::suspend_never 
     */
    [[nodiscard]] Task<int> prepClose(::HANDLE fd) && {
        // ::io_uring_prep_close(_sqe, fd);
        auto&& runingHandleRef = _runingHandle.get();
        bool ok = ::CloseHandle(fd);
        if (!ok) [[unlikely]] {
            // 如果这里抛异常了, 那是不是无法关闭?!
            // 除非 fd 根本就不是由 win32 创建的?!
            // 原本的东西留着也没有用了...
            if (auto it = runingHandleRef.find(fd); it != runingHandleRef.end()) [[unlikely]] {
                runingHandleRef.erase(it);
                throw std::runtime_error{
                    "CloseHandle ERROR: " + std::to_string(::GetLastError())};
            }
            // 那仅仅只是超时了 ...
        } else {
            runingHandleRef.erase(fd);
        }
        // @!!! 这里只能是同步的...
        co_return 0;
    }

    /**
     * @brief 异步关闭套接字
     * @param socket 套接字
     * @return std::suspend_never 
     */
    [[nodiscard]] Task<int> prepClose(::SOCKET socket) && {
        // ::io_uring_prep_close(_sqe, fd);
        auto&& runingHandleRef = _runingHandle.get();
        int ok = ::closesocket(socket);
        if (ok == SOCKET_ERROR) [[unlikely]] {
            // 如果这里抛异常了, 那是不是无法关闭?!
            // 除非 fd 根本就不是由 win32 创建的?!
            // 原本的东西留着也没有用了...
            if (auto it = runingHandleRef.find(reinterpret_cast<::HANDLE>(socket));
                it != runingHandleRef.end()
            ) [[unlikely]] {
                runingHandleRef.erase(it);
                // @todo 应该返回一个 -err
                throw std::runtime_error{
                    "closesocket ERROR: " + std::to_string(::WSAGetLastError())};
            }
            // 超时了 ...
        } else {
            runingHandleRef.erase(reinterpret_cast<::HANDLE>(socket));
        }
        // @!!! 这里只能是同步的...
        co_return 0;
    }

    /**
     * @brief 监测一个fd的pool事件
     * @param fd 需要监测的fd
     * @param pollMask 需要监测的poll事件 (如:`POLLIN`)
     * @return AioTask&& 
     */
    // [[nodiscard]] AioTask&& prepPollAdd(
    //     int fd, 
    //     unsigned int pollMask
    // ) && {
    //     // ::io_uring_prep_poll_add(_sqe, fd, pollMask);
    //     return std::move(*this);
    // }

    /**
     * @brief 创建未链接的超时操作
     * @param ts 超时时间
     * @param flags 
     * @return AioTask&& 
     */
    [[nodiscard]] _AioTimeoutTask prepLinkTimeout(
        TimerLoop::TimerAwaiter&& timerTask
    ) && {
        // ::io_uring_prep_link_timeout(_sqe, ts, flags);
        return {std::move(timerTask)};
    }

    [[nodiscard]] inline static Task<
        container::UninitializedNonVoidVariant<uint64_t, void>
    > linkTimeout(
        AioTask&& task, 
        _AioTimeoutTask&& timeoutTask
    ) {
        timeoutTask._self = &task;
        co_return co_await whenAny(std::move(task), timeoutTask.co());
    }

    ~AioTask() noexcept = default;
};

} // namespace HX::coroutine

#else
    #error "Does not support the current operating system."
#endif

#endif // !_HX_AIO_TASK_H_