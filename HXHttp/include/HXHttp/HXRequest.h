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
#ifndef _HX_HXREQUEST_H_
#define _HX_HXREQUEST_H_

#include <vector>
#include <unordered_map>
#include <string>
#include <optional>

namespace HXHttp {

/**
 * @brief 客户端请求类(Request)
 */
class HXRequest {
    /**
     * @brief 请求行数据分类
     */
    enum RequestLineDataType {
        RequestType = 0,        // 请求类型
        RequestPath = 1,        // 请求路径
        ProtocolVersion = 2,    // 协议版本
    };

    std::vector<std::string> _requestLine; // 请求行
    std::unordered_map<std::string, std::string> _requestHeaders; // 请求头

    // 请求体
    // bool _haveBody = false;
    // char *_bodyStr = NULL;
    std::optional<std::string> _body;
public:
    /**
     * @brief 解析状态
     */
    enum ParseStatus {
        RecvError = -2,      // ::recv的时候出现错误
        NotHttp = -1,        // 无法以Http协议解析
        ClientOut = 0,       // 客户端断开
        ParseSuccessful = 1, // 解析成功
    };

    explicit HXRequest() : _requestLine()
                         , _requestHeaders()
                         , _body(std::nullopt)
    {}

    /**
     * @brief 解析请求, 如果没有读取完毕, 则会继续`::recv(fd)`直到读取完毕
     * @param fd 客户端套接字
     * @param str 待解析字符串
     * @param strLen sizeof(str)
     * @return `HXHttp::HXRequest::ParseStatus` 解析状态
     */
    int resolutionRequest(int fd, char *str, std::size_t strLen);

    /**
     * @brief 获取请求类型
     * @return 请求类型 (如: "GET", "POST"...)
     * @warning 需要保证`resolutionRequest`为`ParseSuccessful`
     */
    std::string getRequesType() const {
        return _requestLine[RequestLineDataType::RequestType];
    }

    /**
     * @brief 获取请求PATH
     * @return 请求PATH (如: "/", "/home?loli=watasi"...)
     * @warning 需要保证`resolutionRequest`为`ParseSuccessful`
     */
    std::string getRequesPath() const {
        return _requestLine[RequestLineDataType::RequestPath];
    }

    /**
     * @brief 获取请求协议版本
     * @return 请求协议版本 (如: "HTTP/1.1", "HTTP/2.0"...)
     * @warning 需要保证`resolutionRequest`为`ParseSuccessful`
     */
    std::string getRequesProtocolVersion() const {
        return _requestLine[RequestLineDataType::ProtocolVersion];
    }
};

} // namespace HXHttp

#endif // _HX_HXREQUEST_H_