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

/**
 * @brief 判断是否为「计算」表达式
 */
template <typename Expr, typename Op, typename VT>
constexpr bool IsCalculateExprTypeVal<Expression<Expr, Op, VT, void>> = true;

template <typename T>
constexpr bool Is3OpExprVal = false;

/**
 * @brief 判断是否为「三操作数」表达式
 */
template <typename Expr1, typename Op, typename Expr2>
    requires (!std::same_as<Expr2, void>)
constexpr bool Is3OpExprVal<Expression<Expr1, Op, Expr2>> = true;

template <typename T>
constexpr bool IsTailOpExprVal = false;

/**
 * @brief 判断是否为「尾操作」表达式 (? op, 如 ? is null)
 */
template <typename Expr, typename Op>
constexpr bool IsTailOpExprVal<Expression<Expr, Op>> = true;


template <typename T>
constexpr bool Is1OpExprVal = false;

/**
 * @brief 判断是否为「一操作数」表达式 (op(?), 如 not ?)
 */
template <typename Op, typename Expr>
constexpr bool Is1OpExprVal<Expression<Op, Expr, void>> = true;

// 计算中的表达式, 如 a.id + 1, 即 `字段 op 字面量`, op 为 +-*/%
template <typename T>
concept CalculateExprType = IsCalculateExprTypeVal<T>;

// 实现 表达式的 &&, == 运算符, 用于 `表达式1 op 表达式2`
#define HX_DB_EXPR_OP_IMPL(OP, OP_NAME)                                        \
    template <ExprType ET>                                                     \
    constexpr Expression<Expression, decltype(OP_NAME), ET> operator OP(ET et) \
        const noexcept {                                                       \
        return {*this, OP_NAME, et};                                           \
    }

template <typename Expr, typename Op, typename VT>
struct Expression<Expr, Op, VT, void> {
    Expr _expr1;
    Op _op;
    // 可能是 A op B
    // 也可能是 A in <B>
    VT _expr2; // 此处的 B 是具体的编译期常量

    HX_DB_EXPR_OP_IMPL(||, op::Or)
    HX_DB_EXPR_OP_IMPL(&&, op::And)
};

// ==, >, <, +, -, ||, &&, ..., like, in
template <typename Expr1, typename Op, typename Expr2>
struct Expression<Expr1, Op, Expr2> {
    Expr1 _expr1;
    Op _op;
    Expr2 _expr2;
    // 可能是 A op B
    // 也可能是 A in <B>

    HX_DB_EXPR_OP_IMPL(||, op::Or)
    HX_DB_EXPR_OP_IMPL(&&, op::And)
};

// is null
template <typename Expr, typename Op>
struct Expression<Expr, Op> {
    Expr _expr;
    Op _op;

    HX_DB_EXPR_OP_IMPL(||, op::Or)
    HX_DB_EXPR_OP_IMPL(&&, op::And)
};

// not
template <typename Op, typename Expr>
struct Expression<Op, Expr, void> {
    Op _op;
    Expr _expr;

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
    constexpr Expression<C1, decltype(OP_NAME), C2> operator OP(               \
        C1 c1, C2 c2) noexcept {                                               \
        return {c1, OP_NAME, c2};                                              \
    }                                                                          \
    template <ColType C>                                                       \
    constexpr auto operator OP(C c, auto v) noexcept                           \
        requires(                                                              \
            IsSqlNumberTypeVal<decltype(v)>                                    \
            && std::same_as<decltype(v),                                       \
                            meta::RemoveOptionalWrapType<typename C::Type>>)   \
    {                                                                          \
        return Expression<C, decltype(OP_NAME), decltype(v), void>{            \
            c, OP_NAME, std::move(v)};                                         \
    }                                                                          \
    template <ColType C>                                                       \
    constexpr auto operator OP(auto v, C c) noexcept                           \
        requires(                                                              \
            IsSqlNumberTypeVal<decltype(v)>                                    \
            && std::same_as<decltype(v),                                       \
                            meta::RemoveOptionalWrapType<typename C::Type>>)   \
    {                                                                          \
        return Expression<C, decltype(OP_NAME), decltype(v), void>{            \
            c, OP_NAME, std::move(v)};                                         \
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
    constexpr Expression<C1, decltype(OP_NAME), C2> operator OP(               \
        C1 c1, C2 c2) noexcept {                                               \
        return {c1, OP_NAME, c2};                                              \
    }                                                                          \
    template <ColType C>                                                       \
    constexpr auto operator OP(C c, auto v) noexcept                           \
        requires(                                                              \
            IsSqlNumberTypeVal<decltype(v)>                                    \
            && std::same_as<decltype(v),                                       \
                            meta::RemoveOptionalWrapType<typename C::Type>>)   \
    {                                                                          \
        return Expression<C, decltype(OP_NAME), decltype(v), void>{            \
            c, OP_NAME, std::move(v)};                                         \
    }                                                                          \
    template <ColType C>                                                       \
    constexpr auto operator OP(auto v, C c) noexcept                           \
        requires(                                                              \
            IsSqlNumberTypeVal<decltype(v)>                                    \
            && std::same_as<decltype(v),                                       \
                            meta::RemoveOptionalWrapType<typename C::Type>>)   \
    {                                                                          \
        return Expression<C, decltype(OP_NAME), decltype(v), void>{            \
            c, OP_NAME, std::move(v)};                                         \
    }                                                                          \
    template <ColType C, CalculateExprType CET>                                \
    constexpr Expression<C, decltype(OP_NAME), CET> operator OP(               \
        C c, CET cet) noexcept {                                               \
        return {c, OP_NAME, cet};                                              \
    }                                                                          \
    template <CalculateExprType CET, ColType C>                                \
    constexpr Expression<C, decltype(OP_NAME), CET> operator OP(               \
        CET cet, C c) noexcept {                                               \
        return {c, OP_NAME, cet};                                              \
    }                                                                          \
    template <CalculateExprType CET>                                           \
    constexpr auto operator OP(CET cet, auto v) noexcept                       \
        requires(IsSqlNumberTypeVal<decltype(v)>)                              \
    {                                                                          \
        return Expression<CET, decltype(OP_NAME), decltype(v), void>{          \
            cet, OP_NAME, std::move(v)};                                       \
    }                                                                          \
    template <CalculateExprType CET>                                           \
    constexpr auto operator OP(auto v, CET cet) noexcept                       \
        requires(IsSqlNumberTypeVal<decltype(v)>)                              \
    {                                                                          \
        return Expression<CET, decltype(OP_NAME), decltype(v), void>{          \
            cet, OP_NAME, std::move(v)};                                       \
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
    constexpr auto operator OP(C c, auto v) noexcept                           \
        requires(                                                              \
            meta::StringType<meta::RemoveOptionalWrapType<typename C::Type>>   \
            && meta::IsFixedStringVal<decltype(v)>)                            \
    {                                                                          \
        return Expression<C, decltype(OP_NAME), decltype(v), void>{            \
            c, OP_NAME, std::move(v)};                                         \
    }                                                                          \
    template <ColType C>                                                       \
    constexpr auto operator OP(auto v, C c) noexcept                           \
        requires(                                                              \
            meta::StringType<meta::RemoveOptionalWrapType<typename C::Type>>   \
            && meta::IsFixedStringVal<decltype(v)>)                            \
    {                                                                          \
        return Expression<C, decltype(OP_NAME), decltype(v), void>{            \
            c, OP_NAME, std::move(v)};                                         \
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
    constexpr auto operator OP(C c) noexcept {                                 \
        return Expression<decltype(OP_NAME), C, void>{OP_NAME, c};             \
    }

HX_DB_1OP_HEAD_IMPL(!, op::Not)

#undef HX_DB_1OP_HEAD_IMPL

} // namespace HX::db