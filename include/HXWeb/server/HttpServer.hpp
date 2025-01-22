#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-01-22 20:58:29
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
#ifndef _HX_HTTP_SERVER_H_
#define _HX_HTTP_SERVER_H_

#include <string>

#include <HXWeb/protocol/http/Http.hpp>
#include <HXWeb/router/Router.hpp>

namespace HX { namespace web { namespace server {

class HttpServer {
public:
    /**
    * @brief 为服务器添加一个端点
    * @tparam Methods 请求类型, 如`GET`、`POST`
    * @tparam Func 端点函数类型
    * @tparam Interceptors 拦截器类型
    * @param key url, 如"/"、"home/{id}"
    * @param endpoint 端点函数
    * @param interceptors 拦截器
    */
    template <protocol::http::HttpMethod... Methods,
        typename Func,
        typename... Interceptors>
    void addEndpoint(std::string key, Func endpoint, Interceptors&&... interceptors) {
        if constexpr (sizeof...(Methods) == 1) {
            (_router.addEndpoint<Methods>(
                std::move(key),
                std::move(endpoint),
                std::forward<Interceptors>(interceptors)...
            ), ...);
        } else {
            (_router.addEndpoint<Methods>(
                key, 
                endpoint, 
                std::forward<Interceptors>(interceptors)...
            ), ...);
        }
    }

private:
    router::Router _router;
};

}}} // namespace HX::web::sever

#endif // !_HX_HTTP_SERVER_H_