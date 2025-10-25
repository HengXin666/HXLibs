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

#include <HXLibs/log/Log.hpp>

namespace HX::net {

class SslHandshake {
public:
    SslHandshake()
        : _ssl{SSL_new(Context::getContext().getSslCtx())}
    {
        if (!_ssl) {
            throw std::runtime_error("SSL_new failed");
        }

        // 创建 BIO 对
        if (!BIO_new_bio_pair(&_sslBio, 0, &_netBio, 0)) {
            SSL_free(_ssl);  // 清理已分配的 SSL
            throw std::runtime_error("BIO_new_bio_pair failed");
        }

        // 正确设置: sslBio 用于 SSL 内部, netBio 用于网络 I/O
        SSL_set_bio(_ssl, _sslBio, _sslBio);
    }

    bool isInitFinished() const noexcept {
        return SSL_is_init_finished(_ssl);
    }

    void setAccept() {
        SSL_set_accept_state(_ssl);
    }

    void setConnect() {
        SSL_set_connect_state(_ssl);
    }

    SslHandshake& operator=(SslHandshake&&) = delete;

    ~SslHandshake() {
        if (_ssl) {
            SSL_free(_ssl);  // 这会自动释放 _sslBio
            _ssl = nullptr;
            _sslBio = nullptr;  // 设置为 nullptr, 因为已经被 SSL_free 释放
        }
        if (_netBio) {
            BIO_free(_netBio);
            _netBio = nullptr;
        }
    }

/*
    bool isConnected() const noexcept {
        return _ssl && SSL_get_state(_ssl) == TLS_ST_OK;
    }

    bool hasPendingData() const noexcept {
        return _ssl && SSL_pending(_ssl) > 0;
    }

    bool shouldRetry(int result) const noexcept {
        if (result <= 0) {
            int err = SSL_get_error(_ssl, result);
            return err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE;
        }
        return false;
    }
*/

    /**
     * @brief 写入 (网络来的) 密文, 从而解密到明文
     * @param cipher 密文
     * @note 之后应该调用 `readPlaintext` 获取明文
     */
    void writeCiphertext(std::span<const char> cipher) {
        if (BIO_write(_netBio, cipher.data(), static_cast<int>(cipher.size())) <= 0) [[unlikely]] {
            throw std::runtime_error("BIO_write failed");
        }
    }

    /**
     * @brief 从SSL读取明文应用数据 (之前应该调用 writeCiphertext)
     * @param buf [out] 明文
     * @return int 读取的明文的字节数
     */
    int readPlaintext(std::span<char> buf) {
        int n = SSL_read(_ssl, buf.data(), static_cast<int>(buf.size()));
        if (n > 0) {
            return n;
        } else {
            int err = SSL_get_error(_ssl, n);
            switch (err) {
                case SSL_ERROR_WANT_READ:
                case SSL_ERROR_WANT_WRITE:
                    return -1; // 等待更多数据
                case SSL_ERROR_ZERO_RETURN:
                    return 0; // SSL连接正常关闭
                case SSL_ERROR_SYSCALL:
                    {
                        // 检查底层错误
                        auto sys_err = ERR_get_error();
                        if (sys_err == 0) {
                            if (n == 0) {
                                log::hxLog.warning("SSL 连接意外关闭");
                                return 0;
                            }
                            throw std::runtime_error("SSL 系统调用错误, 没有错误代码");
                        } else {
                            throw std::runtime_error("SSL 系统调用错误: " +
                                std::string(ERR_error_string(sys_err, nullptr)));
                        }
                    }
                default:
                    throw std::runtime_error("SSL_read failed, error: " + std::to_string(err) +
                        ", reason: " + std::string(ERR_error_string(ERR_get_error(), nullptr)));
            }
        }
    }

    /**
     * @brief 写入明文, 产生密文
     * @param buf 明文
     */
    void writePlaintext(std::span<char const> buf) {
        if (!isInitFinished()) {
            throw std::runtime_error("Cannot write plaintext before handshake finished");
        }
        int res = SSL_write(_ssl, buf.data(), static_cast<int>(buf.size()));
        if (res <= 0) {
            int err = SSL_get_error(_ssl, res);
            if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) [[unlikely]] {
                throw std::runtime_error("SSL_write failed");
            }
        }
    }

    /**
     * @brief 取出要发送的密文 (之前应该调用 writePlaintext)
     * @return std::vector<char> 密文
     */
    std::vector<char> readCiphertext() {
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