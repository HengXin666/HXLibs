#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2026-06-23 00:27:31
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

#include <HXLibs/meta/Wrap.hpp>
#include <HXLibs/rpc/tools/RpcOutputArg.hpp>

namespace HX::rpc {

template <typename Res, typename... OutArgs>
struct RpcHttpResponse {
    Res res;
    RpcOutputArgsTuple<OutArgs...> outputArgs{};

    using OutputAtIdx = MakeRpcOutputArgsIdx<OutArgs...>;

    template <typename... Ts>
        requires (std::convertible_to<Ts, meta::RemoveCvRefType<OutArgs>> && ...)
    void setToOutArgs(Ts&&... ts) {
        auto tp = std::forward_as_tuple(std::forward<Ts>(ts)...);
        [&] <std::size_t... Idx> (std::index_sequence<Idx...>) constexpr {
            outputArgs = RpcOutputArgsForwardTupleType<Ts&&...>{std::get<Idx>(tp)...};
        }(OutputAtIdx{});
    }

    template <typename... Ts>
        requires (std::convertible_to<Ts, OutArgs> && ...)
    void moveToOutArgs(Ts&&... ts) {
        auto tp = std::forward_as_tuple(std::forward<Ts>(ts)...);
        [&] <std::size_t... Idx> (std::index_sequence<Idx...>) constexpr {
            RpcOutputArgsForwardTupleType<Ts&&...>{std::get<Idx>(tp)...} = std::move(outputArgs);
        }(OutputAtIdx{});
    }
};

} // namespace HX::rpc
