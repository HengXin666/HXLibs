#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2026-05-31 01:16:53
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
#include <memory>

#include <HXLibs/coroutine/task/Task.hpp>

namespace HX::coroutine {

/**
 * @brief 协程串行调度器
 */
class SerialExecutor {
    struct SerialTask {
        struct promise_type {
            StopAwaiter<true> initial_suspend() noexcept { return {}; }
            auto get_return_object() noexcept {
                return std::coroutine_handle<promise_type>::from_promise(*this);
            }
            StopAwaiter<false> final_suspend() noexcept {
                _list->erase(_it);
                return {};
            }
            void return_void() noexcept { }
            void unhandled_exception() noexcept { std::terminate(); }

            std::list<std::coroutine_handle<promise_type>>* _list{};
            std::list<std::coroutine_handle<promise_type>>::iterator _it;
        };

        using HandleType = std::coroutine_handle<promise_type>;

        SerialTask(HandleType h) : _handle(h) {}
        SerialTask& operator=(SerialTask&&) = delete;

        SerialTask&& via(std::list<SerialTask::HandleType>& runList) && {
            _handle.promise()._list = &runList;
            _handle.promise()._it = runList.insert(runList.end(), _handle);
            return std::move(*this);
        }
        void detach() && { _handle.resume(); }
    private:
        HandleType _handle;
    };

    struct State {
        bool isRun{};
        // 传递引用, 防止其因为析构引发的 段错误
        std::queue<std::tuple<coroutine::Task<> const&, std::coroutine_handle<>>> q;
        // SerialTask 运行队列
        std::list<SerialTask::HandleType> runList;
    };

    struct SerialAwaiter {
        explicit SerialAwaiter(SerialExecutor& mtx, coroutine::Task<> const& task)
            : _mtx{mtx}
            , _task{task}
        {}
        constexpr bool await_ready() const noexcept { return false; }
        constexpr bool await_suspend(std::coroutine_handle<> co) const noexcept {
            [[maybe_unused]] auto& [isRun, q, _] = *_mtx._runList;
            if (!isRun) {
                q.push({_task, std::noop_coroutine()});
                return false;
            }
            q.push({_task, co});
            return true;
        }
        constexpr void await_resume() const noexcept {}
    private:
        SerialExecutor& _mtx;
        coroutine::Task<> const& _task;
    };

    coroutine::Task<> start() {
        auto runListPtr = _runList;
        auto& [isRun, q, runList] = *runListPtr;
        auto [task, co] = std::move(q.front());
        q.pop();
        co_await task;
        co.resume();
        if (!isRun || q.empty()) {
            isRun = false;
            co_return;
        }
        [this]() -> SerialTask {
            co_await start();
        }().via(runList).detach();
    }
public:
    /**
     * @brief 串行调度
     * @warning 需要保证在有任务的时候, t 应该在其生命周期内
     * @note 也就是说, 需要特别小心 whenAny 的使用
     */
    coroutine::Task<> serial(coroutine::Task<> const& t) {
        co_await SerialAwaiter{*this, t};
        if (auto& isRun = _runList->isRun; !isRun) {
            isRun = true;
            co_await start();
        }
    }

    /**
     * @brief 清空所有正在等待的串行任务
     */
    void clear() {
        auto& [isRun, q, runList] = *_runList;
        q = {};
        isRun = false;
        for (auto& co : runList)
            co.destroy();
        runList.clear();
    }

    ~SerialExecutor() noexcept {
        clear();
    }
private:
    // @todo 需要一个协程的共享智能指针
    const std::shared_ptr<State> _runList = std::make_shared<State>(
        State{false, {}, {}});
};

} // namespace HX::coroutine
