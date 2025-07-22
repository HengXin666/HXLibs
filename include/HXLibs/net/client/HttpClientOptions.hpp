#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-09 21:44:22
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
#ifndef _HX_HTTP_CLIENT_OPTIONS_H_
#define _HX_HTTP_CLIENT_OPTIONS_H_

#include <string>

#include <HXLibs/net/protocol/http/Http.hpp>
#include <HXLibs/net/protocol/proxy/Proxy.hpp>
#include <HXLibs/utils/TimeNTTP.hpp>

namespace HX::net {

template <typename T>
    requires(std::is_same_v<T, typename T::ProxyBase::Type>)
struct ProxyType {
    using Type = T;
    std::string url = {};

    operator std::string() const noexcept {
        return url;
    }

    std::string get() const noexcept {
        return *this;
    }
};

template <typename Timeout = decltype(utils::operator""_ms<'5', '0', '0', '0'>()), typename Proxy = Socks5Proxy>
    requires(requires { Timeout::Val; })
struct HttpClientOptions {
    // 代理地址
    ProxyType<Proxy> proxy = {};

    // 超时时间
    Timeout timeout = Timeout{}; // 5000ms
};

} // namespace HX::net

#endif // !_HX_HTTP_CLIENT_OPTIONS_H_