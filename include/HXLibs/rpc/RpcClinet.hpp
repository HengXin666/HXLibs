#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2026-06-22 22:57:28
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

#include <HXLibs/net/client/HttpClient.hpp>
#include <HXLibs/rpc/tools/FuncName.hpp>
#include <HXLibs/rpc/protocol/http/RpcHttpRequest.hpp>
#include <HXLibs/rpc/protocol/http/RpcHttpResponse.hpp>

namespace HX::rpc {

template <typename Proxy>
class RpcClient {
public:
    // todo 服务发现 server
    explicit RpcClient(
        net::HttpClientOptions<Proxy> config,
        std::shared_ptr<coroutine::EventLoop> loop = nullptr
    )
        : _cli{config, std::move(loop)}
    {}

    ~RpcClient() noexcept {
        _cli.close();
    }

    void setBaseUrl(std::string baseUrl) {
        _baseUrl = std::move(baseUrl);
        if (_baseUrl.back() != '/') {
            _baseUrl += '/';
        }
    }

    template <
        auto Func,
        typename Res = typename decltype(FuncName{Func})::ResType,
        typename... Args
    >
    coroutine::Task<coroutine::RemoveAwaiterWrapType<Res>> req(Args&&... args) {
#if 0
        [] <typename... Ts> (meta::Wrap<Ts...>) {
            constexpr auto IsCntSame = sizeof...(Ts) == sizeof...(Args);
            static_assert(IsCntSame, "incorrect number of parameters");
            if constexpr (IsCntSame) {
                static_assert((std::convertible_to<Args&&, Ts> && ...), "incorrect parameter type");
            }
        }(typename decltype(Func)::ArgsType{});
#endif
        static_assert(std::is_invocable_r_v<Res, decltype(Func), Args&&...>,
            "RPC function cannot be called with these arguments");

        std::string json;
        [&] <typename T, typename... Ts> (FuncName<T, Ts...>) constexpr {
            RpcHttpRequest<Func, meta::RemoveCvRefType<Ts>...> req{
                {meta::RemoveCvRefType<Ts>(args)...}, std::string{reflection::getFuncName<Func>()}
            };
            reflection::toJson(req, json);
        }(FuncName{Func});
        auto resTry = co_await _cli.coPost(
            _baseUrl + std::string{reflection::getFuncName<Func>()},
            std::move(json),
            net::JSON
        );
        if (!resTry) [[unlikely]] {
            resTry.rethrow();
        }
        if constexpr (coroutine::Awaitable<Res>) {
            RpcHttpResponse<coroutine::AwaiterReturnType<Res>, Args&&...> res{};
            reflection::fromJson(res, resTry.get().body);
            res.moveToOutArgs(std::forward<Args>(args)...);
            co_return {std::move(res.res)};
        } else {
            RpcHttpResponse<Res, Args&&...> res{};
            reflection::fromJson(res, resTry.get().body);
            res.moveToOutArgs(std::forward<Args>(args)...);
            co_return {std::move(res.res)};
        }
    }
private:
    net::HttpClient<Proxy> _cli;
    std::string _baseUrl;
};

} // namespace HX::rpc
