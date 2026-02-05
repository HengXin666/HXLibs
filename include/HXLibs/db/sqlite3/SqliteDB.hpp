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
#include <HXLibs/db/sqlite3/MakeCreateDbSql.hpp>

namespace HX::db::sqlite {

struct SqliteDB : public DataBaseCrtp<SqliteDB>, private DataBaseInterface<SqliteDB> {
    using StmtType = SqliteStmt;
    using Base = DataBaseCrtp<SqliteDB>;
    using Base::Base;

    SqliteDB(std::string_view filePath)
        : DataBaseCrtp{}
        , DataBaseInterface{}
    {
        open(filePath);
    }

    SqliteDB& operator=(SqliteDB&& that) noexcept {
        static_cast<DataBaseCrtp<SqliteDB>&>(*this) = std::move(that);
        std::swap(that._db, _db);
        return *this;
    }

    void open(std::string_view filePath) {
        close();
        if (::sqlite3_open(filePath.data(), &_db) != SQLITE_OK) [[unlikely]] {
            throw std::runtime_error{
                "Failed to open database: " + std::string{::sqlite3_errmsg(_db)}
            };
        }
    }

    void close() noexcept {
        ::sqlite3_free(_db);
        _db = nullptr;
    }

    ~SqliteDB() noexcept {
        close();
    }

    void exec(std::string_view sql) {
        char* errMsg = nullptr;
        if (::sqlite3_exec(_db, sql.data(), nullptr, nullptr, &errMsg) != SQLITE_OK) [[unlikely]] {
            std::string err = errMsg ? errMsg : "unknown error";
            ::sqlite3_free(errMsg);
            throw std::runtime_error{err};
        }
    }

    template <typename Table, bool IsNotExists = true>
    container::Try<> createDatabase() noexcept {
        container::Try<> res{container::NonVoidType<>{}};
        try {
            auto sql = sqlite::CreateDbSql::createDatabase<Table, IsNotExists>({});
            exec(sql);
            auto index = sqlite::CreateDbSql::createIndex<Table>({});
            for (auto const& idxSql : index) {
                exec(idxSql);
            }
        } catch (...) {
            res.setException(std::current_exception());
        }
        return res;
    }

    void prepareSql(std::string_view sql, auto& stmt) {
        stmt.prepareSql(sql, _db);
    }
    
private:
    ::sqlite3* _db{};

    template <typename Db, typename SelectT>
    friend struct DataBaseSqlBuildView;
};

} // namespace HX::db::sqlite

namespace HX::db::internal {

template <>
struct GetDataBaseType<DbType::Sqlite3> {
    using Type = sqlite::SqliteDB;
};

} // namespace HX::db::internal