#include <HXWeb/server/IO.h>

#include <poll.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <HXSTL/coroutine/loop/AsyncLoop.h>
#include <HXWeb/protocol/http/Request.h>
#include <HXWeb/protocol/http/Response.h>
#include <HXWeb/protocol/https/Context.h>
#include <HXSTL/utils/FileUtils.h>
#include <HXSTL/utils/StringUtils.h>
#include <HXSTL/tools/ErrorHandlingTools.h>

namespace HX { namespace web { namespace server {

IO<>::IO(int fd)
    : HX::web::socket::IO(fd)
    , _isReuseConnection(true)
{
    _request = std::make_unique<HX::web::protocol::http::Request>(nullptr);
    _response = std::make_unique<HX::web::protocol::http::Response>(this);
}

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

HX::STL::coroutine::task::Task<> IO<>::sendResponseWithChunkedEncoding(
    const std::string& path
) const {
    // 本次请求使用结束, 清空, 复用
    _request->clear();
    auto fileType = HX::web::protocol::http::getMimeType(
        HX::STL::utils::FileUtils::getExtension(path)
    );
    _response->setResponseLine(HX::web::protocol::http::Status::CODE_200);
    _response->addHeader("Content-Type", std::string{fileType});
    _response->addHeader("Transfer-Encoding", "chunked");
    // 生成响应行和响应头
    _response->_buildResponseLineAndHeaders();
    // 先发送一版, 告知我们是分块编码
    co_await _sendResponse(_response->_buf);
    
    HX::STL::utils::FileUtils::AsyncFile file;
    co_await file.open(path);
    std::vector<char> buf(HX::STL::utils::FileUtils::kBufMaxSize);
    while (true) {
        // 读取文件
        std::size_t size = static_cast<std::size_t>(co_await file.read(buf));
        _response->_buildToChunkedEncoding({buf.data(), size});
        co_await _sendResponse(_response->_buf);
        if (size != buf.size()) {
            // 需要使用 长度为 0 的分块, 来标记当前内容实体传输结束
            _response->_buildToChunkedEncoding("");
            co_await _sendResponse(_response->_buf);
            break;
        }
    }
    // 全部写入啦
    _response->clear();
    ++_response->_sendCnt;
}

HX::STL::coroutine::task::Task<> IO<>::sendResponseWithRange(
    const std::string& path
) const {
    using namespace std::string_view_literals;
    // 解析请求的范围
    auto type = _request->getRequesType();
    auto fileType = HX::web::protocol::http::getMimeType(
        HX::STL::utils::FileUtils::getExtension(path)
    );
    auto fileSize = HX::STL::utils::FileUtils::getFileSize(path);
    auto fileSizeStr = std::to_string(fileSize);
    auto headMap = _request->getRequestHeaders();
    if (type == "HEAD"sv) {
        // 返回文件大小
        /*
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 10737418240\r\n"
            "Accept-Ranges: bytes\r\n"
            "\r\n"
        */
        _response->setResponseLine(protocol::http::Status::CODE_200);
        _response->addHeader("Content-Length", fileSizeStr);
        _response->addHeader("Content-Type", std::string{fileType});
        _response->addHeader("Accept-Ranges", "bytes");
        _response->_buildResponseLineAndHeaders();
        co_await _sendResponse(_response->_buf);
    } else if (auto it = headMap.find("Range"); it != headMap.end()) {
        // 开始[断点续传]传输, 先发一下头
        /*
            "HTTP/1.1 206 Partial Content\r\n"
            "Content-Range: bytes X-Y/Z\r\n"
            "Content-Length: Y-X+1\r\n"
            "Accept-Ranges: bytes\r\n"
            "\r\n"

            其中Content-Range为: 本次传输的文件的起始位置-结束位置/总大小
            而Content-Length是 [X, Y] 包含X, 包含Y, 所以是 Y - X + 1个字节
        */
        // 解析范围, 如果访问不合法, 应该返回416
        std::string_view rangeVals = it->second;
        // Range: bytes=<range-start>-<range-end>
        auto rangeNumArr = HX::STL::utils::StringUtil::split<std::string>(rangeVals.substr(6), ",");
        _response->setResponseLine(protocol::http::Status::CODE_206);
        _response->addHeader("Accept-Ranges", "bytes");

        if (rangeNumArr.size() == 1) [[likely]] { // 一般都是请求单个范围
            auto [begin, end] = HX::STL::utils::StringUtil::splitAtFirst(rangeNumArr.back(), "-");
            if (begin.empty()) {
                begin += '0';
            }
            if (end.empty()) {
                end = std::to_string(fileSize - 1);
            }
            u_int64_t beginPos = std::stoull(begin);
            u_int64_t endPos = std::stoull(end);
            u_int64_t remaining = endPos - beginPos + 1;
            _response->addHeader("Content-Type", std::string{fileType});
            _response->addHeader("Content-Length", std::to_string(remaining));
            _response->addHeader("Content-Range", "bytes " + begin + "-" + end + "/" + fileSizeStr);
            _response->_buildResponseLineAndHeaders();
            co_await _sendResponse(_response->_buf); // 先发一个头

            HX::STL::utils::FileUtils::AsyncFile file;
            co_await file.open(path);
            file.setOffset(beginPos);
            std::vector<char> buf(HX::STL::utils::FileUtils::kBufMaxSize);
            // 支持偏移量
            while (remaining > 0) {
                // 读取文件
                std::size_t size = static_cast<std::size_t>(
                    co_await file.read(
                        buf,
                        std::min(remaining, buf.size())
                    )
                );
                if (!size)
                    break;
                co_await _sendResponse(buf);
                remaining -= size;
            }
        } else {
            _response->addHeader("Content-Type", "multipart/byteranges; boundary=BOUNDARY_STRING");
            // todo
        }
    } else {
        // 普通的传输文件
        _response->setResponseLine(protocol::http::Status::CODE_200);
        _response->addHeader("Content-Type", std::string{fileType});
        _response->addHeader("Content-Length", fileSizeStr);
        _response->_buildResponseLineAndHeaders();
        co_await _sendResponse(_response->_buf); // 先发一个头

        HX::STL::utils::FileUtils::AsyncFile file;
        co_await file.open(path);
        std::vector<char> buf(HX::STL::utils::FileUtils::kBufMaxSize);
        u_int64_t remaining = fileSize;
        while (remaining > 0) {
            std::size_t size = static_cast<std::size_t>(
                co_await file.read(buf)
            );
            if (!size) {
                printf("end!\n");
                break;
            }
            co_await _sendResponse(buf);
            remaining -= size;
        }
    }

    // 本次请求使用结束, 清空, 复用
    _request->clear();
    // 全部写入啦
    _response->clear();
    ++_response->_sendCnt;
}

void IO<>::updateReuseConnection() noexcept {
    auto& headMap = _request->getRequestHeaders();
    if (auto it = headMap.find("Connection"); it != headMap.end()) {
        _isReuseConnection = it->second == "keep-alive";
    }
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

IO<HX::web::protocol::https::Https>::~IO() noexcept {
    if (_ssl) {
        ::SSL_shutdown(_ssl);
        ::SSL_free(_ssl);
    }
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
    co_return true;
}

HX::STL::coroutine::task::Task<bool> IO<HX::web::protocol::https::Https>::_recvRequest() {
    // 读取
    std::size_t n = _recvBuf.size();
    while (true) {
        int readLen = SSL_read(_ssl, _recvBuf.data(), n); // 从 SSL 连接中读取数据
        int err = SSL_get_error(_ssl, readLen);
        if (readLen > 0) {
            if (std::size_t size = _request->parserRequest(
                std::string_view {_recvBuf.data(), (std::size_t) readLen}
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
            throw "SSL_write: Err: " + std::to_string(err) + " is " + strerror(errno);
        }
    }
    co_return;
}

}}} // namespace HX::web::server