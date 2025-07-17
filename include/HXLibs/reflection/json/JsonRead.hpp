#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-15 16:40:20
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

#include <cstdint>
#include <stdexcept>
#include <string_view>

#include <unordered_map>

#include <HXLibs/reflection/MemberName.hpp>

namespace HX::reflection {

namespace internal {

// 宽松解析
inline constexpr bool LooseParsing = false;

/**
 * @brief 跳过空白字符
 * @tparam It 
 * @param begin 
 * @param end 
 */
template <typename It>
void skipWhiteSpace(It&& it, It&& end) {
    // '\n'; // 10
    // '\t'; // 9
    // ' ' ; // 32
    // '\r'; // 13
    constexpr uint64_t WhiteSpaceBitMask
        = (1UL << static_cast<uint8_t>(' '))
        | (1UL << static_cast<uint8_t>('\t'))
        | (1UL << static_cast<uint8_t>('\n'))
        | (1UL << static_cast<uint8_t>('\r'));

    while (it != end) {
        if constexpr (LooseParsing) {
            if (static_cast<uint8_t>(*it) < 33) {
                ++it;
            } else {
                break;
            }
        } else {
            if (static_cast<uint8_t>(*it) < 33
                && (1U << static_cast<uint8_t>(*it) & WhiteSpaceBitMask)
            ) {
                ++it;
            } else {
                break;
            }
        }
    }
}

template <char... Cs, typename It>
void match(It&& it, It&& end) {
    const auto n = static_cast<std::size_t>(std::distance(it, end));
    if (n < sizeof...(Cs)) [[unlikely]] {
        throw std::runtime_error{
            "The buffer zone ended prematurely, not as expected"};
    }
    if (((*it++ != Cs) || ...)) [[unlikely]] {
        throw std::runtime_error{
            std::string{"Characters are not as expected: "}.append(
                Cs..., sizeof...(Cs))};
    }
}

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

template <typename K, typename V, std::size_t N>
struct ConstantHashMap {

private:
    inline static constexpr std::size_t Size = nextHighestPowOfTow<N>();
    std::array<V, Size> _data;
};

template <typename T>
constexpr auto getNameIdxMap() noexcept {
    constexpr std::size_t N = membersCountVal<T>;
    ConstantHashMap<std::string, std::size_t, N> res;
    return ConstantHashMap<std::string, std::size_t, N>::Size;
}

template <typename T, typename It>
void fromJson(T &t, It&& it, It&& end) {
    // 无需考虑根是数组的情况, 因为我们是反射库!
    
    // 找 { (去掉空白, 必然得是, 否则非法)
    skipWhiteSpace(it, end);
    match<'{'>(it, end);
    
    // 找 "
    skipWhiteSpace(it, end);

    if (*it == '}') [[unlikely]] {
        ++it;
        return;
    }
    
    constexpr std::size_t N = membersCountVal<T>;

    if constexpr (N > 0) {
        constexpr auto nameArr = getMembersNames<T>();
        auto& tp = internal::getObjTie<T>(t);
        // 需要 name -> idx 映射
        while (it != end) {
            match<'"'>(it, end);
            
            skipWhiteSpace(it, end);
            // 找 "
        
            // 找 :
        
            // 解析值 (非空白字符)
        
            // 找 , 或者 }

        }
    }
}

} // namespace internal

/*
    核心思路:
    1. 因为顺序解析, 肯定会先解析到 key
    2. 通过 key 如果可以直接给 obj.key 赋值, 构造 decltype(obj<I>) (显然 先把字段反射到tuple里面才行)
 */

template <typename T>
void fromJson(T& t, std::string_view json) {
    internal::fromJson(t, json.begin(), json.end());
}

} // namespace HX::reflection

#endif // !_HX_JSON_READ_H_