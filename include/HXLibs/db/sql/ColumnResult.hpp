#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2026-01-10 11:29:26
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

#include <HXLibs/log/serialize/CustomToString.hpp>
#include <HXLibs/db/sql/Column.hpp>
#include <HXLibs/db/sql/Aggregate.hpp>

namespace HX::db {

namespace internal {

template <auto V>
constexpr auto toCol() noexcept {
    using T = decltype(V);
    if constexpr (IsColTypeVal<T>) {
        return V;
    } else if constexpr (IsAggregateFuncVal<T>) {
        return T::ColVal;
    } else {
        static_assert(!sizeof(V), "");
    }
}

} // namespace internal

template <typename SelectT>
struct ColumnResult;

template <auto... Cs>
struct ColumnResult<meta::ValueWrap<Cs...>> {
    using Type = std::tuple<typename decltype(internal::toCol<Cs>())::Type...>;

    constexpr ColumnResult() = default;

    template <typename... Ts>
        requires (requires {
            { Type {Ts{}...} } -> std::same_as<Type>;
        })
    constexpr ColumnResult(Ts&&... ts)
        : _tp{std::forward<Ts>(ts)...}
    {}

    constexpr Type& gets() noexcept {
        return _tp;
    }

    constexpr Type const& gets() const noexcept {
        return _tp;
    }

    template <std::size_t Idx>
    constexpr auto& get() noexcept {
        return std::get<Idx>(_tp);
    }

    template <std::size_t Idx>
    constexpr auto const& get() const noexcept {
        return std::get<Idx>(_tp);
    }

    template <meta::FixedString AsName>
    constexpr auto& get() noexcept {
        // 判断是否存在这个别名
        static_assert(((AsName == internal::toCol<Cs>()._asName)|| ...), "Alias does not exist");
        constexpr auto Pos = meta::find<AsName, internal::toCol<Cs>()._asName...>();
        return std::get<Pos>(_tp);
    }

    template <meta::FixedString AsName>
    constexpr auto const& get() const noexcept {
        // 判断是否存在这个别名
        static_assert(((AsName == internal::toCol<Cs>()._asName)|| ...), "Alias does not exist");
        constexpr auto Pos = meta::find<AsName, internal::toCol<Cs>()._asName...>();
        return std::get<Pos>(_tp);
    }
private:
    Type _tp;
};

} // namespace HX::db

namespace HX::log {

template <auto... Cs, typename FormatType>
struct CustomToString<db::ColumnResult<meta::ValueWrap<Cs...>>, FormatType> {
    FormatType* self;

    constexpr std::string make(db::ColumnResult<meta::ValueWrap<Cs...>> const& t) {
        return self->make(t.gets());
    }

    template <typename Stream>
    constexpr void make(db::ColumnResult<meta::ValueWrap<Cs...>> const& t, Stream& s) {
        self->make(t.gets(), s);
    }
};

} // namespace HX::log