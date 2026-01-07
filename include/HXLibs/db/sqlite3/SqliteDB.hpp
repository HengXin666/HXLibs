#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2026-01-07 22:16:46
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

#include <sqlite3.h>

#include <HXLibs/db/sql/DataBase.hpp>
#include <HXLibs/db/sqlite3/SqliteStmt.hpp>

namespace HX::db::sqlite {

struct SqliteDB : public DataBase<SqliteDB>, public DataBaseInterface<SqliteDB> {
    using StmtType = SqliteStmt;
    using Base = DataBase<SqliteDB>;
    using Base::Base;

    SqliteDB(std::string_view filePath)
        : DataBase{}
    {
        if (::sqlite3_open(filePath.data(), &_db) != SQLITE_OK) [[unlikely]] {
            throw std::runtime_error{
                "Failed to open database: " + std::string{::sqlite3_errmsg(_db)}
            };
        }
    }

    bool exec() noexcept {
        return true;
    }

    std::size_t getLastChanges() const noexcept {
        return 0;
    }

    template <typename T, std::size_t Idx>
    void bind(T&& t) {
        (void)t;
    }

    void clearBind() {

    }
private:
    ::sqlite3* _db{};
};

} // namespace HX::db::sqlite