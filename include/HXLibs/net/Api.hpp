#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-10-31 23:37:15
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
 */

#include <HXLibs/net/server/HttpServer.hpp>

namespace HX::net {

/**
 * @brief 基础控制器
 */
class BaseController {
    HttpServer& _server;
public:
    BaseController(HttpServer& server)
        : _server{server}
    {}

    /**
     * @brief 为服务器添加一个端点
     * @tparam Methods 请求类型, 如`GET`、`POST`; 如果不写, 则全部注册!
     * @tparam Func 端点函数类型
     * @tparam Interceptors 拦截器类型
     * @param key url, 如"/"、"home/{id}"
     * @param endpoint 端点函数
     * @param interceptors 拦截器
     * @return HttpServer& 可链式调用
     */
    template <HttpMethod... Methods, typename Func, typename... Interceptors>
    HttpServer& addEndpoint(std::string_view path, Func endpoint, Interceptors&&... interceptors) {
        return _server.addEndpoint<Methods...>(
            std::move(path),
            std::move(endpoint),
            std::forward<Interceptors>(interceptors)...
        );
    }
};

} // namespace HX::net
