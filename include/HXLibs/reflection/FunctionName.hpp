#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2026-06-24 16:36:44
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

#include <string_view>

namespace HX::reflection {

/**
 * @brief 获取函数名称 (含命名空间)
 * @return constexpr std::string_view 
 */
template <auto T>
inline constexpr std::string_view getFuncNameAndNamespace() noexcept {
#if defined(_MSC_VER)
    constexpr std::string_view funcName = __FUNCSIG__;
#else
    constexpr std::string_view funcName = __PRETTY_FUNCTION__;
#endif
#if defined(__clang__)
    auto split = funcName.substr(funcName.rfind("T = &") + sizeof("T = &") - 1);
    auto pos = split.rfind(']');
    split = split.substr(0, pos);
    pos = split.find('<');
    if (pos != std::string_view::npos) {
        split = split.substr(0, pos);
    }
    pos = split.rfind("::");
    if (pos != std::string_view::npos) {
        pos = split.rfind(' ');
        split = split.substr(pos + 1);
    }
#elif defined(__GNUC__)
    auto split = funcName.substr(funcName.find("[with auto T = ") + sizeof("[with auto T = ") - 1);
    auto pos = split.find(';');
    split = split.substr(0, pos);
    pos = split.find('<');
    if (pos != std::string_view::npos) {
        split = split.substr(0, pos);
    }
#elif defined(_MSC_VER)
    auto split = funcName.substr(funcName.rfind("getFuncNameAndNamespace<") + sizeof("getTypeNameAndNamespace<") - 1);
    auto pos = split.find("__cdecl ");
    split = split.substr(pos + sizeof("__cdecl ") - 1);
    pos = split.find("(");
    split = split.substr(0, pos);
#else
    static_assert(
        false, 
        "You are using an unsupported compiler. Please use GCC, Clang "
        "or MSVC or switch to the rfl::Field-syntax."
    );
#endif
    return split;
}

/**
 * @brief 获取函数名称
 * @return constexpr std::string_view 
 */
template <auto T>
inline constexpr std::string_view getFuncName() noexcept {
    constexpr auto npFuncName = getFuncNameAndNamespace<T>();
    if (auto pos = npFuncName.rfind("::"); pos != std::string_view::npos) {
        return npFuncName.substr(pos + sizeof("::") - 1);
    }
    return npFuncName;
}

} // namespace HX::reflection
