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

#include <HXLibs/log/serialize/CustomToString.hpp>
#include <HXLibs/meta/TypeTraits.hpp>
#include <HXLibs/meta/MemberPtrType.hpp>
#include <HXLibs/meta/Wrap.hpp>
#include <HXLibs/reflection/MemberName.hpp>

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

    constexpr IndexOrder() = default;
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

/**
 * @brief 获取 IndexOrder 的 KeyType
 * @warning 必须保证 IndexOrder 的类型是 IndexOrder, 否则程序非良构
 */
template <typename IndexOrder>
using GetIndexOrderKeyType = typename IndexOrder::KeyType;

} // namespace internal

// 非空约束
struct NotNull {};

// 联合主键约束
template <auto... KeyPtrs>
    requires (meta::IsMemberPtrVal<decltype(KeyPtrs)> && ...)
struct PrimaryKey {
    constexpr PrimaryKey() = default;
};

// 单主键约束
template <>
struct PrimaryKey<> {
    constexpr PrimaryKey() = default;
};

// 唯一约束
struct Unique {};

// 自增约束
struct AutoIncrement {};

// 索引, 多参数则表示为聚合索引, 使用于 struct <Table>::IndexAttr 中
template <typename... Keys>
    requires (internal::IsIndexOrderVal<Keys> && ...)
struct Index {
    // 无法在此处要求 Index 都是来自同一个类的, 因为类型不完整 
    // 空类需要明确书写构造函数, 否则无法反射
    constexpr Index() = default;
};

// 可指定排序顺序的单索引, 仅用于 Constraint 中.
using AseIndex = Index<IndexOrder<&internal::Place::place, Order::Asc>>;
using DescIndex = Index<IndexOrder<&internal::Place::place, Order::Desc>>;

// 单索引, 仅用于 Constraint 中.
template <>
struct Index<> {
    constexpr Index() = default;
};

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
    constexpr UnionForeign() = default;
};

// 默认值约束
template <auto Value>
struct DefaultVal {};

/**
 * @brief 判断是否是主键类型
 */
template <typename T>
constexpr bool IsPrimaryKeyVal = false;

template <auto... KeyPtrs>
constexpr bool IsPrimaryKeyVal<PrimaryKey<KeyPtrs...>> = true;

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

    constexpr operator T&() noexcept { return get(); }
    constexpr operator T const&() const noexcept { return get(); }

    constexpr T& get() & noexcept { return val; }
    constexpr T const& get() const & noexcept { return val; }
    constexpr T&& get() && noexcept { return std::move(val); }
};

// 定义常用的属性组合

/// @brief 普通主键
template <typename T>
using PrimaryKey = Constraint<T, attr::PrimaryKey<>>;

/// @brief 非空主键
template <typename T>
using NotNullPrimaryKey = Constraint<T, attr::PrimaryKey<>, attr::NotNull>;

/// @brief 自增非空主键
template <typename T>
using AutoIncrementPrimaryKey = Constraint<T, attr::PrimaryKey<>, attr::AutoIncrement>;

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

template <typename Table>
constexpr auto getTablePrimaryKeyIdxImpl() noexcept {
    std::size_t idx{};
    reflection::forEach(Table{}, [&] <std::size_t Idx, typename T> (
        std::index_sequence<Idx>, auto, T v
    ) {
        if constexpr (attr::IsPrimaryKeyVal<T>) {
            idx = Idx + 1;
        } else if constexpr (IsConstraintVal<T>) {
            [&] <typename... Attr> (Constraint<typename T::Type, Attr...>) {
                 idx = (attr::IsPrimaryKeyVal<Attr> || ...) ? Idx + 1 : idx;
            }(v);
        }
    });
    return idx;
}

template <auto Ptr, typename Table>
constexpr std::size_t getTablePrimaryKeyIdxTool() noexcept {
    std::size_t res{};
    constexpr auto N = reflection::membersCountVal<Table>;
    constexpr auto& t = reflection::internal::getStaticObj<Table>();
    constexpr auto tp = reflection::internal::getStaticObjPtrTuple<Table>();
    [&] <std::size_t... Idx> (std::index_sequence<Idx...>) constexpr {
        ([&]() constexpr {
            using T1 = meta::RemoveCvRefType<decltype(&(t.*Ptr))>;
            using T2 = meta::RemoveCvRefType<decltype(std::get<Idx>(tp))>;
            if constexpr (std::is_same_v<T1, T2>) {
                if constexpr (&(t.*Ptr) == std::get<Idx>(tp)) {
                    res = Idx;
                }
            }
        }(), ...);
    }(std::make_index_sequence<N>{});
    return res;
}

template <typename Table>
constexpr auto getTablePrimaryKeyIdx() noexcept {
    if constexpr (constexpr auto Idx = getTablePrimaryKeyIdxImpl<Table>(); Idx) {
        // 1. 如果类中有定义
        return meta::ValueWrap<Idx - 1>{};
    } else if constexpr (attr::HasUnionAttr<Table>) {
        // 2. 没有, 则看复合属性
        if constexpr (constexpr auto Idx = getTablePrimaryKeyIdxImpl<typename Table::UnionAttr>(); Idx) {
            constexpr auto Ptr = std::get<Idx - 1>(
                reflection::internal::getStaticObjPtrTuple<typename Table::UnionAttr>());
            using PtrType = meta::RemoveCvRefType<decltype(*Ptr)>;
            return [] <auto... Ptrs> (attr::PrimaryKey<Ptrs...>) constexpr {
                return meta::ValueWrap<getTablePrimaryKeyIdxTool<Ptrs, Table>()...>{};
            }(PtrType{});
        } else {
            // 复合属性也没有定义复合主键
            return meta::ValueWrap<>{};
        }
    } else { // 3. 没有主键
        return meta::ValueWrap<>{};
    }
}

} // namespace internal

/**
 * @brief 去掉 Constraint, 获取到 T
 */
template <typename T>
using RemoveConstraintToType = typename internal::RemoveConstraintToTypeImpl<T>::Type;

template <typename Table>
using GetTablePrimaryKeyIdxType = decltype(internal::getTablePrimaryKeyIdx<Table>());

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

namespace HX::log {

template <typename T, typename... Ts, typename FormatType>
struct CustomToString<db::Constraint<T, Ts...>, FormatType> {
    FormatType* self;
    
    constexpr std::string make(db::Constraint<T, Ts...> const& t) {
        return self->make(t.get());
    }

    template <typename Stream>
    constexpr void make(db::Constraint<T, Ts...> const& t, Stream& s) {
        self->make(t.get(), s);
    }
};

} // namespace HX::log