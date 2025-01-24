#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-01-22 21:24:10
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
#ifndef _HX_ROUTER_H_
#define _HX_ROUTER_H_

#include <string>
#include <functional>

#include <HXWeb/protocol/http/Http.hpp>
#include <HXWeb/protocol/http/Request.h>
#include <HXWeb/protocol/http/Response.h>
#include <HXWeb/router/RouterTree.hpp>
#include <HXSTL/coroutine/task/Task.hpp>
#include <HXSTL/utils/StringUtils.h>

namespace HX { namespace web { namespace router {

namespace internal {

template <typename T, typename = void>
struct has_before 
    : std::false_type
{};

template <typename T>
struct has_before<T, std::void_t<decltype(std::declval<T>().before(
    std::declval<protocol::http::Request&>(),
    std::declval<protocol::http::Response&>()))>>
    : std::true_type
{};

template <typename T, typename = void>
struct has_after 
    : std::false_type
{};

template <typename T>
struct has_after<T, std::void_t<decltype(std::declval<T>().after(
    std::declval<protocol::http::Request&>(),
    std::declval<protocol::http::Response&>()))>>
    : std::true_type
{};

template <typename T>
constexpr auto has_before_v = has_before<T>::value;

template <typename T>
constexpr auto has_after_v = has_after<T>::value;

} // namespace internal

class Router {
    Router() = default;
    Router& operator=(Router&&) = delete;
public:
    /**
     * @brief 获取路由单例
     * @return Router& 
     */
    [[nodiscard]] static Router& getRouter() {
        static Router router{};
        return router;
    }

    /**
     * @brief 获取路由
     * @param method 
     * @param path 
     * @return EndpointFunc 
     */
    const EndpointFunc& getEndpoint(
        std::string_view method,
        std::string_view path
    ) const {
        return _routerTree.find(
            HX::STL::utils::StringUtil::split(
                path, 
                "/", 
                {std::string{method}}
        ));
    }

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
    void addEndpoint(std::string path, Func endpoint, Interceptors&&... interceptors) {
        if constexpr (sizeof...(Methods) == 1) {
            (_addEndpoint<Methods>(
                std::move(path),
                std::move(endpoint),
                std::forward<Interceptors>(interceptors)...
            ), ...);
        } else {
            (_addEndpoint<Methods>(
                path, 
                endpoint, 
                std::forward<Interceptors>(interceptors)...
            ), ...);
        }
    }

    /**
     * @brief 为服务器添加一个端点
     * @tparam Methods 请求类型, 如`GET`、`POST`
     * @tparam Func 端点函数类型
     * @tparam Interceptors 拦截器类型
     * @param key url, 如"/"、"home/{id}"
     * @param endpoint 端点函数
     * @param interceptors 拦截器
     * @return Router& *this (可链式调用)
     */
    template <protocol::http::HttpMethod... Methods,
        typename Func,
        typename... Interceptors>
    Router& on(std::string path, Func endpoint, Interceptors&&... interceptors) {
        addEndpoint<Methods...>(std::move(path), std::move(endpoint), interceptors...);
        return *this;
    }

private:
    /**
     * @brief 添加路由端点
     * @tparam Method 
     * @tparam Func 
     * @tparam Interceptors 
     * @param path 
     * @param endpoint 
     * @param interceptors 
     */
    template <protocol::http::HttpMethod Method,
        typename Func,
        typename... Interceptors>
    void _addEndpoint(std::string path, Func&& endpoint, Interceptors&&... interceptors) {
        std::function<HX::STL::coroutine::task::Task<>(
            protocol::http::Request &, protocol::http::Response &)>
            realEndpoint =
                [this, endpoint = std::move(endpoint),
                 ... interceptors = interceptors] (protocol::http::Request &req,
                                                  protocol::http::Response &res) mutable
            -> HX::STL::coroutine::task::Task<> {
            static_cast<void>(this);
            bool ok = true;
            (doBefore(interceptors, ok, req, res), ...);
            if (ok) {
                co_await endpoint(req, res);
            }
            ok = true;
            (doAfter(interceptors, ok, req, res), ...);
        };
        auto buildLink = HX::STL::utils::StringUtil::split(
            path, 
            "/",
            {std::string{protocol::http::getMethodStringView(Method)}}
        );
        _routerTree.insert(buildLink, std::move(realEndpoint));
    }

    template <typename T>
    void doBefore(
        T& interceptors, 
        bool& ok, 
        protocol::http::Request& req, 
        protocol::http::Response& res
    ) {
        if constexpr (internal::has_before_v<T>) {
            if (!ok) {
                return;
            }
            ok = interceptors.before(req, res);
        }
    }

    template <typename T>
    void doAfter(
        T& interceptors, 
        bool& ok, 
        protocol::http::Request& req, 
        protocol::http::Response& res
    ) {
        if constexpr (internal::has_after_v<T>) {
            if (!ok) {
                return;
            }
            ok = interceptors.after(req, res);
        }
    }

    RouterTree _routerTree {};
};

}}} // namespace HX::web::router

#endif // !_HX_ROUTER_H_