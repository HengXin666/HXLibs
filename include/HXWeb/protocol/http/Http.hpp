#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2024-08-31 21:12:33
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
#ifndef _HX_HTTP_H_
#define _HX_HTTP_H_

#include <string_view>

namespace HX { namespace web { namespace protocol { namespace http {

class Http {};

enum class HttpMethod {
    NIL = 0,
    GET,
    HEAD,
    POST,
    PUT,
    TRACE,
    PATCH,
    CONNECT,
    OPTIONS,
    DEL,
};

inline constexpr auto GET = HttpMethod::GET;
inline constexpr auto POST = HttpMethod::POST;
inline constexpr auto DEL = HttpMethod::DEL;
inline constexpr auto HEAD = HttpMethod::HEAD;
inline constexpr auto PUT = HttpMethod::PUT;
inline constexpr auto CONNECT = HttpMethod::CONNECT;

inline constexpr std::string_view getMethodStringView(HttpMethod const mthd) {
    using namespace std::string_view_literals;
    switch (mthd) {
    case HttpMethod::DEL:     return "DELETE"sv;
    case HttpMethod::GET:     return "GET"sv;
    case HttpMethod::HEAD:    return "HEAD"sv;
    case HttpMethod::POST:    return "POST"sv;
    case HttpMethod::PUT:     return "PUT"sv;
    case HttpMethod::PATCH:   return "PATCH"sv;
    case HttpMethod::CONNECT: return "CONNECT"sv;
    case HttpMethod::OPTIONS: return "OPTIONS"sv;
    case HttpMethod::TRACE:   return "TRACE"sv;
    default:                  return "NIL"sv;
    }
}

inline constexpr std::string_view CRLF{"\r\n", 2};

}}}} // namespace HX::web::protocol::http

#endif // !_HX_HTTP_H_