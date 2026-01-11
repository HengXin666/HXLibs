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
#include <HXLibs/db/sql/SqlType.hpp>
#include <HXLibs/db/sql/Constraint.hpp>
#include <HXLibs/db/sql/CustomType.hpp>
#include <HXLibs/reflection/MemberName.hpp>
#include <HXLibs/reflection/MemberPtr.hpp>
#include <HXLibs/reflection/TypeName.hpp>
#include <HXLibs/reflection/EnumName.hpp>
#include <HXLibs/log/serialize/ToString.hpp>

namespace HX::db::sqlite {

struct CreateDbSql {
    template <typename Type, typename Lambda>
    static constexpr std::string_view getSqlTypeStrImpl(Lambda&& doNotTypeFunc) {
        using namespace std::string_view_literals;
        using T = meta::RemoveOptionalWrapType<Type>;
        if constexpr (std::is_integral_v<T>) {
            return "INTEGER"sv;
        } else if constexpr (std::is_floating_point_v<T>) {
            return "REAL"sv;
        } else if constexpr (meta::StringType<T>) {
            return "TEXT"sv;
        } else if constexpr (std::is_same_v<T, db::Date>) {
            return "DATE"sv;
        } else if constexpr (std::is_same_v<T, db::Time>) {
            return "TIME"sv;
        } else if constexpr (std::is_same_v<T, db::Timestamp>) {
            static_assert(!sizeof(T), "type is not sql type");
        } else if constexpr (std::is_same_v<T, db::Blob>) {
            return "BLOB"sv;
        } {
            return std::forward<Lambda>(doNotTypeFunc)();
        }
    }

    template <typename T>
    static constexpr std::string_view getSqlTypeStr() noexcept {
        return getSqlTypeStrImpl<T>([] {
            // 不支持该类型
            static_assert(!sizeof(T), "type is not sql type");
        });
    }

    template <typename T>
        requires (IsCustomTypeVal<T>)
    static constexpr std::string_view getSqlTypeStr() noexcept {
        return getSqlTypeStrImpl<T>([] {
            using namespace std::string_view_literals;
            return "TEXT"sv;
        });
    }

    template <typename T, typename... ConstraintTypes>
    static constexpr std::size_t setConstraints(
        std::string_view selfName,
        Constraint<T, ConstraintTypes...>&,
        std::string& mainSql,
        std::string& foreignKeySql
    ) {
        std::size_t primaryKeyCnt = 0;
        mainSql += getSqlTypeStr<T>();
        mainSql += ' ';
        ([&] <typename Constraint> (Constraint) {
            if constexpr (std::is_same_v<Constraint, attr::NotNull>) {
                mainSql += "NOT NULL";
            } else if constexpr (attr::IsPrimaryKeyVal<Constraint>) {
                [] <auto... KPs> (attr::PrimaryKey<KPs...>) {
                    // Constraint 中只能使用 attr::PrimaryKey<> 声明
                    static_assert(sizeof...(KPs) == 0,
                        "Only attr::PrimaryKey<> declaration can be used in Constraint");
                }(Constraint{});
                mainSql += "PRIMARY KEY";
                ++primaryKeyCnt;
            } else if constexpr (std::is_same_v<Constraint, attr::Unique>) {
                mainSql += "UNIQUE";
            } else if constexpr (std::is_same_v<Constraint, attr::AutoIncrement>) {
                mainSql += "AUTOINCREMENT";
            } else if constexpr (attr::IsIndexVal<Constraint>) {
                // SQLite 语法规定 索引必须独立语句, 不能写在创建表中.
                return;
            } else if constexpr (attr::IsDefaultValTypeVal<Constraint>) {
                [&] <auto Value> (attr::DefaultVal<Value>) {
                    mainSql += "DEFAULT ";
                    mainSql += log::toString(Value);
                }(Constraint{});
            } else if constexpr (attr::IsForeignVal<Constraint>) {
                [&] <auto Key> (attr::Foreign<Key>) {
                    // 确保外键约束的类型和本类字段的类型一致
                    static_assert(std::is_same_v<T, RemoveConstraintToType<
                        meta::GetMemberPtrType<decltype(Key)>>>,
                        "The foreign key type is inconsistent with the field type of this type");

                    using ForeignTable = meta::GetMemberPtrsClassType<decltype(Key)>;
                    std::string keySql = "FOREIGN KEY(";
                    std::string refSql = " REFERENCES ";
                    refSql += reflection::getTypeName<ForeignTable>();
                    refSql += '(';

                    auto map = reflection::makeMemberPtrToNameMap<ForeignTable>();
                    auto name = map.at(Key);
                    keySql += selfName;
                    keySql += ',';
                    refSql += name;
                    refSql += ',';
                    keySql.back() = ')';
                    refSql.back() = ')';
                    refSql += ',';
                    foreignKeySql += keySql;
                    foreignKeySql += refSql;
                }(Constraint{});
                return;
            } else if constexpr (attr::IsUnionForeignVal<Constraint>) {
                // 不应该在类中使用 UnionForeign, 应该在嵌套类 UnionAttr 中使用
                static_assert(!sizeof(T),
                    "UnionForeign should not be used in the class, "
                    "but should be used in the nested class UnionAttr");
            }
            mainSql += ' ';
        }(ConstraintTypes{}), ...);
        return primaryKeyCnt;
    }

    template <typename T, bool IsNotExists = true>
    static std::string createDatabase(T&& t) {
        using Table = meta::RemoveCvRefType<T>;
        std::size_t primaryKeyCnt = 0;
        std::string sql = "CREATE TABLE ";
        if constexpr (IsNotExists) {
            sql += "IF NOT EXISTS ";
        }
        std::string foreignKeySql{};
        sql += reflection::getTypeName<Table>();
        sql += " (";
        reflection::forEach(t, [&] (auto, auto name, auto&& val) {
            sql += name;
            sql += ' ';
            using Type = meta::RemoveCvRefType<decltype(val)>;
            if constexpr (IsConstraintVal<Type>) {
                primaryKeyCnt += setConstraints(name, val, sql, foreignKeySql);
                sql.back() = ',';
            } else {
                sql += getSqlTypeStr<Type>();
                sql += ',';
            }
        });
        
        // 嵌套类
        if constexpr (attr::HasUnionAttr<Table>) {
            reflection::forEach(typename Table::UnionAttr{}, [&] (auto, auto, auto&& val) {
                using Type = meta::RemoveCvRefType<decltype(val)>;
                if constexpr (attr::IsUnionForeignVal<Type>) {
                    [&] <auto... InternalKeyPtrs, auto... ForeignKeyPtrs> (
                        attr::UnionForeign<attr::ForeignMap<InternalKeyPtrs, ForeignKeyPtrs>...>
                    ) {
                        // 内键应该来源于本表
                        static_assert(std::is_same_v<
                            Table, meta::GetMemberPtrsClassType<decltype(InternalKeyPtrs)...>>, 
                            "Internal key should come from this table");
                        using ForeignTable = meta::GetMemberPtrsClassType<decltype(ForeignKeyPtrs)...>;
                        // 外键应该来自同一个表
                        static_assert(!std::is_void_v<ForeignTable>,
                            "The foreign key should come from the same table");
                        // 内外键对应的类型应该相同 (ForeignMap 类型默认保证)
                        std::string keySql = "FOREIGN KEY(";
                        std::string refSql = " REFERENCES ";
                        refSql += reflection::getTypeName<ForeignTable>();
                        refSql += '(';
                        
                        auto selfNameMap = reflection::makeMemberPtrToNameMap<Table>();
                        auto foreignNameMap = reflection::makeMemberPtrToNameMap<ForeignTable>();
                        ([&] (auto iKey, auto fKey) {
                            keySql += selfNameMap.at(iKey);
                            keySql += ',';
                            refSql += foreignNameMap.at(fKey);
                            refSql += ',';
                        }(InternalKeyPtrs, ForeignKeyPtrs), ...);

                        keySql.back() = ')';
                        refSql.back() = ')';
                        refSql += ',';
                        foreignKeySql += keySql;
                        foreignKeySql += refSql;
                    }(Type{});
                } else if constexpr (attr::IsPrimaryKeyVal<Type>) {
                    auto selfNameMap = reflection::makeMemberPtrToNameMap<Table>();
                    [&] <auto... KeyPtrs> (attr::PrimaryKey<KeyPtrs...>) {
                        // 主键参数不能为空
                        static_assert(sizeof...(KeyPtrs) >= 1,
                            "Primary key parameter cannot be empty");
                        // 主键字段应该来自本类
                        static_assert(std::is_same_v<Table, 
                            meta::GetMemberPtrsClassType<decltype(KeyPtrs)...>>,
                            "The primary key field should come from this class");
                        ++primaryKeyCnt;
                        sql += "PRIMARY KEY(";
                        ((sql += selfNameMap.at(KeyPtrs), sql += ','), ...);
                        sql.back() = ')';
                        sql += ',';
                    }(Type{});
                }
            });
        }

        if (primaryKeyCnt > 1) [[unlikely]] {
            // 只能声明一个主键
            throw std::runtime_error{"Only one primary key can be declared"};
        }

        sql += foreignKeySql;
        sql.back() = ')';
        sql += ';';
        return sql;
    }

    template <typename... Keys>
    static void setSingleIndexOrder(std::string& sql, attr::Index<Keys...>) {
        constexpr std::size_t N = sizeof...(Keys);
        // 不应该在成员变量 Constraint<> 中书写聚合索引, 应该单独写在嵌套类 UnionAttr 中
        static_assert(N <= 1,
            "The aggregate index should not be written in the member variable Constraint<>, "
            "but should be written separately in the nested class UnionAttr"); 
        if constexpr (N == 1) {
            using ClassType = meta::GetMemberPtrsClassType<typename Keys::KeyType...>;
            // 不能指定 成员指针, 应该使用默认 Index<> / AseIndex / DescIndex 类型
            static_assert(std::is_same_v<ClassType, attr::internal::Place>,
                "The member pointer cannot be specified, "
                "and the default Index<>/AseIndex/DescIndex type should be used");
            ((sql += reflection::toEnumName<attr::Order, Keys::IdxOrder>()), ...);
        } else {
            sql += "Asc";
        }
    }

    template <typename T>
    static std::vector<std::string> createIndex(T&& t) {
        using Table = meta::RemoveCvRefType<T>;
        auto sqls = std::vector<std::string>{};
#if defined(_MSC_VER)
        // https://github.com/HengXin666/HXLibs/issues/17 (MSVC ICE)
        const auto tableName = reflection::getTypeName<Table>();
#else
        constexpr auto tableName = reflection::getTypeName<Table>();
#endif // !defined(_MSC_VER)
        reflection::forEach(t, [&] (auto, auto name, auto&& val) {
            using Type = meta::RemoveCvRefType<decltype(val)>;
            if constexpr (IsConstraintVal<Type>) {
                [&] <typename U, typename... ConstraintTypes> (Constraint<U, ConstraintTypes...>&) {
                    ([&] <typename Constraint> (Constraint) {
                        if constexpr (attr::IsIndexVal<Constraint>) {
                            std::string sql = "CREATE INDEX idx_";
                            sql += tableName;
                            sql += '_';
                            sql += name;
                            sql += " ON ";
                            sql += tableName;
                            sql += '(';
                            sql += name;
                            sql += ' ';
                            setSingleIndexOrder(sql, Constraint{});
                            sql += ");";
                            sqls.push_back(std::move(sql));
                        }
                    }(ConstraintTypes{}), ...);
                }(val);
            }
        });
        // 处理嵌套类
        if constexpr (attr::HasUnionAttr<Table>) {
            auto map = reflection::makeMemberPtrToNameMap<Table>();
            reflection::forEach(typename Table::UnionAttr{}, [&] (auto, auto, auto&& val) {
                using Type = meta::RemoveCvRefType<decltype(val)>;
                if constexpr (attr::IsIndexVal<Type>) {
                    // 未明确索引目标字段, 需使用 attr::IndexOrder<&本类::成员> 明确
                    static_assert(meta::IsTypeNotInTypesVal<
                        Type, attr::Index<>, attr::DescIndex, attr::AseIndex>, 
                        "The target field is not explicitly indexed. "
                        "You need to use attr::IndexOrder<&ThisClass::Member> explicitly");
                    [&] <typename... Keys> (attr::Index<Keys...>) {
                        // 要求 Index 都是来自同一个类的 
                        static_assert(!std::is_void_v<meta::GetMemberPtrsClassType<
                            attr::internal::GetIndexOrderKeyType<Keys>...>>,
                            "It is required that all indexes are from the same class");
                        std::string sql = "CREATE INDEX idx_";
                        std::string idxSql = "(";
                        sql += tableName;
                        ([&] <auto KeyPtr, attr::Order SortOrder> (attr::IndexOrder<KeyPtr, SortOrder>) {
                            auto name = map.at(KeyPtr);
                            sql += '_';
                            sql += name;
                            idxSql += name;
                            idxSql += ' ';
                            idxSql += reflection::toEnumName<attr::Order, SortOrder>();
                            idxSql += ',';
                        }(Keys{}), ...);
                        sql += " ON ";
                        sql += tableName;
                        idxSql.back() = ')';
                        idxSql += ';';
                        sql += idxSql;
                        sqls.push_back(std::move(sql));
                    }(val);
                }
            });
        }
        return sqls;
    }
};

} // namespace HX::db::sqlite