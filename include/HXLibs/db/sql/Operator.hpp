#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2026-01-02 12:52:54
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

#include <string_view>

namespace HX::db::op {

#define HX_DB_OP_IMPL(OP_NAME, SQL_STR)                                        \
    inline constexpr struct hx_##OP_NAME##_impl {                              \
        inline static constexpr std::string_view Sql = SQL_STR;                \
    } OP_NAME;

// 比较运算符
HX_DB_OP_IMPL(Eq, "=")
HX_DB_OP_IMPL(Ne, "<>")
HX_DB_OP_IMPL(Lt, "<")
HX_DB_OP_IMPL(Le, "<=")
HX_DB_OP_IMPL(Gt, ">")
HX_DB_OP_IMPL(Ge, ">=")

// 逻辑运算符
HX_DB_OP_IMPL(And, "AND")
HX_DB_OP_IMPL(Or, "OR")
HX_DB_OP_IMPL(Not, "NOT")

// 算术运算符, 暂时不支持 位运算
HX_DB_OP_IMPL(Add, "+")
HX_DB_OP_IMPL(Sub, "-")
HX_DB_OP_IMPL(Mul, "*")
HX_DB_OP_IMPL(Div, "/")
HX_DB_OP_IMPL(Mod, "%")

// NULL 相关
HX_DB_OP_IMPL(IsNull, "IS NULL")
HX_DB_OP_IMPL(IsNotNull, "IS NOT NULL")

// 集合
HX_DB_OP_IMPL(In, "IN")
HX_DB_OP_IMPL(NotIn, "NOT IN")

// 模糊匹配, 不包含正则表达式
HX_DB_OP_IMPL(Like, "LIKE");
HX_DB_OP_IMPL(NotLike, "NOT LIKE")

// 排序
HX_DB_OP_IMPL(Asc, "ASC");
HX_DB_OP_IMPL(Desc, "DESC");

#undef HX_DB_OP_IMPL

} // namespace HX::db::op