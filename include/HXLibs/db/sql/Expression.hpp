#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2026-01-02 10:37:24
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

#include <HXLibs/meta/NumberNTTP.hpp>
#include <HXLibs/db/sql/Column.hpp>
#include <HXLibs/db/sql/Operator.hpp>

namespace HX::db {

template <typename... Ts>
struct Expression {
    static_assert(!(sizeof(Ts), ...), "illegal expression");
};

template <typename T>
constexpr bool IsExpressionVal = false;

template <typename... Ts>
constexpr bool IsExpressionVal<Expression<Ts...>> = true;

template <typename T>
concept ExprType = IsExpressionVal<T>;

// ==, >, <, +, -, ||, &&, ..., like, in
template <typename Expr1, typename Op, typename Expr2>
struct Expression<Expr1, Op, Expr2> {
    using ExprInfo = std::tuple<Expr1, Op, Expr2>;
    // 可能是 A op B
    // 也可能是 A in <B>

    template <ExprType ET>
    constexpr Expression<Expression, op::Or, ET> operator||(ET) const noexcept {
        return {};
    }

    template <ExprType ET>
    constexpr Expression<Expression, op::And, ET> operator&&(ET) const noexcept {
        return {};
    }
};

// is null, asc/desc
template <typename Expr, typename Op>
struct Expression<Expr, Op> {
    using ExprInfo = std::tuple<Expr, Op>;

    template <ExprType ET>
    constexpr Expression<Expression, op::Or, ET> operator||(ET) const noexcept {
        return {};
    }

    template <ExprType ET>
    constexpr Expression<Expression, op::And, ET> operator&&(ET) const noexcept {
        return {};
    }
};

// not
template <typename Op, typename Expr>
struct Expression<Op, Expr, void> {
    using ExprInfo = std::tuple<Op, Expr>;

    template <ExprType ET>
    constexpr Expression<Expression, op::Or, ET> operator||(ET) const noexcept {
        return {};
    }

    template <ExprType ET>
    constexpr Expression<Expression, op::And, ET> operator&&(ET) const noexcept {
        return {};
    }
};

#define HX_DB_3OP_IMPL(OP, OP_NAME)                                            \
    template <ColType C1, ColType C2>                                          \
    constexpr auto operator OP(C1, C2) noexcept {                              \
        return Expression<C1, OP_NAME, C2>{};                                  \
    }

HX_DB_3OP_IMPL(&&, op::And)
HX_DB_3OP_IMPL(||, op::Or)

#undef HX_DB_3OP_IMPL

#define HX_DB_3OP_NUMBER_IMPL(OP, OP_NAME)                                     \
    template <ColType C1, ColType C2>                                          \
    constexpr auto operator OP(C1, C2) noexcept {                              \
        return Expression<C1, OP_NAME, C2>{};                                  \
    }                                                                          \
    template <ColType C1, auto C2>                                             \
    constexpr auto operator OP(C1, meta::NumberNTTP<C2>) noexcept {            \
        return Expression<C1, OP_NAME, meta::NumberNTTP<C2>>{};                \
    }                                                                          \
    template <auto C1, ColType C2>                                             \
    constexpr auto operator OP(meta::NumberNTTP<C1>, C2) noexcept {            \
        return Expression<meta::NumberNTTP<C1>, OP_NAME, C2>{};                \
    }

HX_DB_3OP_NUMBER_IMPL(==, op::Eq)
HX_DB_3OP_NUMBER_IMPL(!=, op::Ne)
HX_DB_3OP_NUMBER_IMPL(<, op::Lt)
HX_DB_3OP_NUMBER_IMPL(<=, op::Le)
HX_DB_3OP_NUMBER_IMPL(>, op::Gt)
HX_DB_3OP_NUMBER_IMPL(>=, op::Ge)

HX_DB_3OP_NUMBER_IMPL(+, op::Add)
HX_DB_3OP_NUMBER_IMPL(-, op::Sub)
HX_DB_3OP_NUMBER_IMPL(*, op::Mul)
HX_DB_3OP_NUMBER_IMPL(/, op::Div)
HX_DB_3OP_NUMBER_IMPL(%, op::Mod)

#undef HX_DB_3OP_NUMBER_IMPL

#define HX_DB_2OP_HEAD_IMPL(OP, OP_NAME)                                       \
    template <ColType C>                                                       \
    constexpr auto operator OP(C) noexcept {                                   \
        return Expression<OP_NAME, C>{};                                       \
    }

HX_DB_2OP_HEAD_IMPL(!, op::Not)

#undef HX_DB_2OP_HEAD_IMPL

/// test

struct Table {
    int id;
    int cd;
    std::string name;
};

constexpr auto _1 = Col{&Table::id}.as<"tableId">() == Col{&Table::cd};
constexpr auto _2 = Col{&Table::id} == Col{&Table::cd};
constexpr auto _3 = _1 || _2 && Col{&Table::name}.like<"">() && !Col{&Table::id};

} // namespace HX::db