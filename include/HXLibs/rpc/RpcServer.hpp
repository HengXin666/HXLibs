#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2026-06-22 22:50:33
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

#include <HXLibs/net/server/HttpServer.hpp>
#include <HXLibs/rpc/tools/FuncName.hpp>
#include <HXLibs/rpc/tools/RpcOutputArg.hpp>
#include <HXLibs/rpc/protocol/http/RpcHttpRequest.hpp>
#include <HXLibs/rpc/protocol/http/RpcHttpResponse.hpp>

namespace HX::rpc {

class RpcServer {
public:
    // @todo 配置
    explicit RpcServer(uint16_t port)
        : _server{port}
    {}

    // @todo 更好的启动方案
    net::HttpServer& getServer() {
        return _server;
    }

    void setPrefix(std::string prefix) {
        _prefix = std::move(prefix);
        if (_prefix.back() != '/') {
            _prefix += '/';
        }
    }

    template <auto Func>
    RpcServer& bind() {
        [&] <typename Res, typename... Args> (FuncName<Res, Args...>) constexpr {
            if constexpr (coroutine::Awaitable<Res>) {
                _server.addEndpoint(
                    _prefix + std::string{reflection::getFuncName<Func>()},
                    [](
                        net::HttpRequest<net::HttpIO>& req,
                        net::HttpResponse<net::HttpIO>& res
                    ) -> coroutine::Task<> {
                        RpcHttpRequest<Func, meta::RemoveCvRefType<Args>...> rpcReq{};
                        reflection::fromJson(rpcReq, co_await req.parseBody());
                        RpcHttpResponse<coroutine::AwaiterReturnType<Res>, Args...> rpcRes{
                            co_await [&] <std::size_t... Idx> (
                                std::index_sequence<Idx...>
                            ) -> coroutine::Task<coroutine::AwaiterReturnType<Res>> {
                                co_return co_await Func(std::get<Idx>(rpcReq.params)...);
                            }(std::make_index_sequence<sizeof...(Args)>{})
                        };
                        [&] <std::size_t... Idx> (std::index_sequence<Idx...>) constexpr {
                            rpcRes.setToOutArgs(std::forward<Args>(std::get<Idx>(rpcReq.params))...);
                        }(std::make_index_sequence<sizeof...(Args)>{});
                        std::string json{};
                        reflection::toJson(rpcRes, json);
                        co_await res.setStatusAndContent(net::Status::CODE_200, json)
                                    .sendRes();
                    }
                );
            } else {                
                _server.addEndpoint(
                    _prefix + std::string{reflection::getFuncName<Func>()},
                    [](
                        net::HttpRequest<net::HttpIO>& req,
                        net::HttpResponse<net::HttpIO>& res
                    ) -> coroutine::Task<> {
                        RpcHttpRequest<Func, meta::RemoveCvRefType<Args>...> rpcReq{};
                        reflection::fromJson(rpcReq, co_await req.parseBody());
                        RpcHttpResponse<Res, Args...> rpcRes{
                            [&] <std::size_t... Idx> (std::index_sequence<Idx...>) constexpr {
                                return Func(std::get<Idx>(rpcReq.params)...);
                            }(std::make_index_sequence<sizeof...(Args)>{})
                        };
                        [&] <std::size_t... Idx> (std::index_sequence<Idx...>) constexpr {
                            rpcRes.setToOutArgs(std::forward<Args>(std::get<Idx>(rpcReq.params))...);
                        }(std::make_index_sequence<sizeof...(Args)>{});
                        std::string json{};
                        reflection::toJson(rpcRes, json);
                        co_await res.setStatusAndContent(net::Status::CODE_200, json)
                                    .sendRes();
                    }
                );
            }
        }(FuncName{Func});
        return *this;
    }
private:
    net::HttpServer _server;
    std::string _prefix = "/rpc/";
};

} // namespace HX::rpc
