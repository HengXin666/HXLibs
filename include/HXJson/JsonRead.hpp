#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-01-18 15:47:33
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
#ifndef _HX_JSON_READ_H_
#define _HX_JSON_READ_H_

#include <HXJson/Json.h>
#include <HXSTL/reflection/MemberName.hpp>
#include <HXSTL/utils/TypeTraits.hpp>
#include <HXSTL/concepts/SingleElementContainer.hpp>
#include <HXSTL/concepts/KeyValueContainer.hpp>

namespace HX { namespace json {

namespace internal {

template <typename T>
extern T fromJson(HX::json::JsonObject& json);

template <HX::STL::concepts::SingleElementContainer Container>
void fromJson(Container& val, HX::json::JsonObject& json) {
    using T = typename Container::value_type;
    for (auto& it : json.move<HX::json::JsonList>()) {
        if constexpr (
            HX::STL::utils::has_variant_type_v<T, HX::json::JsonObject::JsonData>
            || std::is_integral_v<T> 
            || std::is_floating_point_v<T>
        ) {
            val.emplace_back(it.move<T>());
        } else {
            val.emplace_back(fromJson<T>(it));
        }
    }
}

template <HX::STL::concepts::KeyValueContainer Container>
void fromJson(Container& val, HX::json::JsonObject& json) {
    using T = typename Container::mapped_type;
    for (auto& [k, v] : json.move<HX::json::JsonDict>()) {
        if constexpr (
            HX::STL::utils::has_variant_type_v<T, HX::json::JsonObject::JsonData>
            || std::is_integral_v<T> 
            || std::is_floating_point_v<T>
        ) {
            val.emplace(std::move(k), v.move<T>());
        } else {
            // 因为键规定为std::string, 所以可行
            val.emplace(std::move(k), fromJson<T>(v));
        }
    }
}

template <typename T>
void fromJson(T& val, HX::json::JsonObject& json) {
    if constexpr (
        HX::STL::utils::has_variant_type_v<T, HX::json::JsonObject::JsonData> 
        || std::is_integral_v<T> 
        || std::is_floating_point_v<T>
    ) {
        val = json.move<T>();
    } else {
        val = fromJson<T>(json);
    }
}

template <typename T>
T fromJson(HX::json::JsonObject& json) {
    T obj{};
    HX::STL::reflection::forEach(obj, [&](auto index, auto name, auto& val) {
        static_cast<void>(index);
        internal::fromJson(val, json[std::string{name.data(), name.size()}]);
    });
    return obj;
}

} // namespace internal

/**
 * @brief 将json字符串反序列化到`聚合类`中, 解析失败或者字段不存在, 则会抛出异常
 * @tparam Obj `聚合类`
 * @tparam Stream 字符串, 如`std::string`、`std::string_view`
 * @param obj `聚合类`实例对象
 * @param s jsonString
 * @throw `std::runtime_error`: Json解析失败
 * @throw `std::runtime_error`: 成员字段不存在于json中
 * @throw `std::runtime_error`: 解析数字不完全 (很少见)
 * @throw `std::system_error`: 解析数字异常 (常见是数字溢出)
 */
template <typename Obj, typename Stream>
inline void fromJson(Obj& obj, const Stream& s) {
    // 1. 解析json, 必定为 name: val 形式
    auto [json, size] = HX::json::parse(s);
    if (size <= 0) [[unlikely]] {
        throw std::runtime_error("parse json Error!");
    }
    // 2. 获取obj的所有字段, 及其名称
    HX::STL::reflection::forEach(obj, [&](auto index, auto name, auto& val) {
        static_cast<void>(index);
        internal::fromJson(val, json[std::string{name.data(), name.size()}]);
    });
}

}} // namespace HX::json

#endif // !_HX_JSON_READ_H_