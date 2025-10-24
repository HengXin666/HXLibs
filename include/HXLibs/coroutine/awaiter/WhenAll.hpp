#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-10-10 21:35:47
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

#include <span>

#include <HXLibs/coroutine/task/Task.hpp>
#include <HXLibs/coroutine/concepts/Awaiter.hpp>

namespace HX::coroutine {

namespace internal {

/**
 * @brief WhenAll 控制块
 */
struct WhenAllCtlBlock {
    std::coroutine_handle<> previous;
    std::size_t cnt;
};

struct WhenAllPromise {
    using InitStrategy = std::suspend_always;
    using DeleStrategy = PreviousAwaiter;

    InitStrategy initial_suspend() noexcept { return {}; }
    auto get_return_object() noexcept {
        return std::coroutine_handle<WhenAllPromise>::from_promise(*this);
    }
    DeleStrategy final_suspend() noexcept {
        return {_mainCoroutine};
    }
    void return_value(std::coroutine_handle<> res) noexcept {
        _mainCoroutine = res;
    }
    void unhandled_exception() noexcept {
        std::terminate();
    }
    std::coroutine_handle<> _mainCoroutine;
};

struct WhenAllAwaiter {
    constexpr bool await_ready() const noexcept { return false; }
    constexpr auto await_suspend(std::coroutine_handle<> coroutine) const noexcept {
        block.previous = coroutine;
        for (auto const& co : cos.subspan(0, cos.size() - 1)) {
            static_cast<std::coroutine_handle<>>(co).resume();
        }
        return static_cast<std::coroutine_handle<>>(cos.back());
    }
    constexpr void await_resume() const noexcept {}

    std::span<Task<std::coroutine_handle<>, WhenAllPromise> const> cos;
    WhenAllCtlBlock& block;
};

template <std::size_t Idx, AwaitableLike T, typename Res>
Task<std::coroutine_handle<>, WhenAllPromise> allStrat(
    T&& t, Res& res, WhenAllCtlBlock& ctlBlock
) {
    if constexpr (Awaiter<T>) {
        if constexpr (std::is_void_v<decltype(t.await_resume())>) {
            static_cast<void>(co_await t);
        } else {
            std::get<Idx>(res) = co_await t;
        }
    } else if constexpr (Awaitable<T>) {
        if constexpr (std::is_void_v<decltype(t.operator co_await().await_resume())>) {
            static_cast<void>(co_await t);
        } else {
            std::get<Idx>(res) = co_await t;
        }
    } else {
        static_assert(!sizeof(T), "The type is not Awaiter");
    }
    if (--ctlBlock.cnt) {
        co_return std::noop_coroutine();
    }
    co_return ctlBlock.previous; // 全部任务都完成了, 返回到 co_await WhenAllAwaiter{cos, block}; 处
}

template <
    std::size_t... Idx,
    AwaitableLike... Ts,
    typename Res = std::tuple<container::NonVoidType<AwaiterReturnType<Ts>>...>
>
Task<Res> whenAll(std::index_sequence<Idx...>, Ts&&... ts) {
    // 1. 定义返回值
    Res res{};

    // 2. 记录总协程数量
    WhenAllCtlBlock block;
    block.cnt = sizeof...(Ts);

    // 3. 把每个协程外面再套一层协程
    std::array<Task<std::coroutine_handle<>, WhenAllPromise>, sizeof...(Ts)> cos {
        allStrat<Idx>(std::forward<Ts>(ts), res, block)...
    };

    // 4. 启动协程
    co_await WhenAllAwaiter{cos, block};

    // 5. 返回
    co_return res;
}


} // namespace internal

template <AwaitableLike... Ts>
auto whenAll(Ts&&... ts) {
    return internal::whenAll(
        std::make_index_sequence<sizeof...(Ts)>{},
        std::forward<Ts>(ts)...
    );
}

namespace internal {

template <AwaitableLike... Ts>
struct [[nodiscard]] WheyAllWrap {
private:
    static constexpr auto makeWhenAll(std::tuple<Ts...>&& tp) noexcept {
        return [&] <std::size_t... Idx> (std::index_sequence<Idx...>) constexpr {
            return whenAll(std::move(std::get<Idx>(tp))...);
        } (std::make_index_sequence<sizeof...(Ts)>{});
    }
public:
    struct WheyWrapAwaiter {
        WheyWrapAwaiter(std::tuple<Ts...>&& tp)
            : _whenAll{makeWhenAll(std::move(tp))}
        {}
        constexpr bool await_ready() const noexcept { return false; }
        constexpr std::coroutine_handle<> await_suspend(std::coroutine_handle<> coroutine) const noexcept {
            _whenAll.getPromise().promise()._previous = coroutine;  // 当 _whenAll 执行完, 恢复到 coroutine
                                                                    // 原理同 ExitAwaiter
            return _whenAll;
        }
        constexpr auto await_resume() const noexcept {
            return _whenAll.getPromise().promise().result();
        }
        decltype(
            WheyAllWrap<Ts...>::makeWhenAll(std::declval<std::tuple<Ts...>>())
        ) _whenAll;
    };

    constexpr WheyAllWrap(Ts&&... ts) 
        : _tp{std::move(ts)...}
    {}
    
    [[nodiscard]] constexpr auto operator co_await() && noexcept {
        return WheyWrapAwaiter{std::move(_tp)};
    }
private:
    std::tuple<Ts...> _tp;

    template <typename... Us, AwaitableLike U>
    friend constexpr WheyAllWrap<Us..., U> operator&&(WheyAllWrap<Us...>&& ts, U&& u) noexcept;

    template <typename... Us, AwaitableLike U>
    friend constexpr WheyAllWrap<U, Us...> operator&&(U&& u, WheyAllWrap<Us...>&& ts) noexcept;

    template <typename... Us, AwaitableLike... Es>
    friend constexpr WheyAllWrap<Us..., Es...> operator&&(WheyAllWrap<Us...>&& ts, WheyAllWrap<Es...>&& us) noexcept;
};

template <typename... Ts, AwaitableLike U>
[[nodiscard]] constexpr WheyAllWrap<Ts..., U> operator&&(WheyAllWrap<Ts...>&& ts, U&& u) noexcept {
    return [&] <std::size_t... Idx> (std::index_sequence<Idx...>) constexpr -> WheyAllWrap<Ts..., U> {
        return {std::move(std::get<Idx>(ts._tp))..., std::move(u)};
    } (std::make_index_sequence<sizeof...(Ts)>{});
}

template <typename... Ts, AwaitableLike U>
[[nodiscard]] constexpr WheyAllWrap<U, Ts...> operator&&(U&& u, WheyAllWrap<Ts...>&& ts) noexcept {
    return [&] <std::size_t... Idx> (std::index_sequence<Idx...>) constexpr -> WheyAllWrap<U, Ts...> {
        return {std::move(u), std::move(std::get<Idx>(ts._tp))...};
    } (std::make_index_sequence<sizeof...(Ts)>{});
}

template <typename... Ts, AwaitableLike... Us>
[[nodiscard]] constexpr WheyAllWrap<Ts..., Us...> operator&&(WheyAllWrap<Ts...>&& ts, WheyAllWrap<Us...>&& us) noexcept {
    return [&] <std::size_t... I, std::size_t... J> (
        std::index_sequence<I...>, std::index_sequence<J...>
    ) constexpr -> WheyAllWrap<Ts..., Us...> {
        return {std::move(std::get<I>(ts._tp))..., std::move(std::get<J>(us._tp))...};
    } (std::make_index_sequence<sizeof...(Ts)>{}, std::make_index_sequence<sizeof...(Us)>{});
}

} // namespace internal

template <AwaitableLike T, AwaitableLike U>
[[nodiscard]] constexpr internal::WheyAllWrap<T, U> operator&&(T&& t, U&& u) {
    return {std::move(t), std::move(u)};
}

} // namespace HX::coroutine