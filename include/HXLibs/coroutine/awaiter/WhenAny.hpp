#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-06-09 23:09:27
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

#include <utility>
#include <span>

#include <HXLibs/container/Uninitialized.hpp>
#include <HXLibs/container/UninitializedNonVoidVariant.hpp>
#include <HXLibs/coroutine/concepts/Awaiter.hpp>
#include <HXLibs/coroutine/task/Task.hpp>

namespace HX::coroutine {

template <AwaitableLike... Ts>
using WhenAnyReturnType = container::UninitializedNonVoidVariant<
    AwaiterReturnType<Ts>...
>;

namespace internal {

/**
 * @brief WhenAny 控制块
 */
struct WhenAnyCtlBlock {
    std::coroutine_handle<> previous;
    std::exception_ptr _exception{};
    bool isBrack = false;
    bool isInit = false;
};

struct WhenAnyPromise {
    using InitStrategy = std::suspend_always;
    using DeleStrategy = PreviousAwaiter;

    InitStrategy initial_suspend() noexcept { return {}; }
    auto get_return_object() noexcept {
        return std::coroutine_handle<WhenAnyPromise>::from_promise(*this);
    }
    DeleStrategy final_suspend() noexcept {
        return {_mainCoroutine}; // 3) WhenAny 的核心, 协程终止时候, 都执行这个; 然后回到 whenAny 中!
    }
    void return_value(std::coroutine_handle<> previous) noexcept {
        _mainCoroutine = previous; // 2) 实际上就是 ctlBlock.previous; 即 whenAny 协程句柄
    }
    void unhandled_exception() noexcept {
        std::terminate();
    }
    std::coroutine_handle<> _mainCoroutine;
};

struct WhenAnyAwaiter {
    constexpr bool await_ready() const noexcept { return false; }
    constexpr auto await_suspend(std::coroutine_handle<> coroutine) const noexcept {
        ctlBlock.previous = coroutine; // 1) 此处记录了 whenAny 协程句柄
        for (const auto& co : cos.subspan(0, cos.size() - 1)) {
            auto coH = static_cast<std::coroutine_handle<>>(co);
            coH.resume();
            if (ctlBlock.isBrack) [[unlikely]] { // 存在调用者会这样调用, 但是显然不是期望的使用方式
                return coroutine;
            }
        }
        ctlBlock.isInit = true;
        return static_cast<std::coroutine_handle<>>(cos.back());
    }
    constexpr void await_resume() const noexcept {}

    std::span<Task<std::coroutine_handle<>, WhenAnyPromise> const> cos;
    WhenAnyCtlBlock& ctlBlock;
};

template <std::size_t Idx, AwaitableLike T, typename Res>
Task<std::coroutine_handle<>, WhenAnyPromise> anyStart(
    T&& t, Res& res, WhenAnyCtlBlock& ctlBlock
) {
    try {
        if constexpr (Awaiter<T>) {
            if constexpr (std::is_void_v<decltype(t.await_resume())>) {
                static_cast<void>(co_await t);
                res.template emplace<Idx>();
            } else {
                res.template emplace<Idx>(co_await t);
            }
        } else if constexpr (Awaitable<T>) {
            if constexpr (std::is_void_v<decltype(t.operator co_await().await_resume())>) {
                static_cast<void>(co_await t);
                res.template emplace<Idx>();
            } else {
                res.template emplace<Idx>(co_await t);
            }
        } else {
            static_assert(!sizeof(T), "The type is not Awaiter");
        }
    } catch (...) {
        ctlBlock._exception = std::current_exception();
    }
    if (ctlBlock.isInit) [[likely]] {
        co_return ctlBlock.previous;
    }
    // 存在调用者会这样调用, 但是显然不是期望的使用方式
    // 即 协程内部完全没有挂起 (co_await), 导致直接返回
    ctlBlock.isBrack = true;
    co_return std::noop_coroutine();
}

template <
    std::size_t... Idx, 
    AwaitableLike... Ts, 
    typename ResType = WhenAnyReturnType<Ts...>
>
Task<ResType> whenAny(std::index_sequence<Idx...>, Ts&&... ts) {
    // 1. 存储所有的返回值
    ResType res;

    // 2. 启动所有协程
    WhenAnyCtlBlock block;
    std::array<Task<std::coroutine_handle<>, WhenAnyPromise>, sizeof...(Ts)> cos {
        anyStart<Idx>(std::forward<Ts>(ts), res, block)...
    };

    // 3. 等待其中一个 (挂起)
    co_await WhenAnyAwaiter{cos, block};

    if (block._exception) [[unlikely]] {
        std::rethrow_exception(block._exception);
    }
    
    // 4. 通过返回值确定返回谁
    co_return res;
}

} // namespace internal

template <AwaitableLike... Ts>
[[nodiscard]] auto whenAny(Ts&&... ts) {
    return internal::whenAny(
        std::make_index_sequence<sizeof...(ts)>(), 
        std::forward<Ts>(ts)...
    );
}

namespace internal {

template <AwaitableLike... Ts>
struct [[nodiscard]] WhenAnyWrap {
private:
    static constexpr auto makeWhenAny(std::tuple<Ts...>&& tp) noexcept {
        return [&] <std::size_t... Idx> (std::index_sequence<Idx...>) constexpr {
            return whenAny(std::move(std::get<Idx>(tp))...);
        } (std::make_index_sequence<sizeof...(Ts)>{});
    }
public:
    struct WhenWrapAwaiter {
        WhenWrapAwaiter(std::tuple<Ts...>&& tp)
            : _whenAny{makeWhenAny(std::move(tp))}
        {}
        constexpr bool await_ready() const noexcept { return false; }
        constexpr std::coroutine_handle<> await_suspend(std::coroutine_handle<> coroutine) const noexcept {
            _whenAny.getPromise().promise()._previous = coroutine;  // 当 _whenAny 执行完, 恢复到 coroutine
                                                                    // 原理同 ExitAwaiter
            return _whenAny;
        }
        constexpr auto await_resume() const {
            return _whenAny.getPromise().promise().result();
        }
        decltype(
            WhenAnyWrap<Ts...>::makeWhenAny(std::declval<std::tuple<Ts...>>())
        ) _whenAny;
    };

    constexpr WhenAnyWrap(Ts&&... ts) 
        : _tp{std::move(ts)...}
    {}
    
    [[nodiscard]] constexpr auto operator co_await() && noexcept {
        return WhenWrapAwaiter{std::move(_tp)};
    }
private:
    std::tuple<Ts...> _tp;

    template <typename... Us, AwaitableLike U>
    friend constexpr WhenAnyWrap<Us..., U> operator||(WhenAnyWrap<Us...>&& ts, U&& u) noexcept;

    template <typename... Us, AwaitableLike U>
    friend constexpr WhenAnyWrap<U, Us...> operator||(U&& u, WhenAnyWrap<Us...>&& ts) noexcept;

    template <typename... Us, AwaitableLike... Es>
    friend constexpr WhenAnyWrap<Us..., Es...> operator||(WhenAnyWrap<Us...>&& ts, WhenAnyWrap<Es...>&& us) noexcept;
};

template <typename... Ts, AwaitableLike U>
[[nodiscard]] constexpr WhenAnyWrap<Ts..., U> operator||(WhenAnyWrap<Ts...>&& ts, U&& u) noexcept {
    return [&] <std::size_t... Idx> (std::index_sequence<Idx...>) constexpr -> WhenAnyWrap<Ts..., U> {
        return {std::move(std::get<Idx>(ts._tp))..., std::move(u)};
    } (std::make_index_sequence<sizeof...(Ts)>{});
}

template <typename... Ts, AwaitableLike U>
[[nodiscard]] constexpr WhenAnyWrap<U, Ts...> operator||(U&& u, WhenAnyWrap<Ts...>&& ts) noexcept {
    return [&] <std::size_t... Idx> (std::index_sequence<Idx...>) constexpr -> WhenAnyWrap<U, Ts...> {
        return {std::move(u), std::move(std::get<Idx>(ts._tp))...};
    } (std::make_index_sequence<sizeof...(Ts)>{});
}

template <typename... Ts, AwaitableLike... Us>
[[nodiscard]] constexpr WhenAnyWrap<Ts..., Us...> operator||(WhenAnyWrap<Ts...>&& ts, WhenAnyWrap<Us...>&& us) noexcept {
    return [&] <std::size_t... I, std::size_t... J> (
        std::index_sequence<I...>, std::index_sequence<J...>
    ) constexpr -> WhenAnyWrap<Ts..., Us...> {
        return {std::move(std::get<I>(ts._tp))..., std::move(std::get<J>(us._tp))...};
    } (std::make_index_sequence<sizeof...(Ts)>{}, std::make_index_sequence<sizeof...(Us)>{});
}

} // namespace internal

template <AwaitableLike T, AwaitableLike U>
[[nodiscard]] constexpr internal::WhenAnyWrap<T, U> operator||(T&& t, U&& u) {
    return {std::move(t), std::move(u)};
}

} // namespace HX::coroutine
