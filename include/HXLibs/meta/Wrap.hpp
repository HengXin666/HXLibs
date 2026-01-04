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
struct ToTypeWrap {
    inline static constexpr auto Datas = std::make_tuple(Vs...);
};

template <typename T>
constexpr bool IsTypeWrapVal = false;

template <auto... Vs>
constexpr bool IsTypeWrapVal<ToTypeWrap<Vs...>> = true;

/**
 * @brief 类型包装
 */
template <typename... Ts>
struct Wrap {};

template <typename T>
constexpr std::size_t WrapSize = 0;

template <typename... Ts>
constexpr std::size_t WrapSize<Wrap<Ts...>> = sizeof...(Ts);

namespace internal {

template <typename T1, typename T2>
struct ConcatWrap;

template <typename... As, typename... Bs>
struct ConcatWrap<Wrap<As...>, Wrap<Bs...>> {
    using Type = Wrap<As..., Bs...>;
};

template <typename T>
struct FlattenWrap {
    using Type = Wrap<T>;
};

template <typename... Ts>
struct FlattenWrap<Wrap<Ts...>> {
    template <typename... Us>
    struct Helper;

    template <typename First, typename... Rest>
    struct Helper<First, Rest...> {
        using Type = typename ConcatWrap<
            typename FlattenWrap<First>::Type,
            typename Helper<Rest...>::Type
        >::Type;
    };

    template <typename Last>
    struct Helper<Last> {
        using Type = typename FlattenWrap<Last>::Type;
    };

    template <>
    struct Helper<> {
        using Type = Wrap<>;
    };

    using Type = typename Helper<Ts...>::Type;
};

} // namespace internal

template <typename... Ts>
using JoinWrapType = internal::FlattenWrap<Wrap<Ts...>>::Type;

/**
 * @brief 筛选满足 Func 条件的 Ts 类型, 返回 std::tuple<Us...>, 其中 Us 均满足 Func
 * @warning 要求 Us 为非 void 类型
 * @note Func 应该是 类型萃取 类模板, 要求其存在 Val 静态成员变量
 */
template <template <typename> typename Bool, typename... Ts>
    requires (!std::is_void_v<Ts> && ...)
using ScreenTypes = Wrap<decltype([] <typename T> (T) {
    return std::conditional_t<Bool<T>::Val, T, void>{};
} (std::declval<Ts>()))...>;

} // namespace HX::meta