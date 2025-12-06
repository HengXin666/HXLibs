#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-11-29 22:38:47
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

#include <HXLibs/db/SQLiteMeta.hpp>
#include <HXLibs/meta/MemberPtrType.hpp>
#include <HXLibs/meta/TypeTraits.hpp>
#include <HXLibs/reflection/MemberPtr.hpp>
#include <HXLibs/reflection/TypeName.hpp>

namespace HX::db {

struct MakeSqlStr {
    template <typename T, bool IsSetPrimaryKey = false>
    static std::string makeInsertSql() noexcept {
        using U = meta::RemoveCvRefType<T>;
        std::string sqlName = "INSERT INTO ";
        sqlName += reflection::getTypeName<U>();
        sqlName += " (";
        constexpr auto& obj = reflection::internal::getStaticObj<U>();
        constexpr auto N = reflection::membersCountVal<U>;
        std::string sqlVal = ") VALUES (";
        reflection::forEach(obj, [&] <std::size_t Idx> (
            std::index_sequence<Idx>, std::string_view name, auto&& val
        ) constexpr {
            using Type = meta::RemoveCvRefType<decltype(val)>;
            if constexpr (isPrimaryKeyVal<Type> && !IsSetPrimaryKey) {
                // 主键不会进行指定
                return;
            } else {
                sqlName += name;
                sqlVal += '?';
                if constexpr (Idx + 1 != N) {
                    sqlVal += ',';
                    sqlName += ',';
                }
            }
        });
        sqlName += std::move(sqlVal);
        sqlName += ");";
        return sqlName;
    }

    template <typename T, typename... MemberPtr>
        requires (sizeof...(MemberPtr) >= 1 && (meta::IsMemberPtrVal<MemberPtr> && ...))
    static std::string makeInsertSql(MemberPtr... ptrs) noexcept {
        using U = meta::RemoveCvRefType<T>;
        std::string sqlName = "INSERT INTO ";
        sqlName += reflection::getTypeName<U>();
        sqlName += " (";
        constexpr auto& obj = reflection::internal::getStaticObj<U>();
        constexpr auto N = reflection::membersCountVal<U>;
        std::string sqlVal = ") VALUES (";
        auto map = reflection::makeMemberPtrToNameMap<U>();
        (([&](){
            sqlName += map.at(ptrs);
            sqlName += ',';
            sqlVal += '?';
            sqlVal += ',';
        }()), ...);
        sqlName.pop_back();
        sqlVal.back() = ')';
        sqlName += std::move(sqlVal);
        sqlName += ';';
        return sqlName;
    }

    template <typename T, bool IsSetPrimaryKey = false>
    static std::string makeUpdateSqlFragment() noexcept {
        using U = meta::RemoveCvRefType<T>;
        std::string sql = "UPDATE ";
        sql += reflection::getTypeName<U>();
        sql += " SET ";
        constexpr auto& obj = reflection::internal::getStaticObj<U>();
        constexpr auto N = reflection::membersCountVal<U>;
        reflection::forEach(obj, [&] <std::size_t Idx> (
            std::index_sequence<Idx>, std::string_view name, auto&& val
        ) constexpr {
            using Type = meta::RemoveCvRefType<decltype(val)>;
            if constexpr (isPrimaryKeyVal<Type> && !IsSetPrimaryKey) {
                // 主键不会进行指定
                return;
            } else {
                sql += name;
                sql += '=';
                sql += '?';
                if constexpr (Idx + 1 != N) {
                    sql += ',';
                } else {
                    sql += ' ';
                }
            }
        });
        return sql;
    }

    template <typename T, typename... MemberPtr>
        requires (sizeof...(MemberPtr) >= 1 && (meta::IsMemberPtrVal<MemberPtr> && ...))
    static std::string makeUpdateSqlFragment(MemberPtr... ptrs) noexcept {
        using U = meta::RemoveCvRefType<T>;
        std::string sql = "UPDATE ";
        sql += reflection::getTypeName<U>();
        sql += " SET ";
        auto map = reflection::makeMemberPtrToNameMap<U>();
        (([&](){
            sql += map.at(ptrs);
            sql += '=';
            sql += '?';
            sql += ',';
        }()), ...);
        sql.back() = ' ';
        return sql;
    }
};

} // namespace HX::db