#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2026-01-01 23:19:30
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

#include <HXLibs/meta/MemberPtrType.hpp>
#include <HXLibs/meta/FixedString.hpp>
#include <HXLibs/meta/TypeTraits.hpp>
#include <HXLibs/meta/ContainerConcepts.hpp>
#include <HXLibs/meta/Wrap.hpp>
 
#include <HXLibs/db/sql/Operator.hpp>
#include <HXLibs/db/sql/Constraint.hpp>
#include <HXLibs/db/sql/OrderType.hpp>

namespace HX::db {

template <typename... Ts>
struct Expression;

template <typename MemberPtrType, meta::FixedString AsName = "">
struct Col {
    MemberPtrType _ptr;
    decltype(AsName) _asName{AsName};

    // 必须为成员指针
    static_assert(meta::IsMemberPtrVal<MemberPtrType>, "Must be a member pointer");

    // 别名长度必须大于0
    static_assert(AsName.size() > 0, "Alias length must be greater than 0");

    using Table = meta::GetMemberPtrClassType<MemberPtrType>;
    using Type = RemoveConstraintToType<meta::GetMemberPtrType<MemberPtrType>>;

    template <auto... Vs>
    constexpr Expression<Col, decltype(op::In), meta::ToTypeWrap<Vs...>> in() && noexcept {
        return {std::move(*this), op::In, meta::ToTypeWrap<Vs...>{}};
    }

    template <auto... Vs>
    constexpr Expression<Col, decltype(op::NotIn), meta::ToTypeWrap<Vs...>> notIn() && noexcept {
        return {std::move(*this), op::NotIn, meta::ToTypeWrap<Vs...>{}};
    }

    constexpr Expression<Col, decltype(op::IsNull)> isNull() && noexcept requires(meta::IsOptionalVal<Type>) {
        return {std::move(*this), op::IsNull};
    }

    constexpr Expression<Col, decltype(op::IsNotNull)> isNotNull() && noexcept requires(meta::IsOptionalVal<Type>) {
        return {std::move(*this), op::IsNotNull};
    }

    template <meta::FixedString V>
    constexpr Expression<Col, decltype(op::Like), meta::ToTypeWrap<V>> like() && noexcept requires(meta::StringType<Type>) {
        return {std::move(*this), op::Like, meta::ToTypeWrap<V>{}};
    }

    template <meta::FixedString V>
    constexpr Expression<Col, decltype(op::NotLike), meta::ToTypeWrap<V>> notLike() && noexcept requires(meta::StringType<Type>) {
        return {std::move(*this), op::NotLike, meta::ToTypeWrap<V>{}};
    }

    constexpr OrderType<MemberPtrType, op::Asc> asc() && noexcept {
        return {_ptr};
    }

    constexpr OrderType<MemberPtrType, op::Desc> desc() && noexcept {
        return {_ptr};
    }
};

template <typename MemberPtrType>
struct Col<MemberPtrType, ""> {
    MemberPtrType _ptr;

    // 必须为成员指针
    static_assert(meta::IsMemberPtrVal<MemberPtrType>, "Must be a member pointer");

    using Table = meta::GetMemberPtrClassType<MemberPtrType>;
    using Type = RemoveConstraintToType<meta::GetMemberPtrType<MemberPtrType>>;

    template <meta::FixedString AsName>
        requires (AsName.size() > 0)
    constexpr Col<MemberPtrType, AsName> as() && noexcept {
        return {_ptr};
    }

    template <auto... Vs>
    constexpr Expression<Col, decltype(op::In), meta::ToTypeWrap<Vs...>> in() && noexcept {
        return {std::move(*this), op::In, meta::ToTypeWrap<Vs...>{}};
    }

    template <auto... Vs>
    constexpr Expression<Col, decltype(op::NotIn), meta::ToTypeWrap<Vs...>> notIn() && noexcept {
        return {std::move(*this), op::NotIn, meta::ToTypeWrap<Vs...>{}};
    }

    constexpr Expression<Col, decltype(op::IsNull)> isNull() && noexcept requires(meta::IsOptionalVal<Type>) {
        return {std::move(*this), op::IsNull};
    }

    constexpr Expression<Col, decltype(op::IsNotNull)> isNotNull() && noexcept requires(meta::IsOptionalVal<Type>) {
        return {std::move(*this), op::IsNotNull};
    }

    template <meta::FixedString V>
    constexpr Expression<Col, decltype(op::Like), meta::ToTypeWrap<V>> like() && noexcept requires(meta::StringType<Type>) {
        return {std::move(*this), op::Like, meta::ToTypeWrap<V>{}};
    }

    template <meta::FixedString V>
    constexpr Expression<Col, decltype(op::NotLike), meta::ToTypeWrap<V>> notLike() && noexcept requires(meta::StringType<Type>) {
        return {std::move(*this), op::NotLike, meta::ToTypeWrap<V>{}};
    }

    constexpr OrderType<MemberPtrType, op::Asc> asc() && noexcept {
        return {_ptr};
    }

    constexpr OrderType<MemberPtrType, op::Desc> desc() && noexcept {
        return {_ptr};
    }
};

template <typename T>
constexpr bool IsColTypeVal = false;

template <typename MemberPtr, meta::FixedString AsName>
constexpr bool IsColTypeVal<Col<MemberPtr, AsName>> = true;

template <typename T>
concept ColType = IsColTypeVal<T>;

} // namespace HX::db