#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2026-01-01 23:31:22
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

#include <map>

#include <HXLibs/meta/TypeId.hpp>
#include <HXLibs/container/Try.hpp>
#include <HXLibs/db/sql/Column.hpp>
#include <HXLibs/db/sql/Dml.hpp>
#include <HXLibs/db/sql/Stmt.hpp>

namespace HX::db {

template <typename CRTP>
struct DataBase {
    /**
     * @brief 创建一个 SQL 模板, 内部的 SQL 字符串仅会被构建一次. 之后会复用缓存
     * @tparam FuncPtr 当前函数的函数指针等, 表示处境唯一的编译期常量
     * @tparam Feature 更多可选的特征
     * @note <FuncPtr, Feature...> 注册为唯一的编译期 Id
     * @return auto& 
     */
    template <auto FuncPtr, auto... Feature>
    auto& sqlTemplate() {
        constexpr auto Id = meta::TypeId::make<meta::ValueWrap<FuncPtr, Feature...>>();
        if (auto it = _sqlMap.find(Id); it != _sqlMap.end()) {
            return it->second;
        }
        auto [it, _] = _sqlMap.emplace(Id, static_cast<CRTP*>(this));
        return it->second;
    }

private:
    DataBase() = default;
    friend CRTP;

    std::map<meta::TypeId::IdType, DataBaseSqlBuild<CRTP>> _sqlMap{};
};

template <typename CRTP>
struct DataBaseInterface {
#define HX_DB_IMPL static_cast<CRTP>(this)
    /**
     * @brief 执行 SQL 语句
     * @param sql 
     * @throw SQL 执行出错
     */
    void exec(std::string_view sql) {
        return HX_DB_IMPL->exec(sql);
    }

    /**
     * @brief 执行 SQL 语句
     * @param sql 
     * @return container::Try<> 包裹异常
     */
    container::Try<> execNoThrow(std::string_view sql) noexcept {
        container::Try<> res{container::NonVoidType<>{}};
        try {
            HX_DB_IMPL->exec(sql);
        } catch (...) {
            res.setException(std::current_exception());
        }
        return res;
    }

    /**
     * @brief 预编译 SQL
     * @param sql 
     * @param stmt 
     */
    void prepareSql(std::string_view sql, auto& stmt) {
        HX_DB_IMPL->prepareSql(sql, stmt);
    }

    /**
     * @brief 创建数据表
     * @tparam Table 表
     * @tparam IsNotExists 是否仅在不存在时候创建
     * @return container::Try<> 
     */
    template <typename Table, bool IsNotExists = true>
    container::Try<> createDatabase() noexcept {
        return HX_DB_IMPL->template createDatabase<Table, IsNotExists>();
    }

    /**
     * @brief 获取最后一次成功的操作修改的行数
     * @return std::size_t 修改的行数
     */
    std::size_t getLastChanges() const noexcept {
        return HX_DB_IMPL->getLastChanges();
    }

#undef HX_DB_IMPL
};

} // namespace HX::db