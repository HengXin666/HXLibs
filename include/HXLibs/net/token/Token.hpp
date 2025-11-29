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

#if defined(HXLIBS_ENABLE_SSL)
#include <HXLibs/net/protocol/codec/Base64Url.hpp>
#include <HXLibs/reflection/json/JsonRead.hpp>
#include <HXLibs/reflection/json/JsonWrite.hpp>

#include <HXLibs/net/token/tools/Aes256Gcm.hpp>

namespace HX::net::token {

namespace internal {

struct KeyFromType {
    static std::string getKey() {
        return "HX::token::KeyFromType@Heng_Xin@";
    }
};

} // namespace internal

struct TokenApi {

    /**
     * @brief 将 T 转为 token
     * @tparam T 
     * @param t 
     * @return std::string tokenStr
     */
    template <typename T>
    std::string toToken(T&& t) {
        std::string json;
        reflection::toJson(t, json);
        return base64UrlEncode(
            _aes256Gcm.encrypt(json)
        );
    }


    /**
     * @brief 从toekn反序列化为 T
     * @tparam T 
     * @param t
     * @param urlBase64 
     */
    template <typename T>
    void fromToken(T& t, std::string_view urlBase64) {
        reflection::fromJson(t, _aes256Gcm.decrypt(
            base64UrlDecode(urlBase64)
        ));
    }

    template <typename KeyFromType = internal::KeyFromType>
        requires(requires (KeyFromType k) {
            { KeyFromType::getKey() } -> std::convertible_to<std::string>;
        })
    static TokenApi& get() {
        thread_local static TokenApi s{KeyFromType::getKey()}; 
        return s;
    }

private:
    TokenApi(std::string key)
        : _aes256Gcm{std::move(key)}
    {}

    TokenApi& operator=(TokenApi&&) noexcept = delete;

    Aes256Gcm _aes256Gcm;
};

} // namespace HX::net::token

#endif // !defined(HXLIBS_ENABLE_SSL)