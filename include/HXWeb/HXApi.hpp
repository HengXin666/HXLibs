#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2024-7-27 22:42:16
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
#ifndef _HX_API_H_
#define _HX_API_H_

#include <HXWeb/router/Router.hpp>
#include <HXWeb/router/RequestParsing.h>
#include <HXWeb/server/IO.h>
#include <HXWeb/server/Server.h>
#include <HXWeb/protocol/http/Request.h>
#include <HXWeb/protocol/http/Response.h>
#include <HXSTL/coroutine/task/Task.hpp>
#include <HXSTL/utils/StringUtils.h>

#if defined(__GNUC__) || defined(__clang__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wpedantic"
#   pragma GCC diagnostic ignored "-Wunused-parameter"
#elif defined(_MSC_VER)
#   pragma warning(push)
#   pragma warning(disable : 4100 4101)
#endif

/* 简化用户编写的 API 宏 */

#define _EXPAND_2(x, y, z) x##y##z
#define _EXPAND(x, y, z) _EXPAND_2(x, y, z)
#define _UNIQUE_ID(prefix, suffix) _EXPAND(prefix, __LINE__, suffix)

/**
 * @brief 全局路由单例, 可链式调用
 */
#define ROUTER                                  \
const int _UNIQUE_ID(_HXWeb_, _endpoint_) = []{ \
    using namespace HX::web::protocol::http;    \
    using namespace HX::STL::coroutine::task;   \
    HX::web::router::Router::getRouter()

/**
 * @brief 定义标准的端点, 请求使用`req`变量, 响应使用`res`变量
 */
#define ENDPOINT                            \
(                                           \
    HX::web::protocol::http::Request& req,  \
    HX::web::protocol::http::Response& res  \
) -> HX::STL::coroutine::task::Task<>

/**
 * @brief 结束路由单例使用
 */
#define ROUTER_END  \
    ;               \
    return 0;       \
}()

/**
 * @brief 设置路由失败时候的端点函数
 */
#define ROUTER_ERROR_ENDPOINT \
HX::web::router::Router::getRouter().setErrorEndpointFunc

/**
 * @brief 将数据写入响应体, 并且指定状态码
 * @param CODE 状态码 (如`200`)
 * @param DATA 写入`响应体`的字符串数据
 * @param TYPE 响应类型, 如`html, json`
 */
#define RESPONSE_DATA(CODE, TYPE, DATA)                             \
res.setResponseLine(HX::web::protocol::http::Status::CODE_##CODE)   \
   .setContentType(HX::web::protocol::http::ResContentType::TYPE)   \
   .setBodyData(DATA)

/**
 * @brief 设置状态码 (接下来可以继续操作)
 * @param 状态码 (如`200`)
 */
#define RESPONSE_STATUS(CODE) \
res.setResponseLine(HX::web::protocol::http::Status::CODE_##CODE)

/**
 * @brief 用于解析指定索引的路径参数, 并将其转换为指定类型的变量
 * @param INDEX 模版路径的第几个通配参数 (从`0`开始计算)
 * @param TYPE 需要解析成的类型, 如`bool`/`int32_t`/`double`/`std::string`/`std::string_view`等
 * @param NAME 变量名称
 * @return NAME, 类型是`std::optional<TYPE>`
 * @warning 如果解析不到(出错), 则会直接返回错误给客户端
 */
#define PARSE_PARAM(INDEX, TYPE, NAME)                                                  \
auto NAME = HX::web::router::TypeInterpretation<TYPE>::wildcardElementTypeConversion(   \
    req.getPathParam(INDEX)                                                             \
);                                                                                      \
if (!NAME) {                                                                            \
    RESPONSE_DATA(400, json, "Missing PATH parameter '"#NAME"'");                       \
    co_return;                                                                          \
}

/**
 * @brief 解析多级通配符的宏, 如 `/home/ **` 这种
 * @param NAME 解析结果字符串(`std::string`)的变量名称
 */
#define PARSE_MULTI_LEVEL_PARAM(NAME)                           \
std::string NAME = std::string{req.getUniversalWildcardPath()}  \

/**
 * @brief 解析查询参数键值对Map (解析如: `?name=loli&awa=ok&hitori`)
 * @param NAME 键值对Map的变量名
 */
#define GET_PARSE_QUERY_PARAMETERS(NAME) \
auto NAME = req.getParseQueryParameters()

/**
 * @brief 路由绑定, 将控制器绑定到路由
 * @param NAME 控制器类名
 */
#define ROUTER_BIND(NAME) \
{static_cast<void>(NAME {});}

#endif // _HX_API_H_
