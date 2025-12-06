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

} // namespace attr

template <typename T, typename... ConstraintType>
struct Constraint {
    using Type = T;
    using Constraints = std::tuple<ConstraintType...>;
    
    T val;

    operator T&() noexcept { return val; }
    operator T const&() const noexcept { return val; }
};

} // namespace HX::db