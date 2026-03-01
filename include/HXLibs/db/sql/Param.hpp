#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2026-01-03 11:18:28
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

#include <HXLibs/db/sql/Column.hpp>

namespace HX::db {

namespace internal {

template <typename>
struct Param {};

} // namespace internal

template <typename T, meta::FixedString AsName>
struct Col<internal::Param<T>, AsName> {
    using Table = void; // 表示不能放在 select, 仅可作为表达式
    using Type = T;
    decltype(AsName) _asName{AsName};
};

template <typename T>
struct Col<internal::Param<T>, ""> {
    using Table = void; // 表示不能放在 select, 仅可作为表达式
    using Type = T;

    template <meta::FixedString AsName>
        requires (AsName.size() > 0)
    constexpr Col<internal::Param<T>, AsName> bind() const noexcept {
        return {};
    }
};

template <typename T>
inline constexpr Col<internal::Param<T>, ""> param{};

template <typename T>
constexpr bool IsParamVal = false;

template <typename T, meta::FixedString AsName>
constexpr bool IsParamVal<Col<internal::Param<T>, AsName>> = true;

namespace internal {

template <meta::FixedString BindName, typename T = void>
struct Bind {
    // using Type = T&&;
    decltype(BindName) _bindName{BindName};
    T data{};

    constexpr Bind() = default;

    template <typename U>
        requires (std::convertible_to<U, T>)
    constexpr Bind(U&& u) : data(std::forward<U>(u)) {}
};

template <meta::FixedString BindName>
struct Bind<BindName, void> {
    template <typename T>
    Bind<BindName, T> link(T&& t) const noexcept {
        return Bind<BindName, T>(std::forward<T>(t));
    }
};

} // namespace internal

template <meta::FixedString BindName>
inline constexpr internal::Bind<BindName> bind{};

template <typename T>
inline constexpr bool IsBindVal = false;

template <meta::FixedString BindName, typename T>
inline constexpr bool IsBindVal<internal::Bind<BindName, T>> = true;

template <typename... Ts>
inline constexpr bool HasAllTypeIsBindVal = sizeof...(Ts) && (IsBindVal<Ts> && ...);

} // namespace HX::db