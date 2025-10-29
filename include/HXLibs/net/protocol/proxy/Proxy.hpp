#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-22 14:21:35
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

#include <HXLibs/net/socket/IO.hpp>

namespace HX::net {

/**
 * @brief 代理协议基类 CRTP
 * @tparam T 
 */
template <typename T>
class Proxy {
public:
    using Type = T;

    Proxy(HttpIO& io)
        : _io{io}
    {}

    coroutine::Task<> connect(std::string_view url, std::string_view targetUrl) {
        co_await static_cast<T*>(this)->connect(url, targetUrl);
    }
protected:
    HttpIO& _io;
};

/**
 * @brief 无代理, 表示不使用代理
 */
class NoneProxy : public Proxy<NoneProxy> {
public:
    using ProxyBase = Proxy<NoneProxy>;
    using ProxyBase::ProxyBase;
    using IOType = IO;

    coroutine::Task<> connect(std::string_view, std::string_view) noexcept {
        co_return;
    }
};

/**
 * @brief 获取代理协议使用的 IO 类型
 * @tparam T 
 */
template <typename T>
using GetIOType = typename T::IOType;

} // namespace HX::net
