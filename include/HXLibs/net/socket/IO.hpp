#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-07 21:09:45
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

#include <HXLibs/coroutine/task/Task.hpp>
#include <HXLibs/coroutine/loop/EventLoop.hpp>
#include <HXLibs/net/socket/SocketFd.hpp>
#include <HXLibs/exception/ExceptionMode.hpp>
#include <HXLibs/utils/TimeNTTP.hpp>

#ifndef NDEBUG
    #include <HXLibs/log/Log.hpp>
#endif // !NDEBUG

namespace HX::net {

namespace internal {

#if defined(__linux__)

template <typename Timeout>
    requires(utils::HasTimeNTTP<Timeout>)
auto* getTimePtr() noexcept {
    // 为了对外接口统一, 并且尽可能的减小调用次数, 故模板 多实例 特化静态成员, 达到 @cache 的效果
    static auto to = coroutine::durationToKernelTimespec(
        Timeout::StdChronoVal
    );
    return &to;
}

#endif

} // namespace internal

/**
 * @brief Socket IO 接口, 仅提供写入和读取等操作, 不会进行任何解析和再封装
 * @note 如果需要进行读写解析, 可以看 Res / Req 的接口
 */
class IO {
public:
    inline constexpr static std::size_t kBufMaxSize = 1 << 14; // 16kb

    IO(coroutine::EventLoop& eventLoop)
        : _fd{kInvalidSocket}
        , _eventLoop{eventLoop}
    {}

    IO(SocketFdType fd, coroutine::EventLoop& eventLoop)
        : _fd{fd}
        , _eventLoop{eventLoop}
    {}

    IO& operator=(IO&&) noexcept = delete;

    coroutine::Task<int> recv(std::span<char> buf) {
        co_return static_cast<int>(
            co_await _eventLoop.makeAioTask().prepRecv(_fd, buf, 0));
    }

    coroutine::Task<int> recv(std::span<char> buf, std::size_t n) {
        co_return static_cast<int>(
            co_await _eventLoop.makeAioTask().prepRecv(
                _fd, buf.subspan(0, n), 0));
    }

    /**
     * @brief 完全读取
     * @param buf 
     * @return coroutine::Task<> 
     */
    coroutine::Task<> fullyRecv(std::span<char> buf) {
        while (!buf.empty()) {
            auto sent = static_cast<std::size_t>(
                HXLIBS_CHECK_EVENT_LOOP(
                    co_await recv(buf)
                )
            );
            buf = buf.subspan(sent);
        }
    }

    /**
     * @brief 直接将二进制写入到类型T中, 注意需要区分大小端
     * @warning 默认是网络序(大端), 如果需要使用, 注意转换
     * @tparam T 
     * @return coroutine::Task<T> 
     */
    template <typename T>
    coroutine::Task<T> recvStruct() {
        T res;
        co_await fullyRecv(std::span<char>{
            reinterpret_cast<char*>(&res), sizeof(T)
        });
        co_return res;
    }

#if defined(__linux__)
    template <typename Timeout>
        requires(utils::HasTimeNTTP<Timeout>)
    coroutine::Task<coroutine::WhenAnyReturnType<
        coroutine::AioTask,
        decltype(std::declval<coroutine::AioTask>().prepLinkTimeout({}, {}))
    >> recvLinkTimeout(std::span<char> buf) {
        co_return co_await coroutine::AioTask::linkTimeout(
            _eventLoop.makeAioTask().prepRecv(_fd, buf, 0),
            _eventLoop.makeAioTask().prepLinkTimeout(
                internal::getTimePtr<Timeout>(), 0)
        );
    }
#elif defined(_WIN32)
    template <typename Timeout>
        requires(utils::HasTimeNTTP<Timeout>)
    coroutine::Task<
        container::UninitializedNonVoidVariant<uint64_t, void> // 只能显式指定返回值
    > recvLinkTimeout(std::span<char> buf) {                   // 因为 _AioTimeoutTask 是私有字段
                                                               // 如果想方便, 就写好 _AioTimeoutTask.co() 的返回值
                                                               // 为 public using, 然后直接让我们用~
        co_return co_await coroutine::AioTask::linkTimeout(
            _eventLoop.makeAioTask().prepRecv(_fd, buf, 0),
            _eventLoop.makeAioTask().prepLinkTimeout(
                _eventLoop.makeTimer().sleepFor(Timeout::StdChronoVal))
        );
    }
#else
    #error "Does not support the current operating system."
#endif

    /**
     * @brief 写入数据, 内部保证完全写入
     * @param buf 
     * @return coroutine::Task<> 
     */
    coroutine::Task<> fullySend(std::span<char const> buf) {
        // io_uring 也不保证其可以完全一次性写入...
        while (!buf.empty()) {
            auto sent = static_cast<std::size_t>(
                HXLIBS_CHECK_EVENT_LOOP(
                    co_await _eventLoop.makeAioTask()
                                       .prepSend(_fd, buf, 0)
                )
            );
            buf = buf.subspan(sent);
        }
    }

    /**
     * @brief 写入数据, 内部保证完全写入
     * @param buf 
     * @param n 
     * @return coroutine::Task<> 
     */
    coroutine::Task<> fullySend(std::span<char const> buf, std::size_t n) {
        co_await fullySend(buf.subspan(0, n));
    }

    /**
     * @brief 完整的写入数据, 内部保证写入完成; 如果超时则抛出异常
     * @tparam Timeout 
     */
    template <typename Timeout>
        requires(utils::HasTimeNTTP<Timeout>)
    coroutine::Task<> sendLinkTimeout(std::span<char const> buf) {
#if defined(__linux__)
        // io_uring 也不保证其可以完全一次性写入...
        while (!buf.empty()) {
            auto res = co_await coroutine::AioTask::linkTimeout(
                _eventLoop.makeAioTask().prepSend(_fd, buf, 0),
                _eventLoop.makeAioTask().prepLinkTimeout(
                    internal::getTimePtr<Timeout>(), 0)
            );
            if (res.index() == 1) [[unlikely]] {
                throw std::runtime_error{"is Timeout"}; // 超时了
            }
            auto sent = static_cast<std::size_t>(
                HXLIBS_CHECK_EVENT_LOOP(
                    (res.template get<0, exception::ExceptionMode::Nothrow>())
            ));
#if 0 // 虽然但是, 有问题再开启
            if (sent == 0) [[unlikely]] {
                // 对方优雅的关闭了, 虽然再次发送是必然抛异常然后就关闭的
                // 但是直接写判断, 可以减少 io_uring 的压力, 减少内核态切换
                // 但是本身这种事件触发的概率就很低很低了
                throw std::runtime_error{"is Close"};
            }
#endif
            buf = buf.subspan(sent);
        }
#elif defined(_WIN32)
        while (!buf.empty()) {
            auto res = co_await coroutine::AioTask::linkTimeout(
                _eventLoop.makeAioTask().prepSend(_fd, buf, 0),
                _eventLoop.makeAioTask().prepLinkTimeout(
                    _eventLoop.makeTimer().sleepFor(Timeout::StdChronoVal))
            );
            if (res.index() == 1) [[unlikely]] {
                throw std::runtime_error{"is Timeout"}; // 超时了
            }
            auto sent = static_cast<std::size_t>(
                HXLIBS_CHECK_EVENT_LOOP(
                    (res.template get<0, exception::ExceptionMode::Nothrow>())
            ));
            buf = buf.subspan(sent);
        }
#else
        #error "Does not support the current operating system."
#endif
    }

    /**
     * @brief 关闭套接字
     * @warning 注意, 内部不会抛异常! 需要外部自己检查
     * @return coroutine::Task<int> close的返回值
     */
    coroutine::Task<int> close() noexcept {
        /// @todo 此处可能也需要特化 ckose ? 因为 win 下的超时实际上就已经close了(?)
        auto res = co_await _eventLoop.makeAioTask()
                                           .prepClose(_fd);
        _fd = kInvalidSocket;
        co_return res;
    }

    /**
     * @brief 绑定新的 fd
     * @warning 必须把之前的 fd 给 close 了
     * @param fd 
     */
    coroutine::Task<> bindNewFd(SocketFdType fd) noexcept {
        co_await close();
        _fd = fd;
    }

    /**
     * @brief 清空套接字
     * @warning 请注意, 希望你知道你在做什么! 而不是泄漏套接字!
     * @note 期望编译器可以自己优化下面方法为不调用, 仅为 debug 时候使用.
     */
    constexpr void reset() noexcept {
#ifndef NDEBUG
        _fd = kInvalidSocket;
#endif // !NDEBUG
    }

    operator coroutine::EventLoop&() {
        return _eventLoop;
    }

#ifdef NDEBUG
    ~IO() noexcept = default;
#else
    ~IO() noexcept {
        if (_fd != kInvalidSocket) [[unlikely]] {
            log::hxLog.error("IO 没有进行 close");
        }
    }
#endif // !NDEBUG

private:
    SocketFdType _fd;
    coroutine::EventLoop& _eventLoop;
};

} // namespace HX::net

