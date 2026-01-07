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
#include <HXLibs/db/sql/Column.hpp>
#include <HXLibs/db/sql/Dml.hpp>

namespace HX::db {

template <typename Db>
struct DataBaseSqlBuild {
    template <auto... Cs>
    auto select() {
        return SelectBuild<DataBaseSqlBuild<Db>&>{*this, _sql}.template select<Cs...>();
    }

    Db* const _db{};
    std::string _sql{};
    Db::StmtType _stmt{};
    bool _isInit{false};
};

template <typename CRTP>
struct DataBase {
    template <auto FuncPtr, auto... Feature>
    auto& makeSql() {
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
     * @return true 执行成功
     * @return false 执行失败
     */
    bool exec() noexcept {
        return HX_DB_IMPL->exec();
    }

    auto& tryExec() {
        return HX_DB_IMPL->tryExec();
    }

    /**
     * @brief 获取最后一次成功的操作修改的行数
     * @return std::size_t 修改的行数
     */
    std::size_t getLastChanges() const noexcept {
        return HX_DB_IMPL->getLastChanges();
    }

    /**
     * @brief 绑定占位参数
     * @tparam T 占位参数类型
     * @tparam Idx 占位参数序号
     * @param t 占位参数
     */
    template <typename T, std::size_t Idx>
    void bind(T&& t) {
        HX_DB_IMPL->template bind<T, Idx>(t);
    }

    /**
     * @brief 清空绑定的占位参数
     */
    void clearBind() {
        HX_DB_IMPL->clearBind();
    }
#undef HX_DB_IMPL
};

} // namespace HX::db