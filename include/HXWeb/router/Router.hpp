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

#include <functional>

#include <HXWeb/protocol/http/Http.hpp>
#include <HXWeb/protocol/http/Request.h>
#include <HXWeb/protocol/http/Response.h>
#include <HXWeb/router/RouterTree.hpp>
#include <HXWeb/router/RequestParsing.h>
#include <HXSTL/coroutine/task/Task.hpp>
#include <HXSTL/utils/StringUtils.h>

#include <HXprint/print.h>

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
        auto findLink = HX::STL::utils::StringUtil::split<std::string_view>(
                path, 
                "/", 
                {method}
        );
        if (path.back() == '/') { // 为了适配 /** 的情况
            findLink.emplace_back("");
            return _routerTree.find<true>(findLink);
        }
        return _routerTree.find(findLink);
    }

    /**
     * @brief 设置路由找不到时候的端点函数
     * @tparam Func 
     * @tparam Interceptors 
     * @param endpoint 端点函数
     * @param interceptors 拦截器
     */
    template <typename Func, typename... Interceptors>
    void setErrorEndpointFunc(Func&& endpoint, Interceptors&&... interceptors) {
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
        _routerTree.setNotFoundHandler(std::move(realEndpoint));
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
    void addEndpoint(std::string_view path, Func endpoint, Interceptors&&... interceptors) {
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
    Router& on(std::string_view path, Func&& endpoint, Interceptors&&... interceptors) {
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
    void _addEndpoint(std::string_view path, Func&& endpoint, Interceptors&&... interceptors) {
        using namespace std::string_view_literals;
        auto isResolvePathVariable = path.find_first_of("{"sv) != std::string_view::npos;
        auto isParseWildcardPath = path.find("/**"sv) != std::string_view::npos;
        std::function<HX::STL::coroutine::task::Task<>(
            protocol::http::Request &, protocol::http::Response &)> realEndpoint;
        switch (isResolvePathVariable | (isParseWildcardPath << 1)) {
            case 0x0: // 不解析任何参数
                realEndpoint = [this, endpoint = std::move(endpoint),
                                ... interceptors = interceptors](
                                   protocol::http::Request &req,
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
                break;
            case 0x1: { // 仅解析{}参数
                auto indexArr = HX::web::router::RequestTemplateParsing::getPathWildcardAnalysisArr(path);
                realEndpoint = [this, endpoint = std::move(endpoint),
                                indexArr = std::move(indexArr),
                                ... interceptors = interceptors](
                                   protocol::http::Request &req,
                                   protocol::http::Response &res) mutable
                    -> HX::STL::coroutine::task::Task<> {
                    static_cast<void>(this);
                    bool ok = true;
                    auto pureRequesPath = req.getPureRequesPath();
                    auto pathSplitArr = HX::STL::utils::StringUtil::split<std::string_view>(pureRequesPath, "/");
                    std::vector<std::string_view> wildcarArr;
                    for (auto idx : indexArr) {
                        wildcarArr.emplace_back(pathSplitArr[idx]);
                    }
                    HX::web::protocol::http::PathVariable pathVar {req, wildcarArr};
                    (doBefore(interceptors, ok, req, res), ...);
                    if (ok) {
                        co_await endpoint(req, res);
                    }
                    ok = true;
                    (doAfter(interceptors, ok, req, res), ...);
                };
                break;
            }
            case 0x2: {// 仅解析通配符
                auto UWPIndex = path.find("/**"sv);
                realEndpoint = [this, endpoint = std::move(endpoint), UWPIndex,
                                ... interceptors = interceptors](
                                   protocol::http::Request &req,
                                   protocol::http::Response &res) mutable
                    -> HX::STL::coroutine::task::Task<> {
                    static_cast<void>(this);
                    bool ok = true;
                    auto pureRequesPath = req.getPureRequesPath();
                    std::string_view pureRequesPathView = pureRequesPath;
                    HX::web::protocol::http::PathVariable pathVar {
                        req,
                        pureRequesPathView.substr(UWPIndex)
                    };
                    (doBefore(interceptors, ok, req, res), ...);
                    if (ok) {
                        co_await endpoint(req, res);
                    }
                    ok = true;
                    (doAfter(interceptors, ok, req, res), ...);
                };
                break;
            }
            case 0x3: { // 全都要解析
                auto indexArr = HX::web::router::RequestTemplateParsing::getPathWildcardAnalysisArr(path);
                auto UWPIndex = HX::web::router::RequestTemplateParsing::getUniversalWildcardPathBeginIndex(path);
                realEndpoint = [this, endpoint = std::move(endpoint),
                                indexArr = std::move(indexArr), UWPIndex,
                                ... interceptors = interceptors](
                                   protocol::http::Request &req,
                                   protocol::http::Response &res) mutable
                    -> HX::STL::coroutine::task::Task<> {
                    static_cast<void>(this);
                    bool ok = true;
                    auto pureRequesPath = req.getPureRequesPath();
                    std::string_view pureRequesPathView = pureRequesPath;
                    auto pathSplitArr = HX::STL::utils::StringUtil::splitWithPos<std::string_view>(pureRequesPathView, "/");
                    std::vector<std::string_view> wildcarArr;
                    for (auto idx : indexArr) {
                        wildcarArr.emplace_back(pathSplitArr[idx].second);
                    }
                    HX::web::protocol::http::PathVariable pathVar {
                        req,
                        wildcarArr,
                        pathSplitArr.size() > UWPIndex 
                            ? pureRequesPathView.substr(pathSplitArr[UWPIndex].first)
                            : ""sv
                    };
                    (doBefore(interceptors, ok, req, res), ...);
                    if (ok) {
                        co_await endpoint(req, res);
                    }
                    ok = true;
                    (doAfter(interceptors, ok, req, res), ...);
                };
                break;
            }
        }
        auto buildLink = HX::STL::utils::StringUtil::split<std::string_view>(
            path, 
            "/",
            {protocol::http::getMethodStringView(Method)}
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