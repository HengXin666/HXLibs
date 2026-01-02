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

#include <string>

#include <HXLibs/db/sql/Column.hpp>
#include <HXLibs/db/sql/Expression.hpp>

namespace HX::db {

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

    }
};

template <typename Db>
struct OrderByBuild : public LimitBuild<Db> {
    using Base = LimitBuild<Db>;
    using Base::Base;

    template <Expression Expr>
    LimitBuild<Db> orderBy() && {
        return {this->_dbRef, std::move(this->_sql)};
    }
};

template <typename Db>
struct GroupByBuild : public OrderByBuild<Db> {
    using Base = OrderByBuild<Db>;
    using Base::Base;


    template <auto... Ptrs>
    OrderByBuild<Db> groupBy() && {
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
        return {this->_dbRef, std::move(this->_sql)};
    }
};

template <typename Db>
struct FromBuild : public SqlBuild<Db> {
    using Base = SqlBuild<Db>;
    using Base::Base;

    template <typename Table>
    constexpr JoinOrWhereBuild<Db> from() && {
        return {this->_dbRef, std::move(this->_sql)};
    }
};

template <typename Db>
struct SelectBuild : public SqlBuild<Db> {
    using Base = SqlBuild<Db>;
    using Base::Base;

    template <Col... Cs>
    constexpr FromBuild<Db> select() && {
        return {this->_dbRef, std::move(this->_sql)};
    }
};

} // namespace HX::db