#include <HXWeb/client/IO.h>

#include <poll.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <HXSTL/coroutine/loop/AsyncLoop.h>
#include <HXSTL/coroutine/task/WhenAny.hpp>
#include <HXWeb/protocol/http/Request.h>
#include <HXWeb/protocol/http/Response.h>
#include <HXWeb/protocol/https/Context.h>
#include <HXSTL/tools/ErrorHandlingTools.h>
#include <HXSTL/utils/FileUtils.h>
#include <HXSTL/exception/SslException.hpp>

namespace HX { namespace web { namespace client {

IO<>::IO(int fd) 
    : HX::web::socket::IO(fd)
{
    _request = std::make_unique<HX::web::protocol::http::Request>(this);
    _response = std::make_unique<HX::web::protocol::http::Response>(nullptr);
}

HX::STL::coroutine::task::Task<bool> IO<void>::recvResponse(
    std::chrono::milliseconds timeout
) {
    auto&& res = co_await HX::STL::coroutine::task::WhenAny::whenAny(
        HX::STL::coroutine::loop::TimerLoop::sleepFor(timeout),
        _recvResponse()
    );

    if (res.index())
        co_return false;
    co_return true;
}

HX::STL::coroutine::task::Task<> IO<void>::sendRequest() const {
    // 本次响应使用结束, 清空, 复用
    _response->clear();
    // 生成请求字符串, 用于写入
    _request->createRequestBuffer();
    co_await _sendRequest(_request->_buf);
    // 全部写入啦
    _request->clear();
}

HX::STL::coroutine::task::Task<bool> IO<HX::web::protocol::http::Http>::_recvResponse() {
    std::size_t n = co_await recvN(_recvBuf, _recvBuf.size());
    while (true) {
        if (n == 0) {
            // 断开连接
            co_return true;
        }

        // LOG_INFO("读取一次结束... (%llu)", n);
        if (std::size_t size = _response->parserResponse(
            std::string_view {_recvBuf.data(), n}
        )) {
            // LOG_INFO("二次读取中..., 还需要读取 size = %llu", size);
            n = co_await recvN(_recvBuf, std::min(size, _recvBuf.size()));
            continue;
        }
        break;
    }
    co_return false;
}

HX::STL::coroutine::task::Task<> IO<HX::web::protocol::http::Http>::_sendRequest(
    std::span<char> buf
) const {
    std::size_t n = HX::STL::tools::UringErrorHandlingTools::throwingError(
        co_await HX::STL::coroutine::loop::IoUringTask().prepSend(_fd, buf, 0)
    ); // 已经写入的字节数

    while (true) {
        if (n == buf.size()) {
            co_return;
        }
        n = HX::STL::tools::UringErrorHandlingTools::throwingError(
            co_await HX::STL::coroutine::loop::IoUringTask().prepSend(
                _fd, buf = buf.subspan(n), 0
            )
        );
    }
}

IO<HX::web::protocol::https::Https>::~IO() noexcept {
    if (_ssl) {
        ::SSL_shutdown(_ssl);
        ::SSL_free(_ssl);
    }
}

HX::STL::coroutine::task::Task<bool> IO<HX::web::protocol::https::Https>::init(
    std::chrono::milliseconds timeout
) {
    auto&& res = co_await HX::STL::coroutine::task::WhenAny::whenAny(
        HX::STL::coroutine::loop::TimerLoop::sleepFor(timeout),
        handshake()
    );
    if (res.index())
        co_return std::get<1>(res);
    co_return false;
}

HX::STL::coroutine::task::Task<bool> IO<HX::web::protocol::https::Https>::handshake() {
    HX::STL::tools::LinuxErrorHandlingTools::convertError<int>(
        HX::STL::utils::FileUtils::setNonBlock(_fd)
    ).expect("setNonBlock");

    if (POLLERR == co_await _pollAdd(POLLIN | POLLOUT | POLLERR)) {
        // printf("发生错误! err: %s\n", strerror(errno));
        co_return false;
    }

    _ssl = ::SSL_new(HX::web::protocol::https::Context::getContext().getSslCtx());

    if (_ssl == nullptr) [[unlikely]] {
        // printf("ssl == nullptr\n");
        co_return false;
    }

    HX::STL::tools::LinuxErrorHandlingTools::convertError<int>(
        ::SSL_set_fd(_ssl, _fd) // 绑定文件描述符
    ).expect("SSL_set_fd");

    ::SSL_set_connect_state(_ssl); // 设置为接受状态

    while (true) {
        int res = SSL_do_handshake(_ssl); // 执行握手
        if (res == 1)
            break;
        int err = SSL_get_error(_ssl, res);
        if (err == SSL_ERROR_WANT_WRITE) {
            // 设置关注写事件
            if (POLLOUT != co_await _pollAdd(POLLOUT | POLLERR)) {
                printf("POLLOUT error!\n");
                co_return false;
            }
        } else if (err == SSL_ERROR_WANT_READ) {
            // 设置关注读事件
            if (POLLIN != co_await _pollAdd(POLLIN | POLLERR)) {
                printf("POLLIN error!\n");
                co_return false;
            }
        } else {
            ERR_print_errors(HX::web::protocol::https::Context::getContext().getErrBio());
            printf("握手失败, 是不是不是https呀?\n");
            // vector<char> buf(HX::STL::utils::FileUtils::kBufMaxSize);
            // int res = co_await HX::STL::coroutine::loop::IoUringTask().prepRecv(
            //     fd, buf, 0
            // );
            // printf("%s (%d)\n", buf.data(), res);
            co_return false;
        }
    }
    co_return true;
}

HX::STL::coroutine::task::Task<bool> IO<HX::web::protocol::https::Https>::_recvResponse() {
    std::size_t n = _recvBuf.size();
    while (true) {
        int readLen = SSL_read(_ssl, _recvBuf.data(), n); // 从 SSL 连接中读取数据
        int err = SSL_get_error(_ssl, readLen);
        if (readLen > 0) {
            if (std::size_t size = _response->parserResponse(
                std::string_view {_recvBuf.data(), (std::size_t) readLen}
            )) {
                n = std::min(_recvBuf.size(), size);
                int res = co_await _pollAdd(POLLIN | POLLERR);
                if (POLLIN != res) {
                    printf("SSL_read: (request) POLLIN error!\n");
                    co_return true;
                }
                continue;
            }
            break;
        } else if (readLen == 0) { // 服务端断开连接
            co_return true;
        } else if (err == SSL_ERROR_WANT_READ) {
            if (POLLIN != co_await _pollAdd(POLLIN | POLLERR)) {
                printf("SSL_read:  POLLIN error!\n");
                co_return true;
            }
        } else {
            printf("SB SSL_read: err is %d is %s\n", err, strerror(errno));
            co_return true;
        }
    }
    co_return false;
}

HX::STL::coroutine::task::Task<> IO<HX::web::protocol::https::Https>::_sendRequest(
    std::span<char> buf
) const {
    std::size_t n = buf.size();
    while (true) {
        int writeLen = SSL_write(_ssl, buf.data(), n); // 从 SSL 连接中读取数据
        if (writeLen > 0) {
            n -= writeLen;
            if (n > 0) {
                if (POLLOUT != co_await _pollAdd(POLLOUT | POLLERR)) {
                    throw HX::STL::exception::SslException{"SSL_write: (n > 0) POLLOUT error!"};
                }
                continue;
            }
            break;
        } else if (writeLen == 0) { // 服务端断开连接
            throw HX::STL::exception::SslException{"SSL_write: writeLen == 0 (Client Disconnected)"};
        } else  {
            if (int err = SSL_get_error(_ssl, writeLen); err == SSL_ERROR_WANT_WRITE) {
                if (POLLOUT != co_await _pollAdd(POLLOUT | POLLERR)) {
                    throw HX::STL::exception::SslException{"SSL_write: POLLOUT error!"};
                }
            } else {
                throw HX::STL::exception::SslException{
                    "SSL_write: Err: " + std::to_string(err) + " is " + strerror(errno)
                };
            }
        }
    }
}

}}} // namespace HX::web::client