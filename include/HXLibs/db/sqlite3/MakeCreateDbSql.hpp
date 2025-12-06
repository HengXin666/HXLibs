#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-12-06 18:24:34
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

#include <HXLibs/meta/ContainerConcepts.hpp>
#include <HXLibs/db/sql/Constraint.hpp>
#include <HXLibs/db/sql/CustomType.hpp>
#include <HXLibs/reflection/MemberName.hpp>
#include <HXLibs/reflection/MemberPtr.hpp>
#include <HXLibs/reflection/TypeName.hpp>
#include <HXLibs/log/serialize/ToString.hpp>

namespace HX::db::sqlite3 {

struct CreateDbSql {
    template <typename T>
    static constexpr std::string_view getSqlTypeStr() noexcept {
        if constexpr (std::is_integral_v<T>) {
            return "INTEGER";
        } else if constexpr (std::is_floating_point_v<T>) {
            return "REAL";
        } else if constexpr (meta::StringType<T>) {
            return "TEXT";
        } else {
            // 不支持该类型
            static_assert(!sizeof(T), "type is not sql type");
        }
    }

    template <typename T>
        requires (IsCustomTypeVal<T>)
    static constexpr std::string_view getSqlTypeStr() noexcept {
        if constexpr (std::is_integral_v<T>) {
            return "INTEGER";
        } else if constexpr (std::is_floating_point_v<T>) {
            return "REAL";
        } else {
            return "TEXT";
        }
    }

    template <typename T, typename... ConstraintTypes>
    static constexpr void setConstraints(
        std::string_view selfName,
        Constraint<T, ConstraintTypes...>&,
        std::string& mainSql,
        std::string& foreignKeySql
    ) {
        mainSql += getSqlTypeStr<T>();
        mainSql += ' ';
        ([&] <typename Constraint> (Constraint) {
            if constexpr (std::is_same_v<Constraint, attr::NotNull>) {
                mainSql += "NOT NULL";
            } else if constexpr (std::is_same_v<Constraint, attr::PrimaryKey>) {
                mainSql += "PRIMARY KEY";
            } else if constexpr (std::is_same_v<Constraint, attr::Unique>) {
                mainSql += "UNIQUE";
            } else if constexpr (std::is_same_v<Constraint, attr::AutoIncrement>) {
                mainSql += "AUTOINCREMENT";
            } else if constexpr (std::is_same_v<Constraint, attr::Index>) {
                // SQLite 语法规定 索引必须独立语句, 不能写在创建表中.
                return;
            } else if constexpr (attr::IsDefaultValTypeVal<Constraint>) {
                [&] <auto Value> (attr::DefaultVal<Value>) {
                    mainSql += "DEFAULT ";
                    mainSql += log::toString(Value);
                }(Constraint{});
            } else if constexpr (attr::IsForeignVal<Constraint>) {
                [&] <auto... Keys> (attr::Foreign<Keys...>) constexpr {
                    ([&] (auto Key) constexpr {
                        using PtrType = decltype(Key);
                        using ClassType = meta::GetMemberPtrClassType<PtrType>;
                        auto map = reflection::makeMemberPtrToNameMap<ClassType>();
                        auto name = map.at(Key);
                        foreignKeySql += "FOREIGN KEY(";
                        foreignKeySql += selfName;
                        foreignKeySql += ") REFERENCES ";
                        foreignKeySql += reflection::getTypeName<ClassType>();
                        foreignKeySql += '(';
                        foreignKeySql += name;
                        foreignKeySql += "),";
                    }(Keys), ...);
                }(Constraint{});
                return;
            }
            mainSql += ' ';
        }(ConstraintTypes{}), ...);
    }

    template <typename T>
    static std::string createDatabase(T&& t) noexcept {
        using Table = meta::RemoveCvRefType<T>;
        std::string sql = "CREATE TABLE ";
        std::string foreignKeySql{};
        sql += reflection::getTypeName<Table>();
        sql += " (";
        reflection::forEach(t, [&] <std::size_t Idx> (
            std::index_sequence<Idx>, auto name, auto&& val
        ) {
            sql += name;
            sql += ' ';
            using Type = meta::RemoveCvRefType<decltype(val)>;
            if constexpr (IsConstraintVal<Type>) {
                setConstraints(name, val, sql, foreignKeySql);
                sql.back() = ',';
            } else {
                sql += getSqlTypeStr<Type>();
                sql += ',';
            }
        });
        sql += foreignKeySql;
        sql.back() = ')';
        sql += ';';
        return sql;
    }
};


} // namespace HX::db::sqlite3