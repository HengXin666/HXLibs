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
#ifndef _HX_REQUEST_H_
#define _HX_REQUEST_H_

#include <vector>
#include <unordered_map>
#include <string>
#include <string_view>
#include <optional>
#include <stdexcept>

#include <HXWeb/protocol/http/PathVariable.h>
namespace HX { namespace web { namespace client {

template <class T>
class IO;

}}} // namespace HX::web::client

namespace HX { namespace web { namespace router {

class Router;

}}} // namespace HX::web::router

namespace HX { namespace web { namespace protocol { namespace http {

/**
 * @brief 请求类(Request)
 */
class Request {
public:
    /**
     * @brief 请求类型枚举
     */
    // enum class RequestType : int {
    //     GET = 0,
    //     POST = 1,
    //     PUT = 2,
    //     DELETE = 3,
    //     // 极少使用
    //     HEAD = 4,    // 获得报文首部
    //     OPTIONS = 5, // 询问支持的方法
    //     PATCH = 6,   // 局部更新文件
    //     TRACE = 7,   // 追踪路径
    //     CONNECT = 8, // 要求用隧道协议连接代理
    // };

    explicit Request(HX::web::client::IO<void>* io) 
        : _requestLine()
        , _requestHeaders()
        , _requestHeadersIt(_requestHeaders.end())
        , _body()
        , _remainingBodyLen(std::nullopt)
        , _io(io)
    {}
    // ===== ↓客户端使用↓ =====
    /**
     * @brief 设置请求行 (协议使用HTTP/1.1)
     * @param method 请求方法 (如 "GET")
     * @param url url (如 "www.baidu.com")
     * @warning 不需要手动写`/r`或`/n`以及尾部的`/r/n`
     */
    Request& setRequestLine(const std::string& method, const std::string& url) {
        using namespace std::string_literals;
        _requestLine.resize(3);
        _requestLine[RequestLineDataType::RequestType] = method;
        _requestLine[RequestLineDataType::RequestPath] = url;
        _requestLine[RequestLineDataType::ProtocolVersion] = "HTTP/1.1"s;
        return *this;
    }

    /**
     * @brief 向请求头添加一些键值对
     * @param heads 键值对
     * @return Request& 
     */
    Request& addRequestHeaders(const std::vector<std::pair<std::string, std::string>>& heads) {
        _requestHeaders.insert(heads.begin(), heads.end());
        return *this;
    }

    /**
     * @brief 向请求头添加一些键值对
     * @param heads 键值对
     * @return Request& 
     */
    Request& addRequestHeaders(const std::unordered_map<std::string, std::string>& heads) {
        _requestHeaders.insert(heads.begin(), heads.end());
        return *this;
    }

    /**
     * @brief 设置请求体信息
     * @param data 信息
     * @return Request& 
     */
    Request& setRequestBody(const std::string& data) {
        _body = data;
        return *this;
    }

    /**
     * @brief 向请求头添加一个键值对
     * @param key 键
     * @param val 值
     * @return Request&
     * @warning `key`在`map`中是区分大小写的, 故不要使用`大小写不同`的相同的`键`
     */
    Request& addHeader(const std::string& key, const std::string& val) {
        _requestHeaders[key] = val;
        return *this;
    }
    // ===== ↑客户端使用↑ =====

    // ===== ↓服务端使用↓ =====
    /**
     * @brief 解析请求
     * @param buf 需要解析的内容
     * @return 是否需要继续解析;
     *         `== 0`: 不需要;
     *         `>  0`: 需要继续解析`size_t`个字节
     * @warning 假定内容是符合Http协议的
     */
    std::size_t parserRequest(std::string_view buf);

    /**
     * @brief 获取请求头键值对的引用
     * @return const std::unordered_map<std::string, std::string>& 
     */
    const std::unordered_map<std::string, std::string>& getRequestHeaders() const noexcept {
        return _requestHeaders;
    }

    /**
     * @brief 解析查询参数 (解析如: `?name=loli&awa=ok&hitori`)
     * @return 返回解析到的字符串键值对哈希表
     * @warning 如果解析到不是键值对的, 即通过`&`分割后没有`=`的, 默认其全部为Key, 但Val = ""
     */
    std::unordered_map<std::string, std::string> getParseQueryParameters() const;

    /**
     * @brief 获取请求类型
     * @return 请求类型 (如: "GET", "POST"...)
     */
    std::string_view getRequesType() const noexcept {
        return _requestLine[RequestLineDataType::RequestType];
    }

    /**
     * @brief 获取请求体 ( 临时设计的 )
     * @return 如果没有请求体, 则返回`""`
     */
    std::string getRequesBody() const noexcept {
        return _body;
    }

    /**
     * @brief 获取请求体
     * @return 如果没有请求体, 则返回`""`
     */
    std::string getRequesBody() noexcept {
        return std::move(_body);
    }

    /**
     * @brief 获取请求PATH
     * @return 请求PATH (如: "/", "/home?loli=watasi"...)
     */
    std::string getRequesPath() const noexcept {
        return _requestLine[RequestLineDataType::RequestPath];
    }

    /**
     * @brief 获取请求的纯PATH部分
     * @return 请求PATH (如: "/", "/home?loli=watasi"的"/home"部分)
     */
    std::string getPureRequesPath() const noexcept {
        std::string path = getRequesPath();
        std::size_t pos = path.find('?');
        if (pos == std::string::npos)
            return path;
        return path.substr(0, pos);
    }

    /**
     * @brief 获取请求协议版本
     * @return 请求协议版本 (如: "HTTP/1.1", "HTTP/2.0"...)
     */
    std::string getRequesProtocolVersion() const noexcept {
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
        if (!_pathVariable) [[unlikely]] {
            throw std::runtime_error("No path parameters available to parse.");
        }
        return _pathVariable->wildcarDataArr[index];
    }

    /**
     * @brief 获取通配符路径参数的内容
     * @throw std::runtime_error 如果路径参数未初始化则抛出 (端点函数必须为`{val}`格式)
     * @return std::string_view 
     */
    std::string_view getUniversalWildcardPath() const {
        if (!_pathVariable) [[unlikely]] {
            throw std::runtime_error("No path parameters available to parse.");
        }
        return _pathVariable->UWPData;
    }
    // ===== ↑服务端使用↑ =====

    /**
     * @brief 清空已有的请求内容, 并且初始化标准
     */
    void clear() noexcept {
        _requestLine.clear();
        _requestHeaders.clear();
        _requestHeadersIt = _requestHeaders.end();
        _buf.clear();
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
     * @brief 服务端: 之前未解析全的数据
     *        客户端: 待写入的内容
     */
    std::string _buf;

    std::vector<std::string> _requestLine; // 请求行
    std::unordered_map<std::string, std::string> _requestHeaders; // 请求头

    // 上一次解析的请求头
    std::unordered_map<std::string, std::string>::iterator _requestHeadersIt;

    // 请求体
    std::string _body;

    // @brief 仍需读取的请求体长度
    std::optional<std::size_t> _remainingBodyLen;

    // @brief 是否解析完成请求头
    bool _completeRequestHeader = false;

    // @brief 用于存储解析的路径参数, 无需理会它的生命周期, 它的生命周期在[服务端]`端点函数`内
    PathVariable* _pathVariable = nullptr;

    HX::web::client::IO<void>* _io;

    friend HX::web::client::IO<void>;
    friend PathVariable;

    /**
     * @brief [仅客户端] 生成请求字符串, 用于写入
     */
    void createRequestBuffer();
};

}}}} // namespace HX::web::protocol::http

#endif // _HX_REQUEST_H_
