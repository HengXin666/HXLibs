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

#include <HXWeb/protocol/http/Status.hpp>
#include <HXWeb/protocol/http/MimeType.hpp>
#include <HXSTL/coroutine/task/Task.hpp>
#include <HXWeb/server/IO.h>

namespace HX { namespace web { namespace protocol { namespace http {

/**
 * @brief 响应类(Response)
 */
class Response {
public:
    explicit Response(HX::web::server::IO<void>* io) 
        : _statusLine()
        , _responseHeaders()
        , _body()
        , _responseHeadersIt(_responseHeaders.end())
        , _buf()
        , _sendCnt(0)
        , _io(io)
    {}

    Response(const Response& response) = delete;
    Response& operator=(const Response& response) = delete;

    Response(Response&& response) = default;
    Response& operator=(Response&& response) = default;

    const HX::web::server::IO<void>& getIo() const {
        return *_io;
    }

    // ===== ↓客户端使用↓ =====
    /**
     * @brief 解析响应
     * @param buf 需要解析的内容
     * @return 是否需要继续解析;
     *         `== 0`: 不需要;
     *         `>  0`: 需要继续解析`size_t`个字节
     * @warning 假定内容是符合Http协议的
     */
    std::size_t parserResponse(std::string_view buf);

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
     */
    void setStatusAndContent(Status status, std::string const& content) {
        setResponseLine(status).setContentType(TEXT).setBodyData(content);
    }

    /**
     * @brief 使用分块编码传输文件
     * @param filePath 文件路径
     */
    STL::coroutine::task::Task<> useChunkedEncodingTransferFile(std::string const& filePath) const {
        co_return co_await _io->sendResponseWithChunkedEncoding(filePath);
    }

    /**
     * @brief 使用断点续传传输文件
     * @param filePath 文件路径
     */
     STL::coroutine::task::Task<> useRangeTransferFile(std::string const& filePath) const {
        co_return co_await _io->sendResponseWithRange(filePath);
    }

    // ===== ↑服务端使用の更加人性化API↑ =====

    // ===== ↓服务端使用↓ =====
    /**
     * @brief 设置状态行 (协议使用HTTP/1.1)
     * @param statusCode 状态码
     * @param describe 状态码描述: 如果为`""`则会使用该状态码对应默认的描述
     * @warning 不需要手动写`/r`或`/n`以及尾部的`/r/n`
     */
    Response& setResponseLine(Status statusCode, std::string_view describe = "");

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
    Response& addHeader(const std::string& key, const std::string& val) {
        _responseHeaders[key] = val;
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
    std::vector<std::string> _statusLine; // 状态行
    std::unordered_map<std::string, std::string> _responseHeaders; // 响应头
    std::string _body; // 响应体

    // 指向上一次解析的响应头的键值对; 无效时候指向 `.end()`
    std::unordered_map<std::string, std::string>::iterator _responseHeadersIt; 
    std::string _buf; // 上一次未解析全的
    unsigned _sendCnt = 0; // 写入计数
    bool _completeResponseHeader = false; //是否解析完成响应头
    std::optional<std::size_t> _remainingBodyLen; // 仍需读取的请求体长度
    HX::web::server::IO<void>* _io;

    friend HX::web::server::IO<void>;

    /**
     * @brief [仅服务端] 生成响应行和响应头
     */
    void _buildResponseLineAndHeaders();

    /**
     * @brief [仅服务端] 将`buf`转化为`ChunkedEncoding`的Body, 放入`_body`以分片发送
     * @param buf 
     * @warning 内部会清空 `_buf`, 再以`ChunkedEncoding`格式写入 buf 到 `_buf`!
     */
    void _buildToChunkedEncoding(std::string_view buf);

    /**
     * @brief [仅服务端] 生成完整的响应字符串, 用于写入
     * @warning 本方法子适用于`Content-Length`的短消息, 无法使用分块编码
     */
    void createResponseBuffer();
};

}}}} // namespace HX::web::protocol::http

#endif // _HX_RESPONSE_H_