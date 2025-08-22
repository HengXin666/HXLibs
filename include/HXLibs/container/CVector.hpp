#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-17 17:28:49
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
 
#include <utility>

namespace HX::container {

/**
 * @brief 编译期 vector
 * @tparam T 
 * @tparam N 
 */
template <typename T, std::size_t N>
class CVector {
    T _data[N]{};
    std::size_t _dIdx = 0; // 指向位置为无效, 有效索引为 [0, _dIdx)
public:
    // 容器 typdefs
    using value_type = T;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using iterator = pointer;
    using const_iterator = const_pointer;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    constexpr CVector() = default;

    constexpr CVector(std::size_t size, T const& t)
        : _data{}
        , _dIdx{size}
    {
        for (std::size_t i = 0; i < _dIdx; ++i)
            _data[i] = t;
    }

    constexpr size_type size() const noexcept { return _dIdx; }
    constexpr static size_type max_size() noexcept { return N; }
    
    // 访问
    constexpr reference operator[](std::size_t idx) { return _data[idx]; }
    constexpr const_reference operator[](std::size_t idx) const { return _data[idx]; }

    constexpr reference front() { return _data[0]; }
    constexpr const_reference front() const { return _data[0]; }

    constexpr reference back() { return _data[_dIdx - 1]; }
    constexpr const_reference back() const { return _data[_dIdx - 1]; }

    constexpr iterator find(T const& v) noexcept {
        for (std::size_t i = 0; i < _dIdx; ++i) {
            if (_data[i] == v) {
                return &_data[i];
            }
        }
        return end();
    }

    constexpr const_iterator find(T const& v) const noexcept { 
        for (std::size_t i = 0; i < _dIdx; ++i) {
            if (_data[i] == v) {
                return &_data[i];
            }
        }
        return end();
    }

    // 迭代器
    constexpr iterator begin() { return _data; }
    constexpr const_iterator begin() const { return _data; }

    constexpr iterator end() { return _data + _dIdx + 1; }
    constexpr const_iterator end() const { return _data + _dIdx + 1; }

    // 修改
    constexpr void push_back(value_type const& t) { _data[_dIdx++] = t; }
    constexpr void push_back(value_type&& t) { _data[_dIdx++] = std::move(t); }

    constexpr void clear() noexcept { _dIdx = 0; }
};

} // namespace HX::container

