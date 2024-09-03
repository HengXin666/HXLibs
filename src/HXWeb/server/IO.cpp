#include <HXWeb/server/IO.h>

#include <poll.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <HXSTL/coroutine/loop/AsyncLoop.h>
#include <HXWeb/protocol/http/Request.h>
#include <HXWeb/protocol/http/Response.h>
#include <HXWeb/protocol/https/Context.h>
#include <HXSTL/tools/ErrorHandlingTools.h>

namespace HX { namespace web { namespace server {

HX::STL::coroutine::task::Task<> IO<>::sendResponse() const {
    co_await __sendResponse();
    ++_response->_sendCnt;
}

HX::STL::coroutine::task::Task<> IO<>::sendResponse(
    HX::STL::container::NonVoidHelper<>
) const {
    if (_response->_sendCnt) { // 已经发送过响应了
        _response->_sendCnt = 0;
        co_return;
    }
    co_await __sendResponse();
}

HX::STL::coroutine::task::Task<> IO<>::__sendResponse() const {
    // 本次请求使用结束, 清空, 复用
    _request->clear();
    // 生成响应字符串, 用于写入
    _response->createResponseBuffer();
    co_await _sendResponse(_response->_buf);
    // 全部写入啦
    _response->clear();
}

HX::STL::coroutine::task::Task<bool> IO<HX::web::protocol::http::Http>::_recvRequest() {
    std::size_t n = co_await recvN(_recvBuf, _recvBuf.size()); // 读取到的字节数
    while (true) {
        if (n == 0) {
            // 断开连接
            co_return true;
        }

        // LOG_INFO("读取一次结束... (%llu)", n);
        if (std::size_t size = _request->parserRequest(
            std::span<char> {_recvBuf.data(), n}
        )) {
            // LOG_INFO("二次读取中..., 还需要读取 size = %llu", size);
            n = co_await recvN(_recvBuf, std::min(size, _recvBuf.size()));
            continue;
        }
        break;
    }
    co_return false;
}

HX::STL::coroutine::task::Task<> IO<HX::web::protocol::http::Http>::_sendResponse(
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

IO<HX::web::protocol::https::Https>::~IO() {
    if (_ssl) {
        SSL_shutdown(_ssl);
        SSL_free(_ssl);
    }
}

// 设置文件描述符为非阻塞模式
// value 是否 为 非阻塞模式
template <bool value = true>
static int setNonBlock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return errno;
    }
    if constexpr (value) {
        return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
    return fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
}

HX::STL::coroutine::task::Task<bool> IO<HX::web::protocol::https::Https>::handshake() {
    HX::STL::tools::LinuxErrorHandlingTools::convertError<int>(
        setNonBlock(_fd)
    ).expect("setNonBlock");

    if (POLLERR == co_await _pollAdd(POLLIN | POLLOUT | POLLERR)) {
        printf("发生错误! err: %s\n", strerror(errno));
        co_return false;
    }

    _ssl = SSL_new(HX::web::protocol::https::Context::getContext().getSslCtx());

    if (_ssl == nullptr) {
        printf("ssl == nullptr\n");
        co_return false;
    }

    HX::STL::tools::LinuxErrorHandlingTools::convertError<int>(
        ::SSL_set_fd(_ssl, _fd) // 绑定文件描述符
    ).expect("SSL_set_fd");

    ::SSL_set_accept_state(_ssl); // 设置为接受状态

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
            printf("握手失败, 是不是不是https呀?\n"); // 怎么感觉是固定读取了4个字节进行验证?!?!?
            // vector<char> buf(HX::STL::utils::FileUtils::kBufMaxSize);
            // int res = co_await HX::STL::coroutine::loop::IoUringTask().prepRecv(
            //     fd, buf, 0
            // );
            // printf("%s (%d)\n", buf.data(), res);
            co_return false;
        }
    }
}

HX::STL::coroutine::task::Task<bool> IO<HX::web::protocol::https::Https>::_recvRequest() {
    // 读取
    std::size_t n = _recvBuf.size();
    while (true) {
        int readLen = SSL_read(_ssl, _recvBuf.data(), n); // 从 SSL 连接中读取数据
        int err = SSL_get_error(_ssl, readLen);
        if (readLen > 0) {
            if (std::size_t size = _request->parserRequest(
                std::span<char> {_recvBuf.data(), (std::size_t) readLen}
            )) {
                n = std::min(n, size);
                if (POLLIN != co_await _pollAdd(POLLIN | POLLERR)) {
                    printf("SSL_read: (request) POLLIN error!\n");
                    co_return true;
                }
                continue;
            }
            break;
        } else if (readLen == 0) { // 客户端断开连接
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

HX::STL::coroutine::task::Task<> IO<HX::web::protocol::https::Https>::_sendResponse(
    std::span<char> buf
) const {
    std::size_t n = buf.size();
    while (true) {
        int writeLen = SSL_write(_ssl, buf.data(), n); // 从 SSL 连接中读取数据
        int err = SSL_get_error(_ssl, writeLen);
        if (writeLen > 0) {
            n -= writeLen;
            if (n > 0) {
                if (POLLOUT != co_await _pollAdd(POLLOUT | POLLERR)) {
                    throw "SSL_write: (n > 0) POLLOUT error!";
                }
                continue;
            }
            break;
        } else if (writeLen == 0) { // 客户端断开连接
            throw "writeLen == 0";
        } else if (err == SSL_ERROR_WANT_WRITE) {
            if (POLLOUT != co_await _pollAdd(POLLOUT | POLLERR)) {
                throw "SSL_write: POLLOUT error!";
            }
        } else {
            throw "SB SSL_write: err is ? is ?";
        }
    }
}

}}} // namespace HX::web::server