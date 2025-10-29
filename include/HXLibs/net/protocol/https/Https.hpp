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

#include <HXLibs/exception/SslException.hpp>
#include <HXLibs/utils/FileUtils.hpp>

namespace HX::net {

enum class SslVerifyOption {
    // 不验证
    None = SSL_VERIFY_NONE,
    // 客户端启用证书验证
    ClientPeer = SSL_VERIFY_PEER,
    // 服务端启用证书验证, 且必须验证
    ServerPeer = SSL_VERIFY_PEER
               | SSL_VERIFY_FAIL_IF_NO_PEER_CERT
               | SSL_VERIFY_CLIENT_ONCE,
};

struct SslConfig {
    SslVerifyOption verifyOption = SslVerifyOption::None;   // 校验选项
    std::string certFile{};                                 // 证书路径
    std::string keyFile{};                                  // 私钥路径
};

struct SslContext {
    enum class SslType {
        Server,
        Client
    };

    static auto& get() noexcept {
        static SslContext ctx;
        return ctx;
    }

    template <SslType Type = SslType::Server>
    constexpr void init(SslConfig config) {
        auto& ctx = Type == SslType::Server ? _sslSerCtx : _sslCliCtx;
        if (!ctx) {
            ctx = makeSslCtx(std::move(config), Type == SslType::Server);
        }
    }

    // 可能为 nullptr, 因此必须保证先调用了 init
    template <SslType Type>
    constexpr SSL_CTX* getCtx() noexcept {
        if constexpr (Type == SslType::Server) {
            return _sslSerCtx;
        } else {
            return _sslCliCtx;
        }
    }
private:
    SSL_CTX* _sslSerCtx{nullptr};
    SSL_CTX* _sslCliCtx{nullptr};

    static SSL_CTX* makeSslCtx(const SslConfig& config, bool isServer) {
        OPENSSL_init_ssl(0, nullptr);

        SSL_CTX* sslCtx = SSL_CTX_new(isServer ? TLS_server_method() : TLS_client_method());
        if (!sslCtx) {
            throw exception::SslException{"SSL_CTX_new(Failed)"};
        }

        if (isServer) {
            // 服务器端需要设置验证模式, 要求客户端提供证书
            SSL_CTX_set_verify(sslCtx, static_cast<int>(config.verifyOption), nullptr);
        } else {
            // 客户端可以设置验证服务器证书
            SSL_CTX_set_verify(sslCtx, static_cast<int>(config.verifyOption), nullptr);
        }

        SSL_CTX_set_verify_depth(sslCtx, 4);
        SSL_CTX_set_default_verify_paths(sslCtx);

        if (!config.certFile.empty() && !config.keyFile.empty()) {
            if (SSL_CTX_use_certificate_file(sslCtx, config.certFile.c_str(), SSL_FILETYPE_PEM) <= 0) {
                throw exception::SslException{"Client certificate load(Failed)"};
            }
            if (SSL_CTX_use_PrivateKey_file(sslCtx, config.keyFile.c_str(), SSL_FILETYPE_PEM) <= 0) {
                throw exception::SslException{"Client private key load(Failed)"};
            }
            if (!SSL_CTX_check_private_key(sslCtx)) {
                throw exception::SslException{"Client certificate/key mismatch"};
            }
        }

        return sslCtx;
    }

    SslContext() = default;
    SslContext& operator=(SslContext&&) = delete;
    
    ~SslContext() noexcept {
        if (_sslSerCtx) {
            SSL_CTX_free(_sslSerCtx);
        }
        if (_sslCliCtx) {
            SSL_CTX_free(_sslCliCtx);
        }
    }
};

class SslBio {
public:
    SslBio(SslContext::SslType type)
        : _ssl{
            SSL_new(type == SslContext::SslType::Server 
                ? SslContext::get().getCtx<SslContext::SslType::Server>()
                : SslContext::get().getCtx<SslContext::SslType::Client>()
            )}
    {
        if (!_ssl) {
            throw std::runtime_error("SSL_new failed");
        }

        // 创建 BIO 对
        if (!BIO_new_bio_pair(
            &_sslBio, utils::FileUtils::kBufMaxSize + 2048,
            &_netBio, utils::FileUtils::kBufMaxSize + 2048
        )) {
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

    SslBio& operator=(SslBio&&) = delete;

    ~SslBio() {
        if (_ssl) {
            SSL_free(_ssl); // 这会自动释放 _sslBio
        }
        if (_netBio) {
            BIO_free(_netBio);
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
        // 注意! cipher.size() 应该 >= 0
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
        if (n > 0) [[likely]] {
            return n;
        }
        int err = SSL_get_error(_ssl, n);
#if 0
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
#else
        if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) [[unlikely]] {
            if (err == SSL_ERROR_ZERO_RETURN) [[likely]] {
                return 0; // SSL连接正常关闭
            }
            throw std::runtime_error("SSL_write failed");
        }
        return -1; // 等待更多数据
#endif
    }

    /**
     * @brief 写入明文, 产生密文
     * @param buf 明文
     */
    void writePlaintext(std::span<char const> buf) {
        if (!isInitFinished()) [[unlikely]] {
            throw std::runtime_error("Cannot write plaintext before handshake finished");
        }
        int res = SSL_write(_ssl, buf.data(), static_cast<int>(buf.size()));
        if (res <= 0) [[unlikely]] {
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
        if (res == 1) [[likely]] {
            return true;
        }
        int err = SSL_get_error(_ssl, res);
        if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE)
            throw std::runtime_error("SSL handshake failed");
        return false;
    }

private:
    SSL* _ssl{nullptr};         // SSL会话对象, 负责加密解密操作
    BIO* _sslBio{nullptr};      // SSL BIO, 处理SSL协议层数据
    BIO* _netBio{nullptr};      // 网络 BIO, 处理原始网络数据传输
};

} // namespace HX::net

#endif // HXLIBS_ENABLE_SSL