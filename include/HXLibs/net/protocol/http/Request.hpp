#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2024-7-20 17:04:53
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

#include <vector>
#include <optional>
#include <stdexcept>

#include <HXLibs/container/ArrayBuf.hpp>
#include <HXLibs/net/protocol/http/Http.hpp>
#include <HXLibs/net/socket/IO.hpp>
#include <HXLibs/utils/FileUtils.hpp>
#include <HXLibs/utils/StringUtils.hpp>
#include <HXLibs/utils/TimeNTTP.hpp>
#include <HXLibs/meta/ContainerConcepts.hpp>
#include <HXLibs/exception/ErrorHandlingTools.hpp>

namespace HX::net {

/**
 * @brief 请求类(Request)
 */
class Request {
public:
    explicit Request(IO& io) 
        : _recvBuf()
        , _requestLine()
        , _requestHeaders()
        , _requestHeadersIt(_requestHeaders.end())
        , _body()
        , _remainingBodyLen(std::nullopt)
        , _io{io}
        , _boundary{}
    {}

#if 0
    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;

    Request(Request&&) = default;
    Request& operator=(Request&&) = default;
#else
    Request& operator=(Request&&) noexcept = delete;
#endif

    // ===== ↓客户端使用↓ =====
    /**
     * @brief 发送请求
     * @return coroutine::Task<> 
     * @throw 超时
     */
    template <typename Timeout>
        requires(utils::HasTimeNTTP<Timeout>)
    coroutine::Task<> sendHttpReq() {
        using namespace std::string_view_literals;
#ifndef NODEBUG
        // 不能存在 CONTENT_LENGTH_SV, 仅debug模式会检测
        if (auto it = _requestHeaders.find(CONTENT_LENGTH_SV); 
            it != _requestHeaders.end()
        ) [[unlikely]] {
            throw std::runtime_error{"Should not be manually added: Content-Length"};
        }
#endif // !NODEBUG
        std::vector<char> buf;
        buf.reserve(IO::kBufMaxSize); // 预留空间
        _buildLineAndHead(buf);
        if (_body.empty()) {
            utils::StringUtil::append(buf, CRLF);
            co_await _io.sendLinkTimeout<Timeout>(buf);
        } else {
            // 请求体
            utils::StringUtil::append(buf, CONTENT_LENGTH_SV);
            utils::StringUtil::append(buf, HEADER_SEPARATOR_SV);
            utils::StringUtil::append(buf, std::to_string(_body.size()));
            utils::StringUtil::append(buf, HEADER_END_SV);
            co_await _io.sendLinkTimeout<Timeout>(buf);
            co_await _io.sendLinkTimeout<Timeout>(_body);
        }
        co_return;
    }

    /**
     * @brief 分块编码发送文件
     * @tparam Timeout 超时时间
     */
    template <typename Timeout>
        requires(utils::HasTimeNTTP<Timeout>)
    coroutine::Task<> sendChunkedReq(std::string_view path) {
        using namespace std::string_literals;
        using namespace std::string_view_literals;
#ifndef NODEBUG
        // 不能存在 CONTENT_LENGTH_SV, 仅debug模式会检测
        if (auto it = _requestHeaders.find(CONTENT_LENGTH_SV); 
            it != _requestHeaders.end()
        ) [[unlikely]] {
            throw std::runtime_error{"Should not be manually added: Content-Length"};
        }
#endif // !NODEBUG
        utils::AsyncFile file{_io};
        co_await file.open(path, utils::OpenMode::Read);
        _requestHeaders[std::string{TRANSFER_ENCODING_SV}] = "chunked"; // 分块编码请求头要求
        // 开始发送分块编码
        std::vector<char> buf, sendBuf;
        buf.reserve(utils::FileUtils::kBufMaxSize);
        sendBuf.reserve(16);
        // 把 Line + Head 组装好, 然后直接发送
        _buildLineAndHead(buf);
        utils::StringUtil::append(buf, CRLF); // \r\n + \r\n
        co_await _io.sendLinkTimeout<Timeout>(buf);
        buf.resize(utils::FileUtils::kBufMaxSize);
        // 读取文件
        std::size_t size = static_cast<std::size_t>(co_await file.read(buf));
        _buildToChunkedEncoding<true>(size, sendBuf);
        co_await _io.sendLinkTimeout<Timeout>(sendBuf);
        co_await _io.sendLinkTimeout<Timeout>({buf.data(), size});
        for (;;) {
            if (!size) [[unlikely]] {
                // 需要使用 长度为 0 的分块, 来标记当前内容实体传输结束
                _buildToChunkedEncoding<false, true>(0, sendBuf);
                co_await _io.sendLinkTimeout<Timeout>(sendBuf);
                break;
            }
            size = static_cast<std::size_t>(co_await file.read(buf));
            _buildToChunkedEncoding(size, sendBuf);
            co_await _io.sendLinkTimeout<Timeout>(sendBuf);
            co_await _io.sendLinkTimeout<Timeout>({buf.data(), size});
        }
        co_await file.close();
    }

    /**
     * @brief 设置请求行 (协议使用HTTP/1.1)
     * @param method 请求方法 (如 "GET")
     * @param path url的path部分 (如 "www.baidu.com/loli" 的 /loli)
     * @warning 不需要手动写`/r`或`/n`以及尾部的`/r/n`
     */
    template <HttpMethod Method>
    Request& setReqLine(std::string_view path) {
        using namespace std::string_literals;
        _requestLine.resize(3);
        _requestLine[RequestLineDataType::RequestType] = getMethodStringView(Method);
        _requestLine[RequestLineDataType::RequestPath] = path;
        _requestLine[RequestLineDataType::ProtocolVersion] = "HTTP/1.1";
        return *this;
    }

    /**
     * @brief 向请求头添加一些键值对
     * @param heads 键值对
     * @return Request& 
     */
    Request& addHeaders(const std::vector<std::pair<std::string, std::string>>& heads) {
        _requestHeaders.insert(heads.begin(), heads.end());
        return *this;
    }

    /**
     * @brief 向请求头添加一些键值对
     * @param heads 键值对
     * @return Request& 
     */
    Request& addHeaders(const std::unordered_map<std::string, std::string>& heads) {
        _requestHeaders.insert(heads.begin(), heads.end());
        return *this;
    }

    /**
     * @brief 向请求头添加一些键值对
     * @param heads 键值对
     * @return Request& 
     */
    Request& addHeaders(HeaderHashMap&& heads) {
        for (auto&& [k, v] : heads) {
            _requestHeaders[k] = std::move(v);
        }
        return *this;
    }

    /**
     * @brief 设置请求体信息
     * @tparam S 字符串类型
     * @param data 信息
     * @return Request& 
     */
    template <typename S>
        requires (requires (S&& data, std::string s) {
            s += std::forward<S>(data);
        })
    Request& setBody(S&& data) noexcept {
        _body.clear();
        _body += std::forward<S>(data);
        return *this;
    }

    /**
     * @brief 向请求头添加一个键值对
     * @param key 键
     * @param val 值
     * @return Request&
     * @warning `key`在`map`中是区分大小写的, 故不要使用`大小写不同`的相同的`键`
     */
    template <typename Str1, typename Str2>
    Request& addHeaders(Str1&& key, Str2&& val) {
        _requestHeaders[std::forward<Str1>(key)] = std::forward<Str2>(val);
        return *this;
    }

    /**
     * @brief 尝试向请求头添加一个键值对, 如果存在则不插入
     * @param key 键
     * @param val 值
     * @return Request&
     * @warning `key`在`map`中是区分大小写的, 故不要使用`大小写不同`的相同的`键`
     */
    template <typename Str1, typename Str2>
    Request& tryAddHeaders(Str1&& key, Str2&& val) {
        _requestHeaders.try_emplace(std::forward<Str1>(key), std::forward<Str2>(val));
        return *this;
    }
    // ===== ↑客户端使用↑ =====

    // ===== ↓服务端使用↓ =====
    /**
     * @brief 解析请求
     * @return coroutine::Task<bool> 断开连接则为false, 解析成功为true
     */
    template <typename Timeout>
        requires(utils::HasTimeNTTP<Timeout>)
    coroutine::Task<bool> parserReq() {
        for (std::size_t n = IO::kBufMaxSize; n; n = std::min(_parserReq(), IO::kBufMaxSize)) {
            auto res = co_await _io.recvLinkTimeout<Timeout>(
                // 保留原有的数据
                {_recvBuf.data() + _recvBuf.size(),  _recvBuf.data() + n}
            );
            if (res.index() == 1) [[unlikely]] {
                co_return false;  // 超时
            }
            auto recvN = HXLIBS_CHECK_EVENT_LOOP(
                (res.template get<0, exception::ExceptionMode::Nothrow>())
            );
            if (recvN == 0) [[unlikely]] {
                co_return false; // 连接断开
            }
            _recvBuf.addSize(static_cast<std::size_t>(recvN));
        }
        co_return true;
    }

    /**
     * @brief 获取请求头键值对的引用
     * @return const std::unordered_map<std::string, std::string>& 
     */
    const auto& getHeaders() const noexcept {
        return _requestHeaders;
    }

    /**
     * @brief 解析查询参数 (解析如: `?name=loli&awa=ok&hitori`)
     * @return 返回解析到的字符串键值对哈希表
     * @warning 如果解析到不是键值对的, 即通过`&`分割后没有`=`的, 默认其全部为Key, 但Val = ""
     */
    std::unordered_map<std::string, std::string> getParseQueryParameters() const {
        auto path = getReqPath();
        std::size_t pos = path.find('?'); // 没必要反向查找
        if (pos == std::string::npos)
            return {};
        // 如果有#这种, 要删除: 无需处理, 这个只是存在于客户端, 不会传输到服务端(?)至少path没有
        auto parameter = path.substr(pos + 1);
        auto kvArr = HX::utils::StringUtil::split<std::string>(parameter, "&");
        std::unordered_map<std::string, std::string> res;
        for (const auto& it : kvArr) {
            auto&& kvPair = HX::utils::StringUtil::splitAtFirst(it, "=");
            if (kvPair.first == "")
                res.insert_or_assign(it, "");
            else
                res.insert(std::move(kvPair));
        }
        return res;
    }

    /**
     * @brief 获取请求类型
     * @return 请求类型 (如: "GET", "POST"...)
     */
    std::string_view getReqType() const noexcept {
        return _requestLine[RequestLineDataType::RequestType];
    }

    /**
     * @brief 朴素的解析 Body
     * @tparam Timeout 超时时间
     */
    template <typename Timeout = decltype(utils::operator""_s<"5">())>
        requires(utils::HasTimeNTTP<Timeout>)
    coroutine::Task<std::string> parseBody() {
        if (_completeBody) [[unlikely]] {
            // 已经解析过 Http Body 了
            throw std::runtime_error{"Have already analyzed the http body"};
        }
        _completeBody = true;
        for (std::size_t n = _parserReqBody(); n; n = _parserReqBody()) {
            auto res = co_await _io.recvLinkTimeout<Timeout>(
                // 保留原有的数据
                {_recvBuf.data() + _recvBuf.size(),  _recvBuf.data() + _recvBuf.max_size()}
            );
            if (res.index() == 1) [[unlikely]] {
                // 超时
                throw std::runtime_error{"parseBody: Recv timeout"};
            }
            auto recvN = HXLIBS_CHECK_EVENT_LOOP(
                (res.template get<0, exception::ExceptionMode::Nothrow>())
            );
            if (recvN == 0) [[unlikely]] {
                // 连接断开
                throw std::runtime_error{"parseBody: Connection is Broken"};
            }
            _recvBuf.addSize(static_cast<std::size_t>(recvN));
        }
        co_return std::move(_body);
    }

    /**
     * @brief 解析 Body 并且保存到 path
     * @param path 保存路径
     * @return coroutine::Task<> 
     */
    template <typename Timeout = decltype(utils::operator""_s<"5">())>
        requires(utils::HasTimeNTTP<Timeout>)
    coroutine::Task<> saveToFile(std::string_view path) {
        utils::AsyncFile file{_io};
        co_await file.open(path, utils::OpenMode::Write);
        if (_completeBody) [[unlikely]] {
            // 已经解析过 Http Body 了
            throw std::runtime_error{"Have already analyzed the http body"};
        }
        _completeBody = true;
        for (std::size_t n = co_await _coParserReqBody(file); n; n = co_await _coParserReqBody(file)) {
            auto res = co_await _io.recvLinkTimeout<Timeout>(
                // 保留原有的数据
                {_recvBuf.data() + _recvBuf.size(),  _recvBuf.data() + _recvBuf.max_size()}
            );
            if (res.index() == 1) [[unlikely]] {
                // 超时
                throw std::runtime_error{"parseBody: Recv timeout"};
            }
            auto recvN = HXLIBS_CHECK_EVENT_LOOP(
                (res.template get<0, exception::ExceptionMode::Nothrow>())
            );
            if (recvN == 0) [[unlikely]] {
                // 连接断开
                throw std::runtime_error{"parseBody: Connection is Broken"};
            }
            _recvBuf.addSize(static_cast<std::size_t>(recvN));
        }
    }

    // @todo 
    coroutine::Task<> _getMultipartFiles() {
        static_assert(true, "@todo Func...");
        if (auto it = _requestHeaders.find(CONTENT_TYPE_SV); it != _requestHeaders.end()) {
            // multipart/form-data 协议
            if (_boundary.empty()) {            
                std::string_view str = it->second;
                auto pos = str.find("multipart/form-data; boundary=");
                if (pos == std::string::npos) {
                    // return 0;
                }
                _boundary = str.substr(sizeof("multipart/form-data; boundary=") - 1);
            }
            /*
            ------WebKitFormBoundary123\r\n
            Content-Disposition: form-data; name="text"\r\n
            \r\n
            hello\r\n
            ------WebKitFormBoundary123\r\n
            Content-Disposition: form-data; name="file"; filename="a.txt"\r\n
            Content-Type: text/plain\r\n
            \r\n
            <文件内容>\r\n
            ------WebKitFormBoundary123--\r\n
            */
        }
        co_return ;
    }

    /**
     * @brief 获取请求PATH
     * @return 请求PATH (如: "/", "/home?loli=watasi"...)
     */
    std::string_view getReqPath() const noexcept {
        return _requestLine[RequestLineDataType::RequestPath];
    }

    /**
     * @brief 获取请求的纯PATH部分
     * @return 请求PATH (如: "/", "/home?loli=watasi"的"/home"部分)
     */
    std::string getPureReqPath() const noexcept {
        auto path = getReqPath();
        std::size_t pos = path.find('?');
        if (pos != std::string_view::npos) {
            path = path.substr(0, pos);
        }
        return {path.data(), path.size()};
    }

    /**
     * @brief 获取请求协议版本
     * @return 请求协议版本 (如: "HTTP/1.1", "HTTP/2.0"...)
     */
    std::string_view getProtocolVersion() const noexcept {
        return _requestLine[RequestLineDataType::ProtocolVersion];
    }

    /**
     * @brief 获取第`index`个路径参数的内容
     * @param index 路径参数索引, 如`/home/{name}/id`, `index = 0` => {name}
     * @throw std::runtime_error 如果路径参数未初始化则抛出 (端点函数必须为`{val}`格式)
     * @throw std::out_of_range 如果 `index` 超出可用路径参数范围时抛出
     * @note 调用前需要确保路径参数已正确初始化
     * @return std::string_view 
     */
    std::string_view getPathParam(std::size_t index) const {
        if (_wildcarDataArr.empty()) [[unlikely]] {
            throw std::runtime_error("No path parameters available to parse.");
        }
        return _wildcarDataArr[index];
    }

    /**
     * @brief 获取通配符路径参数的内容
     * @throw std::runtime_error 如果路径参数未初始化则抛出 (端点函数必须为`{val}`格式)
     * @return std::string_view 
     */
    std::string_view getUniversalWildcardPath() const {
        if (_urlWildcardData.empty()) [[unlikely]] {
            throw std::runtime_error("No path parameters available to parse.");
        }
        return _urlWildcardData;
    }

    RangeRequestView getRangeRequestView() const {
        return {getReqType(), _requestHeaders};
    }
    // ===== ↑服务端使用↑ =====

    /**
     * @brief 获取内部 IO
     * @return IO& 
     */
    IO& getIO() noexcept {
        return _io;
    }

    /**
     * @brief 清空已有的请求内容, 并且初始化标准
     * @warning 显然应该在 clearBody() 之前调用
     */
    coroutine::Task<> clear() noexcept {
        if (!_completeBody) {
            // 250 ms, 如果解析不完, 就滚蛋! 传递这么多没用的干什么?!
            co_await parseBody<decltype(utils::operator""_ms<"250">())>();
        }
        _completeBody = false;
        _boundary = {};
        _requestLine.clear();
        _requestHeaders.clear();
        _requestHeadersIt = _requestHeaders.end();
        _recvBuf.clear();
        _body.clear();
        _completeRequestHeader = false;
        _remainingBodyLen.reset();
    }

private:
    /**
     * @brief 请求行数据分类
     */
    enum RequestLineDataType {
        RequestType = 0,        // 请求类型
        RequestPath = 1,        // 请求路径
        ProtocolVersion = 2,    // 协议版本
    };

    /**
     * @brief 仅用于读取时候写入的缓冲区
     */
    container::ArrayBuf<char, IO::kBufMaxSize> _recvBuf;

    std::vector<std::string> _requestLine;  // 请求行
    HeaderHashMap _requestHeaders;          // 请求头

    // 上一次解析的请求头
    decltype(_requestHeaders)::iterator _requestHeadersIt;

    // 请求体
    std::string _body;

    // @brief 仍需读取的请求体长度
    std::optional<std::size_t> _remainingBodyLen;

    /**
     * @brief 路径变量, 如`/home/{id}`的`id`, 
     * 存放的是解析后的结果字符串视图(指向的是Request的请求行)
     */
    // 单个路径变量的结果数组
    std::span<std::string_view> _wildcarDataArr;

    // 通配符的结果
    std::string_view _urlWildcardData;

    // IO 对象 (内含 协程事件循环)
    IO& _io;

    // multipart/form-data 协议边界.
    std::string_view _boundary;

    /**
     * @brief 是否解析完成请求头
     */
    bool _completeRequestHeader = false;

    /**
     * @brief 是否解析过 Body
     */
    bool _completeBody = false;

    friend class Router;
    friend class WebSocketFactory;

    template <typename Timeout, typename Proxy>
        requires(utils::HasTimeNTTP<Timeout>)
    friend class HttpClient;

    /**
     * @brief 组装请求行和请求头 (不包含 `content-length`, 和 最终的`\r\n\r\n`分割)
     * @param buf 
     */
    void _buildLineAndHead(std::vector<char>& buf) {
        using namespace std::string_view_literals;
        // 请求行
        utils::StringUtil::append(buf, _requestLine[RequestLineDataType::RequestType]);
        utils::StringUtil::append(buf, " "sv);
        utils::StringUtil::append(buf, _requestLine[RequestLineDataType::RequestPath]);
        utils::StringUtil::append(buf, " "sv);
        utils::StringUtil::append(buf, _requestLine[RequestLineDataType::ProtocolVersion]);
        utils::StringUtil::append(buf, CRLF);
        // 请求头
        for (const auto& [key, val] : _requestHeaders) {
            utils::StringUtil::append(buf, key);
            utils::StringUtil::append(buf, HEADER_SEPARATOR_SV);
            utils::StringUtil::append(buf, val);
            utils::StringUtil::append(buf, CRLF);
        }
    }

    /**
     * @brief [仅客户端] 把 size 大小转换为 16 进制并以符合 ChunkedEncoding 的格式写入 sendBuf
     * @param size 内容大小
     * @param sendBuf 用于写入头部的 buf
     * @warning 内部会清空 `sendBuf`, 再以`ChunkedEncoding`格式写入 buf 到 `sendBuf`!
     */
    template <bool IsFirst = false, bool IsEnd = false>
    void _buildToChunkedEncoding(std::size_t size, std::vector<char>& sendBuf) {
        sendBuf.clear();
        if constexpr (!IsFirst) {
            utils::StringUtil::append(sendBuf, CRLF);
        }
#ifdef HEXADECIMAL_CONVERSION
        utils::StringUtil::append(sendBuf, utils::NumericBaseConverter::hexadecimalConversion(size));
#else
        utils::StringUtil::append(sendBuf, std::format("{:X}", size)); // 需要十六进制嘞
#endif // !HEXADECIMAL_CONVERSION
        utils::StringUtil::append(sendBuf, CRLF);
        if constexpr (IsEnd) {
            utils::StringUtil::append(sendBuf, CRLF);
        }
    }

    /**
     * @brief 解析请求
     * @return 是否需要继续解析;
     *         `== 0`: 不需要;
     *         `>  0`: 需要继续解析`size_t`个字节
     * @warning 假定内容是符合Http协议的
     */
    std::size_t _parserReq() {
        using namespace std::string_literals;
        using namespace std::string_view_literals;
        std::string_view buf{_recvBuf.data(), _recvBuf.size()};
        switch ((_completeRequestHeader << 1) | (!_requestLine.empty())) {
            case 0x00: { // 什么也没有解析, 开始解析请求行
                std::size_t pos = buf.find(CRLF);
                if (pos == std::string_view::npos) [[unlikely]] { // 不可能事件
                    return IO::kBufMaxSize;
                }
                std::string_view reqLine = buf.substr(0, pos);

                // 解析请求行
                _requestLine = utils::StringUtil::split<std::string>(
                    reqLine, " "sv
                );

                if (_requestLine.size() < 3) [[unlikely]] {
                    return IO::kBufMaxSize;
                } 
                // else if (_requestLine.size() > 3) [[unlikely]] {
                // @todo 理论上不会出现
                // }

                buf = buf.substr(pos + 2); // 再前进, 以去掉 "\r\n"
                [[fallthrough]];
            }
            case 0x01: { // 解析完请求行, 开始解析请求头
                /**
                 * @brief 请求头
                 * 通过`\r\n`分割后, 取最前面的, 先使用最左的`:`以判断是否是需要作为独立的键值对;
                 * -  如果找不到`:`, 并且 非空, 那么它需要接在上一个解析的键值对的值尾
                 * -  否则即请求头解析完毕!
                 */
                while (!_completeRequestHeader) { // 请求头未解析完
                    std::size_t pos = buf.find(CRLF);
                    if (pos == std::string_view::npos) { // 没有读取完
                        _recvBuf.moveToHead(buf);
                        return IO::kBufMaxSize;
                    }
                    std::string_view subKVStr = buf.substr(0, pos);
                    auto p = utils::StringUtil::splitAtFirst(subKVStr, HEADER_SEPARATOR_SV);
                    if (p.first.empty()) [[unlikely]] {     // 找不到 ": "
                        if (subKVStr.size()) [[unlikely]] { // 很少会有分片传输请求头的
                            _requestHeadersIt->second.append(subKVStr);
                        } else { // 请求头解析完毕!
                            _completeRequestHeader = true;
                        }
                    } else {
                        // K: V, 其中 V 是区分大小写的, 但是 K 是不区分的
                        utils::StringUtil::toLower(p.first);
                        _requestHeadersIt = _requestHeaders.insert(p).first;
                    }
                    buf = buf.substr(pos + 2); // 再前进, 以去掉 "\r\n"
                }
                [[fallthrough]];
            }
            case 0x03: {
                // 更改到懒解析Body, 如果已经读取了, 那放入缓存趴...
                _recvBuf.moveToHead(buf);
                break;
            }
            [[unlikely]] default:
#ifndef NDEBUG
                throw std::runtime_error{"parserRequest UB Error"}
#endif // NDEBUG
            ;
        }
        return 0;
    }

    /**
     * @brief 解析 Body
     * @return std::size_t 还需要解析的字节数
     */
    std::size_t _parserReqBody() {
        using namespace std::string_literals;
        using namespace std::string_view_literals;
        std::string_view buf{_recvBuf.data(), _recvBuf.size()};
        if (_requestHeaders.contains(CONTENT_LENGTH_SV)) { // 存在content-length模式接收的响应体
            // 计算剩余待解析字节数
            if (!_remainingBodyLen.has_value()) {
                _remainingBodyLen = std::stoull(_requestHeaders.find(CONTENT_LENGTH_SV)->second);
            }
            if (*_remainingBodyLen != 0) {
                *_remainingBodyLen -= buf.size();
                _body.append(buf);
                _recvBuf.clear();
                return *_remainingBodyLen;
            }
        } else if (_requestHeaders.contains(TRANSFER_ENCODING_SV)) { // 存在请求体以`分块传输编码`
            /**
             * @todo 目前只支持 chunked 编码, 不支持压缩的 (2024-9-6 09:36:25) 
             * */
            if (_remainingBodyLen) { // 处理没有读取完的
                if (buf.size() <= *_remainingBodyLen) { // 还没有读取完毕
                    _body.append(buf);
                    *_remainingBodyLen -= buf.size();
                    _recvBuf.clear();
                    return IO::kBufMaxSize;
                } else { // 读取完了
                    _body.append(buf.substr(0, *_remainingBodyLen));
                    buf = buf.substr(std::min(*_remainingBodyLen + 2, buf.size()));
                    _remainingBodyLen.reset();
                }
            }
            for (;;) {
                std::size_t posLen = buf.find(CRLF);
                if (posLen == std::string_view::npos) { // 没有读完
                    _recvBuf.moveToHead(buf);
                    return IO::kBufMaxSize;
                }
                if (!posLen && buf[0] == '\r') [[unlikely]] { // posLen == 0
                    // \r\n 贴脸, 触发原因, std::min(*_remainingBodyLen + 2, buf.size()) 只能 buf.size()
                    buf = buf.substr(posLen + 2);
                    continue;
                }
                _remainingBodyLen = utils::NumericBaseConverter::strToNum<std::size_t, 16>(
                    buf.substr(0, posLen)
                ); // 转换为十进制整数
                if (!*_remainingBodyLen) { // 解析完毕
                    return 0;
                }
                buf = buf.substr(posLen + 2);
                if (buf.size() <= *_remainingBodyLen) { // 没有读完
                    _body.append(buf);
                    *_remainingBodyLen -= buf.size();
                    _recvBuf.clear();
                    return IO::kBufMaxSize;
                }
                _body.append(buf.substr(0, *_remainingBodyLen));
                buf = buf.substr(*_remainingBodyLen + 2);
            }
        }
        return 0;
    }

    /**
     * @brief 解析 Body 写入文件中
     * @return std::size_t 还需要解析的字节数
     */
    coroutine::Task<std::size_t> _coParserReqBody(utils::AsyncFile& file) {
        using namespace std::string_literals;
        using namespace std::string_view_literals;
        std::string_view buf{_recvBuf.data(), _recvBuf.size()};
        if (_requestHeaders.contains(CONTENT_LENGTH_SV)) { // 存在content-length模式接收的响应体
            // 计算剩余待解析字节数
            if (!_remainingBodyLen.has_value()) {
                _remainingBodyLen = std::stoull(_requestHeaders.find(CONTENT_LENGTH_SV)->second);
            }
            
            if (*_remainingBodyLen != 0) {
                *_remainingBodyLen -= buf.size();
                co_await file.write(buf); // _body.append(buf);
                _recvBuf.clear();
                co_return *_remainingBodyLen;
            }
        } else if (_requestHeaders.contains(TRANSFER_ENCODING_SV)) { // 存在请求体以`分块传输编码`
            /**
             * @todo 目前只支持 chunked 编码, 不支持压缩的 (2024-9-6 09:36:25) 
             * */
            if (_remainingBodyLen) { // 处理没有读取完的
                if (buf.size() <= *_remainingBodyLen) { // 还没有读取完毕
                    co_await file.write(buf); // _body.append(buf);
                    *_remainingBodyLen -= buf.size();
                    _recvBuf.clear();
                    co_return IO::kBufMaxSize;
                } else { // 读取完了
                    co_await file.write(buf.substr(0, *_remainingBodyLen)); // _body.append(buf.substr(0, *_remainingBodyLen));
                    buf = buf.substr(std::min(*_remainingBodyLen + 2, buf.size()));
                    _remainingBodyLen.reset();
                }
            }
            for (;;) {
                std::size_t posLen = buf.find(CRLF);
                if (posLen == std::string_view::npos) { // 没有读完
                    _recvBuf.moveToHead(buf);
                    co_return IO::kBufMaxSize;
                }
                if (!posLen && buf[0] == '\r') [[unlikely]] { // posLen == 0
                    // \r\n 贴脸, 触发原因, std::min(*_remainingBodyLen + 2, buf.size()) 只能 buf.size()
                    buf = buf.substr(posLen + 2);
                    continue;
                }
                _remainingBodyLen = utils::NumericBaseConverter::strToNum<std::size_t, 16>(
                    buf.substr(0, posLen)
                ); // 转换为十进制整数
                if (!*_remainingBodyLen) { // 解析完毕
                    co_return 0;
                }
                buf = buf.substr(posLen + 2);
                if (buf.size() <= *_remainingBodyLen) { // 没有读完
                    co_await file.write(buf); // _body.append(buf);
                    *_remainingBodyLen -= buf.size();
                    _recvBuf.clear();
                    co_return IO::kBufMaxSize;
                }
                co_await file.write(buf.substr(0, *_remainingBodyLen)); // _body.append(buf.substr(0, *_remainingBodyLen));
                buf = buf.substr(*_remainingBodyLen + 2);
            }
        }
        co_return 0;
    }
};

} // namespace HX::net
