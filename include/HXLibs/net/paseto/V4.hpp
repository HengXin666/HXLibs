#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-11-28 00:13:50
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

#include <HXLibs/net/protocol/codec/Base64Url.hpp>
#include <HXLibs/reflection/json/JsonRead.hpp>
#include <HXLibs/reflection/json/JsonWrite.hpp>
#include <HXLibs/utils/StringUtils.hpp>

namespace HX::net::paseto {

namespace internal {

inline constexpr void le64(uint64_t x, std::size_t& idx, std::vector<uint8_t>& buf) {
    for (size_t i = 0; i < 8; ++i, x >>= 8) {
        buf[idx++] = static_cast<uint8_t>(x & 0xFF);
    }
}

inline constexpr void leStr(std::string_view sv, std::size_t& idx, std::vector<uint8_t>& buf) {
    le64(sv.size(), idx, buf);
    for (char c : sv) {
        buf[idx++] = static_cast<uint8_t>(c);
    }
}

inline constexpr std::vector<uint8_t> pae(
    std::string_view nonce,
    std::string_view footer,
    std::string_view implicitAssertion
) {
    std::vector<uint8_t> res;
    res.resize(8 * 4 + nonce.size() + footer.size() + implicitAssertion.size());
    std::size_t idx = 0;
    le64(3, idx, res);
    leStr(nonce, idx, res);
    leStr(footer, idx, res);
    leStr(implicitAssertion, idx, res);
    return res;
}

} // namespace internal

inline std::string toPaseto(std::string_view payload) noexcept {
    using namespace std::string_literals;
    // 1. 生成 24 字节随机 nonce
    auto nonce = utils::StringUtil::random<
        utils::StringUtil::kPureNumbers
        | utils::StringUtil::kCapital
        | utils::StringUtil::kLowercase
        | "+-*/\\~!@#$%^&([{}])=<>,._;:'?\"|"
    >(24);
    // 2. 拼装 pre-authenticated encoding (PAE)
    auto pae = internal::pae(nonce, {}, {});
    // 3. 用 XChaCha20-Poly1305 加密 payload

    // 4. 结构拼接 token_body = nonce || ciphertext

    // 5. Base64Url 编码 body_b64 = base64url(token_body)

    // 6. 拼成 token 字符串 token = "v4.local." + body_b64
    return "v4.local."s + base64UrlEncode(payload);
}

} // namespace HX::net::paseto