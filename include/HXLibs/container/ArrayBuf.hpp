#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-08 15:37:52
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
#ifndef _HX_ARRAY_BUF_H_
#define _HX_ARRAY_BUF_H_

#include <span>
#include <cstring>

namespace HX::container {

template <typename T, std::size_t N>
struct ArrayBuf {
    /**
     * @brief s 是指向 arr 的指针
     * @warning s.size() <= arr.size() && s.data 是 arr的子区间 && s.size() != 0
     * @param s 
     */
    void moveToHead(std::span<T const> s) {
        index = s.size() - 1;
        std::memmove(arr, s.data(), s.size());
    }

    constexpr std::size_t size() const {
        return index + 1;
    }

    constexpr static std::size_t max_size() {
        return N;
    }

    constexpr T const* data() const {
        return arr;
    }

    constexpr T* data() {
        return arr;
    }

    void clear() {
        index = 0;
    }

private:
    std::size_t index;
    T arr[N];
};

} // namespace HX::container

#endif // !_HX_ARRAY_BUF_H_