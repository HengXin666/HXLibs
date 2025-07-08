#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2024-7-20 23:18:48
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
 * */
#ifndef _HX_RESPONSE_H_
#define _HX_RESPONSE_H_

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <optional>

#include <HXLibs/net/protocol/http/Http.hpp>
#include <HXLibs/net/protocol/http/Status.hpp>
#include <HXLibs/net/protocol/http/MimeType.hpp>
#include <HXLibs/net/socket/IO.hpp>
#include <HXLibs/coroutine/task/Task.hpp>
#include <HXLibs/utils/StringUtils.hpp>
#include <HXLibs/utils/FileUtils.hpp>

namespace HX::net {

/**
 * @brief 响应类(Response)
 */
class Response {
public:
    explicit Response(IO& io) 
        : _statusLine()
        , _responseHeaders()
        , _body()
        , _responseHeadersIt(_responseHeaders.end())
        , _buf()

        , _io{io}
    {}

#if 0
    Response(const Response& response) = delete;
    Response& operator=(const Response& response) = delete;

    Response(Response&& response) = default;
    Response& operator=(Response&& response) = default;
#else
    Response& operator=(Response&& response) = delete;
#endif
    // ===== ↓客户端使用↓ =====
    /**
     * @brief 解析响应
     * @param buf 需要解析的内容
     * @return 是否需要继续解析;
     *         `== 0`: 不需要;
     *         `>  0`: 需要继续解析`size_t`个字节
     * @warning 假定内容是符合Http协议的
     */
    std::size_t parserResponse(std::string_view buf) { 
        // @todo 需要修改!!!
        using namespace std::string_literals;
        if (_buf.size()) {
            _buf += buf;
            buf = _buf;
        }

        if (_statusLine.empty()) { // 响应行还未解析
            std::size_t pos = buf.find(CRLF);
            if (pos == std::string_view::npos) [[unlikely]] { // 不可能事件
                return HX::utils::FileUtils::kBufMaxSize;
            }

            // 解析响应行, 注意 不能按照空格直接切分! 因为 HTTP/1.1 404 NOF FOND\r\n
            _statusLine = HX::utils::StringUtil::split<std::string>(buf.substr(0, pos), " "s);
            if (_statusLine.size() < 3)
                return HX::utils::FileUtils::kBufMaxSize;
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
            std::size_t pos = buf.find(CRLF);
            if (pos == std::string_view::npos) { // 没有读取完
                _buf = buf;
                return HX::utils::FileUtils::kBufMaxSize;
            }
            std::string_view subStr = buf.substr(0, pos);
            auto p = HX::utils::StringUtil::splitAtFirst(subStr, ": "s);
            if (p.first.empty()) { // 找不到 ": "
                if (subStr.size()) [[unlikely]] { // 很少会有分片传输响应头的
                    _responseHeadersIt->second.append(subStr);
                } else { // 请求头解析完毕!
                    _completeResponseHeader = true;
                }
            } else {
                HX::utils::StringUtil::toLower(p.first);
                _responseHeadersIt = _responseHeaders.insert(p).first;
            }
            buf = buf.substr(pos + 2);
        }

        if (_responseHeaders.count("Content-Length"s)) { // 存在content-length模式接收的响应体
            // 是 空行之后 (\r\n\r\n) 的内容大小(char)
            if (!_remainingBodyLen.has_value()) {
                _body = buf;
                _remainingBodyLen = std::stoull(_responseHeaders["Content-Length"s]) 
                                - _body.size();
            } else {
                *_remainingBodyLen -= buf.size();
                _body.append(buf);
            }

            if (*_remainingBodyLen != 0) {
                _buf.clear();
                return *_remainingBodyLen;
            }
        } else if (_responseHeaders.count("Transfer-Encoding"s)) { // 存在响应体以`分块传输编码`
            if (_remainingBodyLen) { // 处理没有读取完的
                if (buf.size() <= *_remainingBodyLen) { // 还没有读取完毕
                    _body += buf;
                    *_remainingBodyLen -= buf.size();
                    return HX::utils::FileUtils::kBufMaxSize;
                } else { // 读取完了
                    _body.append(buf, 0, *_remainingBodyLen);
                    buf = buf.substr(std::min(*_remainingBodyLen + 2, buf.size()));
                    _remainingBodyLen.reset();
                }
            }
            while (true) {
                std::size_t posLen = buf.find(CRLF);
                if (posLen == std::string_view::npos) { // 没有读完
                    _buf = buf;
                    return HX::utils::FileUtils::kBufMaxSize;
                }
                if (!posLen && buf[0] == '\r') [[unlikely]] { // posLen == 0
                    // \r\n 贴脸, 触发原因, std::min(*_remainingBodyLen + 2, buf.size()) 只能 buf.size()
                    buf = buf.substr(posLen + 2);
                    continue;
                }
                _remainingBodyLen = std::stol(std::string{buf.substr(0, posLen)}, nullptr, 16); // 转换为十进制整数
                if (!*_remainingBodyLen) { // 解析完毕
                    return 0;
                }
                buf = buf.substr(posLen + 2);
                if (buf.size() <= *_remainingBodyLen) { // 没有读完
                    _body += buf;
                    *_remainingBodyLen -= buf.size();
                    return HX::utils::FileUtils::kBufMaxSize;
                }
                _body.append(buf.substr(0, *_remainingBodyLen));
                buf = buf.substr(*_remainingBodyLen + 2);
            }
        }

        return 0; // 解析完毕
    }

    /**
     * @brief 获取协议版本
     * @return std::string 
     */
    std::string getProtocolVersion() const {
        return _statusLine[ResponseLineDataType::ProtocolVersion];
    }

    /**
     * @brief 获取状态码
     * @return std::string 
     */
    std::string getStatusCode() const {
        return _statusLine[ResponseLineDataType::StatusCode];
    }

    /**
     * @brief 获取状态信息
     * @return std::string 
     */
    std::string getStatusMessage() const {
        return _statusLine[ResponseLineDataType::StatusMessage];
    }

    /**
     * @brief 获取响应头键值对
     * @return 响应头键值对 (键均为小写)
     * @todo 无用的std::move!
     */
    std::unordered_map<std::string, std::string> getResponseHeaders() const {
        return std::move(_responseHeaders);
    }

    /**
     * @brief 获取响应体
     * @return std::string 
     */
    std::string getResponseBody() const {
        return _body;
    }

    // ===== ↑客户端使用↑ =====

    // ===== ↓服务端使用の更加人性化API↓ =====

    /**
     * @brief 设置响应码和正文(html)
     * @param status 
     * @param content
     * @return Response& 可链式调用
     */
    Response& setStatusAndContent(Status status, std::string const& content) {
        setResponseLine(status).setContentType(TEXT).setBodyData(content);
        return *this;
    }

    /**
     * @brief 发送已经设置的响应
     * @return coroutine::Task<> 
     */
    coroutine::Task<> sendResponse() {
        createResponseBuffer();
        co_await _io.send(_buf);
    }

    /**
     * @brief 使用分块编码传输文件
     * @param filePath 文件路径
     */
    coroutine::Task<> useChunkedEncodingTransferFile(std::string_view filePath) {
        using namespace std::string_literals;
        auto fileType = getMimeType(
            utils::FileUtils::getExtension(filePath)
        );
        setResponseLine(Status::CODE_200);
        addHeader("Content-Type"s, std::string{fileType});
        addHeader("Transfer-Encoding"s, "chunked"s);
        // 生成响应行和响应头
        _buildResponseLineAndHeaders();
        // 先发送一版, 告知我们是分块编码
        co_await _io.send(_buf);
        
        utils::FileUtils::AsyncFile file;
        co_await file.open(filePath);
        std::vector<char> buf(utils::FileUtils::kBufMaxSize);
        while (true) {
            // 读取文件
            std::size_t size = static_cast<std::size_t>(co_await file.read(buf));
            _buildToChunkedEncoding({buf.data(), size});
            co_await _io.send(_buf);
            if (size != buf.size()) {
                // 需要使用 长度为 0 的分块, 来标记当前内容实体传输结束
                _buildToChunkedEncoding("");
                co_await _io.send(_buf);
                break;
            }
        }
    }

    /**
     * @brief 使用断点续传传输文件
     * @param rrv 断点续传参数包, 通过 `req.getRangeRequestView()` 获取
     * @param filePath 文件路径
     */
    coroutine::Task<> useRangeTransferFile(RangeRequestView rrv, std::string_view filePath) {
        using namespace std::string_literals;
        using namespace std::string_view_literals;
        // 解析请求的范围
        auto& type = rrv.reqType;
        auto fileType = getMimeType(
            utils::FileUtils::getExtension(filePath)
        );
        auto fileSize = utils::FileUtils::getFileSize(filePath);
        auto fileSizeStr = std::to_string(fileSize);
        auto const& headMap = rrv.reqHead;
        if (type == "HEAD"sv) {
            // 返回文件大小
            /*
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: 10737418240\r\n"
                "Accept-Ranges: bytes\r\n"
                "\r\n"
            */
            setResponseLine(Status::CODE_200);
            addHeader("Content-Length"s, fileSizeStr);
            addHeader("Content-Type"s, std::string{fileType});
            addHeader("Accept-Ranges"s, "bytes"s);
            _buildResponseLineAndHeaders();
            co_await _io.send(_buf);
        } else if (auto it = headMap.find("range"s); it != headMap.end()) {
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
            auto rangeNumArr = utils::StringUtil::split<std::string>(rangeVals.substr(6), ","sv);
            setResponseLine(Status::CODE_206);
            addHeader("Accept-Ranges"s, "bytes"s);

            if (rangeNumArr.size() == 1) [[likely]] { // 一般都是请求单个范围
                auto [begin, end] = utils::StringUtil::splitAtFirst(rangeNumArr.back(), "-"sv);
                if (begin.empty()) {
                    begin += '0';
                }
                if (end.empty()) {
                    end = std::to_string(fileSize - 1);
                }
                u_int64_t beginPos = std::stoull(begin);
                u_int64_t endPos = std::stoull(end);
                if (beginPos > endPos || endPos > fileSize) [[unlikely]] {
                    // 范围不合法: 返回416, 表示请求错误
                    setResponseLine(Status::CODE_416);
                    _buildResponseLineAndHeaders();
                    co_await _io.send(_buf);
                } else {
                    uint64_t remaining = endPos - beginPos + 1;
                    addHeader("Content-Range", "bytes " + begin + "-" + end + "/" + fileSizeStr);
                    addHeader("Content-Type", std::string{fileType});
                    addHeader("Content-Length", std::to_string(remaining));
                    _buildResponseLineAndHeaders();
                    co_await _io.send(_buf); // 先发一个头
                    
                    utils::FileUtils::AsyncFile file;
                    co_await file.open(filePath);
                    file.setOffset(beginPos);
                    std::vector<char> buf(utils::FileUtils::kBufMaxSize);
                    // 支持偏移量
                    while (remaining > 0) {
                        // 读取文件
                        std::size_t size = static_cast<std::size_t>(
                            co_await file.read(
                                buf,
                                static_cast<uint32_t>(std::min(remaining, buf.size()))
                            )
                        );
                        if (!size) [[unlikely]] {
                            break;
                        }
                        co_await _io.send(buf);
                        remaining -= size;
                    }
                }
            } else {
                /*
                    HTTP/1.1 206 Partial Content\r\n
                    Content-Type: multipart/byteranges; boundary=BOUNDARY_STRING\r\n
                    \r\n
                    --BOUNDARY_STRING\r\n
                    Content-Range: bytes X1-Y1/Z\r\n
                    Content-Length: L1\r\n
                    Content-Type: application/octet-stream\r\n
                    \r\n
                    [BINARY DATA PART 1]\r\n
                    --BOUNDARY_STRING\r\n
                    Content-Range: bytes X2-Y2/Z\r\n
                    Content-Length: L2\r\n
                    Content-Type: application/octet-stream\r\n
                    \r\n
                    [BINARY DATA PART 2]\r\n
                    --BOUNDARY_STRING--\r\n
                */
                addHeader("Content-Type", "multipart/byteranges; boundary=BOUNDARY_STRING");
                _buildResponseLineAndHeaders();
                co_await _io.send(_buf); // 先发一个头
                for (auto& ragen : rangeNumArr) {
                    auto [begin, end] = utils::StringUtil::splitAtFirst(ragen, "-");
                    if (begin.empty()) {
                        begin += '0';
                    }
                    if (end.empty()) {
                        end = std::to_string(fileSize - 1);
                    }
                    u_int64_t beginPos = std::stoull(begin);
                    u_int64_t endPos = std::stoull(end);
                    // 范围不合法: 不会报错, 而是忽略!
                    if (beginPos > endPos || endPos > fileSize) [[unlikely]] {
                        continue;
                    }
                    _buf.clear();

                    u_int64_t remaining = endPos - beginPos + 1;
                    _buf.append("--BOUNDARY_STRING\r\n"sv);
                    _buf.append("Content-Range: bytes "sv);
                    _buf.append(begin);
                    _buf.append("-"sv);
                    _buf.append(end);
                    _buf.append("/"sv);
                    _buf.append(fileSizeStr);
                    _buf.append(CRLF);
                    _buf.append("Content-Length: "sv);
                    _buf.append(std::to_string(remaining));
                    _buf.append(CRLF);
                    _buf.append("Content-Type: application/octet-stream\r\n"sv);
                    _buf.append(CRLF);
                    co_await _io.send(_buf); // 先发头

                    utils::FileUtils::AsyncFile file;
                    co_await file.open(filePath);
                    file.setOffset(beginPos);
                    std::vector<char> buf(utils::FileUtils::kBufMaxSize);
                    // 支持偏移量
                    while (remaining > 0) {
                        // 读取文件
                        std::size_t size = static_cast<std::size_t>(
                            co_await file.read(
                                buf,
                                static_cast<uint32_t>(std::min(remaining, buf.size()))
                            )
                        );
                        if (!size) [[unlikely]] {
                            break;
                        }
                        co_await _io.send(buf);
                        co_await _io.send(CRLF);
                        remaining -= size;
                    }
                }
                co_await _io.send("--BOUNDARY_STRING--\r\n"sv);
            }
        } else {
            // 普通的传输文件
            setResponseLine(Status::CODE_200);
            addHeader("Content-Type"s, std::string{fileType});
            addHeader("Content-Length"s, fileSizeStr);
            _buildResponseLineAndHeaders();
            co_await _io.send(_buf); // 先发一个头

            utils::FileUtils::AsyncFile file;
            co_await file.open(filePath);
            std::vector<char> buf(utils::FileUtils::kBufMaxSize);
            u_int64_t remaining = fileSize;
            while (remaining > 0) {
                std::size_t size = static_cast<std::size_t>(
                    co_await file.read(buf)
                );
                if (!size) [[unlikely]] {
                    break;
                }
                co_await _io.send(buf);
                remaining -= size;
            }
        }
    }

    // ===== ↑服务端使用の更加人性化API↑ =====

    // ===== ↓服务端使用↓ =====
    /**
     * @brief 设置状态行 (协议使用HTTP/1.1)
     * @param statusCode 状态码
     * @param describe 状态码描述: 如果为`""`则会使用该状态码对应默认的描述
     * @warning 不需要手动写`/r`或`/n`以及尾部的`/r/n`
     */
    Response& setResponseLine(Status statusCode, std::string_view describe = "") {
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

    /**
     * @brief 设置响应头部: 设置响应类型, 如果响应体是文本, 你需要指定字符编码(不指定则留空`""`)
     * @param type 响应类型, 如`text/html`
     * @param encoded 字符编码, 如`UTF-8` ~~(如果是图片等就可以不用指定)~~
     * @return [this&] 可以链式调用
     * @warning 不需要手动写`/r`或`/n`以及尾部的`/r/n`
     */
    Response& setContentType(ResContentType type) {
        using namespace std::string_literals;
        _responseHeaders["Content-Type"s] = getContentTypeStrView(type);
        return *this;
    }

    /**
     * @brief 设置响应体
     * @param data 响应体数据
     * @return [this&] 可以链式调用
     * @warning 不需要手动写`/r`或`/n`以及尾部的`/r/n`
     */
    Response& setBodyData(const std::string& data) {
        _body = data;
        return *this;
    }

    /**
     * @brief 向响应头部添加一个键值对
     * @param key 键
     * @param val 值
     * @return Response&
     * @warning `key`在`map`中是区分大小写的, 故不要使用`大小写不同`的相同的`键`
     */
    template <typename Str>
    Response& addHeader(const std::string& key, Str&& val) {
        _responseHeaders[key] = std::forward<Str>(val);
        return *this;
    }
    // ===== ↑服务端使用↑ =====

    /**
     * @brief 清空的响应, 重置状态
     */
    void clear() noexcept {
        _statusLine.clear();
        _responseHeaders.clear();
        _body.clear();
        _responseHeadersIt = _responseHeaders.end();
        _buf.clear();
        _completeResponseHeader = false;
    }
private:
    /**
     * @brief 响应行数据分类
     */
    enum ResponseLineDataType {
        ProtocolVersion = 0,   // 协议版本
        StatusCode = 1,        // 状态码
        StatusMessage = 2,     // 状态信息
    };

    // 注意: 他们的末尾并没有事先包含 \r\n, 具体在to_string才提供
    std::vector<std::string> _statusLine;                           // 状态行
    std::unordered_map<std::string, std::string> _responseHeaders;  // 响应头
    std::string _body;                                              // 响应体

    // 指向上一次解析的响应头的键值对; 无效时候指向 `.end()`
    std::unordered_map<std::string, std::string>::iterator _responseHeadersIt; 

    std::string _buf;                               // 上一次未解析全的
    std::optional<std::size_t> _remainingBodyLen;   // 仍需读取的请求体长度
    IO& _io;
    bool _completeResponseHeader = false;           //是否解析完成响应头

    /**
     * @brief [仅服务端] 生成响应行和响应头
     */
    void _buildResponseLineAndHeaders() {
        using namespace std::string_literals;
        using namespace std::string_view_literals;
        _responseHeaders["Connection"s] = "keep-alive"sv; // 长连接
        _responseHeaders["Server"s] = "HX-Net"sv;
        
        _buf.append(_statusLine[ResponseLineDataType::ProtocolVersion]);
        _buf.append(" "sv);
        _buf.append(_statusLine[ResponseLineDataType::StatusCode]);
        _buf.append(" "sv);
        _buf.append(_statusLine[ResponseLineDataType::StatusMessage]);
        _buf.append(CRLF);
        for (const auto& [key, val] : _responseHeaders) {
            _buf.append(key);
            _buf.append(HEADER_SEPARATOR_SV);
            _buf.append(val);
            _buf.append(CRLF);
        }
        _buf.append(CRLF);
    }

    /**
     * @brief [仅服务端] 将`buf`转化为`ChunkedEncoding`的Body, 放入`_body`以分片发送
     * @param buf 
     * @warning 内部会清空 `_buf`, 再以`ChunkedEncoding`格式写入 buf 到 `_buf`!
     */
    void _buildToChunkedEncoding(std::string_view buf) {
        _buf.clear();
#ifdef HEXADECIMAL_CONVERSION
        _buf.append(utils::NumericBaseConverter::hexadecimalConversion(buf.size()));
#else
        _buf.append(std::format("{:X}", buf.size())); // 需要十六进制嘞
#endif // !HEXADECIMAL_CONVERSION
        _buf.append(CRLF);
        _buf.append(buf);
        _buf.append(CRLF);
    }

    /**
     * @brief [仅服务端] 生成完整的响应字符串, 用于写入
     * @warning 本方法子适用于`Content-Length`的短消息, 无法使用分块编码
     */
    void createResponseBuffer() {
        using namespace std::string_view_literals;
        _buf.clear();
        _buildResponseLineAndHeaders();
        _buf.pop_back(); // 去掉\n
        _buf.pop_back(); // 去掉\r
        // 补充这个
        _buf.append("Content-Length: "sv);
        _buf.append(std::to_string(_body.size()));
        _buf.append(CRLF);

        _buf.append(CRLF);
        _buf.append(std::move(_body));
    }
};

} // namespace HX::net

#endif // _HX_RESPONSE_H_