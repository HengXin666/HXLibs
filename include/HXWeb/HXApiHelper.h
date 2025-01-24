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
#ifndef _HX_API_HELPER_H_
#define _HX_API_HELPER_H_

#include <HXWeb/router/Router.hpp>
#include <HXWeb/router/RouterSingleton.h>
#include <HXWeb/router/RequestParsing.h>
#include <HXWeb/server/IO.h>
#include <HXWeb/server/Server.h>
#include <HXWeb/protocol/http/Request.h>
#include <HXWeb/protocol/http/Response.h>
#include <HXSTL/coroutine/task/Task.hpp>
#include <HXSTL/utils/StringUtils.h>

/* 简化用户编写的 API 宏 */

// 定义常用的请求类型
#define API_GET "GET"
#define API_POST "POST"
#define API_PUT "PUT"
#define API_DELETE "DELETE"

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
 * @brief 结束路由单例使用
 */
#define ROUTER_END  \
    ;               \
    return 0;       \
}()

/**
 * @brief 定义一个端点, 其中形参定义了`io`(HX::web::server::IO)
 * @param METHOD 请求类型, 如"GET"
 * @param PATH 端点对应的路径, 如"/home/{id}"
 * @param FUNC_NAME 端点函数名称
 */
#define ENDPOINT_BEGIN(METHOD, PATH, FUNC_NAME) \
const int _HX_endpoint_##FUNC_NAME = []() -> int { \
    std::string templatePath = PATH; \
    HX::web::router::RouterSingleton::getSingleton().addEndpoint( \
        METHOD,\
        templatePath,\
        [=](const HX::web::server::IO<>& io) -> HX::web::router::RouterSingleton::EndpointReturnType { \
            static_cast<void>(io); // 让 -Wunused-parameter 闭嘴

/**
 * @brief 结束端点的定义
 */
#define ENDPOINT_END \
        co_return true; \
        } \
    );\
    return 0;\
}()

/**
 * @brief 设置路由失败时候的端点函数, 其中形参定义了`io`(HX::web::server::IO)
 */
#define ERROR_ENDPOINT_BEGIN \
HX::web::router::RouterSingleton::getSingleton().setErrorEndpointFunc([=](const HX::web::server::IO<>& io) -> HX::web::router::RouterSingleton::EndpointReturnType { \
    static_cast<void>(io);

/**
 * @brief 结束设置路由失败时候的端点函数的定义
 */
#define ERROR_ENDPOINT_END \
})

/**
 * @brief 将数据写入响应体, 并且指定状态码
 * @param CODE 状态码 (如`200`)
 * @param DATA 写入`响应体`的字符串数据
 * @param __VA_ARGS__ 响应类型(第一个是响应类型(必选), 第二个是响应编码(可选)), 
 * 如 `"text/html", "UTF-8"`, `"image/x-icon"`
 */
#define RESPONSE_DATA(CODE, DATA, ...) \
io.getResponse().setResponseLine(HX::web::protocol::http::Response::Status::CODE_##CODE) \
  .setBodyData(DATA) \
  .setContentType(__VA_ARGS__)

/**
 * @brief 使用`Transfer-Encoding`分块编码响应, 以传输文件
 * @param CODE 状态码 (如`200`)
 * @param PATH 需要传输的文件的文件路径
 * @param __VA_ARGS__ 响应类型(第一个是响应类型(必选), 第二个是响应编码(可选)), 
 * 如 `"text/html", "UTF-8"`, `"image/x-icon"`
 */
#define RESPONSE_FILE(CODE, PATH, ...) \
io.getResponse().setResponseLine(HX::web::protocol::http::Response::Status::CODE_##CODE) \
                .setContentType(__VA_ARGS__); \
co_await io.sendResponseWithChunkedEncoding(PATH)

/**
 * @brief 设置状态码 (接下来可以继续操作)
 * @param 状态码 (如`200`)
 */
#define RESPONSE_STATUS(CODE) \
io.getResponse().setResponseLine(HX::web::protocol::http::Response::Status::CODE_##CODE)

/**
 * @brief 开始解析请求路径参数
 * @warning 接下来请使用`PARSE_PARAM`宏
 */
#define START_PARSE_PATH_PARAMS \
static const auto wildcarIndexArr = HX::web::router::RequestTemplateParsing::getPathWildcardAnalysisArr(templatePath); \
auto pathSplitArr = HX::STL::utils::StringUtil::split(io.getRequest().getPureRequesPath(), "/")

/**
 * @brief 用于解析指定索引的路径参数, 并将其转换为指定类型的变量
 * @param INDEX 模版路径的第几个通配参数 (从`0`开始计算)
 * @param TYPE 需要解析成的类型, 如`bool`/`int32_t`/`double`/`std::string`/`std::string_view`等
 * @param NAME 变量名称
 * @param __VA_ARGS__ (`bool`类型): 是否复用连接 (默认复用)
 * @return NAME, 类型是`std::optional<TYPE>`
 * @warning 如果解析不到(出错), 则会直接返回错误给客户端
 */
#define PARSE_PARAM(INDEX, TYPE, NAME, ...) \
auto NAME = HX::web::router::TypeInterpretation<TYPE>::wildcardElementTypeConversion(pathSplitArr[static_cast<std::size_t>(wildcarIndexArr[static_cast<std::size_t>(INDEX)])]); \
if (!NAME) { \
    RESPONSE_DATA(400, "Missing PATH parameter '"#NAME"'", "application/json", "UTF-8"); \
    co_return __VA_OPT__(void)(0) __VA_OPT__(, __VA_ARGS__); \
} // C++20才引入的 __VA_OPT__

/**
 * @brief 解析多级通配符的宏, 如 `/home/ **` 这种
 * @param NAME 解析结果字符串(`std::string`)的变量名称
 */
#define PARSE_MULTI_LEVEL_PARAM(NAME) \
static const auto UWPIndex = HX::web::router::RequestTemplateParsing::getUniversalWildcardPathBeginIndex(templatePath); \
std::string NAME = io.getRequest().getPureRequesPath().substr(UWPIndex)

/**
 * @brief 解析查询参数键值对Map (解析如: `?name=loli&awa=ok&hitori`)
 * @param NAME 键值对Map的变量名
 */
#define GET_PARSE_QUERY_PARAMETERS(NAME) \
auto NAME = io.getRequest().getParseQueryParameters()

/**
 * @brief 路由绑定, 将控制器绑定到路由
 * @param NAME 控制器类名
 */
#define ROUTER_BIND(NAME) \
{static_cast<void>(NAME {});}

#endif // _HX_API_HELPER_H_
