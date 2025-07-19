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

namespace internal {

template <std::size_t N>
constexpr std::size_t nextHighestPowOfTow() noexcept {
    if constexpr (N > 0x7FFF'FFFF'FFFF'FFFF) {
        static_assert(!sizeof(N), "N is too big");
    }
    // https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
    std::size_t v = N;
    --v;
    for (std::size_t i = 1; i < sizeof(std::size_t) * 8; i <<= 1)
        v |= v >> i;
    ++v;
    return v;
}

} // namespace internal

template <typename Key, typename Val, std::size_t N,
          typename Hasher = meta::Hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class CHashMap {
    inline static constexpr std::size_t M = internal::nextHighestPowOfTow<N>();

    using ContainerType = std::array<std::pair<Key, Val>, N>;

    ContainerType _data;
    CPmhTable<M, Hasher> _pmhTable;
    Hasher _hash;
    KeyEqual const _keyEqual;
public:
    using key_type = Key;
    using mapped_type = Val;
    using value_type = typename ContainerType::value_type;
    using size_type = typename ContainerType::size_type;
    using difference_type = typename ContainerType::difference_type;
    using hasher = Hasher;
    using key_equal = KeyEqual;
    using reference = typename ContainerType::reference;
    using const_reference = typename ContainerType::const_reference;
    using pointer = typename ContainerType::pointer;
    using const_pointer = typename ContainerType::const_pointer;
    using iterator = typename ContainerType::iterator;
    using const_iterator = typename ContainerType::const_iterator;

    constexpr CHashMap(ContainerType data, Hasher const& hash, KeyEqual const& keyEqual)
        : _data{data}
        , _pmhTable{makeCPmhTable<M>(_data, hash, utils::XorShift32{114514})}
        , _hash{hash}
        , _keyEqual{keyEqual}
    {}

    constexpr CHashMap(ContainerType list) 
        : CHashMap(list, Hasher{}, KeyEqual{})
    {}

    // 查找与访问
    template <typename _Key>
    constexpr mapped_type const& at(_Key const& key) const {
        auto const& kv = lookup(key);
        if (_keyEqual(meta::getKey(kv), key)) {
            return kv.second;
        }
        throw std::out_of_range("unknown key");
    }

    template <typename _Key>
    constexpr mapped_type& at(_Key const& key) {
        auto& kv = lookup(key);
        if (_keyEqual(meta::getKey(kv), key)) {
            return kv.second;
        }
        throw std::out_of_range("unknown key");
    }

    template <typename _Key>
    constexpr const_iterator find(_Key const& key) const noexcept {
        auto index = _pmhTable.lookup(key, _hash);
        if (_keyEqual(meta::getKey(_data[index]), key)) {
            return _data.begin() + index;
        }
        return end();
    }

    template <typename _Key>
    constexpr mapped_type& find(_Key const& key) noexcept {
        auto index = _pmhTable.lookup(key, _hash);
        if (_keyEqual(meta::getKey(_data[index]), key)) {
            return _data.begin() + index;
        }
        return end();
    }

    // 迭代器
    constexpr iterator begin() noexcept { return _data.begin(); }
    constexpr const_iterator begin() const noexcept { return _data.begin(); }

    constexpr iterator end() noexcept { return _data.end(); }
    constexpr const_iterator end() const noexcept { return _data.end(); }

    // 容量
    static constexpr bool empty() noexcept { return !N; }
    static constexpr size_type size() noexcept { return N; }
    static constexpr size_type max_size() noexcept { return N; }
private:
    template <typename _Key>
    constexpr value_type const& lookup(_Key const& key) const noexcept {
        return _data[_pmhTable.lookup(key, _hash)];
    }

    template <typename _Key>
    constexpr value_type& lookup(_Key const& key) noexcept {
        return _data[_pmhTable.lookup(key, _hash)];
    }
};

} // namespace HX::container

#endif // !_HX_C_HASH_MAP_H_