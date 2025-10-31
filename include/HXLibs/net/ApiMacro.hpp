/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-09 11:08:08
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

#include <HXLibs/net/Api.hpp>

/**
 * @brief 定义标准的端点, 请求使用`req`变量, 响应使用`res`变量
 */
#define ENDPOINT (                           \
    [[maybe_unused]] HX::net::Request& req,  \
    [[maybe_unused]] HX::net::Response& res  \
) -> HX::coroutine::Task<>

/**
 * @brief 定义一个控制器类
 */
#define HX_CONTROLLER(__NAME__)                                                \
    class __NAME__ : public HX::net::BaseController {                          \
        template <typename T, typename... Args>                                \
        friend void HX::net::addController(HX::net::HttpServer&, Args&&...);   \
                                                                               \
    public:                                                                    \
        __NAME__(HX::net::HttpServer& server)                                  \
            : BaseController {server}

/**
 * @brief 定义控制器端点, 可以定义参数进行依赖注入
 * @param ... 依赖注入的参数
 */
#define HX_ENDPOINT_MAIN(...)                   \
        }                                       \
    private:                                    \
        void dependencyInjection(__VA_ARGS__)
