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
#ifndef _HX_MEMBER_COUNT_H_
#define _HX_MEMBER_COUNT_H_

#include <HXLibs/utils/TypeTraits.hpp>

namespace HX::reflection {

namespace internal {

struct Any {
    /**
     * @brief 万能类型转换运算符, 只用于模版判断
     */
    template <typename T>
    operator T();
};

/**
 * @brief 获取`聚合类`的成员变量个数
 * @tparam T `聚合类`类型
 * @tparam Args 占位
 * @return consteval 
 */
template <typename T, typename... Args>
inline consteval std::size_t membersCount() {
    /**
     * T { Args{}..., internal::Any{} } 是逐个直接初始化参数, 构造成类似于:
     *   T{ Any{}, Any{}, Any{}, Any{} }, 这依赖于 T 的构造函数或聚合顺序。
     *   若 T 不是聚合类型, 或成员不能隐式接收 Any, 会导致构造失败。
     *
     * T { {Args{}}..., {internal::Any{}} } 是通过列表初始化各成员, 构造成:
     *   T{ U1{Any{}}, U2{Any{}}, U3{Any{}}, U4{Any{}} }, 此形式触发聚合初始化的每个成员, 
     *   并允许 Any 隐式转换为成员类型, 更稳健地支持结构体成员推导。
     */
    if constexpr (requires {
        T { {Args{}}..., {internal::Any{}} }; // 列表初始化
    }) {
        return membersCount<T, Args..., internal::Any>();
    } else {
        return sizeof...(Args);
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
    return internal::membersCount<utils::remove_cvref_t<T>>();
}

/**
 * @brief 计算成员变量个数, 并且作为全局(编译期)常量
 * @tparam T 需要计算成员变量个数的`聚合类`类型
 */
template <typename T>
constexpr std::size_t membersCountVal = membersCount<T>();

} // namespace HX::reflection

#endif // !_HX_MEMBER_COUNT_H_