#include <HXWeb/protocol/https/Context.h>

#include <openssl/bio.h>
#include <openssl/ssl.h>  
#include <openssl/err.h>

#include <HXSTL/exception/SslException.hpp>

namespace HX { namespace web { namespace protocol { namespace https {

void Context::initServerSSL(
    const HttpsVerifyBuilder& verifyBuilder
) {
    SSL_load_error_strings(); // 加载错误字符串

    // 初始化 SSL 库
    if (!SSL_library_init()) {
        throw HX::STL::exception::SslException{
            "SSL_library_init (Failed)"
        };
    }

    _sslCtx = ::SSL_CTX_new(::TLS_server_method()); // 创建新的 SSL 上下文

    // 设置验证模式
    ::SSL_CTX_set_verify(_sslCtx, verifyBuilder.verifyMod, nullptr);

    if (!_sslCtx) {
        throw HX::STL::exception::SslException{
            "SSL_CTX_new (Failed)"
        };
    }

    _errBio = ::BIO_new_fd(2, BIO_NOCLOSE); // 创建错误输出的 BIO

    if (!_errBio) {
        throw HX::STL::exception::SslException{
            "BIO_new_fd (Failed)"
        };
    }

    // 加载证书
    if (::SSL_CTX_use_certificate_file(_sslCtx, verifyBuilder.certificate.c_str(), SSL_FILETYPE_PEM) <= 0) {
        throw HX::STL::exception::SslException{
            "SSL_CTX_use_certificate_file (Failed): " + verifyBuilder.certificate + " Not Find"
        };
    }
    // 加载私钥
    if (::SSL_CTX_use_PrivateKey_file(_sslCtx, verifyBuilder.privateKey.c_str(), SSL_FILETYPE_PEM) <= 0) {
        throw HX::STL::exception::SslException{
            "SSL_CTX_use_certificate_file (Failed): " + verifyBuilder.privateKey + " Not Find"
        };
    }
    // 检查私钥
    if (!::SSL_CTX_check_private_key(_sslCtx)) {
        throw HX::STL::exception::SslException{
            "SSL_CTX_check_private_key (Failed)"
        };
    }
}

void Context::initClientSSL(const HttpsVerifyBuilder& verifyBuilder) {
    SSL_load_error_strings(); // 加载错误字符串

    // 初始化 SSL 库
    if (!SSL_library_init()) {
        throw HX::STL::exception::SslException{
            "SSL_library_init (Failed)"
        };
    }
    
    _sslCtx = SSL_CTX_new(TLS_client_method()); // 创建新的 SSL 上下文

    if (!_sslCtx) {
        throw HX::STL::exception::SslException{
            "SSL_CTX_new (Failed)"
        };
    }

    // 设置验证模式
    SSL_CTX_set_verify(_sslCtx, verifyBuilder.verifyMod, nullptr);

    // 创建错误输出的 BIO
    _errBio = BIO_new_fd(2, BIO_NOCLOSE);
    if (!_errBio) {
        throw HX::STL::exception::SslException{
            "BIO_new_fd (Failed)"
        };
    }
    
    // 如果 verifyBuilder 中提供了证书和私钥, 则加载它们
    // 加载证书
    if (verifyBuilder.certificate.size() && ::SSL_CTX_use_certificate_file(_sslCtx, verifyBuilder.certificate.c_str(), SSL_FILETYPE_PEM) <= 0) {
        throw HX::STL::exception::SslException{
            "SSL_CTX_use_certificate_file (Failed): " + verifyBuilder.certificate + " Not Find"
        };
    }
    // 加载私钥
    if (verifyBuilder.privateKey.size() && ::SSL_CTX_use_PrivateKey_file(_sslCtx, verifyBuilder.privateKey.c_str(), SSL_FILETYPE_PEM) <= 0) {
        throw HX::STL::exception::SslException{
            "SSL_CTX_use_certificate_file (Failed): " + verifyBuilder.privateKey + " Not Find"
        };
    }
    // 检查私钥
    if (verifyBuilder.certificate.size() 
        && verifyBuilder.privateKey.size()
        && !::SSL_CTX_check_private_key(_sslCtx)
    ) {
        throw HX::STL::exception::SslException{
            "SSL_CTX_check_private_key (Failed)"
        };
    }
}

}}}} // namespace HX::web::protocol::http