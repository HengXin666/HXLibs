#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2024-08-06 21:54:21
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

#include <HXSTL/coroutine/promise/Promise.hpp>
#include <HXSTL/coroutine/awaiter/ExitAwaiter.hpp>

namespace HX { namespace STL { namespace coroutine { namespace task {

/**
 * @brief 协程任务类: 直接返回, 而不是马上执行
 * @tparam T 协程的返回值类型
 * @tparam P 协程的`promise_type`类型
 * @tparam A 被`co_await`时的行为
 */
template <
    class T = void, 
    class P = HX::STL::coroutine::promise::Promise<T>, 
    class A = HX::STL::coroutine::awaiter::ExitAwaiter<T, P>
>
struct [[nodiscard]] Task {
    using promise_type = P;

    Task(std::coroutine_handle<promise_type> coroutine = nullptr) noexcept
        : _coroutine(coroutine) {}

    // Task(Task &&) = delete;

    Task(Task &&that) noexcept : _coroutine(that._coroutine) {
        that._coroutine = nullptr;
    }

    Task &operator=(Task &&that) noexcept {
        std::swap(_coroutine, that._coroutine);
        return *this;
    }

    virtual ~Task() {
        if (_coroutine)
            _coroutine.destroy();
    }

    auto operator co_await() const noexcept {
        return A(_coroutine); // C++20 自动生成构造
    }

    operator std::coroutine_handle<>() const noexcept {
        return _coroutine;
    }

    auto getCoroutine() const {
        return _coroutine;
    }

private:
    std::coroutine_handle<promise_type> _coroutine; // 当前协程句柄
};

/**
 * @brief 协程任务类: 会立马开始执行
 * @tparam T 协程的返回值类型
 * @tparam P 协程的`promise_type`类型
 * @tparam A 被`co_await`时的行为
 */
template <
    class T = void, 
    class P = HX::STL::coroutine::promise::Promise<T, false>, 
    class A = HX::STL::coroutine::awaiter::ExitAwaiter<T, P>
>
struct [[nodiscard]] ImmediatelyTask {
    using promise_type = P;

    ImmediatelyTask(std::coroutine_handle<promise_type> coroutine = nullptr) noexcept
        : _coroutine(coroutine) {}

    // ImmediatelyTask(ImmediatelyTask &&) = delete;

    ImmediatelyTask(ImmediatelyTask &&that) noexcept : _coroutine(that._coroutine) {
        that._coroutine = nullptr;
    }

    ImmediatelyTask &operator=(ImmediatelyTask &&that) noexcept {
        std::swap(_coroutine, that._coroutine);
        return *this;
    }

    ~ImmediatelyTask() {
        if (_coroutine)
            _coroutine.destroy();
    }

    auto operator co_await() const noexcept {
        return A(_coroutine); // C++20 自动生成构造
    }

    operator std::coroutine_handle<>() const noexcept {
        return _coroutine;
    }

    auto getCoroutine() const {
        return _coroutine;
    }

private:
    std::coroutine_handle<promise_type> _coroutine; // 当前协程句柄
};

/**
 * @brief 启动协程任务
 * @tparam Loop 用于监测的循环的类型, 需要提供`.run()`方法
 * @tparam T 协程任务的返回类型
 * @tparam P 协程的`promise_type`类型
 * @tparam A 被`co_await`时的行为
 * @param loop 监测者Loop
 * @param t 协程任务
 * @return T 协程任务的返回值
 */
template <class Loop, class T, class P, class A>
T runTask(Loop& loop, const Task<T, P, A>& t) {
    auto a = t.operator co_await();
    // 为什么是`std::noop_coroutine()`呢? 因为 Task 是可以恢复到父协程继续执行的
    // 而如果这个协程它就是根协程呢? 那么就定义它的父协程是这个, 故而执行但是马上就退出啦~
    a.await_suspend(std::noop_coroutine()).resume();
    loop.run();
    return a.await_resume();
};

}}}} // namespace HX::STL::coroutine::task

#endif // !_HX_TASK_H_