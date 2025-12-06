#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-11-29 22:38:56
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

#include <HXLibs/reflection/MemberName.hpp>

namespace HX::db {

/**
 * @brief 自定义序列化类型
 * @tparam T 
 */
template <typename T>
struct SQLiteSqlType {
    inline static constexpr bool _hx_Val = true;

    // 序列化方法
    static constexpr std::string bind(T const&) noexcept {
        return {};
    }

    // 反序列化方法
    static constexpr T columnType(std::string_view) {
        return {};
    }
};

template <typename T>
constexpr bool isSQLiteSqlTypeVal = !requires { SQLiteSqlType<T>::_hx_Val; };

/**
 * @brief 设置为主键
 * @tparam T 
 */
template <typename T>
struct PrimaryKey {
    // 主键期望为整数
    static_assert(std::is_integral_v<T>, "The expected primary key is an integer");
    using PrimaryKeyType = T;

    T val;

    operator T&() noexcept { return val; }
    operator const T&() const noexcept { return val; }
};

template <typename T>
constexpr bool isPrimaryKeyVal = requires { typename T::PrimaryKeyType; };

namespace internal {

template <typename T>
struct RemovePrimaryKeyTypeImpl {
    using Type = T;
};

template <typename T>
struct RemovePrimaryKeyTypeImpl<PrimaryKey<T>> {
    using Type = typename PrimaryKey<T>::PrimaryKeyType;
};

template <typename T, typename... Args>
struct GetFirstPrimaryKeyTypeImpl {
    using Type = GetFirstPrimaryKeyTypeImpl<Args...>::Type;
};

template <typename T, typename... Args>
struct GetFirstPrimaryKeyTypeImpl<db::PrimaryKey<T>, Args...> {
    using Type = db::PrimaryKey<T>::PrimaryKeyType;
};

template <>
struct GetFirstPrimaryKeyTypeImpl<void> {
    using Type = void;
};

template <typename Idx, typename... Args>
struct GetFirstPrimaryKeyIndex;

template <std::size_t I, std::size_t... Is, typename T, typename... Args>
struct GetFirstPrimaryKeyIndex<std::index_sequence<I, Is...>, T, Args...> {
    inline static constexpr std::size_t Val 
        = GetFirstPrimaryKeyIndex<std::index_sequence<Is...>, Args...>::Val;
};

template <std::size_t I, std::size_t... Is, typename T, typename... Args>
struct GetFirstPrimaryKeyIndex<std::index_sequence<I, Is...>, db::PrimaryKey<T>, Args...> {
    inline static constexpr std::size_t Val = I;
};

} // namespace internal

/**
 * @brief 去除主键类型包装
 * @tparam T 
 */
template <typename T>
using RemovePrimaryKeyType = internal::RemovePrimaryKeyTypeImpl<T>::Type;

/**
 * @brief 获取 T 类 中的表示主键成员的类型
 * @tparam T 
 */
template <typename T>
using GetFirstPrimaryKeyType = decltype([]() constexpr {
    constexpr auto tp = reflection::internal::getStaticObjPtrTuple<T>();
    return [&] <std::size_t... Is> (std::index_sequence<Is...>) {
        return typename internal::GetFirstPrimaryKeyTypeImpl<
            meta::RemoveCvRefType<decltype(*std::get<Is>(tp))>..., void
        >::Type {};
    }(std::make_index_sequence<std::tuple_size_v<decltype(tp)>>{});
}());

/**
 * @brief 获取主键在类对象中的索引 (从 0 开始)
 * @tparam T 
 */
template <typename T>
constexpr std::size_t GetFirstPrimaryKeyIndex = [](){
    constexpr auto tp = reflection::internal::getStaticObjPtrTuple<T>();
    return [&] <std::size_t... Is> (std::index_sequence<Is...>) {
        return internal::GetFirstPrimaryKeyIndex<
            std::make_index_sequence<std::tuple_size_v<decltype(tp)>>,
            meta::RemoveCvRefType<decltype(*std::get<Is>(tp))>...
        >::Val;
    }(std::make_index_sequence<std::tuple_size_v<decltype(tp)>>{});
}();

/**
 * @brief 获取主键引用
 * @tparam T 
 * @param t 
 * @return constexpr auto& 
 */
template <typename T>
inline constexpr auto& getFirstPrimaryKeyRef(T& t) noexcept {
    auto tr = reflection::internal::getObjTie(t);
    return std::get<GetFirstPrimaryKeyIndex<T>>(tr).val;
}

} // namespace HX::db