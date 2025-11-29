#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-11-28 00:02:23
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
#include <string>
#include <array>
#include <string_view>
#include <span>
#include <stdexcept>

namespace HX::net {

inline constexpr std::array<uint8_t, 64> kEncodeTable{
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
    'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
    'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '-', '_'};

inline constexpr auto kDecodeTable = []() constexpr {
    std::array<uint8_t, 256> t{};
    t.fill(0xFF);
    for (size_t i = 0; i < 64; ++i)
        t[kEncodeTable[i]] = uint8_t(i);
    return t;
}();

/**
 * @brief 编码
 * @param input 
 * @return std::string 
 */
inline std::string base64UrlEncode(std::span<const uint8_t> input) {
    const size_t len = input.size();
    size_t outLen = (len * 4 + 2) / 3; // Base64Url 无 '=' 填充
    std::string out;
    out.resize(outLen);

    size_t o = 0;
    size_t i = 0;
    while (i + 3 <= len) {
        uint32_t v = (uint32_t(input[i]) << 16) | (uint32_t(input[i + 1]) << 8)
                     | uint32_t(input[i + 2]);
        i += 3;

        out[o++] = (char)kEncodeTable[(v >> 18) & 0x3F];
        out[o++] = (char)kEncodeTable[(v >> 12) & 0x3F];
        out[o++] = (char)kEncodeTable[(v >> 6) & 0x3F];
        out[o++] = (char)kEncodeTable[v & 0x3F];
    }

    size_t rem = len - i;
    if (rem) {
        uint32_t v = uint32_t(input[i]) << 16;
        if (rem == 2)
            v |= uint32_t(input[i + 1]) << 8;

        out[o++] = (char)kEncodeTable[(v >> 18) & 0x3F];
        out[o++] = (char)kEncodeTable[(v >> 12) & 0x3F];
        if (rem == 2)
            out[o++] = (char)kEncodeTable[(v >> 6) & 0x3F];
    }

    out.resize(o);
    return out;
}

inline std::string base64UrlEncode(std::span<const char> input) {
    return base64UrlEncode(std::span<const uint8_t>{
        reinterpret_cast<const uint8_t*>(input.data()),
        input.size()
    });
}

/**
 * @brief 解码
 * @param input 
 * @return std::vector<uint8_t> 
 */
inline std::string base64UrlDecode(std::string_view input) {
    const std::size_t len = input.size();
    if (len == 0) [[unlikely]] {
        return {};
    }

    std::size_t fullBlocks = len / 4;
    std::size_t rem = len % 4;
    if (rem == 1) [[unlikely]] {
        throw std::runtime_error("invalid base64 length");
    }

    std::string out(fullBlocks * 3 + (rem ? rem - 1 : 0), '\0');

    const uint8_t* inPtr  = reinterpret_cast<const uint8_t*>(input.data());
    uint8_t* outPtr = reinterpret_cast<uint8_t*>(out.data());

    // 处理整块 4 字符
    for (std::size_t i = 0; i < fullBlocks; ++i) {
        uint32_t v = (static_cast<uint32_t>(kDecodeTable[inPtr[0]]) << 18u) |
                     (static_cast<uint32_t>(kDecodeTable[inPtr[1]]) << 12u) |
                     (static_cast<uint32_t>(kDecodeTable[inPtr[2]]) << 6u)  |
                      static_cast<uint32_t>(kDecodeTable[inPtr[3]]);

        *outPtr++ = (v >> 16) & 0xFF;
        *outPtr++ = (v >> 8) & 0xFF;
        *outPtr++ = v & 0xFF;
        inPtr += 4;
    }

    // 处理最后残余 2~3 字符
    if (rem == 2) {
        uint32_t v = (static_cast<uint32_t>(kDecodeTable[inPtr[0]]) << 6u) | 
                      static_cast<uint32_t>(kDecodeTable[inPtr[1]]);
        *outPtr++ = (v >> 4) & 0xFF;
    } else if (rem == 3) {
        uint32_t v = (static_cast<uint32_t>(kDecodeTable[inPtr[0]]) << 12u) |
                     (static_cast<uint32_t>(kDecodeTable[inPtr[1]]) << 6u)  |
                      static_cast<uint32_t>(kDecodeTable[inPtr[2]]);
        *outPtr++ = (v >> 10) & 0xFF;
        *outPtr++ = (v >> 2) & 0xFF;
    }

    return out;
}

} // namespace HX::net