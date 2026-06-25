#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2026-06-23 00:17:08
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

namespace HX::rpc {

template <typename Res, typename... Args>
struct FuncName {
    using ResType = Res;
    using ArgsType = meta::Wrap<Args...>;
    using FuncType = Res(Args...);
    using FuncPtrType = Res(*)(Args...);

    constexpr FuncName(FuncPtrType fp)
        : _fp{fp}
    {}

    constexpr FuncPtrType operator()() const noexcept { return _fp; }

    FuncPtrType _fp{};
};

} // namespace HX::rpc
