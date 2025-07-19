#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-18 15:24:40
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
#ifndef _HX_HASH_H_
#define _HX_HASH_H_

#include <type_traits>
#include <string_view>

namespace HX::meta {

template <typename KV>
constexpr auto& getKey(KV const& kv) noexcept {
    return kv.first;
}

template <typename String>
constexpr std::size_t hashString(const String& value) {
    std::size_t d = 5381;
    for (const auto& c : value) 
        d = d * 33 + static_cast<size_t>(c);
    return d;
}

// https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
// 根据实验设置删除最低位
template <typename String>
constexpr std::size_t hashString(const String& value, std::size_t seed) {
    std::size_t d = (0x811c9dc5 ^ seed) * static_cast<size_t>(0x01000193);
    for (const auto& c : value)
        d = (d ^ static_cast<size_t>(c)) * static_cast<size_t>(0x01000193);
    return d >> 8;
}

template <typename T = void>
struct Hash {
    static_assert(std::is_integral_v<T> || std::is_enum_v<T>,
        "only integer types are supported");

    constexpr std::size_t operator()(T const& val, std::size_t seed) const noexcept {
        std::size_t key = seed ^ static_cast<std::size_t>(val);
        key = (~key) + (key << 21); // key = (key << 21) - key - 1;
        key = key ^ (key >> 24);
        key = (key + (key << 3)) + (key << 8); // key * 265
        key = key ^ (key >> 14);
        key = (key + (key << 2)) + (key << 4); // key * 21
        key = key ^ (key >> 28);
        key = key + (key << 31);
        return key;
    }
};

template <typename Char>
struct Hash<std::basic_string_view<Char>> {
private:
    using T = std::basic_string_view<Char>;
public:
    // 兼容 std::hash 的参数
    constexpr std::size_t operator()(T const& val) const noexcept {
        return hashString(val);
    }

    constexpr std::size_t operator()(T const& val, std::size_t seed) const noexcept {
        return hashString(val, seed);
    }
};

} // namespace HX::meta

#endif // !_HX_HASH_H_