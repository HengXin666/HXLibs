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
#include <HXLibs/db/sql/ColumnResult.hpp>

#include <HXLibs/log/Log.hpp> // debug

namespace HX::db {

template <typename Db>
struct SelectBuild;

template <typename Db, typename SelectT>
struct DataBaseSqlBuildView {
    using SelectType = SelectT;
    Db* _db;

    constexpr bool isInit() const noexcept {
        return _db->_isInit;
    }

    constexpr std::string& sql() noexcept {
        return _db->_sql;
    }

    void prepareSql() {
        _db->_db->prepareSql(sql(), _db->_stmt);
        _db->_isInit = true;
    }

    template <auto... Vs>
    void selectForEach(std::vector<ColumnResult<meta::ValueWrap<Vs...>>>& resArr) {
        _db->_stmt.selectForEach(resArr);
    }

    template <typename T>
    bool bind(std::size_t idx, T&& t) {
        return _db->_stmt.bind(idx, std::forward<T>(t));
    }
};

template <typename Db>
struct DataBaseSqlBuild {
    template <auto... Cs>
    auto select() {
        using DbView = DataBaseSqlBuildView<DataBaseSqlBuild<Db>, meta::ValueWrap<Cs...>>;
        return SelectBuild<DbView>{DbView{this}}.template select<Cs...>();
    }

    Db* const _db{};
    std::string _sql{};
    Db::StmtType _stmt{};
    bool _isInit{false};
};

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
    } else if constexpr (meta::IsValueWrapVal<T>) {
        [&] <auto... Vs> (meta::ValueWrap<Vs...>) {
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
    } else if constexpr (meta::IsValueWrapVal<T>) {
        return [&] <auto... Vs> (meta::ValueWrap<Vs...>) {
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
    } else if constexpr (meta::IsValueWrapVal<T>) {
        return [&] <auto... Vs> (meta::ValueWrap<Vs...>) {
            return meta::FlattenWrapType<decltype(expandGetExprParamsTypeFunc<Vs>())...>{};
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
            return meta::FlattenWrapType<
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
    SqlBuild(Db db)
        : _dbRef{db}
    {}

    template <typename... Ts>
    auto exec(std::tuple<Ts...>& ts) {
        if (!this->_dbRef.isInit()) [[unlikely]] {
            this->_dbRef.prepareSql();
        }
        [&] <std::size_t... Idx> (std::index_sequence<Idx...>) constexpr {
            ([&]() constexpr {
                if (!this->_dbRef.bind(Idx + 1, std::forward<Ts>(std::get<Idx>(ts)))) [[unlikely]] {
                    throw std::runtime_error{"Bind idx = " + std::to_string(Idx) + " failure"};
                }
            }(), ...);
        }(std::make_index_sequence<sizeof...(Ts)>{});
        using SelectT = meta::RemoveCvRefType<Db>::SelectType;
        std::vector<ColumnResult<SelectT>> res;
        this->_dbRef.selectForEach(res);
        return res;
    }

    Db _dbRef;
};

template <typename Db, typename... Ts>
struct LimitBuild : public SqlBuild<Db> {
    using Base = SqlBuild<Db>;
    using Base::Base;
    std::tuple<Ts&&...> _tp; // @todo
    inline static constexpr auto N = sizeof...(Ts);

    LimitBuild(Db db, Ts&&... ts)
        : SqlBuild<Db>{db}
        , _tp{std::forward<Ts>(ts)...}
    {}

    template <auto Offset, auto MaxCnt>
        requires ((std::is_integral_v<decltype(Offset)> || IsParamVal<decltype(Offset)>)
               && (std::is_integral_v<decltype(MaxCnt)> || IsParamVal<decltype(MaxCnt)>))
    void limit() {
        this->_dbRef.sql() += "LIMIT ";
        internal::addLimitVal<Offset>(this->_dbRef.sql());
        this->_dbRef.sql() += ',';
        internal::addLimitVal<MaxCnt>(this->_dbRef.sql());
        log::hxLog.info("sql:", this->_dbRef.sql());
    }

    template <auto Offset>
        requires (std::is_integral_v<decltype(Offset)> || IsParamVal<decltype(Offset)>)
    void limit() {
        this->_dbRef.sql() += "LIMIT ";
        internal::addLimitVal<Offset>(this->_dbRef.sql());
        log::hxLog.info("sql:", this->_dbRef.sql());
    }
};

template <typename Db, typename... Ts>
struct OrderByBuild : public LimitBuild<Db> {
    using Base = LimitBuild<Db>;
    using Base::Base;
    std::tuple<Ts&&...> _tp;
    inline static constexpr auto N = sizeof...(Ts);

    OrderByBuild(Db db, Ts&&... ts)
        : LimitBuild<Db>{db}
        , _tp{std::forward<Ts>(ts)...}
    {}

    template <OrderType... Orders>
        requires (sizeof...(Orders) > 0)
    LimitBuild<Db, Ts&&...> orderBy() && {
        if (!this->_dbRef.isInit()) [[unlikely]] {
            this->_dbRef.sql() += "ORDER BY ";
            ([this] <OrderType Order> (meta::ValueWrap<Order>) {
                this->_dbRef.sql() += internal::makeColName<Col{Order._ptr}>();
                this->_dbRef.sql() += ' ';
                this->_dbRef.sql() += Order._op.Sql;
                this->_dbRef.sql() += ',';
            } (meta::ValueWrap<Orders>{}), ...);
            this->_dbRef.sql().back() = ' ';
        }
        return [&] <std::size_t... I> (std::index_sequence<I...>) constexpr {
            return LimitBuild<Db, Ts&&...>{this->_dbRef, std::get<I>(_tp)...};
        }(std::make_index_sequence<N>{});
    }
};

template <typename Db, typename... Ts>
struct HavingBuild : public OrderByBuild<Db> {
    using Base = OrderByBuild<Db>;
    using Base::Base;
    std::tuple<Ts&&...> _tp;
    inline static constexpr auto N = sizeof...(Ts);

    HavingBuild(Db db, Ts&&... ts)
        : OrderByBuild<Db>{db}
        , _tp{std::forward<Ts>(ts)...}
    {}

    template <Expression Expr, typename... Us>
    OrderByBuild<Db, Ts&&..., Us&&...> having(Us&&... us) && {
        using ParamWrap = internal::GetExprParamsType<Expr>;
        constexpr auto ParamCnt = meta::WrapSizeVal<ParamWrap>;
        // 绑定参数个数不正确
        static_assert(ParamCnt == sizeof...(us),
            "The number of binding parameters is incorrect");
        [] <std::size_t... Idx> (std::index_sequence<Idx...>) {
            // Us 类型 与 对应 占位参数 param 类型 不一致
            static_assert((std::is_same_v<
                    typename decltype(meta::get<Idx>(ParamWrap{}))::Type, meta::RemoveCvRefType<Us>
                > && ...),
                "Us type is inconsistent with the corresponding placeholder parameter param type");
        } (std::make_index_sequence<ParamCnt>{});
        if (!this->_dbRef.isInit()) [[unlikely]] {
            this->_dbRef.sql() += "HAVING ";
            this->_dbRef.sql() += internal::exprToString<Expr>();
            this->_dbRef.sql() += ' ';
        }
        return [&] <std::size_t... I> (std::index_sequence<I...>) constexpr {
            return OrderByBuild<Db, Ts&&..., Us&&...>{this->_dbRef,
                std::get<I>(_tp)..., std::forward<Us>(us)...};
        }(std::make_index_sequence<N>{});
    }
};

template <typename Db, typename... Ts>
struct GroupByBuild : public HavingBuild<Db> {
    using Base = HavingBuild<Db>;
    using Base::Base;
    std::tuple<Ts&&...> _tp;
    inline static constexpr auto N = sizeof...(Ts);

    GroupByBuild(Db db, Ts&&... ts)
        : HavingBuild<Db>{db}
        , _tp{std::forward<Ts>(ts)...}
    {}

    // @todo
    template <auto... Ptrs>
        requires (sizeof...(Ptrs) > 0 && (meta::IsMemberPtrVal<decltype(Ptrs)> && ...))
    HavingBuild<Db> groupBy() && {
        return std::move(*this).template groupBy<Col{Ptrs}...>();
    }

    // @todo
    template <Col... Cs>
        requires (sizeof...(Cs) > 0)
    HavingBuild<Db> groupBy() && {
        this->_dbRef.sql() += "GROUP BY ";
        ((this->_dbRef.sql() += internal::makeColName<Cs>(), this->_dbRef.sql() += ','), ...);
        this->_dbRef.sql().back() = ' ';
        return {this->_dbRef};
    }

    template <Expression Expr, typename... Us>
    HavingBuild<Db, Ts&&..., Us&&...> groupBy(Us&&... us) && {
        using ParamWrap = internal::GetExprParamsType<Expr>;
        constexpr auto ParamCnt = meta::WrapSizeVal<ParamWrap>;
        // 绑定参数个数不正确
        static_assert(ParamCnt == sizeof...(us),
            "The number of binding parameters is incorrect");
        [] <std::size_t... Idx> (std::index_sequence<Idx...>) {
            // Us 类型 与 对应 占位参数 param 类型 不一致
            static_assert((std::is_same_v<
                    typename decltype(meta::get<Idx>(ParamWrap{}))::Type, meta::RemoveCvRefType<Us>
                > && ...),
                "Us type is inconsistent with the corresponding placeholder parameter param type");
        } (std::make_index_sequence<ParamCnt>{});
        if (!this->_dbRef.isInit()) [[unlikely]] {
            this->_dbRef.sql() += "GROUP BY ";
            this->_dbRef.sql() += internal::exprToString<Expr>();
            this->_dbRef.sql() += ' ';
        }
        return [&] <std::size_t... I> (std::index_sequence<I...>) constexpr {
            return HavingBuild<Db, Ts&&..., Us&&...>{this->_dbRef,
                std::get<I>(_tp)..., std::forward<Us>(us)...};
        }(std::make_index_sequence<N>{});
    }

    auto exec() {
        return SqlBuild<Db>::exec(_tp);
    }
};

template <typename Db, typename... Ts>
struct WhereBuild : public GroupByBuild<Db> {
    using Base = GroupByBuild<Db>;
    using Base::Base;
    std::tuple<Ts&&...> _tp;
    inline static constexpr auto N = sizeof...(Ts);

    WhereBuild(Db db, Ts&&... ts)
        : GroupByBuild<Db>{db}
        , _tp{std::forward<Ts>(ts)...}
    {}
    
    template <Expression Expr, typename... Us>
    GroupByBuild<Db, Ts&&..., Us&&...> where(Us&&... us) && {
        using ParamWrap = internal::GetExprParamsType<Expr>;
        constexpr auto ParamCnt = meta::WrapSizeVal<ParamWrap>;
        // 绑定参数个数不正确
        static_assert(ParamCnt == sizeof...(us),
            "The number of binding parameters is incorrect");
        [] <std::size_t... Idx> (std::index_sequence<Idx...>) {
            // Us 类型 与 对应 占位参数 param 类型 不一致
            static_assert((std::is_same_v<
                    typename decltype(meta::get<Idx>(ParamWrap{}))::Type, meta::RemoveCvRefType<Us>
                > && ...),
                "Us type is inconsistent with the corresponding placeholder parameter param type");
        } (std::make_index_sequence<ParamCnt>{});
        if (!this->_dbRef.isInit()) [[unlikely]] {
            this->_dbRef.sql() += "WHERE ";
            this->_dbRef.sql() += internal::exprToString<Expr>();
            this->_dbRef.sql() += ' ';
        }
        return [&] <std::size_t... I> (std::index_sequence<I...>) constexpr {
            return GroupByBuild<Db, Ts&&..., Us&&...>{this->_dbRef,
                std::get<I>(_tp)..., std::forward<Us>(us)...};
        }(std::make_index_sequence<N>{});
    }
};

template <typename Db, typename... Ts>
struct JoinOrWhereBuild;

template <typename Db, typename... Ts>
struct OnBuild : public JoinOrWhereBuild<Db> {
    using Base = JoinOrWhereBuild<Db>;
    using Base::Base;
    std::tuple<Ts&&...> _tp;
    inline static constexpr auto N = sizeof...(Ts);
    
    template <Expression Expr, typename... Us>
    JoinOrWhereBuild<Db, Ts&&..., Us&&...> on(Us&&... us) && {
        using ParamWrap = internal::GetExprParamsType<Expr>;
        constexpr auto ParamCnt = meta::WrapSizeVal<ParamWrap>;
        // 绑定参数个数不正确
        static_assert(ParamCnt == sizeof...(us),
            "The number of binding parameters is incorrect");
        [] <std::size_t... Idx> (std::index_sequence<Idx...>) {
            // Us 类型 与 对应 占位参数 param 类型 不一致
            static_assert((std::is_same_v<
                    typename decltype(meta::get<Idx>(ParamWrap{}))::Type, meta::RemoveCvRefType<Us>
                > && ...),
                "Us type is inconsistent with the corresponding placeholder parameter param type");
        } (std::make_index_sequence<ParamCnt>{});
        if (!this->_dbRef.isInit()) [[unlikely]] {
            this->_dbRef.sql() += "ON ";
            this->_dbRef.sql() += internal::exprToString<Expr>();
            this->_dbRef.sql() += ' ';
        }
        return [&] <std::size_t... I> (std::index_sequence<I...>) constexpr {
            return JoinOrWhereBuild<Db, Ts&&..., Us&&...>{this->_dbRef,
                std::get<I>(_tp)..., std::forward<Us>(us)...};
        }(std::make_index_sequence<N>{});
    }
};

template <typename Db, typename... Ts>
struct JoinOrWhereBuild : public WhereBuild<Db> {
    using Base = WhereBuild<Db>;
    using Base::Base;
    std::tuple<Ts&&...> _tp;
    inline static constexpr auto N = sizeof...(Ts);

    template <typename Table>
    constexpr OnBuild<Db, Ts&&...> join() && {
        if (!this->_dbRef.isInit()) [[unlikely]] {
            this->_dbRef.sql() += "JOIN ";
            this->_dbRef.sql() += reflection::getTypeName<Table>();
            this->_dbRef.sql() += ' ';
        }
        return [&] <std::size_t... I> (std::index_sequence<I...>) constexpr {
            return OnBuild<Db, Ts&&...>{this->_dbRef, std::get<I>(_tp)...};
        }(std::make_index_sequence<N>{});
    }

    template <typename Table>
    constexpr OnBuild<Db> leftJoin() && {
        if (!this->_dbRef.isInit()) [[unlikely]] {
            this->_dbRef.sql() += "LEFT JOIN ";
            this->_dbRef.sql() += reflection::getTypeName<Table>();
            this->_dbRef.sql() += ' ';
        }
        return [&] <std::size_t... I> (std::index_sequence<I...>) constexpr {
            return OnBuild<Db, Ts&&...>{this->_dbRef, std::get<I>(_tp)...};
        }(std::make_index_sequence<N>{});
    }

    template <typename Table>
    constexpr OnBuild<Db> rightJoin() && {
        if (!this->_dbRef.isInit()) [[unlikely]] {
            this->_dbRef.sql() += "RIGHT JOIN ";
            this->_dbRef.sql() += reflection::getTypeName<Table>();
            this->_dbRef.sql() += ' ';
        }
        return [&] <std::size_t... I> (std::index_sequence<I...>) constexpr {
            return OnBuild<Db, Ts&&...>{this->_dbRef, std::get<I>(_tp)...};
        }(std::make_index_sequence<N>{});
    }
};

template <typename Db>
struct FromBuild : public SqlBuild<Db> {
    using Base = SqlBuild<Db>;
    using Base::Base;

    template <typename Table>
    constexpr JoinOrWhereBuild<Db> from() && {
        if (!this->_dbRef.isInit()) [[unlikely]] {
            this->_dbRef.sql() += "FROM ";
            this->_dbRef.sql() += reflection::getTypeName<Table>();
            this->_dbRef.sql() += ' ';
        }
        return {this->_dbRef};
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
        if (!this->_dbRef.isInit()) [[unlikely]] {
            this->_dbRef.sql() += "SELECT ";
            ([this] <auto C> (meta::ValueWrap<C>) {
                using U = decltype(C);
                if constexpr (IsColTypeVal<U>) {
                    // select 不可使用占位符
                    static_assert(internal::ParamCnt<U> == 0, "Select cannot use placeholder");
                    this->_dbRef.sql() += internal::makeColName<C>();
                } else if constexpr (IsAggregateFuncVal<U>) {
                    this->_dbRef.sql() += U::AggFunc::toSql(internal::makeColName<U::ColVal>());
                } else {
                    // 非法查询参数
                    static_assert(!sizeof(U), "Illegal query parameter");
                }
                this->_dbRef.sql() += ',';
            }(meta::ValueWrap<Cs>{}), ...);
            this->_dbRef.sql().back() = ' ';
        }
        return {this->_dbRef};
    }
};

} // namespace HX::db