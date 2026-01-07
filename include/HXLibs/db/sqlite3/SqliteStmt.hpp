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

namespace HX::db::sqlite {

class SqliteStmt {
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
    int step() const noexcept {
        return ::sqlite3_step(_stmt);
    }

    /**
     * @brief 获取最后一次成功插入的 主键 Id
     * @param db 
     * @return auto 
     */
    auto getLastInsertPrimaryKeyId() const noexcept {
        return ::sqlite3_last_insert_rowid(::sqlite3_db_handle(_stmt));;
    }

    /**
     * @brief 获取最后一次成功的操作修改的行数
     * @return auto (int) 修改的行数
     */
    auto getLastChanges() const noexcept {
        return ::sqlite3_changes(::sqlite3_db_handle(_stmt));
    }

    /**
     * @brief 重置 stmt
     */
    void reset() {
        if (::sqlite3_reset(_stmt) != SQLITE_OK) [[unlikely]] {
            throw std::runtime_error{"reset: " + getErrMsg()};
        }
    }

    /**
     * @brief 清除绑定值
     */
    void clearBind() {
        if (::sqlite3_clear_bindings(_stmt) != SQLITE_OK) [[unlikely]] {
            throw std::runtime_error{"clearBind: " + getErrMsg()};
        }
    }

    /**
     * @brief 获取错误字符串
     * @return std::string 
     */
    std::string getErrMsg() const noexcept {
        return ::sqlite3_errmsg(::sqlite3_db_handle(_stmt));
    }

    operator ::sqlite3_stmt*() noexcept {
        return _stmt;
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
};

} // namespace HX::db::sqlite