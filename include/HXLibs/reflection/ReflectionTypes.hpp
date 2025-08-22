#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-25 11:23:37
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

namespace HX::reflection {

namespace internal {

template <typename T, typename = void>
struct HasInsideReflection : std::false_type {};

template <typename T>
struct HasInsideReflection<T, std::void_t<decltype(T::membersCount())>> 
    : std::true_type {}; 

template <typename T, typename = void>
struct HasOutReflection : std::false_type {};

template <typename T>
struct HasOutReflection<T, std::void_t<decltype(membersCount(std::declval<T>()))>> 
    : std::true_type {}; 

} // namespace internal

/**
 * @brief 是否在类的内部使用反射宏注册
 * @tparam T 
 */
template <typename T>
constexpr bool HasInsideReflection = internal::HasInsideReflection<meta::remove_cvref_t<T>>::value;

/**
 * @brief 是否在类的内部使用反射宏注册
 * @tparam T 
 */
template <typename T>
constexpr bool HasOutReflection = internal::HasOutReflection<meta::remove_cvref_t<T>>::value;

/**
 * @brief 是否为HXLibs可反射类型
 * @tparam T 
 */
template <typename T>
constexpr bool IsReflective = (std::is_aggregate_v<T> 
             && !std::is_same_v<T, std::monostate>
             && !meta::is_std_array_v<T>)
             || HasInsideReflection<T>
             || HasOutReflection<T>;

} // namespace HX::reflection

