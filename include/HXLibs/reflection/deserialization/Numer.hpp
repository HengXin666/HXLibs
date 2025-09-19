#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-09-19 23:19:25
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

#include <cstdint>
#include <stdexcept>
#include <string>
#include <array>
#include <charconv>

#include <HXLibs/reflection/EnumName.hpp>

namespace HX::reflection {

/**
 * @brief 统一的数字的反序列化类
 */
struct Numer {
    template <typename T, typename It>
        requires(std::is_same_v<bool, T>)
    static void fromNumer(T& t, It&& it, It&& end) {
        // 解析布尔
        if (*it == '0' || *it == '1') {
            t = *it++ - '0';
        } else {
            if (std::distance(it, end) < 4) [[unlikely]] {
                throw std::runtime_error{"The buffer zone ended prematurely, not as expected"};
            }
            constexpr uint8_t Mask = static_cast<uint8_t>(~' '); // 大小写掩码

            if ((*(it + 0) & Mask) == 'T'
             && (*(it + 1) & Mask) == 'R'
             && (*(it + 2) & Mask) == 'U'
             && (*(it + 3) & Mask) == 'E'
            ) {
                t = true;
                it += 4;
                return;
            }

            if (std::distance(it, end) < 5) [[unlikely]] {
                throw std::runtime_error{"The buffer zone ended prematurely, not as expected"};
            }

            if ((*(it + 0) & Mask) == 'F'
             && (*(it + 1) & Mask) == 'A'
             && (*(it + 2) & Mask) == 'L'
             && (*(it + 3) & Mask) == 'S'
             && (*(it + 4) & Mask) == 'E'
            ) {
                t = false;
                it += 5;
            }
        }
    }

    template <typename T, typename It>
        requires(!std::is_same_v<bool, T> && (std::is_integral_v<T> || std::is_floating_point_v<T>))
    static void fromNumer(T& t, It&& it, It&& end) {
        // 解析数字
        auto left = it;
        constexpr static auto IsNumChars = []() constexpr {
            std::array<bool, 128> res{};
            res['0'] = true;
            res['1'] = true;
            res['2'] = true;
            res['3'] = true;
            res['4'] = true;
            res['5'] = true;
            res['6'] = true;
            res['7'] = true;
            res['8'] = true;
            res['9'] = true;
            res['+'] = true;
            res['-'] = true;
            res['e'] = true;
            res['E'] = true;
            res['.'] = true;
            return res;
        }();
        while (it != end) {
            if (*it > 0 && !IsNumChars[static_cast<std::size_t>(*it)])
                break;
            ++it;
        }
        // MSVC 没有迭代器隐私转换的重载, 应该使用 const char* 作为参数
        // 注意, 如果 it 是 end 迭代器
        auto* endPtr = &*left + (it - left);
        auto [ptr, ec] = std::from_chars(&*left, endPtr, t);
        // ptr 指向与模式不匹配的第一个字符; 当所有都匹配的时候: ptr == it
        if (ec != std::errc() || ptr != endPtr) [[unlikely]] { // 必需保证整个str都是数字
            // 解析数字出错
            throw std::runtime_error{
                "There was an error parsing the number: " + std::string{left, it}};
        }
    }

    template <typename T, typename It>
        requires(std::is_enum_v<T>)
    static void fromNumer(T& t, It&& it, It&& end) {
        constexpr static auto IsEnumNameChar = []() constexpr {
            std::array<bool, 128> res{};
            for (std::size_t c = '0'; c <= '9'; ++c)
                res[c] = true;
            for (std::size_t c = 'a'; c <= 'z'; ++c)
                res[c] = true;
            for (std::size_t c = 'A'; c <= 'Z'; ++c)
                res[c] = true;
            res['_'] = true;
            return res;
        }();
        auto begin = it;
        while (it != end) {
            if (*it > 0 && !IsEnumNameChar[static_cast<std::size_t>(*it)])
                break;
            ++it;
        }
        t = reflection::toEnum<T>(std::string_view{begin, it});
    }
};

} // namespace HX::reflection