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
#include <HXLibs/meta/ValToTypeWrap.hpp>
 
#include <HXLibs/db/sql/Operator.hpp>
#include <HXLibs/db/sql/Constraint.hpp>

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
    constexpr Expression<Col, op::In, meta::ToTypeWrap<Vs...>> in() && noexcept {
        return {};
    }

    template <auto... Vs>
    constexpr Expression<Col, op::NotIn, meta::ToTypeWrap<Vs...>> notIn() && noexcept {
        return {};
    }

    constexpr Expression<Col, op::IsNull> isNull() && noexcept requires(meta::IsOptionalVal<Type>) {
        return {};
    }

    constexpr Expression<Col, op::IsNotNull> isNotNull() && noexcept requires(meta::IsOptionalVal<Type>) {
        return {};
    }

    template <meta::FixedString V>
    constexpr Expression<Col, op::Like, meta::ToTypeWrap<V>> like() && noexcept requires(meta::StringType<Type>) {
        return {};
    }

    template <meta::FixedString V>
    constexpr Expression<Col, op::NotLike, meta::ToTypeWrap<V>> notLike() && noexcept requires(meta::StringType<Type>) {
        return {};
    }

    constexpr Expression<Col, op::Asc> asc() && noexcept {
        return {};
    }

    constexpr Expression<Col, op::Desc> desc() && noexcept {
        return {};
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
    constexpr Col<MemberPtrType, AsName> as() && noexcept {
        return {};
    }

    template <auto... Vs>
    constexpr Expression<Col, op::In, meta::ToTypeWrap<Vs...>> in() && noexcept {
        return {};
    }

    template <auto... Vs>
    constexpr Expression<Col, op::NotIn, meta::ToTypeWrap<Vs...>> notIn() && noexcept {
        return {};
    }

    constexpr Expression<Col, op::IsNull> isNull() && noexcept requires(meta::IsOptionalVal<Type>) {
        return {};
    }

    constexpr Expression<Col, op::IsNotNull> isNotNull() && noexcept requires(meta::IsOptionalVal<Type>) {
        return {};
    }

    template <meta::FixedString V>
    constexpr Expression<Col, op::Like, meta::ToTypeWrap<V>> like() && noexcept requires(meta::StringType<Type>) {
        return {};
    }

    template <meta::FixedString V>
    constexpr Expression<Col, op::NotLike, meta::ToTypeWrap<V>> notLike() && noexcept requires(meta::StringType<Type>) {
        return {};
    }

    constexpr Expression<op::Not, Col> operator!() && noexcept {
        return {};
    }

    constexpr Expression<Col, op::Asc> asc() && noexcept {
        return {};
    }

    constexpr Expression<Col, op::Desc> desc() && noexcept {
        return {};
    }
};

template <typename T>
constexpr bool IsColTypeVal = false;

template <typename MemberPtr, meta::FixedString AsName>
constexpr bool IsColTypeVal<Col<MemberPtr, AsName>> = true;

template <typename T>
concept ColType = IsColTypeVal<T>;

} // namespace HX::db