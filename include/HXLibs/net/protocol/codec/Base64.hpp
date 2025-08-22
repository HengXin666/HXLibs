#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-20 18:11:49
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
#include <span>

namespace HX::net {

inline constexpr const char Base64Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                            "abcdefghijklmnopqrstuvwxyz"
                                            "0123456789+/";

inline std::string base64Encode(std::span<uint8_t> str) {
    std::string res;
    int i = 0;
    int j = 0;
    char charArray3[3];
    char charArray4[4];

    auto bytesToEncode = str.data();
    std::size_t inLen = str.size();
    while (inLen--) {
        charArray3[i++] = static_cast<char>(*(bytesToEncode++));
        if (i == 3) {
            charArray4[0] = static_cast<char>((charArray3[0] & 0xfc) >> 2);
            charArray4[1] = static_cast<char>(((charArray3[0] & 0x03) << 4)
                          + ((charArray3[1] & 0xf0) >> 4));
            charArray4[2] = static_cast<char>(((charArray3[1] & 0x0f) << 2)
                          + ((charArray3[2] & 0xc0) >> 6));
            charArray4[3] = static_cast<char>(charArray3[2] & 0x3f);

            for (i = 0; i < 4; ++i)
                res += Base64Chars[static_cast<std::size_t>(charArray4[i])];
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 3; ++j)
            charArray3[j] = '\0';

        charArray4[0] = static_cast<char>((charArray3[0] & 0xfc) >> 2);
        charArray4[1] = static_cast<char>(((charArray3[0] & 0x03) << 4)
                      + ((charArray3[1] & 0xf0) >> 4));
        charArray4[2] = static_cast<char>(((charArray3[1] & 0x0f) << 2)
                      + ((charArray3[2] & 0xc0) >> 6));
        charArray4[3] = static_cast<char>(charArray3[2] & 0x3f);

        for (j = 0; j < i + 1; ++j)
            res += Base64Chars[static_cast<std::size_t>(charArray4[j])];

        while (i++ < 3)
            res += '=';
    }

    return res;
}

} // namespace HX::net

