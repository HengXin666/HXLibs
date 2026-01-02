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
 * @tparam T 注册的自定义 C++ 类型
 * @tparam SqlStrType 对应的 Sql 存储的类型
 */
template <typename T>
struct CustomType {
    using Type = T;

    static_assert(IsSqlTypeVal<T>); // 屏蔽主模板

    // Type 序列化为 std::string
    // static std::string toSql(Type const&) noexcept {
    //     return {};
    // }

    // std::string_view 反序列化为 Type
    // static Type fromSql(std::string_view) noexcept {
    //     return {};
    // }

    static Type const& toSql(Type const& t) noexcept {
        return t;
    }

    static Type const& fromSql(Type const& t) noexcept {
        return t;
    }
};

template <typename T>
    requires (IsSqlTypeVal<T>)
struct CustomType<std::optional<T>> {
    using Type = std::optional<T>;

    static Type toSql(Type t) noexcept {
        return std::move(t);
    }

    static Type fromSql(Type t) noexcept {
        return std::move(t);
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