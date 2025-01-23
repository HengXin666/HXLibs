#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2024-7-18 17:40:27
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
#ifndef _HX_ROUTER_SINGLETON_H_
#define _HX_ROUTER_SINGLETON_H_

#include <string>
#include <functional>

#include <HXSTL/coroutine/task/Task.hpp>
#include <HXWeb/router/RouteMapPrefixTree.hpp>

namespace HX { namespace web { namespace server {

template <class T>
class IO;

}}} // namespace HX::web::server

namespace HX { namespace web { namespace router {

/**
 * @brief 路由类: 懒汉单例
 */
class RouterSingleton {
public:
    /**
     * @brief 端点函数返回值
     * @return bool true代表复用(保留)连接, false代表(回复这一次响应后立即)关闭连接
     */
    using EndpointReturnType = HX::STL::coroutine::task::Task<bool>;
protected:
    /// @brief 端点函数
    using EndpointFunc = std::function<EndpointReturnType(
        const HX::web::server::IO<void>&
    )>;

    explicit RouterSingleton();

    RouterSingleton(const RouterSingleton&) = delete;
    RouterSingleton& operator=(const RouterSingleton&) = delete;
public:
    /**
     * @brief 获取路由类单例
     */
    [[nodiscard]] static RouterSingleton& getSingleton() {
        static RouterSingleton router {};
        return router;
    }

    /**
     * @brief 添加端点函数
     * @param requestType 请求类型, 如`GET`, `POST` (全大写)
     * @param path 挂载的PTAH, 如`"/home/{id}"`, 尾部不要`/`
     * @param func 端点函数
     */
    void addEndpoint(
        const std::string& requestType,
        const std::string& path,
        const EndpointFunc& func
    );

    /**
     * @brief 设置路由找不到时候的端点函数
     * @param func 端点函数
     */
    void setErrorEndpointFunc(const EndpointFunc& func) {
        _errorEndpointFunc = func;
    }

    /**
     * @brief 获取路由找不到时候的端点函数
     * @return const 端点函数引用 
     */
    const EndpointFunc& getErrorEndpointFunc() const {
        return _errorEndpointFunc;
    }

    /**
     * @brief 获取该请求类型和URL(PTAH)绑定的端点函数
     * @param requestType 请求类型, 如`GET`, `POST` (全大写)
     * @param path 访问的目标地址, 如`"/home\**?loli=imouto"`, 尾部不要`/`, 会解析为`?`之前的内容
     * @return 存在则返回, 否则返回`getErrorEndpointFunc()`
     */
    EndpointFunc getEndpointFunc(
        const std::string& requestType,
        const std::string& path
    ) const;
protected:
    /**
     * don't use a char * as a key
     * std::string keys are never your bottleneck
     * the performance difference between a char * and a std::string is a myth.
     */
    /// @brief 请求类型 -> URL -> 端点函数 路由映射
    RouteMapPrefixTree<EndpointFunc> _routerRadixTree;

    /// @brief 路由找不到时候的端点函数
    EndpointFunc _errorEndpointFunc;
};

}}} // namespace HX::web::router

#endif // _HX_ROUTER_SINGLETON_H_
