#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-11-29 12:46:52
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
#include <stdexcept>
#include <vector>
#include <string>

#include <openssl/evp.h>
#include <openssl/rand.h>

namespace HX::net::token {

class Aes256Gcm {
public:
    inline constexpr static std::size_t KeyLen = 32;

    explicit Aes256Gcm(const std::string& key)
        : _ctx{::EVP_CIPHER_CTX_new()}
        , _key{reinterpret_cast<const unsigned char*>(key.data()), 
               reinterpret_cast<const unsigned char*>(key.data()) + key.size()}
    {
        if (key.size() != KeyLen) [[unlikely]] {
            throw std::runtime_error("AES-256-GCM key size must be 32 bytes");
        }

        if (!_ctx) [[unlikely]] {
            throw std::runtime_error("EVP_CIPHER_CTX_new failed");
        }
    }

    ~Aes256Gcm() {
        if (_ctx) {
            ::EVP_CIPHER_CTX_free(_ctx);
            _ctx = nullptr;
        }
    }

    Aes256Gcm(const Aes256Gcm&) = delete;
    Aes256Gcm& operator=(const Aes256Gcm&) = delete;

    Aes256Gcm(Aes256Gcm&& other) noexcept
        : _ctx(other._ctx)
        , _key(std::move(other._key))
    {
        other._ctx = nullptr;
    }

    Aes256Gcm& operator=(Aes256Gcm&& other) noexcept {
        if (this != &other) {
            if (_ctx) {
                ::EVP_CIPHER_CTX_free(_ctx);
            }
            _ctx = other._ctx;
            _key = std::move(other._key);
            other._ctx = nullptr;
        }
        return *this;
    }

    // 生成随机IV
    static std::vector<unsigned char> generateRandomIV(std::size_t length = 12) {
        std::vector<unsigned char> iv(length);
        if (!::RAND_bytes(iv.data(), static_cast<int>(iv.size()))) {
            throw std::runtime_error("IV generation failed");
        }
        return iv;
    }

    // AES-GCM加密
    std::string encrypt(const std::string& plaintext) {
        constexpr std::size_t ivLen = 12;
        auto iv = generateRandomIV(ivLen);

        std::vector<unsigned char> ciphertext(plaintext.size() + 16);
        std::vector<unsigned char> tag(16);

        if (::EVP_EncryptInit_ex(_ctx, ::EVP_aes_256_gcm(), nullptr, _key.data(), iv.data()) != 1) [[unlikely]] {
            throw std::runtime_error("EncryptInit failed");
        }

        int outLen = 0;
        if (::EVP_EncryptUpdate(
            _ctx,
            ciphertext.data(),
            &outLen,
            reinterpret_cast<const unsigned char*>(plaintext.data()),
            static_cast<int>(plaintext.size())) != 1
        ) [[unlikely]] {
            throw std::runtime_error("EncryptUpdate failed");
        }
        int totalLen = outLen;

        if (
            ::EVP_EncryptFinal_ex(_ctx, ciphertext.data() + outLen, &outLen) != 1
        ) [[unlikely]] {
            throw std::runtime_error("EncryptFinal failed");
        }
        totalLen += outLen;

        if (
            ::EVP_CIPHER_CTX_ctrl(_ctx, EVP_CTRL_GCM_GET_TAG, 16, tag.data()) != 1
        ) [[unlikely]] {
            throw std::runtime_error("Get tag failed");
        }

        // 拼接: IV + Ciphertext + Tag
        std::string res;
        res.reserve(ivLen + static_cast<std::size_t>(totalLen) + tag.size());
        res.append(reinterpret_cast<const char*>(iv.data()), ivLen);
        res.append(reinterpret_cast<const char*>(ciphertext.data()), static_cast<std::size_t>(totalLen));
        res.append(reinterpret_cast<const char*>(tag.data()), tag.size());

        return res;
    }

    // AES-GCM解密
    std::string decrypt(const std::string& encrypted) {
        constexpr std::size_t ivLen = 12;
        constexpr std::size_t tagLen = 16;

        if (encrypted.size() < ivLen + tagLen) [[unlikely]] {
            throw std::runtime_error("Invalid encrypted data");
        }

        const unsigned char* iv = reinterpret_cast<const unsigned char*>(encrypted.data());
        const unsigned char* cipher = iv + ivLen;
        size_t cipherLen = encrypted.size() - ivLen - tagLen;
        const unsigned char* tag = iv + encrypted.size() - tagLen;

        std::vector<unsigned char> plaintext(cipherLen);

        if (
            ::EVP_DecryptInit_ex(_ctx, ::EVP_aes_256_gcm(), nullptr, _key.data(), iv) != 1
        ) [[unlikely]] {
            throw std::runtime_error("DecryptInit failed");
        }

        int outLen = 0;
        if (::EVP_DecryptUpdate(
            _ctx,
            plaintext.data(), 
            &outLen,
            cipher,
            static_cast<int>(cipherLen)) != 1
        ) [[unlikely]] {
            throw std::runtime_error("DecryptUpdate failed");
        }
        int totalLen = outLen;

        if (::EVP_CIPHER_CTX_ctrl(
            _ctx,
            EVP_CTRL_GCM_SET_TAG,
            static_cast<int>(tagLen),
            const_cast<unsigned char*>(tag)) != 1
        ) [[unlikely]] {
            throw std::runtime_error("Set tag failed");
        }

        if (::EVP_DecryptFinal_ex(
            _ctx,
            plaintext.data() + outLen,
            &outLen) != 1
        ) [[unlikely]] {
            throw std::runtime_error("DecryptFinal failed: authentication failed");
        }
        totalLen += outLen;

        return std::string(
            reinterpret_cast<char*>(plaintext.data()),
            static_cast<std::size_t>(totalLen)
        );
    }

private:
    ::EVP_CIPHER_CTX* _ctx = nullptr;
    std::vector<unsigned char> _key;
};

} // namespace HX::net::token

#endif // !defined(HXLIBS_ENABLE_SSL)
