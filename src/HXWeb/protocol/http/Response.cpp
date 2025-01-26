#include <HXWeb/protocol/http/Response.h>

#include <sys/socket.h>
#include <cstring>

#ifndef HEXADECIMAL_CONVERSION
#   include <format>
#else
#   include <HXSTL/utils/NumericBaseConverter.h>
#endif // !HEXADECIMAL_CONVERSION

#include <HXSTL/utils/StringUtils.h>
#include <HXSTL/utils/FileUtils.h>

namespace HX { namespace web { namespace protocol { namespace http {

Response& Response::setResponseLine(Status statusCode, std::string_view describe /*= ""*/) {
    using namespace std::string_view_literals;
    _statusLine.clear();
    _statusLine.resize(3);
    _statusLine[ResponseLineDataType::ProtocolVersion] = "HTTP/1.1"sv;
    _statusLine[ResponseLineDataType::StatusCode] = std::to_string(static_cast<int>(statusCode));
    if (!describe.size()) {
        _statusLine[ResponseLineDataType::StatusMessage] = getStatusCodeDataStrView(statusCode);
    } else {
        _statusLine[ResponseLineDataType::StatusMessage] = describe;
    }
    return *this;
}

STL::coroutine::task::Task<> Response::useChunkedEncodingTransferFile(std::string const& filePath) {
    auto fileType = HX::web::protocol::http::getMimeType(
        HX::STL::utils::FileUtils::getExtension(filePath)
    );
    setResponseLine(Status::CODE_200);
    addHeader("Content-Type", std::string{fileType});
    addHeader("Transfer-Encoding", "chunked");
    co_return co_await _io->sendResponseWithChunkedEncoding(filePath);
}

std::size_t Response::parserResponse(std::string_view buf) {
    if (_buf.size()) {
        _buf += buf;
        buf = _buf;
    }

    if (_statusLine.empty()) { // 响应行还未解析
        std::size_t pos = buf.find("\r\n");
        if (pos == std::string_view::npos) [[unlikely]] { // 不可能事件
            return HX::STL::utils::FileUtils::kBufMaxSize;
        }

        // 解析响应行, 注意 不能按照空格直接切分! 因为 HTTP/1.1 404 NOF FOND\r\n
        _statusLine = HX::STL::utils::StringUtil::split<std::string>(buf.substr(0, pos), " ");
        if (_statusLine.size() < 3)
            return HX::STL::utils::FileUtils::kBufMaxSize;
        if (_statusLine.size() > 3) {
            for (std::size_t i = 4; i < _statusLine.size(); ++i) {
                _statusLine[ResponseLineDataType::StatusMessage] += std::move(_statusLine[i]);
            }
            _statusLine.resize(3);
        }
        buf = buf.substr(pos + 2); // 再前进, 以去掉 "\r\n"
    }

    /**
     * @brief 请求头
     * 通过`\r\n`分割后, 取最前面的, 先使用最左的`:`以判断是否是需要作为独立的键值对;
     * -  如果找不到`:`, 并且 非空, 那么它需要接在上一个解析的键值对的值尾
     * -  否则即请求头解析完毕!
     */
    while (!_completeResponseHeader) { // 响应头未解析完
        std::size_t pos = buf.find("\r\n");
        if (pos == std::string_view::npos) { // 没有读取完
            _buf = buf;
            return HX::STL::utils::FileUtils::kBufMaxSize;
        }
        std::string_view subStr = buf.substr(0, pos);
        auto p = HX::STL::utils::StringUtil::splitAtFirst(subStr, ": ");
        if (p.first.empty()) { // 找不到 ": "
            if (subStr.size()) [[unlikely]] { // 很少会有分片传输响应头的
                _responseHeadersIt->second.append(subStr);
            } else { // 请求头解析完毕!
                _completeResponseHeader = true;
            }
        } else {
            HX::STL::utils::StringUtil::toLower(p.first);
            _responseHeadersIt = _responseHeaders.insert(p).first;
        }
        buf = buf.substr(pos + 2);
    }

    if (_responseHeaders.count("content-length")) { // 存在content-length模式接收的响应体
        // 是 空行之后 (\r\n\r\n) 的内容大小(char)
        if (!_remainingBodyLen.has_value()) {
            _body = buf;
            _remainingBodyLen = std::stoll(_responseHeaders["content-length"]) 
                              - _body.size();
        } else {
            *_remainingBodyLen -= buf.size();
            _body.append(buf);
        }

        if (*_remainingBodyLen != 0) {
            _buf.clear();
            return *_remainingBodyLen;
        }
    } else if (_responseHeaders.count("transfer-encoding")) { // 存在响应体以`分块传输编码`
        if (_remainingBodyLen) { // 处理没有读取完的
            if (buf.size() <= *_remainingBodyLen) { // 还没有读取完毕
                _body += buf;
                *_remainingBodyLen -= buf.size();
                return HX::STL::utils::FileUtils::kBufMaxSize;
            } else { // 读取完了
                _body.append(buf, 0, *_remainingBodyLen);
                buf = buf.substr(std::min(*_remainingBodyLen + 2, buf.size()));
                _remainingBodyLen.reset();
            }
        }
        while (true) {
            std::size_t posLen = buf.find("\r\n");
            if (posLen == std::string_view::npos) { // 没有读完
                _buf = buf;
                return HX::STL::utils::FileUtils::kBufMaxSize;
            }
            if (!posLen && buf[0] == '\r') [[unlikely]] { // posLen == 0
                // \r\n 贴脸, 触发原因, std::min(*_remainingBodyLen + 2, buf.size()) 只能 buf.size()
                buf = buf.substr(posLen + 2);
                continue;
            }
            _remainingBodyLen = std::stol(std::string {buf.substr(0, posLen)}, nullptr, 16); // 转换为十进制整数
            if (!*_remainingBodyLen) { // 解析完毕
                return 0;
            }
            buf = buf.substr(posLen + 2);
            if (buf.size() <= *_remainingBodyLen) { // 没有读完
                _body += buf;
                *_remainingBodyLen -= buf.size();
                return HX::STL::utils::FileUtils::kBufMaxSize;
            }
            _body.append(buf.substr(0, *_remainingBodyLen));
            buf = buf.substr(*_remainingBodyLen + 2);
        }
    }

    return 0; // 解析完毕
}

void Response::_buildResponseLineAndHeaders() {
    _buf.append(_statusLine[ResponseLineDataType::ProtocolVersion]);
    _buf.append(" ");
    _buf.append(_statusLine[ResponseLineDataType::StatusCode]);
    _buf.append(" ");
    _buf.append(_statusLine[ResponseLineDataType::StatusMessage]);
    _buf.append("\r\n");
    for (const auto& [key, val] : _responseHeaders) {
        _buf.append(key);
        _buf.append(": ");
        _buf.append(val);
        _buf.append("\r\n");
    }
    _buf.append("\r\n");
}

void Response::_buildToChunkedEncoding(std::string_view buf) {
    _buf.clear();
#ifdef HEXADECIMAL_CONVERSION
    _buf.append(HX::STL::utils::NumericBaseConverter::hexadecimalConversion(buf.size()));
#else
    _buf.append(std::format("{:X}", buf.size())); // 需要十六进制嘞
#endif // HEXADECIMAL_CONVERSION
    _buf.append("\r\n");
    _buf.append(buf);
    _buf.append("\r\n");
}

void Response::createResponseBuffer() {
    _buf.clear();
    _buildResponseLineAndHeaders();
    _buf.pop_back(); // 去掉\n
    _buf.pop_back(); // 去掉\r
    // 补充这个
    _buf.append("Content-Length: ");
    _buf.append(std::to_string(_body.size()));
    _buf.append("\r\n\r\n");
    _buf.append(_body);
}

}}}} // namespace HX::web::protocol::http
