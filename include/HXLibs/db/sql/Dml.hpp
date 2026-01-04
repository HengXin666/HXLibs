#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2026-01-02 01:08:14
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


#include <HXLibs/reflection/MemberPtr.hpp>
#include <HXLibs/log/serialize/ToString.hpp>
#include <HXLibs/db/sql/Table.hpp>
#include <HXLibs/db/sql/Column.hpp>
#include <HXLibs/db/sql/Param.hpp>
#include <HXLibs/db/sql/Aggregate.hpp>
#include <HXLibs/db/sql/Expression.hpp>

#include <HXLibs/log/Log.hpp> // debug

namespace HX::db {

namespace internal {

template <ColType... Cs>
constexpr std::size_t ParamCnt = (static_cast<std::size_t>(std::is_same_v<typename Cs::Table, void>) + ...);

template <Col C>
constexpr std::string makeColName() {
    using T = decltype(C);
    std::string res;
    res += getTableName<typename T::Table>();
    res += '.';
    res += reflection::makeMemberPtrToNameMap<typename T::Table>().at(C._ptr);
    return res;
}

template <Expression Expr>
constexpr std::string exprToString();

template <auto Expr>
constexpr void expandExpr(std::string& res) {
    using T = decltype(Expr);
    if constexpr (IsParamVal<T>) {
        res += "?";
    } else if constexpr (IsExpressionVal<T>) {
        res += exprToString<Expr>();
    } else if constexpr (IsColTypeVal<T>) {
        res += makeColName<Expr>();
    } else if constexpr (meta::IsFixedStringVal<T>) {
        res += '\"';
        res += Expr;
        res += '\"';
    } else if constexpr (IsSqlNumberTypeVal<T>) {
        res += log::toString(Expr);
    } else if constexpr (meta::IsTypeWrapVal<T>) {
        [&] <auto... Vs> (meta::ToTypeWrap<Vs...>) {
            if constexpr (sizeof...(Vs) > 1) {
                res += '(';
            }
            ((expandExpr<Vs>(res), res += ','), ...);
            if constexpr (sizeof...(Vs) > 1) {
                res.back() = ')';
            } else {
                res.pop_back();
            }
        } (Expr);
    }
}

template <Expression Expr>
constexpr std::string exprToString() {
    std::string res{};
    using T = decltype(Expr);
    if constexpr (IsCalculateExprTypeVal<T> || Is3OpExprVal<T>) {
        using Expr1 = decltype(Expr._expr1);
        if constexpr (IsExpressionVal<Expr1>) {
            res += exprToString<Expr._expr1>();
        } else if constexpr (IsColTypeVal<Expr1>) {
            res += makeColName<Expr._expr1>();
        } else {
            // 非法表达式
            static_assert(!sizeof(Expr), "illegal expression");
        }
        res += ' ';
        res += Expr._op.Sql;
        res += ' ';
        expandExpr<Expr._expr2>(res);
    } else if constexpr (IsTailOpExprVal<T>) {
        res += makeColName<Expr._expr>();
        res += ' ';
        res += Expr._op.Sql;
    } else if constexpr (Is1OpExprVal<T>) {
        res += Expr._op.Sql;
        res += ' ';
        res += makeColName<Expr._expr>();
    } else {
        // 非法表达式
        static_assert(!sizeof(Expr), "illegal expression");
    }
    return res;
}

template <Expression Expr>
constexpr std::size_t exprParamCnt() noexcept;

template <auto Expr>
constexpr std::size_t expandExprParamCnt() noexcept {
    using T = decltype(Expr);
    if constexpr (IsParamVal<T>) {
        return 1;
    } else if constexpr (IsExpressionVal<T>) {
        return exprParamCnt<Expr>();
    } else if constexpr (meta::IsTypeWrapVal<T>) {
        return [&] <auto... Vs> (meta::ToTypeWrap<Vs...>) {
            return (expandExprParamCnt<Vs>() + ...);
        } (Expr);
    } else {
        return 0;
    }
}

template <Expression Expr>
constexpr std::size_t exprParamCnt() noexcept {
    using T = decltype(Expr);
    if constexpr (IsCalculateExprTypeVal<T> || Is3OpExprVal<T>) {
        std::size_t res{};
        if constexpr (IsExpressionVal<decltype(Expr._expr1)>) {
            res += exprParamCnt<Expr._expr1>();
        }
        return res + expandExprParamCnt<Expr._expr2>();
    } else {
        return 0;
    }
}

template <Expression Expr>
constexpr auto GetExprParamsTypeFunc() noexcept;

template <auto Expr>
constexpr auto expandGetExprParamsTypeFunc() noexcept {
    using T = decltype(Expr);
    if constexpr (IsParamVal<T>) {
        return Expr;
    } else if constexpr (IsExpressionVal<T>) {
        return GetExprParamsTypeFunc<Expr>();
    } else if constexpr (meta::IsTypeWrapVal<T>) {
        return [&] <auto... Vs> (meta::ToTypeWrap<Vs...>) {
            return meta::JoinWrapType<decltype(expandGetExprParamsTypeFunc<Vs>())...>{};
        } (Expr);
    } else {
        return meta::Wrap<>{};
    }
}

/*
    Expr -> std::tuple<Ts...>, [Ts = db::param<T>, ...]
*/
template <Expression Expr>
constexpr auto GetExprParamsTypeFunc() noexcept {
    using T = decltype(Expr);
    if constexpr (IsCalculateExprTypeVal<T> || Is3OpExprVal<T>) {
        if constexpr (IsExpressionVal<decltype(Expr._expr1)>) {
            return meta::JoinWrapType<
                decltype(GetExprParamsTypeFunc<Expr._expr1>()),
                decltype(expandGetExprParamsTypeFunc<Expr._expr2>())
            >{};
        } else {
            return expandGetExprParamsTypeFunc<Expr._expr2>();
        }
    } else {
        return meta::Wrap<>{};
    }
}

template <Expression Expr>
using GetExprParamsType = decltype(GetExprParamsTypeFunc<Expr>());

template <auto Val>
constexpr void addLimitVal(std::string& res) {
    if constexpr (std::is_integral_v<decltype(Val)>) {
        res += log::toString(Val);
    } else if constexpr (IsParamVal<decltype(Val)>) {
        res += "?";
    }
}

} // namespace internal

template <typename Db>
struct SqlBuild {
    SqlBuild(Db db, std::string sql)
        : _dbRef{db}
        , _sql{std::move(sql)}
    {}

    Db _dbRef;
    std::string _sql;
};

template <typename Db>
struct LimitBuild : public SqlBuild<Db> {
    using Base = SqlBuild<Db>;
    using Base::Base;

    template <auto Offset, auto MaxCnt>
        requires ((std::is_integral_v<decltype(Offset)> || IsParamVal<decltype(Offset)>)
               && (std::is_integral_v<decltype(MaxCnt)> || IsParamVal<decltype(MaxCnt)>))
    void limit() {
        this->_sql += "LIMIT ";
        internal::addLimitVal<Offset>(this->_sql);
        this->_sql += ',';
        internal::addLimitVal<MaxCnt>(this->_sql);
        log::hxLog.info("sql:", this->_sql);
    }

    template <auto Offset>
        requires (std::is_integral_v<decltype(Offset)> || IsParamVal<decltype(Offset)>)
    void limit() {
        this->_sql += "LIMIT ";
        internal::addLimitVal<Offset>(this->_sql);
        log::hxLog.info("sql:", this->_sql);
    }
};

template <typename Db>
struct OrderByBuild : public LimitBuild<Db> {
    using Base = LimitBuild<Db>;
    using Base::Base;

    template <OrderType... Orders>
        requires (sizeof...(Orders) > 0)
    LimitBuild<Db> orderBy() && {
        this->_sql += "ORDER BY ";
        ([this] <OrderType Order> (meta::ToTypeWrap<Order>) {
            this->_sql += internal::makeColName<Col{Order._ptr}>();
            this->_sql += ' ';
            this->_sql += Order._op.Sql;
            this->_sql += ',';
        } (meta::ToTypeWrap<Orders>{}), ...);
        this->_sql.back() = ' ';
        return {this->_dbRef, std::move(this->_sql)};
    }
};

template <typename Db>
struct HavingBuild : public OrderByBuild<Db> {
    using Base = OrderByBuild<Db>;
    using Base::Base;

    template <Expression Expr>
    OrderByBuild<Db> having() && {
        this->_sql += "HAVING ";
        this->_sql += internal::exprToString<Expr>();
        this->_sql += ' ';
        return {this->_dbRef, std::move(this->_sql)};
    }
};

template <typename Db>
struct GroupByBuild : public HavingBuild<Db> {
    using Base = HavingBuild<Db>;
    using Base::Base;

    template <auto... Ptrs>
        requires (sizeof...(Ptrs) > 0 && (meta::IsMemberPtrVal<decltype(Ptrs)> && ...))
    HavingBuild<Db> groupBy() && {
        return std::move(*this).template groupBy<Col{Ptrs}...>();
    }

    template <Col... Cs>
        requires (sizeof...(Cs) > 0)
    HavingBuild<Db> groupBy() && {
        this->_sql += "GROUP BY ";
        ((this->_sql += internal::makeColName<Cs>(), this->_sql += ','), ...);
        this->_sql.back() = ' ';
        return {this->_dbRef, std::move(this->_sql)};
    }

    template <Expression Expr>
    HavingBuild<Db> groupBy() && {
        this->_sql += "GROUP BY ";
        this->_sql += internal::exprToString<Expr>();
        this->_sql += ' ';
        return {this->_dbRef, std::move(this->_sql)};
    }
};

template <typename Db>
struct WhereBuild : public GroupByBuild<Db> {
    using Base = GroupByBuild<Db>;
    using Base::Base;
    
    template <Expression Expr, typename... Ts>
    GroupByBuild<Db> where(Ts&&... ts) && {
        // 绑定参数个数不正确
        static_assert(internal::exprParamCnt<Expr>() == sizeof...(ts),
            "The number of binding parameters is incorrect");
        // 确保类型正确
        this->_sql += "WHERE ";
        this->_sql += internal::exprToString<Expr>();
        this->_sql += ' ';
        return {this->_dbRef, std::move(this->_sql)};
    }
};

template <typename Db>
struct JoinOrWhereBuild;

template <typename Db>
struct OnBuild : public JoinOrWhereBuild<Db> {
    using Base = JoinOrWhereBuild<Db>;
    using Base::Base;
    
    template <Expression Expr>
    JoinOrWhereBuild<Db> on() && {
        this->_sql += "ON ";
        this->_sql += internal::exprToString<Expr>();
        this->_sql += ' ';
        return {this->_dbRef, std::move(this->_sql)};
    }
};

template <typename Db>
struct JoinOrWhereBuild : public WhereBuild<Db> {
    using Base = WhereBuild<Db>;
    using Base::Base;

    template <typename Table>
    constexpr OnBuild<Db> join() && {
        this->_sql += "JOIN ";
        this->_sql += reflection::getTypeName<Table>();
        this->_sql += ' ';
        return {this->_dbRef, std::move(this->_sql)};
    }

    template <typename Table>
    constexpr OnBuild<Db> leftJoin() && {
        this->_sql += "LEFT JOIN ";
        this->_sql += reflection::getTypeName<Table>();
        this->_sql += ' ';
        return {this->_dbRef, std::move(this->_sql)};
    }

    template <typename Table>
    constexpr OnBuild<Db> rightJoin() && {
        this->_sql += "RIGHT JOIN ";
        this->_sql += reflection::getTypeName<Table>();
        this->_sql += ' ';
        return {this->_dbRef, std::move(this->_sql)};
    }
};

template <typename Db>
struct FromBuild : public SqlBuild<Db> {
    using Base = SqlBuild<Db>;
    using Base::Base;

    template <typename Table>
    constexpr JoinOrWhereBuild<Db> from() && {
        this->_sql += "FROM ";
        this->_sql += reflection::getTypeName<Table>();
        this->_sql += ' ';
        return {this->_dbRef, std::move(this->_sql)};
    }

    // op= 当使用这个的时候, 进行build
};

template <typename Db>
struct SelectBuild : public SqlBuild<Db> {
    using Base = SqlBuild<Db>;
    using Base::Base;

    template <auto... Cs>
        requires (sizeof...(Cs) > 0)
    constexpr FromBuild<Db> select() && {
        this->_sql += "SELECT ";
        ([this] <auto C> (meta::ToTypeWrap<C>) {
            using U = decltype(C);
            if constexpr (IsColTypeVal<U>) {
                // select 不可使用占位符
                static_assert(internal::ParamCnt<U> == 0, "Select cannot use placeholder");
                this->_sql += internal::makeColName<C>();
            } else if constexpr (IsAggregateFuncVal<U>) {
                this->_sql += U::AggFunc::toSql(internal::makeColName<U::ColVal>());
            } else {
                // 非法查询参数
                static_assert(!sizeof(U), "Illegal query parameter");
            }
            this->_sql += ',';
        }(meta::ToTypeWrap<Cs>{}), ...);
        this->_sql.back() = ' ';
        return {this->_dbRef, std::move(this->_sql)};
    }
};

} // namespace HX::db