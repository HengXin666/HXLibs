#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-01-17 22:14:08
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
#ifndef _HX_TYPE_TRAITS_H_
#define _HX_TYPE_TRAITS_H_

#include <variant>
#include <optional>
#include <memory>
#include <type_traits>

namespace HX::meta {

/**
 * @brief 删除 T 类型的 const、引用、v 修饰
 * @tparam T 
 */
template <typename T>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

/**
 * @brief 判断`variant<Ts...>`中是否含有`T`类型
 * @tparam T 
 * @tparam Ts 
 */
template <typename T, typename... Ts>
constexpr bool has_variant_type_v = false;

template <typename T, typename... Ts>
constexpr bool has_variant_type_v<T, std::variant<Ts...>> = std::disjunction_v<std::is_same<T, Ts>...>;

/**
 * @brief 类型萃取: 是否为 optional 类型
 * @tparam T 
 */
template <typename T>
constexpr bool is_optional_v = false;

template <typename T>
constexpr bool is_optional_v<std::optional<T>> = true;

/**
 * @brief 类型萃取: 是否为 智能指针 类型
 * @tparam T 
 */
template <typename T>
concept is_smart_pointer_v = std::same_as<T, std::unique_ptr<typename T::element_type>>
                          || std::same_as<T, std::shared_ptr<typename T::element_type>>
                          || std::same_as<T, std::weak_ptr<typename T::element_type>>;

/**
 * @brief 类型萃取: 是否为 std::array 类型
 * @tparam T 
 */
template <typename T>
constexpr bool is_std_array_v = false;

template <typename T, std::size_t N>
constexpr bool is_std_array_v<std::array<T, N>> = true;

/**
 * @brief 概念: 是否为 span 类型 (支持 subspan 方法的)
 * @tparam T 
 */
template <typename T>
concept is_span_v = requires (T&& t) {
    t.subspan(0);
};

/**
 * @brief 判断 T 是否可以从 Ts 中被唯一构造
 * @tparam T 
 * @tparam Ts 
 * @return consteval std::size_t 如果可以从 Ts 中唯一构造, 返回 Ts 元素的索引
 *                               如果无法从 Ts 中唯一构造, 返回 sizeof...(Ts)
 */
template <typename T, typename... Ts>
consteval std::size_t findUniqueConstructibleIndex() noexcept {
    constexpr std::size_t N = sizeof...(Ts);
    std::size_t res = N;
    std::size_t i = 0, cnt = 0;
    ([&]() {
        // 没有 requires 可以使用 std::is_constructible_v<Ts, T&&> 代替
        if (requires (T&& t) {
            Ts{std::forward<T>(t)};
        }) {
            res = i;
            ++cnt;
        }
        ++i;
    }(), ...);
    if (cnt != 1) {
        // 无法从 U 构造到 T, 可能有0个或者多个匹配
        return N;
    }
    return res;
}

/**
 * @brief T 可以从 {Args..., U} 中被构造
 * @tparam T 
 * @tparam U 
 * @tparam Args 
 */
template <typename T, typename U, typename... Args>
constexpr bool isConstructible = requires {
    T {{Args{}}..., {U{}}};
};

} // namespace HX::meta

#endif // !_HX_TYPE_TRAITS_H_