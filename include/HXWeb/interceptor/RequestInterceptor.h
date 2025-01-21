#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-01-20 22:06:51
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
#ifndef _HX_REQUEST_INTERCEPTOR_H_
#define _HX_REQUEST_INTERCEPTOR_H_

#include <vector>
#include <functional>

#include <HXWeb/server/IO.h>

namespace HX { namespace web { namespace interceptor {

enum class RequestFlow : bool {
    Block = false,  // 请求被拦截
    Pass = true     // 请求通过
};

/**
 * @brief 请求拦截器
 */
class RequestInterceptor {
    RequestInterceptor() = default;
    RequestInterceptor operator=(RequestInterceptor&&) = delete;
public:
    /**
     * @brief 前向拦截器类型
     * @return RequestFlow 
     */
    using PreHandleFunc = std::function<HX::web::interceptor::RequestFlow(HX::web::server::IO<>&)>;

    /**
     * @brief 获取请求拦截器单例
     * @return const RequestInterceptor& 
     */
    static RequestInterceptor& getRequestInterceptor() {
        static RequestInterceptor requestInterceptor{};
        return requestInterceptor;
    }

    /**
     * @brief 添加一个`请求拦截器`: 在端点前访问
     * @param func 
     */
    void addPreHandle(PreHandleFunc&& func) {
        _preHandleFuncArr.emplace_back(func);
    }

    /**
     * @brief 检查请求是否符合要求, 如果不符合, 就返回ture
     * @param io 
     * @return RequestFlow 
     */
    RequestFlow checkPreHandle(HX::web::server::IO<>& io) const {
        for (auto const& func : _preHandleFuncArr) {
            if (func(io) == RequestFlow::Block) {
                return RequestFlow::Block;
            }
        }
        return RequestFlow::Pass;
    }
private:
    std::vector<PreHandleFunc> _preHandleFuncArr {};
};

}}} // namespace HX::web::interceptor

#endif // !_HX_REQUEST_INTERCEPTOR_H_