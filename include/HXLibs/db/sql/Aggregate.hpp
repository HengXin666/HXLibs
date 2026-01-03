#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2026-01-03 14:42:09
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

namespace agg {

#define HX_DB_AGG_OP_IMPL(OP_NAME, SQL_STR)                                    \
    struct OP_NAME {                                                           \
        static constexpr std::string toSql(std::string name) noexcept {        \
            name = SQL_STR "(" + std::move(name);                              \
            name += ")";                                                       \
            return name;                                                       \
        }                                                                      \
    };

// 聚合函数名
HX_DB_AGG_OP_IMPL(Sum, "SUM")
HX_DB_AGG_OP_IMPL(Avg, "AVG")
HX_DB_AGG_OP_IMPL(Min, "MIN")
HX_DB_AGG_OP_IMPL(Max, "MAX")
HX_DB_AGG_OP_IMPL(Count, "COUNT") // 统计列中非 NULL 的行数

#undef HX_DB_AGG_OP_IMPL

// 统计行数, 不管 NULL
struct CountAll {
    static constexpr std::string toSql(std::string_view) noexcept {
        return "COUNT(*)";
    }
};

// 统计列中不同值的数量
struct CountDistinct {
    static constexpr std::string toSql(std::string name) noexcept {
        name = "COUNT(DISTINCT " + std::move(name);
        name += ")";
        return name;
    }
};

} // namespace agg

template <Col C, typename Func>
struct AggregateFunc {
    using AggFunc = Func;
    inline static constexpr auto ColVal = C;
};

template <typename T>
constexpr bool IsAggregateFuncVal = false;

template <Col C, typename Func>
constexpr bool IsAggregateFuncVal<AggregateFunc<C, Func>> = true;

#define HX_DB_AGG_FUNC_IMPL(FUNC_NAME, AGG_NAME)                               \
    template <Col C>                                                           \
    inline constexpr AggregateFunc<C, agg::AGG_NAME> FUNC_NAME{};

HX_DB_AGG_FUNC_IMPL(sum, Sum)
HX_DB_AGG_FUNC_IMPL(avg, Avg)
HX_DB_AGG_FUNC_IMPL(min, Min)
HX_DB_AGG_FUNC_IMPL(max, Max)
HX_DB_AGG_FUNC_IMPL(count, Count)
HX_DB_AGG_FUNC_IMPL(countAll, CountAll)
HX_DB_AGG_FUNC_IMPL(countDistinct, CountDistinct)

#undef HX_DB_AGG_FUNC_IMPL

} // namespace HX::db