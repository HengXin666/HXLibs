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

#include <cstddef>

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
     */
    template <std::size_t Idx, typename T>
    void bind(T&& t) {
        HX_DB_STMT_IMPL->template bind<Idx, T>(t);
    }

    /**
     * @brief 清空绑定的占位参数
     */
    void clearBind() {
        HX_DB_STMT_IMPL->clearBind();
    }
#undef HX_DB_STMT_IMPL
};

} // namespace HX::db