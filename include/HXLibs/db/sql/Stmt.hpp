#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2026-01-10 13:22:01
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

#include <utility>

#include <HXLibs/db/sql/DbType.hpp>
#include <HXLibs/db/sql/CustomType.hpp>
#include <HXLibs/db/sql/Constraint.hpp>
#include <HXLibs/db/sql/ColumnResult.hpp>

namespace HX::db {

template <typename CRTP>
struct Stmt {
#define HX_DB_STMT_IMPL static_cast<CRTP>(this)
    /**
     * @brief 执行 SQL 语句
     * @return true 执行成功
     * @return false 执行失败
     */
    bool exec() noexcept {
        return HX_DB_STMT_IMPL->exec();
    }

    /**
     * @brief 获取最后一次成功的操作修改的行数
     * @return std::size_t 修改的行数
     */
    std::size_t getLastChanges() const noexcept {
        return HX_DB_STMT_IMPL->getLastChanges();
    }

    /**
     * @brief 绑定占位参数
     * @tparam T 占位参数类型
     * @tparam Idx 占位参数序号
     * @param t 占位参数
     * @return bool 是否绑定成功
     */
    template <typename T>
    bool bind(std::size_t idx, T&& t) {
        HX_DB_STMT_IMPL->template bind<T>(idx, std::forward<T>(t));
    }

    /**
     * @brief 执行 stmt, 遍历每一个查询结果
     */
    template <auto... Vs>
    void selectForEach(std::vector<ColumnResult<meta::ValueWrap<Vs...>>>& resArr) {
        HX_DB_STMT_IMPL->selectForEach(resArr);
    }
#undef HX_DB_STMT_IMPL
};

template <typename T, typename AnyFunc, typename NullFunc>
    requires (std::is_invocable_v<AnyFunc, T>
           && std::is_invocable_v<NullFunc>)
constexpr bool stmtBind(T&& t, AnyFunc&& anyFunc, NullFunc&& nullFunc) noexcept(
    noexcept(std::forward<AnyFunc>(anyFunc)(std::declval<T&&>()))
 && noexcept(std::forward<NullFunc>(nullFunc)())
) {
    using Type = RemoveConstraintToType<meta::RemoveCvRefType<T>>;
    auto& data = static_cast<Type&>(t);
    if constexpr (IsCustomTypeVal<Type>) {
        return std::forward<AnyFunc>(anyFunc)(data);
    } else if constexpr (meta::IsOptionalVal<Type>) {
        if constexpr (IsCustomTypeVal<typename Type::value_type>) {        
            if (data) {
                return std::forward<AnyFunc>(anyFunc)(data);
            } else {
                return std::forward<NullFunc>(nullFunc)();
            }
        } else {
            static_assert(!sizeof(Type), "Illegal type, please register db::CustomType");
        }
    } else {
        // 非法类型, 请注册 db::CustomType
        static_assert(!sizeof(Type), "Illegal type, please register db::CustomType");
    }
}

} // namespace HX::db