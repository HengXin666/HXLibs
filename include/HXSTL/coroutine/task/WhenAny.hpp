#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2024-08-16 14:35:47
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
#ifndef _HX_WHEN_ANY_H_
#define _HX_WHEN_ANY_H_

#include <span>
#include <variant>

#include <HXSTL/coroutine/awaiter/AwaiterConcept.hpp>
#include <HXSTL/coroutine/task/Task.hpp>

namespace HX { namespace STL { namespace coroutine { namespace task {

/**
 * @brief 完成如何一个协程任务就退出 (多和定时器配合)
 */
class WhenAny {
    struct ReturnPreviousPromise {
        auto initial_suspend() noexcept {
            return std::suspend_always();
        }

        auto final_suspend() noexcept {
            return HX::STL::coroutine::awaiter::PreviousAwaiter(_previous);
        }

        void unhandled_exception() {
            throw;
        }

        void return_value(std::coroutine_handle<> previous) noexcept {
            _previous = previous;
        }

        auto get_return_object() {
            return std::coroutine_handle<ReturnPreviousPromise>::from_promise(*this);
        }

        std::coroutine_handle<> _previous{};

        ReturnPreviousPromise &operator=(ReturnPreviousPromise &&) = delete;
    };

    struct ReturnPreviousTask {
        using promise_type = ReturnPreviousPromise;

        ReturnPreviousTask(std::coroutine_handle<promise_type> coroutine) noexcept
            : _coroutine(coroutine) {}

        ReturnPreviousTask(ReturnPreviousTask &&) = delete;

        ~ReturnPreviousTask() {
            _coroutine.destroy();
        }

        std::coroutine_handle<promise_type> _coroutine;
    };

    /**
     * @brief WhenAny 控制块
     */
    struct WhenAnyCtlBlock {
        inline static constexpr std::size_t kNullIndex = std::size_t(-1);

        std::size_t _index {kNullIndex};
        std::coroutine_handle<> _previous {}; // 根协程 (whenAnyImpl)
        std::exception_ptr _exception {};
    };

    struct WhenAnyAwaiter {
        bool await_ready() const noexcept {
            return false;
        }

        std::coroutine_handle<>
        await_suspend(std::coroutine_handle<> coroutine) const {
            if (_tasks.empty()) 
                return coroutine;
            _control._previous = coroutine;
            // 启动任务, 如果协程里面有co_await则会返回到这里, 那么这里继续启动
            for (const auto &t : _tasks.subspan(0, _tasks.size() - 1))
                t._coroutine.resume();
            return _tasks.back()._coroutine; // 最终启动这个, 并且 co_await 挂起
        } // 直到有一个完成, 它就会通过控制块恢复出来.

        void await_resume() const {
            if (_control._exception) [[unlikely]] {
                std::rethrow_exception(_control._exception);
            }
        }

        WhenAnyCtlBlock &_control;
        std::span<ReturnPreviousTask const> _tasks;
    };

    template <class T>
    static ReturnPreviousTask whenAnyHelper(
        auto const &t, 
        WhenAnyCtlBlock &control,
        HX::STL::container::Uninitialized<T> &res, std::size_t index
    ) {
        try {
            res.putVal(co_await t); // 如果任务返回了, 那么就存入返回值
        } catch (...) {
            control._exception = std::current_exception();
            co_return control._previous;
        }
        control._index = index;
        co_return control._previous; // 执行控制块
    }

    template <std::size_t... Is, class... Ts>
    static HX::STL::coroutine::task::Task<
        std::variant<
            typename HX::STL::coroutine::awaiter::AwaitableTraits<Ts>::NonVoidRetType...
        >
    > whenAnyImpl(
        std::index_sequence<Is...>, 
        Ts&&... ts
    ) {
        WhenAnyCtlBlock control{}; // 创建控制块
        
        std::tuple<HX::STL::container::Uninitialized<
            typename HX::STL::coroutine::awaiter::AwaitableTraits<Ts>::RetType>...
        > res; // 创建存储任务结果的元组
        
        ReturnPreviousTask taskArray[] {
            whenAnyHelper(ts, control, std::get<Is>(res), Is)...
        }; // 创建任务数组 (注意: 任务协程创建但不是立即运行的!)
        
        // 启动所有的任务, 并且等待其中一个执行完毕
        co_await WhenAnyAwaiter(control, taskArray);

        HX::STL::container::Uninitialized<std::variant<
            typename HX::STL::coroutine::awaiter::AwaitableTraits<Ts>::NonVoidRetType...>
        > varResult; // 创建结果变量

        // 找到控制块的返回值, 并且 move 给 varResult 以返回
        ((control._index == Is && (varResult.putVal(
            std::in_place_index<Is>, 
            std::get<Is>(res).moveVal()
        ), 0)), ...);
        co_return varResult.moveVal();
    }

public:
    /**
     * @brief 完成其中任何一个协程则返回, 并且终止其他协程
     * @tparam Ts... 协程 (返回值不能为`void`, 
     * 但是可以用`HX::STL::container::NonVoidHelper<void>`来擦除void类型)
     * @return 执行成功的那个协程的返回值 (std::variant<...>)
     * @warning 请把计时放在第一个参数(如果有的话), 否则可能报错?!
     */
    template <HX::STL::coroutine::awaiter::Awaitable... Ts>
        requires(sizeof...(Ts) != 0)
    static auto whenAny(Ts&&... ts) {
        return whenAnyImpl(
            std::make_index_sequence<sizeof...(Ts)>{},
            std::forward<Ts>(ts)...
        );
    }
};

}}}} // namespace HX::STL::coroutine::task

#endif // !_HX_WHEN_ANY_H_