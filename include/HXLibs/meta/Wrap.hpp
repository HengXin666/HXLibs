#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2026-01-02 13:02:25
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

#include <tuple>

namespace HX::meta {

/**
 * @brief 编译期常量转为类型包裹
 * @tparam Vs 
 */
template <auto... Vs>
struct ValueWrap {
    inline static constexpr auto Datas = std::make_tuple(Vs...);
};

template <typename T>
constexpr bool IsValueWrapVal = false;

template <auto... Vs>
constexpr bool IsValueWrapVal<ValueWrap<Vs...>> = true;

template <auto V, typename T>
constexpr bool NotInValueWrapVal = true;

/**
 * @brief 判断 V 是否不等于 Vs...
 * @tparam V 
 * @tparam Vs 
 */
template <auto V, auto... Vs>
constexpr bool NotInValueWrapVal<V, ValueWrap<Vs...>> = ((V != Vs) && ...);

namespace internal {

template <std::size_t Idx, typename T>
struct WrapHead {
    T static get() noexcept;
};

template <typename Idx, typename... Ts>
struct WrapImpl;

template <std::size_t... Idx, typename... Ts>
struct WrapImpl<std::index_sequence<Idx...>, Ts...> : internal::WrapHead<Idx, Ts>... {};

} // namespace internal

/**
 * @brief 类型包装
 */
template <typename... Ts>
struct Wrap : internal::WrapImpl<std::make_index_sequence<sizeof...(Ts)>, Ts...> {
    using Base = internal::WrapImpl<std::make_index_sequence<sizeof...(Ts)>, Ts...>;
    using Base::Base;
};

// 应该仅用于编译期判断
template <std::size_t Idx, typename T>
constexpr T get(internal::WrapHead<Idx, T> const&) noexcept {
    static_assert(!sizeof(T));
}

template <typename T>
constexpr std::size_t WrapSizeVal = 0;

template <typename... Ts>
constexpr std::size_t WrapSizeVal<Wrap<Ts...>> = sizeof...(Ts);

namespace internal {

template <typename T1, typename T2>
struct UnfoldWrap;

template <typename... As, typename... Bs>
struct UnfoldWrap<Wrap<As...>, Wrap<Bs...>> {
    using Type = Wrap<As..., Bs...>;
};

template <typename T>
struct FlattenWrapImpl {
    using Type = Wrap<T>;
};

template <typename... Us>
struct FlattenWrapHelper;

template <typename First, typename... Rest>
struct FlattenWrapHelper<First, Rest...> {
    // 此处递归
    // 把 First 展开为 Wrap<???>, 直到 FlattenWrapHelper<Last> (还可能触发展开) 最终即 Wrap<T>
    // 把 Rest... 展开为 Wrap<!!!>
    using Type = typename UnfoldWrap<
        typename FlattenWrapImpl<First>::Type,
        typename FlattenWrapHelper<Rest...>::Type
    >::Type;
};

template <typename Last>
struct FlattenWrapHelper<Last> {
    using Type = typename FlattenWrapImpl<Last>::Type;
};

template <>
struct FlattenWrapHelper<> {
    using Type = Wrap<>;
};

template <typename... Ts>
struct FlattenWrapImpl<Wrap<Ts...>> {
    using Type = typename FlattenWrapHelper<Ts...>::Type;
};

} // namespace internal

/**
 * @brief 展开 Wrap 包装: Wrap<Wrap<Wrap<A, B>, C>, D>, Wrap<<<E>>>> -> Wrap<A, B, C, D, E>
 */
template <typename... Ts>
using FlattenWrapType = internal::FlattenWrapImpl<Wrap<Ts...>>::Type;

#if 0
namespace internal {

template <typename V1, typename V2>
struct JoinValueWrap;

template <auto... Vs1, auto... Vs2>
struct JoinValueWrap<ValueWrap<Vs1...>, ValueWrap<Vs2...>> {
    inline static constexpr auto Val = ValueWrap<Vs1..., Vs2...>{};
};

template <typename T>
struct FlattenValueWrapTypeImpl;

template <auto V>
struct FlattenValueWrapTypeImpl<ValueWrap<V>> {
    inline static constexpr auto Val = V;
};

template <auto... Vs>
struct FlattenValueWrapTypeHelper;

template <auto V, auto... Vs>
struct FlattenValueWrapTypeHelper<V, Vs...> {
    inline static constexpr auto _V1 = FlattenValueWrapTypeHelper<V>::Val;
    inline static constexpr auto _V2 = FlattenValueWrapTypeImpl<ValueWrap<Vs...>>::Val;

    inline static constexpr auto Val = JoinValueWrap<
        std::remove_const_t<decltype(_V1)>,
        std::remove_const_t<decltype(_V2)>
    >::Val;
};

template <auto V>
struct FlattenValueWrapTypeHelper<V> {
    inline static constexpr auto Val = FlattenValueWrapTypeImpl<ValueWrap<V>>::Val;
};

template <>
struct FlattenValueWrapTypeHelper<> {
    inline static constexpr auto Val = ValueWrap<>{};
};

template <auto... Vs>
struct FlattenValueWrapTypeImpl<ValueWrap<Vs...>> {
    inline static constexpr auto Val = FlattenValueWrapTypeHelper<Vs...>::Val;
};

} // namespace internal

template <auto... Vs>
constexpr auto FlattenValueWrapType = internal::FlattenValueWrapTypeImpl<ValueWrap<Vs...>>::Val;

constexpr auto _1 = FlattenValueWrapType<ValueWrap<1, 2, 3, ValueWrap<1, ValueWrap<2233>{},2, 3>{}>{}, ValueWrap<1, 2, 3>{}>;
#endif // !0

/**
 * @brief 筛选满足 Func 条件的 Ts 类型, 返回 Wrap<Us...>, 其中 Us 均满足 Func
 * @warning 要求 Us 为非 Wrap 类型
 * @note Func 应该是 类型萃取 类模板, 要求其存在 Val 静态成员变量
 */
template <template <typename> typename Bool, typename... Ts>
    requires (!std::is_void_v<Ts> && ...)
using ScreenTypes = FlattenWrapType<decltype([] <typename T> (T) {
    return std::conditional_t<Bool<T>::Val, Wrap<T>, Wrap<>>{};
} (std::declval<Ts>()))...>;

} // namespace HX::meta