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
#include <HXLibs/container/Try.hpp>
#include <HXLibs/db/sql/Table.hpp>
#include <HXLibs/db/sql/Column.hpp>
#include <HXLibs/db/sql/Param.hpp>
#include <HXLibs/db/sql/Aggregate.hpp>
#include <HXLibs/db/sql/Expression.hpp>
#include <HXLibs/db/sql/ColumnResult.hpp>

#include <HXLibs/log/Log.hpp> // debug

namespace HX::db {

namespace internal {

template <std::size_t... I, typename ValWrap>
    requires (meta::IsValueWrapVal<ValWrap>)
constexpr auto removeNumInValueWarp(std::index_sequence<I...>, ValWrap) noexcept {
    return meta::FlattenWrapType<meta::Wrap<decltype([]{
        if constexpr (meta::NotInValueWrapVal<I, ValWrap>) {
            return meta::ValueWrap<I>{};
        } else {
            return meta::Wrap<>{};
        }
    }())...>>{};
}

} // namespace internal

template <typename Db>
struct SelectBuild;

template <typename Db, typename... Ts>
struct ReturningInsertBuild;

template <typename Db, typename... Ts>
struct WhereMutationBuild;

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

    bool exec() {
        return _db->_stmt.exec();
    }

    std::uint64_t getLastChanges() const noexcept {
        return _db->_stmt.getLastChanges();
    }

    void reset() {
        _db->_stmt.reset();
    }

    template <auto... Vs>
    void selectForEach(std::vector<ColumnResult<meta::ValueWrap<Vs...>>>& resArr) {
        _db->_stmt.selectForEach(resArr);
    }

    template <auto... Vs>
    void returningForEach(std::vector<ColumnResult<meta::ValueWrap<Vs...>>>& resArr) {
        _db->_stmt.returningForEach(resArr);
    }

    template <typename T>
    bool bind(std::size_t idx, T&& t) {
        return _db->_stmt.bind(idx, std::forward<T>(t));
    }

    template <auto... Vs>
    ColumnResult<meta::ValueWrap<Vs...>> returning() {
        return _db->_stmt.template returning<Vs...>();
    }
};

template <typename Db>
struct DataBaseSqlBuild {
    /**
     * @brief 查询语句
     * @tparam Cs 需要查询的列
     * @return auto std::vector<查询的列...>
     */
    template <auto... Cs>
    auto select() {
        using DbView = DataBaseSqlBuildView<DataBaseSqlBuild<Db>, meta::ValueWrap<Cs...>>;
        return SelectBuild<DbView>{DbView{this}}.template select<Cs...>();
    }

    /**
     * @brief 插入语句
     * @tparam T 插入的表类型
     * @tparam InsertPrimaryKey 是否插入主键 (默认: false, 不插入, 使用自增主键)
     */
    template <typename Table, bool InsertPrimaryKey = false>
    auto insert(Table const& t) {
        if (!_isInit) [[unlikely]] {
            _sql = "INSERT INTO ";
            _sql += getTableName<Table>();
            _sql += " (";
            std::string param = " VALUES (";
            reflection::forEach(t, [&] <std::size_t Idx> (
                std::index_sequence<Idx>, std::string_view name, auto&
            ) {
                // 判断主键
                if constexpr (InsertPrimaryKey) {
                    _sql += name;
                    _sql += ',';
                    param += "?,";
                } else if constexpr (meta::NotInValueWrapVal<Idx, GetTablePrimaryKeyIdxType<Table>>) {
                    _sql += name;
                    _sql += ',';
                    param += "?,";
                }
            });
            if (_sql.back() != '(') [[likely]] {
                _sql.back() = ')';
                param.back() = ')';
                _sql += std::move(param);
            } else {
                _sql.back() = 'D';
                _sql += "EFAULT VALUES";
            }
        }

        using DbView = DataBaseSqlBuildView<DataBaseSqlBuild<Db>, void>;
        auto const tp = reflection::internal::getObjTie(t);
        if constexpr (InsertPrimaryKey) {
            return [&] <std::size_t... I> (std::index_sequence<I...>) constexpr {
                return ReturningInsertBuild<DbView, decltype(std::get<I>(tp))...>{
                    DbView{this}, std::get<I>(tp)...};
            }(std::make_index_sequence<reflection::membersCountVal<Table>>{});
        } else {
            return [&] <std::size_t... I> (meta::Wrap<meta::ValueWrap<I>...>) constexpr {
                return ReturningInsertBuild<DbView, decltype(std::get<I>(tp))...>{
                    DbView{this}, std::get<I>(tp)...};
            }(internal::removeNumInValueWarp(
                std::make_index_sequence<reflection::membersCountVal<Table>>{},
                GetTablePrimaryKeyIdxType<Table>{}
            ));
        }
    }

    /**
     * @brief 插入语句
     * @tparam Ptrs 插入的字段 (类成员指针)
     * @tparam Ts 对应的值
     * @tparam Table
     */
    template <
        auto... Ptrs,
        typename... Ts,
        typename Table = meta::GetMemberPtrsClassType<decltype(Ptrs)...>
    >
        requires (sizeof...(Ptrs) > 0 && !std::is_void_v<Table>)
    auto insert(Ts const&... ts) {
        // 参数数量不匹配
        static_assert(sizeof...(Ptrs) == sizeof...(Ts), "The number of parameters does not match");
        if (!_isInit) [[unlikely]] {
            _sql = "INSERT INTO ";
            _sql += getTableName<Table>();
            _sql += " (";
            std::string param = " VALUES (";
            auto mp = reflection::makeMemberPtrToNameMap<Table>();
            ((_sql += mp.at(Ptrs), _sql += ',', param += "?,"), ...);
            if (_sql.back() != '(') [[likely]] {
                _sql.back() = ')';
                param.back() = ')';
                _sql += std::move(param);
            } else {
                _sql.back() = 'D';
                _sql += "EFAULT VALUES";
            }
        }
        using DbView = DataBaseSqlBuildView<DataBaseSqlBuild<Db>, void>;
        return ReturningInsertBuild<DbView, Ts const&...>{DbView{this}, ts...};
    }

    /**
     * @brief 更新语句
     * @tparam T 更新的表类型
     * @tparam UpdatePrimaryKey 是否更新主键 (默认: false, 不更新)
     */
    template <typename Table, bool UpdatePrimaryKey = false>
    auto update(Table const& t) {
        if (!_isInit) [[unlikely]] {
            _sql = "UPDATE ";
            _sql += getTableName<Table>();
            _sql += " SET ";
            reflection::forEach(t, [&] <std::size_t Idx> (
                std::index_sequence<Idx>, std::string_view name, auto&
            ) {
                // 判断主键
                if constexpr (UpdatePrimaryKey) {
                    _sql += name;
                    _sql += "=?,";
                } else if constexpr (meta::NotInValueWrapVal<Idx, GetTablePrimaryKeyIdxType<Table>>) {
                    _sql += name;
                    _sql += "=?,";
                }
            });
            _sql.back() = ' ';
        }
        using DbView = DataBaseSqlBuildView<DataBaseSqlBuild<Db>, void>;
        auto const tp = reflection::internal::getObjTie(t);
        if constexpr (UpdatePrimaryKey) {
            return [&] <std::size_t... I> (std::index_sequence<I...>) constexpr {
                return WhereMutationBuild<DbView, decltype(std::get<I>(tp))...>{
                    DbView{this}, std::get<I>(tp)...};
            }(std::make_index_sequence<reflection::membersCountVal<Table>>{});
        } else {
            return [&] <std::size_t... I> (meta::Wrap<meta::ValueWrap<I>...>) constexpr {
                // 不允许 SET 空: UPDATE 必须至少指定一个 SET 赋值项
                static_assert(sizeof...(I), "UPDATE must specify at least one SET assignment");
                return WhereMutationBuild<DbView, decltype(std::get<I>(tp))...>{
                    DbView{this}, std::get<I>(tp)...};
            }(internal::removeNumInValueWarp(
                std::make_index_sequence<reflection::membersCountVal<Table>>{},
                GetTablePrimaryKeyIdxType<Table>{}
            ));
        }
    }

    /**
     * @brief 更新语句
     * @tparam Ptrs 插入的字段 (类成员指针)
     * @tparam Ts 对应的值
     * @tparam Table
     */
    template <
        auto... Ptrs,
        typename... Ts,
        typename Table = meta::GetMemberPtrsClassType<decltype(Ptrs)...>
    >
        requires (sizeof...(Ptrs) > 0 && !std::is_void_v<Table>)
    auto update(Ts const&... ts) {
        // 参数数量不匹配
        static_assert(sizeof...(Ptrs) == sizeof...(Ts), "The number of parameters does not match");
        if (!_isInit) [[unlikely]] {
            _sql = "UPDATE ";
            _sql += getTableName<Table>();
            _sql += " SET ";
            auto mp = reflection::makeMemberPtrToNameMap<Table>();
            ((_sql += mp.at(Ptrs), _sql += "=?,"), ...);
            _sql.back() = ' ';
        }
        using DbView = DataBaseSqlBuildView<DataBaseSqlBuild<Db>, void>;
        return WhereMutationBuild<DbView, Ts const&...>{DbView{this}, ts...};
    }

    template <typename Table>
    auto deleteForm() {
        if (!_isInit) [[unlikely]] {
            _sql = "DELETE FROM ";
            _sql += getTableName<Table>();
            _sql += ' ';
        }
        using DbView = DataBaseSqlBuildView<DataBaseSqlBuild<Db>, void>;
        return WhereMutationBuild<DbView>{DbView{this}};
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
        res += '\'';
        res += Expr;
        res += '\'';
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
struct SelectSqlBuild {
    using ColumnResType = ColumnResult<typename meta::RemoveCvRefType<Db>::SelectType>;

    constexpr SelectSqlBuild(Db db)
        : _dbRef{db}
    {}

    template <typename... Ts>
    std::vector<ColumnResType> exec(std::tuple<Ts...>& ts) {
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
        std::vector<ColumnResType> res{};
        this->_dbRef.selectForEach(res);
        this->_dbRef.reset();
        return res;
    }

    template <typename... Ts>
    container::Try<std::vector<ColumnResType>> execNoThrow(std::tuple<Ts...>& ts) noexcept {
        container::Try<std::vector<ColumnResType>> res{};
        try {
            res.setVal(exec(ts));
        } catch (...) {
            res.setException(std::current_exception());
        }
        return res;
    }

    Db _dbRef;
};

#define HX_DB_DML_SELECT_EXEC_IMPL                                             \
    std::vector<typename SelectSqlBuild<Db>::ColumnResType> exec() && {        \
        return SelectSqlBuild<Db>::exec(_tp);                                  \
    }                                                                          \
                                                                               \
    container::Try<std::vector<typename SelectSqlBuild<Db>::ColumnResType>>    \
    execNoThrow() && noexcept {                                                \
        return SelectSqlBuild<Db>::execNoThrow(_tp);                           \
    }

template <typename Db, typename... Ts>
struct SelectEndBuild : public SelectSqlBuild<Db> {
    using Base = SelectSqlBuild<Db>;
    using Base::Base;
    std::tuple<Ts&&...> _tp;

    constexpr SelectEndBuild(Db db, Ts&&... ts)
        : Base{db}
        , _tp{std::forward<Ts>(ts)...}
    {}

    HX_DB_DML_SELECT_EXEC_IMPL
};

template <typename Db, typename... Ts>
struct LimitBuild : public SelectSqlBuild<Db> {
    using Base = SelectSqlBuild<Db>;
    using Base::Base;
    std::tuple<Ts&&...> _tp; // @todo
    inline static constexpr auto N = sizeof...(Ts);

    constexpr LimitBuild(Db db, Ts&&... ts)
        : Base{db}
        , _tp{std::forward<Ts>(ts)...}
    {}

    template <auto Offset, auto MaxCnt, typename... Us>
        requires ((std::is_integral_v<decltype(Offset)> || IsParamVal<decltype(Offset)>)
               && (std::is_integral_v<decltype(MaxCnt)> || IsParamVal<decltype(MaxCnt)>))
    SelectEndBuild<Db, Ts&&..., Us&&...> limit(Us&&... us) && {
        // 绑定参数个数不正确
        static_assert(IsParamVal<decltype(Offset)> + IsParamVal<decltype(MaxCnt)> == sizeof...(us),
            "The number of binding parameters is incorrect");
        this->_dbRef.sql() += "LIMIT ";
        internal::addLimitVal<Offset>(this->_dbRef.sql());
        this->_dbRef.sql() += ',';
        internal::addLimitVal<MaxCnt>(this->_dbRef.sql());
        return [&] <std::size_t... I> (std::index_sequence<I...>) constexpr {
            return SelectEndBuild<Db, Ts&&..., Us&&...>{this->_dbRef,
                std::get<I>(_tp)..., std::forward<Us>(us)...};
        }(std::make_index_sequence<N>{});
    }

    template <auto Offset, typename... Us>
        requires (std::is_integral_v<decltype(Offset)> || IsParamVal<decltype(Offset)>)
    SelectEndBuild<Db, Ts&&..., Us&&...> limit(Us&&... us) && {
        // 绑定参数个数不正确
        static_assert(IsParamVal<decltype(Offset)> == sizeof...(us),
            "The number of binding parameters is incorrect");
        this->_dbRef.sql() += "LIMIT ";
        internal::addLimitVal<Offset>(this->_dbRef.sql());
        return [&] <std::size_t... I> (std::index_sequence<I...>) constexpr {
            return SelectEndBuild<Db, Ts&&..., Us&&...>{this->_dbRef,
                std::get<I>(_tp)..., std::forward<Us>(us)...};
        }(std::make_index_sequence<N>{});
    }

    HX_DB_DML_SELECT_EXEC_IMPL
};

template <typename Db, typename... Ts>
struct OrderByBuild : public LimitBuild<Db> {
    using Base = LimitBuild<Db>;
    using Base::Base;
    std::tuple<Ts&&...> _tp;
    inline static constexpr auto N = sizeof...(Ts);

    constexpr OrderByBuild(Db db, Ts&&... ts)
        : Base{db}
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

    HX_DB_DML_SELECT_EXEC_IMPL
};

template <typename Db, typename... Ts>
struct HavingBuild : public OrderByBuild<Db> {
    using Base = OrderByBuild<Db>;
    using Base::Base;
    std::tuple<Ts&&...> _tp;
    inline static constexpr auto N = sizeof...(Ts);

    constexpr HavingBuild(Db db, Ts&&... ts)
        : Base{db}
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

    HX_DB_DML_SELECT_EXEC_IMPL
};

template <typename Db, typename... Ts>
struct GroupByBuild : public HavingBuild<Db> {
    using Base = HavingBuild<Db>;
    using Base::Base;
    std::tuple<Ts&&...> _tp;
    inline static constexpr auto N = sizeof...(Ts);

    constexpr GroupByBuild(Db db, Ts&&... ts)
        : Base{db}
        , _tp{std::forward<Ts>(ts)...}
    {}

    template <auto... Ptrs>
        requires (sizeof...(Ptrs) > 0 && (meta::IsMemberPtrVal<decltype(Ptrs)> && ...))
    HavingBuild<Db, Ts&&...> groupBy() && {
        return std::move(*this).template groupBy<Col{Ptrs}...>();
    }

    template <Col... Cs>
        requires (sizeof...(Cs) > 0)
    HavingBuild<Db, Ts&&...> groupBy() && {
        this->_dbRef.sql() += "GROUP BY ";
        ((this->_dbRef.sql() += internal::makeColName<Cs>(), this->_dbRef.sql() += ','), ...);
        this->_dbRef.sql().back() = ' ';
        return [&] <std::size_t... I> (std::index_sequence<I...>) constexpr {
            return HavingBuild<Db, Ts&&...>{this->_dbRef, std::get<I>(_tp)...};
        }(std::make_index_sequence<N>{});
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

    HX_DB_DML_SELECT_EXEC_IMPL
};

template <typename Db, typename... Ts>
struct WhereBuild : public GroupByBuild<Db> {
    using Base = GroupByBuild<Db>;
    using Base::Base;
    std::tuple<Ts&&...> _tp;
    inline static constexpr auto N = sizeof...(Ts);

    constexpr WhereBuild(Db db, Ts&&... ts)
        : Base{db}
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

    HX_DB_DML_SELECT_EXEC_IMPL
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

    HX_DB_DML_SELECT_EXEC_IMPL
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

    HX_DB_DML_SELECT_EXEC_IMPL
};

#undef HX_DB_DML_SELECT_EXEC_IMPL

template <typename Db>
struct FromBuild : public SelectSqlBuild<Db> {
    using Base = SelectSqlBuild<Db>;
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
};

template <typename Db>
struct SelectBuild : public SelectSqlBuild<Db> {
    using Base = SelectSqlBuild<Db>;
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

template <typename Db, Col... Cs>
struct InsertSqlBuild {
    using ColumnResType = std::conditional_t<(sizeof...(Cs) > 0), ColumnResult<meta::ValueWrap<Cs...>>, void>;

    constexpr InsertSqlBuild(Db db)
        : _dbRef{db}
    {}

    template <typename... Us>
    ColumnResType exec(std::tuple<Us const&...> const& ts) {
        if (!_dbRef.isInit()) [[unlikely]] {
            _dbRef.prepareSql();
        }
        [&] <std::size_t... Idx> (std::index_sequence<Idx...>) constexpr {
            ([&]() constexpr {
                if (!_dbRef.bind(Idx + 1, std::get<Idx>(ts))) [[unlikely]] {
                    throw std::runtime_error{"Bind idx = " + std::to_string(Idx) + " failure"};
                }
            }(), ...);
        }(std::make_index_sequence<sizeof...(Us)>{});
        if (_dbRef.exec()) [[unlikely]] {
            // 执行失败
            std::runtime_error{"SQL execution failed"};
        }
        if constexpr (!std::is_same_v<ColumnResType, void>) {
            return _dbRef.template returning<Cs...>();
        }
    }

    template <typename... Us>
    container::Try<ColumnResType> execNoThrow(std::tuple<Us const&...> const& ts) noexcept {
        container::Try<ColumnResType> res{};
        try {
            res.setVal(exec(ts));
            if constexpr (std::is_same_v<ColumnResType, void>) {
                res.setVal(container::NonVoidType<>{});
            }
        } catch (...) {
            res.setException(std::current_exception());
        }
        return res;
    }

    Db _dbRef;
};

template <typename Db, typename Col, typename... Ts>
struct ReturningInsertImplBuild;

template <typename Db, Col... Cs, typename... Ts>
struct ReturningInsertImplBuild<Db, meta::ValueWrap<Cs...>, Ts...> : public InsertSqlBuild<Db, Cs...> {
    using Base = InsertSqlBuild<Db, Cs...>;
    using Base::Base;

    std::tuple<Ts const&...> _tp;
    inline static constexpr auto N = sizeof...(Ts);

    constexpr ReturningInsertImplBuild(Db db, Ts const&... ts)
        : Base{db}
        , _tp{ts...}
    {}

    Base::ColumnResType exec() && {
        return Base::exec(_tp);
    }

    container::Try<typename Base::ColumnResType> execNoThrow() && noexcept {
        return Base::execNoThrow(_tp);
    }
};

template <typename Db, typename... Ts>
struct ReturningInsertBuild : public InsertSqlBuild<Db> {
    using Base = InsertSqlBuild<Db>;

    std::tuple<Ts const&...> _tp;
    inline static constexpr auto N = sizeof...(Ts);

    constexpr ReturningInsertBuild(Db db, Ts const&... ts)
        : Base{db}
        , _tp{ts...}
    {}

    template <auto... Cs>
        requires (sizeof...(Cs) > 0 && (meta::IsMemberPtrVal<decltype(Cs)> && ...))
    constexpr auto returning() && {
        return std::move(*this).template returning<Col{Cs}...>();
    }

    template <Col... Cs>
        requires (sizeof...(Cs) > 0)
    constexpr ReturningInsertImplBuild<Db, meta::ValueWrap<Cs...>, Ts&&...> returning() && {
        if (!this->_dbRef.isInit()) [[unlikely]] {
            this->_dbRef.sql() += " RETURNING ";
            auto mp = reflection::makeMemberPtrToNameMap<
                meta::GetMemberPtrsClassType<decltype(Cs._ptr)...>>();
            [&]() constexpr {
                ((this->_dbRef.sql() += mp.at(Cs._ptr), this->_dbRef.sql() += ','), ...);
            }();
            this->_dbRef.sql().back() = ';';
        }
        return [&] <std::size_t... I> (std::index_sequence<I...>) constexpr {
            return ReturningInsertImplBuild<Db, meta::ValueWrap<Cs...>, Ts const&...>{
                this->_dbRef, std::get<I>(_tp)...};
        }(std::make_index_sequence<N>{});
    }

    void exec() && {
        return Base::exec(_tp);
    }

    container::Try<> execNoThrow() && noexcept {
        return Base::execNoThrow(_tp);
    }
};

template <typename Db, Col... Cs>
struct MutationSqlBuild {
    using ColumnResType = void;
    using ColumnChangesResType = std::uint64_t;

    constexpr MutationSqlBuild(Db db)
        : _dbRef{db}
    {}

    template <typename... Us>
    ColumnResType exec(std::tuple<Us const&...> const& ts) {
        if (!_dbRef.isInit()) [[unlikely]] {
            _dbRef.prepareSql();
        }
        [&] <std::size_t... Idx> (std::index_sequence<Idx...>) constexpr {
            ([&]() constexpr {
                if (!_dbRef.bind(Idx + 1, std::get<Idx>(ts))) [[unlikely]] {
                    throw std::runtime_error{"Bind idx = " + std::to_string(Idx) + " failure"};
                }
            }(), ...);
        }(std::make_index_sequence<sizeof...(Us)>{});
        if (!_dbRef.exec()) [[unlikely]] {
            // 执行失败
            std::runtime_error{"SQL execution failed"};
        }
    }

    template <typename... Us>
    container::Try<ColumnResType> execNoThrow(std::tuple<Us const&...> const& ts) noexcept {
        container::Try<ColumnResType> res{};
        try {
            res.setVal(exec(ts));
            if constexpr (std::is_same_v<ColumnResType, void>) {
                res.setVal(container::NonVoidType<>{});
            }
        } catch (...) {
            res.setException(std::current_exception());
        }
        return res;
    }

    template <typename... Us>
    ColumnChangesResType execGetChanges(std::tuple<Us const&...> const& tp) {
        exec(tp);
        return _dbRef.getLastChanges();
    }

    template <typename... Us>
    container::Try<ColumnChangesResType> execNoThrowGetChanges(
        std::tuple<Us const&...> const& tp
    ) noexcept {
        container::Try<ColumnChangesResType> res;
        try {
            res.setVal(execGetChanges(tp));
        } catch (...) {
            res.setException(std::current_exception());
        }
        return res;
    }

    Db _dbRef;
};

template <typename ColumnRes>
struct MutationRes {
    std::vector<ColumnRes> columnRes;
    std::uint64_t changes;
};

template <typename Db, Col... Cs>
    requires (sizeof...(Cs) > 0)
struct MutationReturningSqlBuild {
    using ColumnResType = ColumnResult<meta::ValueWrap<Cs...>>;
    using ColumnChangesResType = MutationRes<ColumnResType>;

    constexpr MutationReturningSqlBuild(Db db)
        : _dbRef{db}
    {}

    template <typename... Us>
    std::vector<ColumnResType> exec(std::tuple<Us const&...> const& ts) {
        if (!_dbRef.isInit()) [[unlikely]] {
            _dbRef.prepareSql();
        }
        [&] <std::size_t... Idx> (std::index_sequence<Idx...>) constexpr {
            ([&]() constexpr {
                if (!_dbRef.bind(Idx + 1, std::get<Idx>(ts))) [[unlikely]] {
                    throw std::runtime_error{"Bind idx = " + std::to_string(Idx) + " failure"};
                }
            }(), ...);
        }(std::make_index_sequence<sizeof...(Us)>{});
        std::vector<ColumnResType> res;
        _dbRef.template returningForEach<Cs...>(res);
        return res;
    }

    template <typename... Us>
    container::Try<std::vector<ColumnResType>> execNoThrow(std::tuple<Us const&...> const& ts) noexcept {
        container::Try<ColumnResType> res{};
        try {
            res.setVal(exec(ts));
            if constexpr (std::is_same_v<ColumnResType, void>) {
                res.setVal(container::NonVoidType<>{});
            }
        } catch (...) {
            res.setException(std::current_exception());
        }
        return res;
    }

    template <typename... Us>
    ColumnChangesResType execGetChanges(std::tuple<Us const&...> const& tp) {
        auto&& res = exec(tp);
        return {
            std::move(res),
            _dbRef.getLastChanges()
        };
    }

    template <typename... Us>
    container::Try<ColumnChangesResType> execNoThrowGetChanges(
        std::tuple<Us const&...> const& tp
    ) noexcept {
        container::Try<ColumnChangesResType> res;
        try {
            res.setVal(execGetChanges(tp));
        } catch (...) {
            res.setException(std::current_exception());
        }
        return res;
    }

    Db _dbRef;
};

template <typename Db, typename Col, typename... Ts>
struct ReturningMutationImplBuild;

#define HX_DB_MUTATION_EXEC_IMPL                                               \
    Base::ColumnResType exec() {                                               \
        return Base::exec(_tp);                                                \
    }                                                                          \
                                                                               \
    container::Try<typename Base::ColumnResType> execNoThrow() noexcept {      \
        return Base::execNoThrow(_tp);                                         \
    }                                                                          \
                                                                               \
    Base::ColumnChangesResType execGetChanges() {                              \
        return Base::execGetChanges(_tp);                                      \
    }                                                                          \
                                                                               \
    container::Try<typename Base::ColumnChangesResType>                        \
    execNoThrowGetChanges() noexcept {                                         \
        return Base::execNoThrowGetChanges(_tp);                               \
    }

template <typename Db, Col... Cs, typename... Ts>
struct ReturningMutationImplBuild<Db, meta::ValueWrap<Cs...>, Ts...> : public MutationReturningSqlBuild<Db, Cs...> {
    using Base = MutationReturningSqlBuild<Db, Cs...>;
    using Base::Base;

    std::tuple<Ts const&...> _tp;
    inline static constexpr auto N = sizeof...(Ts);

    constexpr ReturningMutationImplBuild(Db db, Ts const&... ts)
        : Base{db}
        , _tp{ts...}
    {}

    HX_DB_MUTATION_EXEC_IMPL
};

template <typename Db, typename... Ts>
struct ReturningMutationBuild : public MutationSqlBuild<Db> {
    using Base = MutationSqlBuild<Db>;

    std::tuple<Ts const&...> _tp;
    inline static constexpr auto N = sizeof...(Ts);

    constexpr ReturningMutationBuild(Db db, Ts const&... ts)
        : Base{db}
        , _tp{ts...}
    {}

    template <auto... Cs>
        requires (sizeof...(Cs) > 0 && (meta::IsMemberPtrVal<decltype(Cs)> && ...))
    constexpr auto returning() && {
        return std::move(*this).template returning<Col{Cs}...>();
    }

    template <Col... Cs>
        requires (sizeof...(Cs) > 0)
    constexpr ReturningMutationImplBuild<Db, meta::ValueWrap<Cs...>, Ts&&...> returning() && {
        if (!this->_dbRef.isInit()) [[unlikely]] {
            this->_dbRef.sql() += " RETURNING ";
            auto mp = reflection::makeMemberPtrToNameMap<
                meta::GetMemberPtrsClassType<decltype(Cs._ptr)...>>();
            [&]() constexpr {
                ((this->_dbRef.sql() += mp.at(Cs._ptr), this->_dbRef.sql() += ','), ...);
            }();
            this->_dbRef.sql().back() = ';';
        }
        return [&] <std::size_t... I> (std::index_sequence<I...>) constexpr {
            return ReturningMutationImplBuild<Db, meta::ValueWrap<Cs...>, Ts const&...>{
                this->_dbRef, std::get<I>(_tp)...};
        }(std::make_index_sequence<N>{});
    }

    HX_DB_MUTATION_EXEC_IMPL
};

template <typename Db, typename... Ts>
struct WhereMutationBuild : public ReturningMutationBuild<Db> {
    using Base = ReturningMutationBuild<Db>;

    std::tuple<Ts const&...> _tp;
    inline static constexpr auto N = sizeof...(Ts);

    constexpr WhereMutationBuild(Db db, Ts const&... ts)
        : Base{db}
        , _tp{ts...}
    {}

    template <Expression Expr, typename... Us>
    ReturningMutationBuild<Db, Ts&&..., Us&&...> where(Us&&... us) && {
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
            return ReturningMutationBuild<Db, Ts&&..., Us&&...>{this->_dbRef,
                std::get<I>(_tp)..., std::forward<Us>(us)...};
        }(std::make_index_sequence<N>{});
    }

    HX_DB_MUTATION_EXEC_IMPL
};

#undef HX_DB_MUTATION_EXEC_IMPL

} // namespace HX::db