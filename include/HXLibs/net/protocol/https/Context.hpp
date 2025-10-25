#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2024-09-01 22:19:58
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

#include <openssl/bio.h>
#include <openssl/ssl.h>  
#include <openssl/err.h>

#include <HXLibs/exception/SslException.hpp>

#include <HXLibs/log/Log.hpp> // debug

#ifdef __GNUC__
#define HOT_FUNCTION [[gnu::hot]]
#else
#define HOT_FUNCTION
#endif

namespace HX::net {

/**
 * @brief Https验证模式设置参数包
 */
struct HttpsVerifyBuilder {
    std::string certificate;
    std::string privateKey;

    /**
     * @brief 校验模式
     * SSL_VERIFY_NONE: 不验证对方证书 (通常不推荐)
     * SSL_VERIFY_PEER: 验证对方证书
     * SSL_VERIFY_FAIL_IF_NO_PEER_CERT: 如果对方证书不存在, 则验证失败
     */
    int verifyMod = 0x01; // = SSL_VERIFY_PEER
};

/**
 * @brief Https 上下文, 用于管理公钥/秘钥, 以及提供`sslCtx`
 */
class Context {
    explicit Context() {};
    Context& operator=(Context&&) = delete;
public:
    /**
     * @brief 获取 Https 上下文
     * @return Context& 
     */
    HOT_FUNCTION static Context& getContext() {
        thread_local static Context context;
        return context;
    }

    /**
     * @brief 初始化服务端的SSL
     * @param verifyBuilder Https验证模式设置参数包
     * @throw 加载证书出现问题
     * @throw 证书不匹配
     */
    void initServerSSL(
        const HttpsVerifyBuilder& verifyBuilder
    ) {
        SSL_load_error_strings(); // 加载错误字符串

        // 初始化 SSL 库
        if (!SSL_library_init()) {
            throw exception::SslException{
                "SSL_library_init (Failed)"
            };
        }

        _sslCtx = ::SSL_CTX_new(::TLS_server_method()); // 创建新的 SSL 上下文

        // 设置验证模式
        ::SSL_CTX_set_verify(_sslCtx, verifyBuilder.verifyMod, nullptr);

        if (!_sslCtx) {
            throw exception::SslException{
                "SSL_CTX_new (Failed)"
            };
        }

        _errBio = ::BIO_new_fd(2, BIO_NOCLOSE); // 创建错误输出的 BIO

        if (!_errBio) {
            throw exception::SslException{
                "BIO_new_fd (Failed)"
            };
        }

        // 加载证书
        if (::SSL_CTX_use_certificate_file(_sslCtx, verifyBuilder.certificate.c_str(), SSL_FILETYPE_PEM) <= 0) {
            throw exception::SslException{
                "SSL_CTX_use_certificate_file (Failed): " + verifyBuilder.certificate + " Not Find"
            };
        }
        // 加载私钥
        if (::SSL_CTX_use_PrivateKey_file(_sslCtx, verifyBuilder.privateKey.c_str(), SSL_FILETYPE_PEM) <= 0) {
            throw exception::SslException{
                "SSL_CTX_use_certificate_file (Failed): " + verifyBuilder.privateKey + " Not Find"
            };
        }
        // 检查私钥
        if (!::SSL_CTX_check_private_key(_sslCtx)) {
            throw exception::SslException{
                "SSL_CTX_check_private_key (Failed)"
            };
        }
    }

    /**
     * @brief 初始化客户端的SSL
     * @param verifyBuilder Https验证模式设置参数包
     * @throw 加载证书出现问题
     * @throw 证书不匹配
     */
    void initClientSSL(
        const HttpsVerifyBuilder& verifyBuilder
    ) {
        SSL_load_error_strings(); // 加载错误字符串

        // 初始化 SSL 库
        if (!SSL_library_init()) {
            throw exception::SslException{
                "SSL_library_init (Failed)"
            };
        }
        
        _sslCtx = SSL_CTX_new(TLS_client_method()); // 创建新的 SSL 上下文

        if (!_sslCtx) {
            throw exception::SslException{
                "SSL_CTX_new (Failed)"
            };
        }

        // 设置验证模式
        SSL_CTX_set_verify(_sslCtx, verifyBuilder.verifyMod, nullptr);

        // 创建错误输出的 BIO
        _errBio = BIO_new_fd(2, BIO_NOCLOSE);
        if (!_errBio) {
            throw exception::SslException{
                "BIO_new_fd (Failed)"
            };
        }
        
        // 如果 verifyBuilder 中提供了证书和私钥, 则加载它们
        // 加载证书
        if (verifyBuilder.certificate.size() && ::SSL_CTX_use_certificate_file(_sslCtx, verifyBuilder.certificate.c_str(), SSL_FILETYPE_PEM) <= 0) {
            throw exception::SslException{
                "SSL_CTX_use_certificate_file (Failed): " + verifyBuilder.certificate + " Not Find"
            };
        }
        // 加载私钥
        if (verifyBuilder.privateKey.size() && ::SSL_CTX_use_PrivateKey_file(_sslCtx, verifyBuilder.privateKey.c_str(), SSL_FILETYPE_PEM) <= 0) {
            throw exception::SslException{
                "SSL_CTX_use_certificate_file (Failed): " + verifyBuilder.privateKey + " Not Find"
            };
        }
        // 检查私钥
        if (verifyBuilder.certificate.size() 
            && verifyBuilder.privateKey.size()
            && !::SSL_CTX_check_private_key(_sslCtx)
        ) {
            throw exception::SslException{
                "SSL_CTX_check_private_key (Failed)"
            };
        }
    }

    BIO* getErrBio() const {
        return _errBio;
    }

    SSL_CTX* getSslCtx() const {
        // 发现问题!, 客户端的崩溃是潜在的跨线程调用导致的...
        log::hxLog.debug("get getSslCtx:", _sslCtx);
        return _sslCtx;
    }
private:
    BIO* _errBio = nullptr;     // 用于 SSL 错误输出的 BIO
    SSL_CTX* _sslCtx = nullptr; // 全局 SSL 上下文
};

} // namespace HX::net

#undef HOT_FUNCTION
