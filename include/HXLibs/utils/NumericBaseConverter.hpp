#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-07 13:39:49
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
#include <array>
#include <numeric>

namespace HX::utils {

/**
 * @brief 进制转化工具类
 */
struct NumericBaseConverter {
    /**
     * @brief 从十进制`num`转化为十六进制字符串
     * @param num 十进制 (非负数)
     * @return std::string 十六进制
     */
    static std::string hexadecimalConversion(std::size_t num) {
        using namespace std::string_view_literals;
        static std::string_view str = "0123456789ABCDEF"sv;
        std::string res;
        do {
            res += str[num % 16];
            num /= 16;
        } while (num);
        return {res.rbegin(), res.rend()};
    }

    /**
     * @brief 进制转换, 从 str 转换为 T 整数类型, 仅支持 [2 ~ (10 + 26)] 进制, 不区分字母大小写
     * @tparam T 整数类型
     * @tparam Base 进制
     * @tparam Str 
     * @param sv 
     * @return T 
     */
    template <typename T, T Base = static_cast<T>(10), typename Str>
    static T strToNum(Str&& sv) noexcept {
        T res{0};
        T pow{1};
        for (auto it = sv.rbegin(); it != sv.rend(); ++it) {
            if constexpr (Base > 10) {
                if constexpr (Base > 10 + 26) {
                    // 未定义, 不支持
                    static_assert(!sizeof(T), "UB: Base > 36");
                }
                constexpr static std::array<char, 128> Benchmark = []() {
                    std::array<char, 128> arr{};
                    std::iota(arr.begin(), arr.end(), 0);
                    for (char c = '0'; c <= '9'; ++c)
                        arr[static_cast<std::size_t>(c)] = '0';
                    for (char c = 'a'; c <= 'z'; ++c)
                        arr[static_cast<std::size_t>(c)] = 'a' - 10;
                    for (char c = 'A'; c <= 'Z'; ++c)
                        arr[static_cast<std::size_t>(c)] = 'A' - 10;
                    return arr;
                }();
                res += static_cast<T>(*it - Benchmark[static_cast<std::size_t>(*it)]) * pow;
            } else {
                res += static_cast<T>(*it - '0') * pow;
            }
            pow *= Base;
        }
        return res;
    }
};

} // namespace HX::utils

