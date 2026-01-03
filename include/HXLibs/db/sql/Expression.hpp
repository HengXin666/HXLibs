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

#include <HXLibs/db/sql/SqlType.hpp>
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

template <typename T>
constexpr bool IsCalculateExprTypeVal = false;

template <typename Expr, typename Op, typename VT>
constexpr bool IsCalculateExprTypeVal<Expression<Expr, Op, VT, void>> = true;

// 计算中的表达式, 如 a.id + 1, 即 `字段 op 字面量`, op 为 +-*/%
template <typename T>
concept CalculateExprType = IsCalculateExprTypeVal<T>;

// 实现 表达式的 &&, == 运算符, 用于 `表达式1 op 表达式2`
#define HX_DB_EXPR_OP_IMPL(OP, OP_NAME)                                        \
    template <ExprType ET>                                                     \
    constexpr Expression<Expression, OP_NAME, ET> operator OP(ET)              \
        const noexcept {                                                       \
        return {};                                                             \
    }

template <typename Expr, typename Op, typename VT>
struct Expression<Expr, Op, VT, void> {
    using ExprInfo = std::tuple<Expr, Op, void>;
    // 可能是 A op B
    // 也可能是 A in <B>
    VT _opVal; // 此处的 B 是具体的编译期常量

    HX_DB_EXPR_OP_IMPL(||, op::Or)
    HX_DB_EXPR_OP_IMPL(&&, op::And)
};

// ==, >, <, +, -, ||, &&, ..., like, in
template <typename Expr1, typename Op, typename Expr2>
struct Expression<Expr1, Op, Expr2> {
    using ExprInfo = std::tuple<Expr1, Op, Expr2>;
    // 可能是 A op B
    // 也可能是 A in <B>

    HX_DB_EXPR_OP_IMPL(||, op::Or)
    HX_DB_EXPR_OP_IMPL(&&, op::And)
};

// is null, asc/desc
template <typename Expr, typename Op>
struct Expression<Expr, Op> {
    using ExprInfo = std::tuple<Expr, Op>;

    HX_DB_EXPR_OP_IMPL(||, op::Or)
    HX_DB_EXPR_OP_IMPL(&&, op::And)
};

// not
template <typename Op, typename Expr>
struct Expression<Op, Expr, void> {
    using ExprInfo = std::tuple<Op, Expr>;

    HX_DB_EXPR_OP_IMPL(||, op::Or)
    HX_DB_EXPR_OP_IMPL(&&, op::And)
};

#undef HX_DB_EXPR_OP_IMPL

// 实现 比较运算符, 支持 列的比较, 列与常量的比较
#define HX_DB_3OP_COMP_IMPL(OP, OP_NAME)                                       \
    template <ColType C1, ColType C2>                                          \
        requires(                                                              \
            std::same_as<meta::RemoveOptionalWrapType<typename C1::Type>,      \
                         meta::RemoveOptionalWrapType<typename C2::Type>>)     \
    constexpr Expression<C1, OP_NAME, C2> operator OP(C1, C2) noexcept {       \
        return {};                                                             \
    }                                                                          \
    template <ColType C>                                                       \
    constexpr auto operator OP(C, auto v) noexcept                             \
        requires(                                                              \
            IsSqlNumberTypeVal<decltype(v)>                                    \
            && std::same_as<decltype(v),                                       \
                            meta::RemoveOptionalWrapType<typename C::Type>>)   \
    {                                                                          \
        return Expression<C, OP_NAME, decltype(v), void>{std::move(v)};        \
    }                                                                          \
    template <ColType C>                                                       \
    constexpr auto operator OP(auto v, C) noexcept                             \
        requires(                                                              \
            IsSqlNumberTypeVal<decltype(v)>                                    \
            && std::same_as<decltype(v),                                       \
                            meta::RemoveOptionalWrapType<typename C::Type>>)   \
    {                                                                          \
        return Expression<C, OP_NAME, decltype(v), void>{std::move(v)};        \
    }

HX_DB_3OP_COMP_IMPL(<, op::Lt)
HX_DB_3OP_COMP_IMPL(<=, op::Le)
HX_DB_3OP_COMP_IMPL(>, op::Gt)
HX_DB_3OP_COMP_IMPL(>=, op::Ge)

#undef HX_DB_3OP_COMP_IMPL

// 实现 数学运算符, 支持 `列 eq/ne 列`, `列 op 字面量`, `列 op 数学运算符`
#define HX_DB_3OP_NUMBER_IMPL(OP, OP_NAME)                                     \
    template <ColType C1, ColType C2>                                          \
        requires(                                                              \
            std::same_as<meta::RemoveOptionalWrapType<typename C1::Type>,      \
                         meta::RemoveOptionalWrapType<typename C2::Type>>)     \
    constexpr Expression<C1, OP_NAME, C2> operator OP(C1, C2) noexcept {       \
        return {};                                                             \
    }                                                                          \
    template <ColType C>                                                       \
    constexpr auto operator OP(C, auto v) noexcept                             \
        requires(                                                              \
            IsSqlNumberTypeVal<decltype(v)>                                    \
            && std::same_as<decltype(v),                                       \
                            meta::RemoveOptionalWrapType<typename C::Type>>)   \
    {                                                                          \
        return Expression<C, OP_NAME, decltype(v), void>{std::move(v)};        \
    }                                                                          \
    template <ColType C>                                                       \
    constexpr auto operator OP(auto v, C) noexcept                             \
        requires(                                                              \
            IsSqlNumberTypeVal<decltype(v)>                                    \
            && std::same_as<decltype(v),                                       \
                            meta::RemoveOptionalWrapType<typename C::Type>>)   \
    {                                                                          \
        return Expression<C, OP_NAME, decltype(v), void>{std::move(v)};        \
    }                                                                          \
    template <ColType C, CalculateExprType CET>                                \
    constexpr Expression<C, OP_NAME, CET> operator OP(C, CET) noexcept {       \
        return {};                                                             \
    }                                                                          \
    template <CalculateExprType CET, ColType C>                                \
    constexpr Expression<C, OP_NAME, CET> operator OP(CET, C) noexcept {       \
        return {};                                                             \
    }                                                                          \
    template <CalculateExprType CET>                                           \
    constexpr auto operator OP(CET, auto v) noexcept                           \
        requires(IsSqlNumberTypeVal<decltype(v)>)                              \
    {                                                                          \
        return Expression<CET, OP_NAME, decltype(v), void>{std::move(v)};      \
    }                                                                          \
    template <CalculateExprType CET>                                           \
    constexpr auto operator OP(auto v, CET) noexcept                           \
        requires(IsSqlNumberTypeVal<decltype(v)>)                              \
    {                                                                          \
        return Expression<CET, OP_NAME, decltype(v), void>{std::move(v)};      \
    }

HX_DB_3OP_NUMBER_IMPL(==, op::Eq)
HX_DB_3OP_NUMBER_IMPL(!=, op::Ne)
HX_DB_3OP_NUMBER_IMPL(+, op::Add)
HX_DB_3OP_NUMBER_IMPL(-, op::Sub)
HX_DB_3OP_NUMBER_IMPL(*, op::Mul)
HX_DB_3OP_NUMBER_IMPL(/, op::Div)
HX_DB_3OP_NUMBER_IMPL(%, op::Mod)

#undef HX_DB_3OP_NUMBER_IMPL

// 对字符串列与字面量提供比较
#define HX_DB_3OP_STR_IMPL(OP, OP_NAME)                                        \
    template <ColType C>                                                       \
    constexpr auto operator OP(C, auto v) noexcept                             \
        requires(                                                              \
            meta::StringType<meta::RemoveOptionalWrapType<typename C::Type>>   \
            && meta::IsFixedStringVal<decltype(v)>)                            \
    {                                                                          \
        return Expression<C, OP_NAME, decltype(v), void>{std::move(v)};        \
    }                                                                          \
    template <ColType C>                                                       \
    constexpr auto operator OP(auto v, C) noexcept                             \
        requires(                                                              \
            meta::StringType<meta::RemoveOptionalWrapType<typename C::Type>>   \
            && meta::IsFixedStringVal<decltype(v)>)                            \
    {                                                                          \
        return Expression<C, OP_NAME, decltype(v), void>{std::move(v)};        \
    }

HX_DB_3OP_STR_IMPL(==, op::Eq)
HX_DB_3OP_STR_IMPL(!=, op::Ne)
HX_DB_3OP_STR_IMPL(<, op::Lt)
HX_DB_3OP_STR_IMPL(<=, op::Le)
HX_DB_3OP_STR_IMPL(>, op::Gt)
HX_DB_3OP_STR_IMPL(>=, op::Ge)

#undef HX_DB_3OP_STR_IMPL

// 为一元运算符提供实现
#define HX_DB_1OP_HEAD_IMPL(OP, OP_NAME)                                       \
    template <ColType C>                                                       \
    constexpr auto operator OP(C) noexcept {                                   \
        return Expression<OP_NAME, C>{};                                       \
    }

HX_DB_1OP_HEAD_IMPL(!, op::Not)

#undef HX_DB_1OP_HEAD_IMPL

/// test

struct Table {
    int id;
    int cd;
    std::string name;
};

struct Table2 {
    int id;
    int cd;
    std::string name;
};

constexpr auto _1 = Col{&Table::id}.as<"tableId">() == Col{&Table::cd};
constexpr auto _2 = Col{&Table::id} == Col{&Table::cd};
constexpr auto _3 = _1 || (_2 && Col{&Table::name}.like<"">() && !Col{&Table::id});

constexpr auto _4 = Col{&Table::name} == meta::fixed_string_literals::operator""_fs<"暂时">();

constexpr auto _5 = Col{&Table::id} == Col{&Table2::id} + 1;
constexpr auto _6 = Col{&Table::id} - 1 == Col{&Table2::id};
constexpr auto _7 = Col{&Table::id} % 2 == 1;
// constexpr auto _8 = Col{&Table::id} + Col{&Table2::id} == 1; // 不合法, 一边只能放一个`列`

} // namespace HX::db