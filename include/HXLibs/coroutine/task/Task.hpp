#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-06-09 22:52:07
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
#ifndef _HX_TASK_H_
#define _HX_TASK_H_

#include <stdexcept>

#include <HXLibs/coroutine/awaiter/ExitAwaiter.hpp>
#include <HXLibs/coroutine/promise/Promise.hpp>

namespace HX::coroutine {

template <
    typename T = void,
    typename P = Promise<T>,
    typename Awaiter = ExitAwaiter<T, P>
>
struct [[nodiscard]] Task {
    using promise_type = P;

    constexpr Task(std::coroutine_handle<promise_type> h = nullptr)
        : _handle(h)
    {}

    ~Task() noexcept {
        if constexpr (std::is_same_v<typename P::DeleStrategy, StopAwaiter<false>>) {
            return;
        } else {
            if (_handle) {
                _handle.destroy();
                _handle = nullptr;
            }
        }
    }

    Task(Task&& that) : _handle(that._handle) {
        that._handle = nullptr;
    }

    Task &operator=(Task&& that) noexcept {
        std::swap(_handle, that._handle);
        return *this;
    }

    Task(Task const&) noexcept = delete;
    Task& operator=(Task const&) noexcept = delete;

    constexpr Awaiter operator co_await() noexcept {
        return Awaiter{_handle};
    }

    constexpr operator std::coroutine_handle<>() const noexcept {
        return _handle;
    }

    // constexpr std::coroutine_handle<promise_type> getPromise() const noexcept {
    //     return _handle;
    // }

    /**
     * @brief 立即执行协程
     * @warning 除非你可以保证它可以执行到 co_return 并且在此之前不会返回, 否则不要调用该方法
     * @return constexpr auto 
     */
    constexpr auto start() const {
        _handle.resume();
        if constexpr (requires {
            _handle.promise().result();
        }) {
            if (_handle.done()) [[likely]] {
                return _handle.promise().result();
            } else {
                // 不是期望的! 协程还没有执行完毕
                throw std::runtime_error{"The collaborative process has not been completed yet"};
            }
        }
    }
private:
    std::coroutine_handle<promise_type> _handle;
};

} // namespace HX::coroutine

#endif // !_HX_TASK_H_