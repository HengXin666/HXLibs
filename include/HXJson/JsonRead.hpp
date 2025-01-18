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
extern T fromJson(const HX::json::JsonObject& json);

template <HX::STL::concepts::SingleElementContainer Container>
void fromJson(Container& val, const HX::json::JsonObject& json) {
    using T = typename Container::value_type;
    for (const auto& it : json.get<HX::json::JsonList>()) {
        if constexpr (
            HX::STL::utils::has_variant_type_v<T, HX::json::JsonObject::JsonData>
            || std::is_integral_v<T> 
            || std::is_floating_point_v<T>
        ) {
            val.emplace_back(it.get<T>());
        } else {
            val.emplace_back(fromJson<T>(it));
        }
    }
}

template <HX::STL::concepts::KeyValueContainer Container>
void fromJson(Container& val, const HX::json::JsonObject& json) {
    using T = typename Container::mapped_type;
    for (const auto& [k, v] : json.get<HX::json::JsonDict>()) {
        if constexpr (
            HX::STL::utils::has_variant_type_v<T, HX::json::JsonObject::JsonData>
            || std::is_integral_v<T> 
            || std::is_floating_point_v<T>
        ) {
            val.emplace(k, v.get<T>());
        } else {
            // 因为键规定为std::string, 所以可行
            val.emplace(k, fromJson<T>(v));
        }
    }
}

template <typename T>
void fromJson(T& val, const HX::json::JsonObject& json) {
    if constexpr (
        HX::STL::utils::has_variant_type_v<T, HX::json::JsonObject::JsonData> 
        || std::is_integral_v<T> 
        || std::is_floating_point_v<T>
    ) {
        val = json.get<T>();
    } else {
        val = fromJson<T>(json);
    }
}

template <typename T>
T fromJson(const HX::json::JsonObject& json) {
    T obj{};
    HX::STL::reflection::forEach(obj, [&](auto index, auto name, auto& val) {
        static_cast<void>(index);
        internal::fromJson(val, json[std::string {name}]);
    });
    return obj;
}

} // namespace internal

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
        internal::fromJson(val, json[std::string {name}]);
    });
}

}} // namespace HX::json

#endif // !_HX_JSON_READ_H_