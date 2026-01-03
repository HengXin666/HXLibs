#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2026-01-03 18:10:55
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

#include <string_view>

#include <HXLibs/reflection/TypeName.hpp>

namespace HX::db {

/**
 * @brief 判断是否定义了自定义表名 (inline constexpr static std::string_view TableName = "xxx")
 * @tparam Table 
 */
template <typename Table>
concept HasCustomTableName = requires (Table) {
    { Table::TableName } -> std::convertible_to<std::string_view>;
};

/**
 * @brief 获取表名
 * @tparam Table 
 * @return constexpr std::string_view 
 */
template <typename Table>
constexpr std::string_view getTableName() noexcept {
    if constexpr (HasCustomTableName<Table>) {
        return Table::TableName;
    } else {
        return reflection::getTypeName<Table>();
    }
}

} // namespace HX::db