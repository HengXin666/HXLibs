#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-11-29 22:39:11
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
#include <vector>
#include <map>

#include <sqlite3.h>

#include <HXLibs/meta/StaticConstexpr.hpp>
#include <HXLibs/meta/MemberPtrType.hpp>
#include <HXLibs/meta/TypeId.hpp>
#include <HXLibs/meta/FixedString.hpp>
#include <HXLibs/reflection/MemberName.hpp>
#include <HXLibs/reflection/MemberPtr.hpp>
#include <HXLibs/reflection/EnumName.hpp>
#include <HXLibs/reflection/TypeName.hpp>
#include <HXLibs/log/serialize/ToString.hpp>

// debug
#include <HXLibs/log/Log.hpp>

#include <HXLibs/db/MakeSqlStr.hpp>
#include <HXLibs/db/SQLiteMeta.hpp>
#include <HXLibs/db/sqlite3/SQLiteStmt.hpp>

namespace HX::db {

/**
 * @brief 字段匹配对 (成员指针, 值)
 * @tparam MemberPtr
 */
template <typename MemberPtr>
    requires (meta::IsMemberPtrVal<MemberPtr>)
struct FieldPair {
    MemberPtr ptr;                                      // 成员指针
    meta::GetMemberPtrType<MemberPtr> const& dataView;  // 成员值
};

namespace internal {

enum class SqlOp : uint8_t {
    Where   = 1 << 0,
    GroupBy = 1 << 1,
    Having  = 1 << 2,
    OrderBy = 1 << 3,
    Limit   = 1 << 4,
};

constexpr auto SqlOpCnt = reflection::getValidEnumValueCnt<SqlOp>();

inline constexpr uint8_t operator&(uint8_t a, SqlOp b) noexcept {
    return a & static_cast<uint8_t>(b);
}

inline constexpr uint8_t& operator|=(uint8_t& a, SqlOp b) noexcept {
    return a |= static_cast<uint8_t>(b);
}

template <typename T>
constexpr std::string_view getSqlTypeStr() {
    if constexpr (std::is_integral_v<T>) {
        return "INTEGER";
    } else if constexpr (std::is_floating_point_v<T>) {
        return "REAL";
    } else if constexpr (meta::StringType<T> || isSQLiteSqlTypeVal<T>) {
        return "TEXT";
    } else {
        // 不支持该类型
        static_assert(!sizeof(T), "type is not sql type");
    }
}

inline void execSql(std::string_view sql, ::sqlite3* db) {
    char* errMsg = nullptr;
    if (::sqlite3_exec(db, sql.data(), nullptr, nullptr, &errMsg) != SQLITE_OK) [[unlikely]] {
        std::string err = errMsg ? errMsg : "unknown error";
        ::sqlite3_free(errMsg);
        throw std::runtime_error{err};
    }
}

#if 0

constexpr bool isSpace(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

constexpr bool isPunct(char c) noexcept {
    // https://en.cppreference.com/w/cpp/string/byte/isalnum
    constexpr auto arr = meta::StaticConstexpr<[]{
        std::array<bool, 127> arr{};
        for (char c : R"(!"#$%&'()*+,-./ 	:;<=>?@[\]^_`{|}~)") {
            arr[c] = true;
        }
        return arr;
    }>::get();
    return c >= 0 && arr[c];
}

constexpr bool isAlnumOrUnderscore(char c) noexcept {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') ||
            c == '_';
}

/**
 * @brief 解析 SQL 的占位符对应的字段名称
 * @param sql
 * @return std::vector<std::string_view>
 */
template <meta::FixedString Sql>
inline constexpr auto extractFields() {
    constexpr auto view = meta::ToCharPack<Sql>::view();
    constexpr auto len = Sql.size();

    // 先扫描问号数量
    constexpr std::size_t MaxFields = [&]() constexpr {
        std::size_t count = 0;
        for (std::size_t i = 0; i < len; ++i) {
            count += view[i] == '?';
        }
        return count;
    }();

    std::array<std::string_view, MaxFields> fields{};
    std::size_t found = 0;

    for (std::size_t pos = 0; pos < len; ++pos) {
        if (view[pos] != '?') {
            continue;
        }

        // 向前找到字段名末尾 (第一个非空格非标点)
        std::size_t r = pos;
        while (r > 0 && (isSpace(view[r - 1]) || isPunct(view[r - 1]))) {
            --r;
        }

        // 再向前找到字段名起点 (非字母数字且非下划线)
        std::size_t l = r;
        while (l > 0 && (isAlnumOrUnderscore(view[l - 1]))) {
            --l;
        }

        if (r > l) {
            fields[found++] = view.substr(l, r - l);
        }
    }

    // 截断数组
    std::array<std::string_view, MaxFields> res{};
    for (std::size_t i = 0; i < found; ++i) {
        res[i] = fields[i];
    }
    return res;
}

template <typename T>
class SqlCallChain {
public:
    SqlCallChain(std::string sql)
        : _sql{std::move(sql)}
    {}

    template <meta::FixedString Str>
    constexpr SqlCallChain& where() {
        if (_opEd & SqlOp::Where) [[unlikely]] {
            throw std::runtime_error{"@todo"};
        }
        _opEd |= SqlOp::Where;

        [[maybe_unused]] constexpr auto fields = extractFields<Str>();

        constexpr auto nameMap = reflection::getMembersNamesMap<T>();

        [&] <std::size_t... Is> (std::index_sequence<Is...>) {
            constexpr auto tp = reflection::internal::getStaticObjPtrTuple<T>();
            (([&] <std::size_t Idx> (std::index_sequence<Idx>) {
                constexpr auto I = nameMap.at(fields[Idx]);
                using Type = decltype(*std::get<I>(tp));
                // 完全没必要... 仅仅只是为了编译时判断类型吗? 没用
            }(std::index_sequence<Is>{})), ...);
        } (std::make_index_sequence<fields.size()>{});

        _sql += meta::ToCharPack<Str>::view();
        return *this;
    }
private:
    std::string _sql;
    uint8_t _opEd;
    std::size_t _idx = 0;
};

#endif

struct [[nodiscard]] StmtCallChain {
    struct [[nodiscard]] ChangeLine {
        using ChangeCntType = decltype(std::declval<SQLiteStmt>().getLastChanges());

        constexpr ChangeLine(ChangeCntType cnt)
            : _cnt{cnt}
        {}

        template <ChangeCntType MinCnt = 1>
        constexpr void check() const {
            if (_cnt < MinCnt) {
                throw std::runtime_error{"check: Change < " + std::to_string(MinCnt)};
            }
        }

        constexpr operator ChangeCntType() const noexcept {
            return _cnt;
        }

        ChangeCntType _cnt;
    };

    StmtCallChain(std::string_view sql, ::sqlite3* db)
        : _stmt{sql, db}
        , _cnt{1}
    {}

    StmtCallChain(StmtCallChain&) = delete;
    StmtCallChain(StmtCallChain&& that) noexcept
        : _stmt{std::move(that._stmt)}
        , _cnt{that._cnt}
    {}

    StmtCallChain& operator=(StmtCallChain&) = delete;
    StmtCallChain& operator=(StmtCallChain&& that) noexcept {
        if (this == std::addressof(that)) {
            return *this;
        }
        std::swap(that._stmt, _stmt);
        std::swap(that._cnt, _cnt);
        return *this;
    }

    // 绑定, 不可重入
    template <bool IsSetPrimaryKey, typename... Args>
    [[nodiscard]] StmtCallChain& bind(Args&&... args) noexcept {
        auto tp = std::forward_as_tuple(std::forward<Args>(args)...);
        [&] <std::size_t... Is> (std::index_sequence<Is...>) {
            (([&] <std::size_t Idx> (std::index_sequence<Idx>) {
                auto& t = std::get<Idx>(tp);
                using T = meta::RemoveCvRefType<decltype(t)>;
                using RemoveKeyT = RemovePrimaryKeyType<T>;
                if constexpr (isPrimaryKeyVal<T> && !IsSetPrimaryKey) {
                    return; // 忽略
                }
                if constexpr (std::is_integral_v<RemoveKeyT>) {
                    ::sqlite3_bind_int64(_stmt, _cnt, t);
                } else if constexpr (std::is_floating_point_v<RemoveKeyT>) {
                    ::sqlite3_bind_double(_stmt, _cnt, t);
                } else if constexpr (meta::StringType<RemoveKeyT> || isSQLiteSqlTypeVal<RemoveKeyT>) {
                    if constexpr (isSQLiteSqlTypeVal<RemoveKeyT>) {
                        auto str = SQLiteSqlType<RemoveKeyT>::bind(t);
                        ::sqlite3_bind_text(_stmt, _cnt, str.data(), str.size(), SQLITE_TRANSIENT);
                    } else {
                        ::sqlite3_bind_text(_stmt, _cnt, t.data(), t.size(), SQLITE_TRANSIENT);
                    }
                } else {
                    // 不支持该类型
                    static_assert(!sizeof(T), "type is not sql type");
                }
                ++_cnt;
            }(std::index_sequence<Is>{})), ...);
        } (std::make_index_sequence<std::tuple_size_v<decltype(tp)>>{});
        return *this;
    }

    // 执行
    bool exec() noexcept {
        bool res = _stmt.step() == SQLITE_DONE;
        reset();
        return res;
    }

    // 带异常的
    StmtCallChain& execOnThrow() {
        if (!exec()) [[unlikely]] {
            throw std::runtime_error{"SQL Error: " + _stmt.getErrMsg()};
        }
        return *this;
    }

    /**
     * @brief 获取最后一次成功的操作修改的行数
     * @return ChangeLine 修改的行数
     */
    ChangeLine getLastChanges() const noexcept {
        return {_stmt.getLastChanges()};
    }

    // 执行SQL, 并获取上一个插入的数据的主键值
    template <typename T>
    auto getLastInsertPrimaryKeyId() {
        if (exec()) [[likely]] {
            if constexpr (!std::is_void_v<GetFirstPrimaryKeyType<T>>) {
                return static_cast<GetFirstPrimaryKeyType<T>>(
                    _stmt.getLastInsertPrimaryKeyId()
                );
            } else {
                return _stmt.getLastInsertPrimaryKeyId();
            }
        } else [[unlikely]] {
            throw std::runtime_error{"Insert failed: " + _stmt.getErrMsg()};
        }
    }

private:
    void reset() {
        _cnt = 1;
        _stmt.reset();
        _stmt.clearBind();
    }

    SQLiteStmt _stmt;
    std::size_t _cnt;
};

} // namespace internal

class SQLiteDB {
    template <typename T>
    using PrimaryKeyType = decltype(
        std::declval<internal::StmtCallChain>(
        ).template getLastInsertPrimaryKeyId<meta::RemoveCvRefType<T>>()
    );

    void exec(std::string_view sql) const {
        internal::execSql(sql, _db);
    }

    template <typename InitFunc>
        requires (std::is_same_v<std::invoke_result_t<InitFunc>, internal::StmtCallChain>)
    internal::StmtCallChain& getSqlCache(meta::TypeId::IdType typeId, InitFunc&& init) {
        auto it = _sqlCache.find(typeId);
        if (it == _sqlCache.end()) [[unlikely]] {
            auto [jt, ok] = _sqlCache.emplace(typeId, init());
            if (!ok) [[unlikely]] {
                throw std::runtime_error{"getSqlCache: emplace err"};
            }
            return jt->second;
        }
        return it->second;
    }
public:
    SQLiteDB() : _db{} {}

    SQLiteDB(std::string_view filePath)
        : SQLiteDB{}
    {
        log::hxLog.debug("make dbFile:", filePath); // debug
        if (::sqlite3_open(filePath.data(), &_db) != SQLITE_OK) [[unlikely]] {
            throw std::runtime_error{
                "Failed to open database: " + std::string{::sqlite3_errmsg(_db)}
            };
        }
    }

    SQLiteDB(SQLiteDB const&) = delete;
    SQLiteDB(SQLiteDB&& that) noexcept
        : _db{that._db}
    {
        that._db = nullptr;
    }

    SQLiteDB& operator=(SQLiteDB const&) noexcept = delete;
    SQLiteDB& operator=(SQLiteDB&& that) noexcept {
        std::swap(_db, that._db);
        return *this;
    }

    ~SQLiteDB() noexcept {
        if (_db) {
            ::sqlite3_close(_db);
        }
    }

    template <typename T>
    void createDatabase() const {
        // @todo 非空等属性
        std::string sql = "CREATE TABLE IF NOT EXISTS ";
        sql += reflection::getTypeName<T>();
        sql += " (";
        auto obj = reflection::internal::getStaticObj<T>();
        reflection::forEach(obj, [&] <std::size_t Idx> (
            std::index_sequence<Idx>, std::string_view name, auto&& val
        ) {
            using U = meta::RemoveCvRefType<decltype(val)>;
            if constexpr (Idx > 0) {
                sql += ", ";
            }
            sql += name;
            sql += ' ';
            if constexpr (isPrimaryKeyVal<U>) {
                sql += internal::getSqlTypeStr<typename U::PrimaryKeyType>();
                sql += " PRIMARY KEY";
            } else {
                sql += internal::getSqlTypeStr<U>();
            }
        });
        sql += ");";
        exec(sql);
    }

    template <typename T, bool IsSetPrimaryKey = false>
    PrimaryKeyType<T> insert(T&& t) {
        using U = meta::RemoveCvRefType<T>;
        auto tp = reflection::internal::getObjTie<U>(t);
        return [&] <std::size_t... Idx> (std::index_sequence<Idx...>) {
            return getSqlCache(
                meta::TypeId::make<&SQLiteDB::insert<U, IsSetPrimaryKey>>(),
                [&] {
                    return internal::StmtCallChain{MakeSqlStr::makeInsertSql<U, IsSetPrimaryKey>(), _db};
                }
            ).template bind<IsSetPrimaryKey>(std::get<Idx>(tp)...)
             .template getLastInsertPrimaryKeyId<U>();
        }(std::make_index_sequence<std::tuple_size_v<decltype(tp)>>{});
    }

    template <
        typename... MemberPtr,
        typename U = meta::GetMemberPtrsClassType<MemberPtr...>
    >
    PrimaryKeyType<U> insertBy(FieldPair<MemberPtr>... fmPair) {
        return getSqlCache(
            meta::TypeId::make<&SQLiteDB::insertBy<MemberPtr...>>(),
            [&] {
                return internal::StmtCallChain{
                    MakeSqlStr::makeInsertSql<U, MemberPtr...>(fmPair.ptr...),
                    _db
                };
            }
        ).template bind<true>(fmPair.dataView...)
         .template getLastInsertPrimaryKeyId<U>();
    }

    template <typename T>
    std::vector<T> queryAll() const {
        std::string sql = "SELECT * FROM ";
        sql += reflection::getTypeName<T>();
        SQLiteStmt stmt{sql, _db};
        std::vector<T> res;
        for (int rc = stmt.step(); rc == SQLITE_ROW; rc = stmt.step()) {
            reflection::forEach(res.emplace_back(), [&] <std::size_t Idx> (
                std::index_sequence<Idx>, std::string_view, auto& val
            ) {
                using ValType = meta::RemoveCvRefType<decltype(val)>;
                val = stmt.getColumnByIndex<ValType>(Idx);
            });
        }
        return res;
    }

    template <typename T>
    internal::StmtCallChain deleteBy(std::string sqlBody) {
        std::string sql = "DELETE FROM ";
        sql += reflection::getTypeName<T>();
        sql += ' ';
        sql += std::move(sqlBody);
        return {sql, _db};
    }

    // 默认不修改主键, 如果需要请使用 updateBy 显示指定
    template <meta::FixedString... SqlBody, typename T>
    internal::StmtCallChain& update(T&& t) {
        using U = meta::RemoveCvRefType<T>;
        auto tp = reflection::internal::getObjTie<U>(t);
        return [&] <std::size_t... Idx> (std::index_sequence<Idx...>) -> internal::StmtCallChain& {
            using Ptr = internal::StmtCallChain& (SQLiteDB::*) (T&&);
            constexpr Ptr ptr = &SQLiteDB::update<SqlBody...>;
            return getSqlCache(
                meta::TypeId::make<ptr>(),
                [this] {
                    auto sql = MakeSqlStr::makeUpdateSqlFragment<U, false>();
                    ((sql += meta::ToCharPack<SqlBody>::view()), ...);
                    return internal::StmtCallChain{sql, _db};
                }
            ).template bind<false>(std::get<Idx>(tp)...);
        } (std::make_index_sequence<std::tuple_size_v<decltype(tp)>>{});
    }

    template <
        meta::FixedString... SqlBody,
        typename... MemberPtr,
        typename U = meta::GetMemberPtrsClassType<MemberPtr...>
    >
    internal::StmtCallChain& updateBy(FieldPair<MemberPtr>... fmPair) {
        using Ptr = internal::StmtCallChain& (SQLiteDB::*) (FieldPair<MemberPtr>...);
        constexpr Ptr ptr = &SQLiteDB::updateBy<SqlBody...>;
        return getSqlCache(
            meta::TypeId::make<ptr>(),
            [&] {
                auto sql = MakeSqlStr::makeUpdateSqlFragment<U, MemberPtr...>(fmPair.ptr...);
                ((sql += meta::ToCharPack<SqlBody>::view()), ...);
                return internal::StmtCallChain{sql, _db};
            }
        ).template bind<true>(fmPair.dataView...);
    }
private:
    ::sqlite3* _db{};
    std::map<meta::TypeId::IdType, internal::StmtCallChain> _sqlCache{};
};

[[nodiscard]] inline SQLiteDB open(std::string_view filePath) {
    return SQLiteDB{filePath};
}

} // namespace HX::db