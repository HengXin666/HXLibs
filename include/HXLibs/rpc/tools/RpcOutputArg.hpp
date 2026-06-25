#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2026-06-24 14:22:06
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

#include <HXLibs/meta/TypeTraits.hpp>
#include <HXLibs/meta/Wrap.hpp>

namespace HX::rpc {

/**
 * @brief 判断是否为 RPC 传出参数, 仅限制其为非 const 的左值引用
 * @tparam T 
 */
template <typename T>
inline constexpr bool IsRpcOutputArg = false;

template <typename T>
inline constexpr bool IsRpcOutputArg<T&> = true;

template <typename T>
inline constexpr bool IsRpcOutputArg<T const&> = false;

template <typename T>
inline constexpr bool IsRpcOutputArg<T&&> = false;

namespace internal {

template <typename... Ts>
struct RpcOutputArgsImpl;

template <typename... Ts>
struct RpcOutputArgsImpl<meta::Wrap<Ts...>> {
    using ForwardTupleType = std::tuple<Ts...>;
    using TupleType = std::tuple<meta::RemoveCvRefType<Ts>...>;
};

template <typename... Args>
using RpcOutputArgs = RpcOutputArgsImpl<
    meta::FlattenWrapType<
        std::conditional_t<IsRpcOutputArg<Args>, Args, meta::Wrap<>>...
    >
>;

template <typename Idx, typename... Args>
struct MakeRpcOutputArgsIdx;

template <std::size_t... Idx, typename... Args>
struct MakeRpcOutputArgsIdx<std::index_sequence<Idx...>, Args...> {
    using ValueWrapsType = meta::FlattenWrapType<
        std::conditional_t<IsRpcOutputArg<Args>, meta::ValueWrap<Idx>, meta::Wrap<>>...
    >;

    template <std::size_t... I>
    static constexpr auto makeIdxSeq(
        meta::Wrap<meta::ValueWrap<I>...>
    ) noexcept -> std::index_sequence<I...> { return {}; }

    using Type = decltype(makeIdxSeq(ValueWrapsType{}));
};

} // namespace internal

/**
 * @brief 过滤 参数, 保留可作为 RPC 传出参数的类型, 提供 tuple<remove_rv_const_t<传出参数>...>
 * @tparam Args&& 函数参数
 */
template <typename... Args>
using RpcOutputArgsTuple = typename internal::RpcOutputArgs<Args...>::TupleType;

/**
 * @brief 过滤 参数, 保留可作为 RPC 传出参数的类型, 提供 tuple<传出参数&&...>
 * @tparam Args&& 函数参数
 */
template <typename... Args>
using RpcOutputArgsForwardTupleType = typename internal::RpcOutputArgs<Args...>::ForwardTupleType;

template <typename... Args>
using MakeRpcOutputArgsIdx = typename internal::MakeRpcOutputArgsIdx<
    std::make_index_sequence<sizeof...(Args)>, Args...>::Type;

// @todo, tmp test
static_assert(std::same_as<RpcOutputArgsTuple<int, int&>, std::tuple<int>>);

static_assert(std::same_as<
    MakeRpcOutputArgsIdx<int, int&, int, int&&, int const&, int&>,
    std::index_sequence<1u, 5u>
>);

} // namespace HX::rpc
