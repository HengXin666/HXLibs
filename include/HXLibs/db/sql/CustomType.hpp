#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-12-06 18:48:21
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

#include <HXLibs/meta/ContainerConcepts.hpp>
#include <HXLibs/db/sql/SqlType.hpp>

namespace HX::db {

/**
 * @brief 自定义SQL类型
 * @tparam T 注册的自定义 C++ 类型, T 不应该为 std::optional
 * @note 如果实现了CustomType<U> 那么 默认支持 optional<U> 和 U
 * @tparam SqlStrType 对应的 Sql 存储的类型
 */
template <typename T>
struct CustomType {
    using Type = T;

    static_assert(IsSqlTypeVal<T>); // 默认支持类型不报错

    // Type 序列化为 std::string    
    static std::string const& toSql(Type const& t) noexcept {
        return t;
    }
    
    // std::string_view 反序列化为 Type
    static Type const& fromSql(std::string_view t) noexcept {
        return t;
    }
};

/**
 * @brief 判断 T 是否是已经注册的自定义SQL类型
 * @tparam T
 */
template <typename T>
constexpr bool IsCustomTypeVal = requires {
    CustomType<T>{};
};

} // namespace HX::db