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

enum class Order {
    Asc,    // 升序
    Desc    // 降序
};

template <auto Key, Order SortOrder = Order::Asc>
struct IndexOrder {
    static_assert(meta::IsMemberPtrVal<decltype(Key)>, "Is Not Member Ptr Val");
    using KeyType = decltype(Key);
    inline static constexpr Order IdxOrder = SortOrder;
};

template <auto InternalKeyPtr, auto ForeignKeyPtr>
struct ForeignMap;

namespace internal {

// 占位
struct Place {
    Place const* place;
};

/**
 * @brief 判断是否是 IndexOrder 类型
 */
template <typename T>
constexpr bool IsIndexOrderVal = false;

template <auto Key, Order SortOrder>
constexpr bool IsIndexOrderVal<IndexOrder<Key, SortOrder>> = true;

/**
 * @brief 判断是否是 ForeignMap 类型
 */
template <typename T>
constexpr bool IsForeignMapVal = false;

template <auto InternalKeyPtr, auto ForeignKeyPtr>
constexpr bool IsForeignMapVal<ForeignMap<InternalKeyPtr, ForeignKeyPtr>> = true;

} // namespace internal

// 非空约束
struct NotNull {};

// 主键约束
struct PrimaryKey {};

// 唯一约束
struct Unique {};

// 自增约束
struct AutoIncrement {};

// 索引, 多参数则表示为聚合索引, 使用于 struct <Table>::IndexAttr 中
template <typename... Keys>
    requires (internal::IsIndexOrderVal<Keys> && ...)
struct Index {};

// 可指定排序顺序的单索引, 仅用于 Constraint 中.
using AseIndex = Index<IndexOrder<&internal::Place::place, Order::Asc>>;
using DescIndex = Index<IndexOrder<&internal::Place::place, Order::Desc>>;

// 单索引, 仅用于 Constraint 中.
template <>
struct Index<> {};

// 单外键约束
template <auto KeyPtr>
struct Foreign {
    static_assert(meta::IsMemberPtrVal<decltype(KeyPtr)>, "Is Not Member Ptr Val");
};

// 联合外键约束, 使用于 UnionAttr 嵌套类中
template <typename... Keys>
struct UnionForeign {
    // 应该使用 外键映射 (ForeignMap<>) 包装
    static_assert((internal::IsForeignMapVal<Keys> && ...),
        "The ForeignMap<> wrapper should be used");
};

// 默认值约束
template <auto Value>
struct DefaultVal {
    // inline static constexpr auto Val = Value;
};

/**
 * @brief 判断是否是索引约束类型
 */
template <typename T>
constexpr bool IsIndexVal = false;

template <typename... Keys>
constexpr bool IsIndexVal<Index<Keys...>> = true;

/**
 * @brief 判断是否是单外键约束类型
 */
template <typename T>
constexpr bool IsForeignVal = false;

template <auto KeyPtr>
constexpr bool IsForeignVal<Foreign<KeyPtr>> = true;

/**
 * @brief 判断是否是联合外键约束类型
 */
template <typename T>
constexpr bool IsUnionForeignVal = false;

template <typename... Keys>
constexpr bool IsUnionForeignVal<UnionForeign<Keys...>> = true;

/**
 * @brief 判断是否是默认值约束类型
 */
template <typename T>
constexpr bool IsDefaultValTypeVal = false;

template <auto Value>
constexpr bool IsDefaultValTypeVal<DefaultVal<Value>> = true;

/**
 * @brief 判断是否存在嵌套类 UnionAttr
 */
template <typename T>
constexpr bool HasUnionAttr = requires { typename T::UnionAttr; };

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
 */
template <typename T>
constexpr bool IsConstraintVal = false;

template <typename T, typename... ConstraintTypes>
constexpr bool IsConstraintVal<Constraint<T, ConstraintTypes...>> = true;

namespace internal {

template <typename T>
struct RemoveConstraintToTypeImpl {
    using Type = T;
};

template <typename T, typename... ConstraintTypes>
struct RemoveConstraintToTypeImpl<Constraint<T, ConstraintTypes...>> {
    using Type = T;
};

} // namespace internal

/**
 * @brief 去掉 Constraint, 获取到 T
 */
template <typename T>
using RemoveConstraintToType = typename internal::RemoveConstraintToTypeImpl<T>::Type;

namespace attr {

template <auto InternalKeyPtr, auto ForeignKeyPtr>
struct ForeignMap {
    using InternalKey = decltype(InternalKeyPtr);
    using ForeignKey = decltype(ForeignKeyPtr);
    static_assert(meta::IsMemberPtrVal<InternalKey> && meta::IsMemberPtrVal<ForeignKey>,
        "Is Not Member Ptr Val");
    // 内外键类型得相同
    static_assert(std::is_same_v<
        RemoveConstraintToType<meta::GetMemberPtrType<InternalKey>>,
        RemoveConstraintToType<meta::GetMemberPtrType<ForeignKey>>>, 
        "Foreign key type is different from internal key");
};

} // namespace attr

} // namespace HX::db