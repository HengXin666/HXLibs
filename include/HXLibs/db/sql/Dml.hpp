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
#include <HXLibs/db/sql/Table.hpp>
#include <HXLibs/db/sql/Column.hpp>
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
constexpr std::string ExprToString() noexcept {
    std::string res{};
    
    return res;
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
    void limit() {
        log::hxLog.info("sql:", this->_sql);
    }
};

template <typename Db>
struct OrderByBuild : public LimitBuild<Db> {
    using Base = LimitBuild<Db>;
    using Base::Base;

    template <OrderType... Orders>
    LimitBuild<Db> orderBy() && {
        return {this->_dbRef, std::move(this->_sql)};
    }
};

template <typename Db>
struct HavingBuild : public OrderByBuild<Db> {
    using Base = OrderByBuild<Db>;
    using Base::Base;

    template <Expression Expr>
    OrderByBuild<Db> having() && {
        return {this->_dbRef, std::move(this->_sql)};
    }
};

template <typename Db>
struct GroupByBuild : public HavingBuild<Db> {
    using Base = HavingBuild<Db>;
    using Base::Base;

    template <auto... Ptrs>
    HavingBuild<Db> groupBy() && {
        return {this->_dbRef, std::move(this->_sql)};
    }

    template <Col... Cs>
    HavingBuild<Db> groupBy() && {
        return {this->_dbRef, std::move(this->_sql)};
    }

    template <Expression Expr>
    HavingBuild<Db> groupBy() && {
        return {this->_dbRef, std::move(this->_sql)};
    }
};

template <typename Db>
struct WhereBuild : public GroupByBuild<Db> {
    using Base = GroupByBuild<Db>;
    using Base::Base;
    
    template <Expression Expr>
    GroupByBuild<Db> where() && {
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