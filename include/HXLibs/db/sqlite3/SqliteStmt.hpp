#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-11-29 22:37:36
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
#include <string_view>
#include <stdexcept>

#include <sqlite3.h>

#include <HXLibs/meta/ContainerConcepts.hpp>
#include <HXLibs/db/sql/Stmt.hpp>
#include <HXLibs/db/sql/Constraint.hpp>
#include <HXLibs/db/sql/CustomType.hpp>

namespace HX::db::sqlite {

class SqliteStmt : Stmt<SqliteStmt> {
#define HX_DB_STMT_IMPL
public:
    SqliteStmt() = default;

    SqliteStmt(SqliteStmt&) = delete;
    SqliteStmt(SqliteStmt&& that) noexcept
        : _stmt{that._stmt}
    {
        that._stmt = nullptr;
    }

    void prepareSql(std::string_view sql, ::sqlite3* db) {
        // 预编译
        if (::sqlite3_prepare_v2(
            db, sql.data(), static_cast<int>(sql.size()),
            &_stmt, nullptr) != SQLITE_OK
        ) [[unlikely]] {
            throw std::runtime_error{
                "Failed to prepare statement: " 
                + std::string(::sqlite3_errmsg(db))
                + "\n In: " + std::string{sql}
            };
        }
    }

    /**
     * @brief 执行 stmt
     * @return int 
     */
    int step() noexcept {
        return ::sqlite3_step(_stmt);
    }

    template <typename Type>
    auto selectForEachBuild(int idx) {
        if constexpr (std::is_integral_v<Type> && !meta::IsInt64Val<Type>) {
            return static_cast<Type>(::sqlite3_column_int(_stmt, idx));
        } else if constexpr (meta::IsInt64Val<Type>) {
            return static_cast<Type>(::sqlite3_column_int64(_stmt, idx));
        } else if constexpr (std::is_floating_point_v<Type>) {
            return static_cast<Type>(::sqlite3_column_double(_stmt, idx));
        } else if constexpr (meta::StringType<Type>) {
            auto* str = reinterpret_cast<const char *>(
                ::sqlite3_column_text(_stmt, idx)
            );
            auto len = ::sqlite3_column_bytes(_stmt, idx);
            return Type{str, static_cast<std::size_t>(len)};
        } else if constexpr (std::is_same_v<Type, Blob>) {
            auto blob = ::sqlite3_column_blob(_stmt, idx);
            auto len = ::sqlite3_column_bytes(_stmt, idx);
            return Type(blob, static_cast<std::size_t>(len));
        } else if constexpr (IsCustomTypeVal<Type>) {
            auto* str = reinterpret_cast<const char *>(
                ::sqlite3_column_text(_stmt, idx)
            );
            auto len = ::sqlite3_column_bytes(_stmt, idx);
            return CustomType<Type>::fromSql({str, len});
        } else {
            // 不支持该类型
            static_assert(!sizeof(Type), "type is not sql type");
        }
    }

    template <auto... Vs>
    HX_DB_STMT_IMPL void selectForEach(std::vector<ColumnResult<meta::ValueWrap<Vs...>>>& resArr) {
        for (int rc = step(); rc == SQLITE_ROW; rc = step()) {
            [&] <std::size_t... Idx> (std::index_sequence<Idx...>) {
                resArr.emplace_back([&] {
                    using Res = typename decltype(internal::toCol<Vs>())::Type;
                    using Type = meta::RemoveOptionalWrapType<Res>;
                    if constexpr (meta::IsOptionalVal<Type>) {
                        if (sqlite3_column_type(_stmt, static_cast<int>(Idx)) == SQLITE_NULL) {
                            return Res{};
                        }
                        return Res{selectForEachBuild<Type>(static_cast<int>(Idx))};
                    } else {
                        return selectForEachBuild<Type>(static_cast<int>(Idx));
                    }
                }()...);
            }(std::make_index_sequence<sizeof...(Vs)>{});
        }
    }

    HX_DB_STMT_IMPL bool exec() noexcept {
        int res = step();
        return res == SQLITE_DONE || res == SQLITE_ROW;
    }

    /**
     * @brief 获取最后一次成功的操作修改的行数
     * @return auto (int) 修改的行数
     */
    HX_DB_STMT_IMPL auto getLastChanges() const noexcept {
        return ::sqlite3_changes(::sqlite3_db_handle(_stmt));
    }

    /**
     * @brief 重置 stmt
     */
    HX_DB_STMT_IMPL void reset() {
        if (::sqlite3_reset(_stmt) != SQLITE_OK) [[unlikely]] {
            throw std::runtime_error{"reset: " + getErrMsg()};
        }
    }

    template <typename T>
    HX_DB_STMT_IMPL bool bind(std::size_t idx, T&& t) {
        auto i = static_cast<int>(idx);
        return stmtBind(std::forward<T>(t), [&] <typename Type> (Type const& data) {
            using SqlType = meta::RemoveCvRefType<Type>;
            if constexpr (std::is_integral_v<SqlType> && !meta::IsInt64Val<SqlType>) {
                return SQLITE_OK == ::sqlite3_bind_int(_stmt, i, data);
            } else if constexpr (meta::IsInt64Val<SqlType>) {
                return SQLITE_OK == ::sqlite3_bind_int64(_stmt, i, data);
            } else if constexpr (std::is_floating_point_v<SqlType>) {
                return SQLITE_OK == ::sqlite3_bind_double(_stmt, i, data);
            } else if constexpr (meta::StringType<SqlType>) {
                return SQLITE_OK == ::sqlite3_bind_text(_stmt, i, data.data(), static_cast<int>(data.size()), nullptr);
            } else if constexpr (std::is_same_v<SqlType, Blob>) {
                return SQLITE_OK == ::sqlite3_bind_blob(_stmt, i, data.data(), static_cast<int>(data.size()), nullptr);
            } else if constexpr (IsCustomTypeVal<SqlType>) {
                auto const& sqlData = CustomType<Type>::toSql(data);
                return SQLITE_OK == ::sqlite3_bind_text(_stmt, i, sqlData.data(), static_cast<int>(sqlData.size()), nullptr);
            } else {
                // 不支持该类型
                static_assert(!sizeof(SqlType), "type is not sql type");
            }
        }, [&] {
            return SQLITE_OK == ::sqlite3_bind_null(_stmt, i);
        });
    }

    /**
     * @brief 返回已修改/插入行的数据
     * @tparam Vs 需要返回 字段
     * @return ColumnResult<meta::ValueWrap<Vs...>> 
     */
    template <auto... Vs>
    HX_DB_STMT_IMPL ColumnResult<meta::ValueWrap<Vs...>> returning() {
        return [&] <std::size_t... Idx> (std::index_sequence<Idx...>) constexpr {
            return ColumnResult<meta::ValueWrap<Vs...>>([&]() constexpr {
                using Res = typename decltype(internal::toCol<Vs>())::Type;
                using Type = meta::RemoveOptionalWrapType<Res>;
                if constexpr (meta::IsOptionalVal<Type>) {
                    if (sqlite3_column_type(_stmt, static_cast<int>(Idx)) == SQLITE_NULL) {
                        return Res{};
                    }
                    return Res{selectForEachBuild<Type>(static_cast<int>(Idx))};
                } else {
                    return selectForEachBuild<Type>(static_cast<int>(Idx));
                }
            }()...);
        }(std::make_index_sequence<sizeof...(Vs)>{});
    }

    /**
     * @brief 获取错误字符串
     * @return std::string 
     */
    std::string getErrMsg() const noexcept {
        return ::sqlite3_errmsg(::sqlite3_db_handle(_stmt));
    }

    SqliteStmt& operator=(SqliteStmt&) = delete;
    SqliteStmt& operator=(SqliteStmt&& that) noexcept {
        if (this == std::addressof(that)) {
            return *this;
        }
        std::swap(that._stmt, _stmt);
        return *this;
    }

    ~SqliteStmt() noexcept {
        if (_stmt) [[likely]] {
            ::sqlite3_finalize(_stmt);
        }
    }
private:
    ::sqlite3_stmt* _stmt{};
#undef HX_DB_STMT_IMPL
};

} // namespace HX::db::sqlite