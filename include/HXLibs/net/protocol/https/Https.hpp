#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2024-08-31 22:07:47
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

#ifdef HXLIBS_ENABLE_SSL

#include <stdexcept>
#include <span>
#include <vector>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <HXLibs/net/protocol/https/Context.hpp>

namespace HX::net {

class SslHandshake {
public:
    SslHandshake()
        : _ssl{SSL_new(Context::getContext().getSslCtx())}
    {
        if (!_ssl) {
            throw std::runtime_error("SSL_new failed");
        }

        BIO* sslBio = nullptr;
        BIO* netBio = nullptr;
        if (!BIO_new_bio_pair(&sslBio, 0, &netBio, 0))
            throw std::runtime_error("BIO_new_bio_pair failed");

        _sslBio = sslBio;
        _netBio = netBio;
        SSL_set_bio(_ssl, _sslBio, _sslBio);
    }

    void setAccept() {
        SSL_set_accept_state(_ssl);
    }

    void setConnect() {
        SSL_set_connect_state(_ssl);
    }

    ~SslHandshake() {
        if (_ssl)
            SSL_free(_ssl);
        if (_netBio)
            BIO_free(_netBio);
    }

    // 外层提供网络密文
    void feedNetworkData(std::span<const char> data) {
        if (BIO_write(_netBio, data.data(), (int)data.size()) <= 0) [[unlikely]] {
            throw std::runtime_error("BIO_write failed");
        }
    }

    // 从SSL读取明文应用数据
    int readAppData(std::span<char> buf) {
        int n = SSL_read(_ssl, buf.data(), (int)buf.size());
        if (n > 0) {
            return n;
        } else {
            int err = SSL_get_error(_ssl, n);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
                return 0; // 等待更多密文
            else
                throw std::runtime_error("SSL_read failed");
        }
    }

    // 应用层写入明文, 产生密文
    void writeAppData(std::span<char const> buf) {
        int res = SSL_write(_ssl, buf.data(), static_cast<int>(buf.size()));
        if (res <= 0) {
            int err = SSL_get_error(_ssl, res);
            if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) [[unlikely]] {
                throw std::runtime_error("SSL_write failed");
            }
        }
    }

    // 取出要发送的密文
    std::vector<char> takeNetworkData() {
        std::vector<char> out;
        int pending = BIO_pending(_netBio);
        if (pending > 0) {
            out.resize(static_cast<std::size_t>(pending));
            int n = BIO_read(_netBio, out.data(), pending);
            if (n <= 0) {
                throw std::runtime_error("BIO_read failed");
            }
        }
        return out;
    }

    // 驱动握手状态机, true 为握手成功
    bool driveHandshake() {
        int res = SSL_do_handshake(_ssl);
        if (res == 1) {
            return true;
        }
        int err = SSL_get_error(_ssl, res);
        if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE)
            throw std::runtime_error("SSL handshake failed");
        return false;
    }

private:
    SSL* _ssl{nullptr};
    BIO* _sslBio{nullptr};
    BIO* _netBio{nullptr};
};

} // namespace HX::net

#endif // HXLIBS_ENABLE_SSL