#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2026-05-30 00:18:21
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

#include <queue>
#include <list>

#include <HXLibs/coroutine/task/Task.hpp>
#include <HXLibs/coroutine/loop/EventLoop.hpp>

// debug
#include <HXLibs/meta/FixedString.hpp>
#include <HXLibs/log/debug/RaiiLog.hpp>

namespace HX::coroutine {

/**
 * @brief 协程串行调度器
 */
class SerialScheduler {
    struct BackgroundTask;

    struct BackgroundPromise {
        constexpr StopAwaiter<true> initial_suspend() noexcept { return {}; }
        constexpr auto get_return_object() noexcept {
            return std::coroutine_handle<BackgroundPromise>::from_promise(*this);
        }
        constexpr StopAwaiter<true> final_suspend() noexcept {
            if (_btoq) {
                // _btoq->erase(_it);
            }
            return {};
        }
        constexpr void return_void() noexcept { }
        void unhandled_exception() noexcept { std::terminate(); }

        BackgroundPromise& operator=(BackgroundPromise&&) = delete;

        std::list<BackgroundTask>* _btoq{};
        std::list<BackgroundTask>::iterator _it{};
    };

    struct [[nodiscard]] BackgroundTask {
        using promise_type = BackgroundPromise;

        constexpr BackgroundTask(std::coroutine_handle<promise_type> h = nullptr)
            : _handle{h}
        {}

        constexpr BackgroundTask(BackgroundTask&& that) noexcept
            : _handle{that._handle}
        {
            that._handle = nullptr;
        }

        constexpr BackgroundTask& operator=(BackgroundTask&& that) noexcept {
            std::swap(_handle, that._handle);
            return *this;
        }

        BackgroundTask(BackgroundTask const&) noexcept = delete;
        BackgroundTask& operator=(BackgroundTask const&) noexcept = delete;

        BackgroundTask& via(std::list<BackgroundTask>& btoq) {
            _handle.promise()._btoq = &btoq;
            _handle.promise()._it = --btoq.end();
            return *this;
        }

        constexpr void detach() {
            _handle.resume();
        }

        ~BackgroundTask() noexcept {
            if (_handle) {
                _handle.destroy();
                _handle = nullptr;
            }
        }

        constexpr operator std::coroutine_handle<>() const noexcept {
            return _handle;
        }
    private:
        std::coroutine_handle<promise_type> _handle;
    };

    struct SerialAwaiter {
        explicit SerialAwaiter(SerialScheduler& mtx, coroutine::Task<> const& task)
            : _mtx{mtx}
            , _task{task}
        {}
        constexpr bool await_ready() const noexcept { return false; }
        constexpr bool await_suspend(std::coroutine_handle<> co) const noexcept {
            if (!_mtx._runCnt) {
                ++_mtx._runCnt;
                _mtx._q.push({_task, std::noop_coroutine()});
                return false;
            }
            _mtx._q.push({_task, co});
            return true;
        }
        constexpr void await_resume() const noexcept {}
    private:
        SerialScheduler& _mtx;
        coroutine::Task<> const& _task;
    };

    coroutine::Task<> start() {
        auto [task, co] = std::move(_q.front());
        _q.pop();
        co_await task;
        // 核心原因: 内部 start 的 task 是 const&, 而外部被析构了, 导致不继续
        // 导致协程无法恢复, 因为 RootTask 是协程完毕才释放, 导致无法被释放了
        // 需要定制调度所有权: 现在 BTask 所有权在 _btoq, 当它完成时会自动删除 _it 元素
        // 从而触发 ~BTask(), 从而 RAII; 即便任务没有完成, 依旧因为 _btoq 的析构. 而析构, 以防内存泄漏
        using namespace meta::fixed_string_literals;
        _btoq.insert(_btoq.end(), [this, co]() -> BackgroundTask {
            log::RaiiLog<"里面"_fs> _{};
            co.resume();
            --_runCnt; // 不能在 resume() 后使用 _q.pop(), 会导致玄学段错误 (MSVC)
            if (_q.empty()) {
                co_return;
            }
            co_await start();
        }())->via(_btoq).detach();
    }
public:
    SerialScheduler(EventLoop& loop)
        : _loop{loop}
    {}

    coroutine::Task<> serial(coroutine::Task<> const& t) {
        co_await SerialAwaiter{*this, t};
        if (_runCnt == 1) {
            co_await start();
        }
        co_await _loop.makeTimer().yield();
    }

    /**
     * @brief 清空所有正在等待的串行任务
     */
    void clear() {
        _q = {};
        _btoq.clear();
        _runCnt = 0;
    }
private:
    EventLoop& _loop;
    // 传递引用, 防止其因为析构引发的 段错误
    std::queue<std::tuple<coroutine::Task<> const&, std::coroutine_handle<>>> _q;
    // 后台任务所有权队列 (RootTask)
    std::list<BackgroundTask> _btoq;
    std::size_t _runCnt{};
};

} // namespace HX::coroutine
