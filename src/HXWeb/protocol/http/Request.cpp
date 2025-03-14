#include <HXWeb/protocol/http/Request.h>

#include <sys/socket.h> // 套接字
#include <cstring>

#include <HXSTL/utils/StringUtils.h>
#include <HXSTL/utils/FileUtils.h>
#include <HXWeb/protocol/http/Http.hpp>

namespace HX { namespace web { namespace protocol { namespace http {

void Request::createRequestBuffer() {
    _buf.clear();
    _buf.append(_requestLine[RequestLineDataType::RequestType]);
    _buf.append(" ");
    _buf.append(_requestLine[RequestLineDataType::RequestPath]);
    _buf.append(" ");
    _buf.append(_requestLine[RequestLineDataType::ProtocolVersion]);
    _buf.append("\r\n");
    for (const auto& [key, val] : _requestHeaders) {
        _buf.append(key);
        _buf.append(": ");
        _buf.append(val);
        _buf.append("\r\n");
    }
    if (_body.size()) {
        _buf.append("Content-Length: ");
        _buf.append(std::to_string(_body.size()));
        _buf.append("\r\n\r\n");
        _buf.append(_body);
    } else {
        _buf.append("\r\n\r\n");
    }
}

std::size_t Request::parserRequest(
    std::string_view buf
) {
    using namespace std::string_literals;
    using namespace std::string_view_literals;
    if (_buf.size()) {
        _buf += buf;
        buf = _buf;
    }

    if (_requestLine.empty()) { // 请求行还未解析
        std::size_t pos = buf.find(HX::web::protocol::http::CRLF);
        if (pos == std::string_view::npos) [[unlikely]] { // 不可能事件
            return HX::STL::utils::FileUtils::kBufMaxSize;
        }

        // 解析请求行
        _requestLine = HX::STL::utils::StringUtil::split<std::string>(buf.substr(0, pos), " "sv);
        if (_requestLine.size() != 3)
            return HX::STL::utils::FileUtils::kBufMaxSize;
        buf = buf.substr(pos + 2); // 再前进, 以去掉 "\r\n"
    }

    /**
     * @brief 请求头
     * 通过`\r\n`分割后, 取最前面的, 先使用最左的`:`以判断是否是需要作为独立的键值对;
     * -  如果找不到`:`, 并且 非空, 那么它需要接在上一个解析的键值对的值尾
     * -  否则即请求头解析完毕!
     */
    while (!_completeRequestHeader) { // 请求头未解析完
        std::size_t pos = buf.find(HX::web::protocol::http::CRLF);
        if (pos == std::string_view::npos) { // 没有读取完
            _buf = buf;
            return HX::STL::utils::FileUtils::kBufMaxSize;
        }
        std::string_view subStr = buf.substr(0, pos);
        auto p = HX::STL::utils::StringUtil::splitAtFirst(subStr, ": "sv);
        if (p.first.empty()) { // 找不到 ": "
            if (subStr.size()) [[unlikely]] { // 很少会有分片传输请求头的
                _requestHeadersIt->second.append(subStr);
            } else { // 请求头解析完毕!
                _completeRequestHeader = true;
            }
        } else {
            HX::STL::utils::StringUtil::toLower(p.second);
            _requestHeadersIt = _requestHeaders.insert(p).first;
        }
        buf = buf.substr(pos + 2);
    }

    if (_requestHeaders.count("content-length"s)) { // 存在content-length模式接收的响应体
        // 是 空行之后 (\r\n\r\n) 的内容大小(char)
        if (!_remainingBodyLen.has_value()) {
            _body = buf;
            _remainingBodyLen = std::stoll(_requestHeaders["content-length"s]) 
                              - _body.size();
        } else {
            *_remainingBodyLen -= buf.size();
            _body.append(buf);
        }

        if (*_remainingBodyLen != 0) {
            _buf.clear();
            return *_remainingBodyLen;
        }
    } else if (_requestHeaders.count("transfer-encoding"s)) { // 存在请求体以`分块传输编码`
        /**
         * TODO: 目前只支持 chunked 编码, 不支持压缩的 (2024-9-6 09:36:25) 
         * */
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
            std::size_t posLen = buf.find(HX::web::protocol::http::CRLF);
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

std::unordered_map<std::string, std::string> Request::getParseQueryParameters() const {
    std::string path = getRequesPath();
    std::size_t pos = path.find('?'); // 没必要反向查找
    if (pos == std::string::npos)
        return {};
    // 如果有#这种, 要删除: 无需处理, 这个只是存在于客户端, 不会传输到服务端(?)至少path没有
    std::string parameter = path.substr(pos + 1);
    auto kvArr = HX::STL::utils::StringUtil::split<std::string>(parameter, "&");
    std::unordered_map<std::string, std::string> res;
    for (const auto& it : kvArr) {
        auto&& kvPair = HX::STL::utils::StringUtil::splitAtFirst(it, "=");
        if (kvPair.first == "")
            res.insert_or_assign(it, "");
        else
            res.insert(std::move(kvPair));
    }
    return res;
}

}}}} // namespace HX::web::protocol::http
