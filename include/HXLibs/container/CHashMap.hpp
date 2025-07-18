#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-18 15:36:54
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
#ifndef _HX_C_HASH_MAP_H_
#define _HX_C_HASH_MAP_H_

#include <array>
#include <functional>
#include <stdexcept>

#include <HXLibs/meta/Hash.hpp>
#include <HXLibs/container/CPmhTable.hpp>
#include <HXLibs/utils/Random.hpp>

namespace HX::container {

template <typename Key, typename Val, std::size_t N,
          typename Hasher = meta::Hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class CHashMap {
    inline static constexpr std::size_t M = N * 2;

    using ContainerType = std::array<std::pair<Key, Val>, N>;

    ContainerType _data;
    CPmhTable<M, Hasher> _pmhTable;
    Hasher _hash;
    KeyEqual const _keyEqual;
public:
    using key_type = Key;
    using mapped_type = Val;
    using value_type = typename ContainerType::value_type;

    constexpr CHashMap(ContainerType data, Hasher const& hash, KeyEqual const& keyEqual)
        : _data{data}
        , _pmhTable{makeCPmhTable<M>(_data, hash, utils::XorShift32{114514})}
        , _hash{hash}
        , _keyEqual{keyEqual}
    {}

    constexpr CHashMap(ContainerType list) 
        : CHashMap(list, Hasher{}, KeyEqual{})
    {}

    template <typename _Key>
    constexpr mapped_type at(_Key const& key) const {
        auto const& kv = _data[_pmhTable.lookup(key, _hash)];
        if (_keyEqual(meta::getKey(kv), key)) {
            return kv.second;
        }
        throw std::out_of_range("unknown key");
    }
};

} // namespace HX::container

#endif // !_HX_C_HASH_MAP_H_