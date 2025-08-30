#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-01-08 21:38:31
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
#include <HXLibs/reflection/ReflectionTypes.hpp>

namespace HX::reflection {

namespace internal {

struct Any {
    /**
     * @brief 万能类型转换运算符, 只用于模版判断
     */
    template <typename T>
    operator T();
};

struct AnyOpt {
    template <typename T>
        requires(requires(T t) {
            typename T::value_type;
            t.has_value();
            t.value();
            t.operator*();
        })
    operator T();
};

struct AnyPtr {
    operator std::nullptr_t(); // 智能指针 / GCC std::string_view 都匹配这个
};

template <typename T, typename... Args>
inline constexpr std::size_t membersCountImpl() {
    if constexpr (meta::IsConstructibleVal<T, Any, Args...>) {
        return membersCountImpl<T, Args..., Any>();
    } else if constexpr (meta::IsConstructibleVal<T, AnyOpt, Args...>) {
        return membersCountImpl<T, Args..., AnyOpt>();
    } else if constexpr (meta::IsConstructibleVal<T, AnyPtr, Args...>) {
        return membersCountImpl<T, Args..., AnyPtr>();
    } else {
        return sizeof...(Args);
    }
}

/**
 * @brief 获取`聚合类`的成员变量个数
 * @tparam T `聚合类`类型
 * @tparam Args 占位
 * @return consteval 
 */
template <typename T, typename... Args>
inline consteval std::size_t membersCount() {
    if constexpr (std::is_aggregate_v<T>) {
        return membersCountImpl<T>();
    } else if constexpr (reflection::HasInsideReflection<T>) {
        return T::membersCount();
    } else if constexpr (reflection::HasOutReflection<T>) {
        return _membersCount(std::declval<T>());
    }else {
        // 暂时不支持反射成员个数
        static_assert(!sizeof(T),
            "reflection member count is not currently supported");
    }
}

} // namespace internal

/**
 * @brief 获取`聚合类`的成员变量个数
 * @tparam T 需要计算成员变量个数的`聚合类`类型
 * @return std::size_t 成员变量个数
 */
template <typename T>
inline consteval std::size_t membersCount() {
    return internal::membersCount<meta::remove_cvref_t<T>>();
}

/**
 * @brief 计算成员变量个数, 并且作为全局(编译期)常量
 * @tparam T 需要计算成员变量个数的`聚合类`类型
 */
template <typename T>
constexpr std::size_t membersCountVal = membersCount<T>();

} // namespace HX::reflection

