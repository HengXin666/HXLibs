#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2024-08-14 20:43:15
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
#ifndef _HX_IO_URING_LOOP_H_
#define _HX_IO_URING_LOOP_H_

#include <liburing.h>
#include <chrono>

namespace HX { namespace STL { namespace coroutine { namespace loop {

// 绕过操作系统的缓存, 直接将数据从用户空间读写到磁盘
#if IO_URING_DIRECT
static constexpr size_t kOpenModeDefaultFlags = O_LARGEFILE | O_CLOEXEC | O_DIRECT;
#else
static constexpr size_t kOpenModeDefaultFlags = O_LARGEFILE | O_CLOEXEC;
#endif

enum class OpenMode : int {
    Read = O_RDONLY | kOpenModeDefaultFlags,
    Write = O_WRONLY | O_TRUNC | O_CREAT | kOpenModeDefaultFlags,
    ReadWrite = O_RDWR | O_CREAT | kOpenModeDefaultFlags,
    Append = O_WRONLY | O_APPEND | O_CREAT | kOpenModeDefaultFlags,
    Directory = O_RDONLY | O_DIRECTORY | kOpenModeDefaultFlags,
};

class IoUringLoop {
    IoUringLoop& operator=(IoUringLoop&&) = delete;
public:
    /**
     * @brief 创建一个 io_uring 的 Loop
     * 
     * @param entries 环形队列长度
     */
    explicit IoUringLoop(unsigned int entries = 512);

    bool run(std::optional<std::chrono::system_clock::duration> timeout);

    [[gnu::hot]] bool hasEvent() const noexcept {
        return _numSqesPending/* != 0*/;
    }

    [[gnu::hot]] struct io_uring_sqe *getSqe() {
        struct io_uring_sqe *sqe = ::io_uring_get_sqe(&_ring);
        while (!sqe) {
            int res = ::io_uring_submit(&_ring);
            if (res < 0) [[unlikely]] {
                if (res == -EINTR) {
                    continue;
                }
                throw std::system_error(-res, std::system_category());
            }
            sqe = ::io_uring_get_sqe(&_ring);
        }
        ++_numSqesPending;
        return sqe;
    }

    ~IoUringLoop() {
        io_uring_queue_exit(&_ring);
    }

private:
    ::io_uring _ring;
    std::size_t _numSqesPending = 0; // 还未完成的任务
};

struct [[nodiscard]] IoUringTask {
    IoUringTask(IoUringTask &&) = delete;

    explicit IoUringTask();

    struct Awaiter {
        bool await_ready() const noexcept {
            return false;
        }

        void await_suspend(std::coroutine_handle<> coroutine) {
            _task->_previous = coroutine;
            _task->_res = -ENOSYS;
        }

        int await_resume() const noexcept {
            return _task->_res;
        }

        IoUringTask *_task;
    };

    Awaiter operator co_await() {
        return Awaiter {this};
    }

    struct ::io_uring_sqe *getSqe() const noexcept {
        return _sqe;
    }

private:
    std::coroutine_handle<> _previous;

    friend IoUringLoop;

    union {
        int _res;
        struct ::io_uring_sqe *_sqe;
    };

public:
    /**
     * @brief 异步创建一个套接字
     * @param domain 指定 socket 的协议族 (AF_INET(ipv4)/AF_INET6(ipv6)/AF_UNIX/AF_LOCAL(本地))
     * @param type 套接字类型 SOCK_STREAM(tcp)/SOCK_DGRAM(udp)/SOCK_RAW(原始)
     * @param protocol 使用的协议, 通常为 0 (默认协议), 或者指定具体协议(如 IPPROTO_TCP、IPPROTO_UDP 等)
     * @param flags 
     * @return IoUringTask&& 
     */
    IoUringTask &&prepSocket(
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
     * @return IoUringTask&& 
     */
    IoUringTask &&prepAccept(
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
     * @param addrlen [out] 服务端信息长度指针
     * @return IoUringTask&& 
     */
    IoUringTask &&prepConnect(
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
     * @return IoUringTask&& 
     */
    IoUringTask &&prepRead(
        int fd,
        std::span<char> buf,
        std::uint64_t offset
    ) && {
        ::io_uring_prep_read(_sqe, fd, buf.data(), static_cast<unsigned int>(buf.size()), offset);
        return std::move(*this);
    }

    /**
     * @brief 异步写入文件
     * @param fd 文件描述符
     * @param buf [in] 写入的数据
     * @param offset 文件偏移量
     * @return IoUringTask&& 
     */
    IoUringTask &&prepWrite(
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
     * @return IoUringTask&& 
     */
    IoUringTask &&prepRecv(
        int fd,
        std::span<char> buf,
        int flags
    ) && {
        ::io_uring_prep_recv(_sqe, fd, buf.data(), buf.size(), flags);
        return std::move(*this);
    }

    /**
     * @brief 异步写入网络套接字文件
     * @param fd 文件描述符
     * @param buf [in] 写入的数据
     * @param flags 
     * @return IoUringTask&& 
     */
    IoUringTask &&prepSend(
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
     * @return IoUringTask&& 
     */
    IoUringTask &&prepClose(int fd) && {
        ::io_uring_prep_close(_sqe, fd);
        return std::move(*this);
    }
};


}}}} // namespace HX::STL::coroutine::loop

#endif // !_HX_IO_URING_LOOP_H_