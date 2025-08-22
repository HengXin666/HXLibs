#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2024-08-19 14:49:57
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

#include <type_traits>
#include <bit>

namespace HX::utils {

struct ByteUtils {
    template <typename T>
    static constexpr T bruteForceByteSwap(T value) {
        if constexpr (sizeof(T) > 1) {
            char* ptr = reinterpret_cast<char*>(&value);
            for (std::size_t i = 0; i < sizeof(T) / 2; ++i) {
                std::swap(ptr[i], ptr[sizeof(T) - 1 - i]);
            }
        }
        return value;
    }

    template <typename T>
        requires (std::is_trivial_v<T> && !std::is_integral_v<T>)
    static constexpr T byteswap(T value) {
        return bruteForceByteSwap(value);
    }

    template <typename T>
        requires(std::is_integral_v<T>)
    static constexpr T byteswap(T value) {
#if __cpp_lib_byteswap
        return std::byteswap(value);
#elif defined(__GNUC__) && defined(__has_builtin)
#if __has_builtin(__builtin_bswap)
        return __builtin_bswap(value);
#else
        return bruteForceByteSwap(value);
#endif
#else
        return bruteForceByteSwap(value);
#endif
    }

#if __cpp_lib_endian // C++20 支持的 <bit> 头文件中可以方便地判断本地硬件的大小端
// 是否为小端
inline static constexpr bool kIsLittleEndian = std::endian::native == std::endian::little;
#else
#if _MSC_VER
#include <endian.h>
#if defined(__BYTE_ORDER) && __BYTE_ORDER != 0 && __BYTE_ORDER == __BIG_ENDIAN
inline static constexpr bool kIsLittleEndian = false;
#else
inline static constexpr bool kIsLittleEndian = true;
#endif
#else
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != 0
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
inline static constexpr bool kIsLittleEndian = false;
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
inline static constexpr bool kIsLittleEndian = true;
#else
inline static constexpr bool kIsLittleEndian = true;
#endif
#else
inline static constexpr bool kIsLittleEndian = true;
#endif
#endif
#endif

    template <typename T>
        requires(std::is_trivial_v<T>)
    static constexpr T byteswapIfLittle(T value) {
        if constexpr (kIsLittleEndian) {
            return byteswap(value);
        } else {
            return value;
        }
    }
};

} // namespace HX::utils

