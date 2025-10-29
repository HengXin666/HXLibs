#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-10-30 00:20:36
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

#include <HXLibs/net/protocol/proxy/Proxy.hpp>
#include <HXLibs/net/protocol/http/Request.hpp>
#include <HXLibs/net/protocol/http/Response.hpp>
#include <HXLibs/net/protocol/url/UrlParse.hpp>

namespace HX::net {

class HttpProxy : public Proxy<HttpProxy> {
public:
    using ProxyBase = Proxy<HttpProxy>;
    using ProxyBase::ProxyBase;
    using IOType = HttpIO;

    coroutine::Task<> connect(std::string_view url, std::string_view targetUrl) {
        UrlInfoExtractor parser(targetUrl);
        Request req{_io};
        
        if (parser.getService() == "https") {
            req.setReqLine<HttpMethod::CONNECT>(
                std::string{parser.getHostname()}
                + ":"
                + std::to_string(UrlParse::getProtocolPort(parser.getService()))
            );
        } else {
            req.setReqLine<HttpMethod::GET>(url);
        }

        req.tryAddHeaders("Host", UrlParse::extractDomainName(std::string{url}));
        req.tryAddHeaders("Accept", "*/*");
        req.tryAddHeaders("Connection", "keep-alive");
        req.tryAddHeaders("User-Agent", "HXLibs/1.0");

        using namespace HX::utils;
        co_await req.sendHttpReq<decltype(3_s)>();

        Response res{_io};
        if (!co_await res.parserRes<decltype(3_s)>()) {
            throw std::runtime_error{"Response parser error"};
        }
        if (res.getStatusCode() != "200") {
            throw std::runtime_error("HTTP Proxy connect failed");
        }
        log::hxLog.info(res.makeResponseData());
    }
};

} // namespace HX::net