#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-12-05 21:53:52
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

#include <HXLibs/meta/TypeTraits.hpp>
#include <HXLibs/meta/MemberPtrType.hpp>

namespace HX::db {

namespace attr {

// 非空约束
struct NotNull {};

// 主键约束
struct PrimaryKey {};

// 唯一约束
struct Unique {};

// 自增约束
struct AutoIncrement {};

// 索引
struct Index {};

// 外键约束, 多个时候, 为联合外键
template <auto... Keys>
struct Foreign {
    static_assert((meta::IsMemberPtrVal<decltype(Keys)> && ...), "Is Not Member Ptr Val");
    using ForeignKey = decltype(std::make_tuple(Keys...));
};

// 默认值约束
template <auto Value>
struct DefaultVal {
    inline static constexpr auto Val = Value;
};

/**
 * @brief 判断是否是外键约束类型
 * @tparam T 
 */
template <typename T>
constexpr bool IsForeignVal = false;

template <auto... Keys>
constexpr bool IsForeignVal<Foreign<Keys...>> = true;

/**
 * @brief 判断是否是默认值约束类型
 * @tparam T 
 */
template <typename T>
constexpr bool IsDefaultValTypeVal = false;

template <auto Value>
constexpr bool IsDefaultValTypeVal<DefaultVal<Value>> = true;


} // namespace attr

template <typename T, typename... ConstraintTypes>
struct Constraint {
    // 没有使用约束, 不应该使用 Constraint
    static_assert(sizeof...(ConstraintTypes) > 0,
        "Constraint should not be used because no constraint is used");

    // ConstraintType 类型不能重复
    static_assert(meta::IsAllUniqueVal<ConstraintTypes...>, "Constraint Type Repeat");

    using Type = T;
    using Constraints = std::tuple<ConstraintTypes...>;
    
    T val;

    operator T&() noexcept { return val; }
    operator T const&() const noexcept { return val; }
};

/**
 * @brief 判断是否是约束包类型
 * @tparam T 
 */
template <typename T>
constexpr bool IsConstraintVal = false;

template <typename T, typename... ConstraintTypes>
constexpr bool IsConstraintVal<Constraint<T, ConstraintTypes...>> = true;

} // namespace HX::db