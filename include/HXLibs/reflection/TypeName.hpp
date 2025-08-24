#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-08-24 15:40:35
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
 * @brief 获取类型名称
 * @tparam T 仅 enum / class / struct / union
 * @return constexpr std::string_view 
 */
template <typename T>
    requires(std::is_enum_v<T>
          || std::is_class_v<T>
          || std::is_union_v<T>)
inline constexpr std::string_view getTypeName() noexcept {
#if defined(_MSC_VER)
    constexpr std::string_view funcName = __FUNCSIG__;
#else
    constexpr std::string_view funcName = __PRETTY_FUNCTION__;
#endif
    // return funcName; // debug
#if defined(__clang__)
    auto split = funcName.substr(funcName.rfind("T = ") + sizeof("T = ") - 1);
    auto pos = split.rfind(']');
    split = split.substr(0, pos);
    pos = split.find("<");
    if (pos != std::string_view::npos) {
        split = split.substr(0, pos);
    }
    pos = split.rfind("::");
    if (pos != std::string_view::npos) {
        split = split.substr(pos + 2);
    }
#elif defined(__GNUC__)
    auto split = funcName.substr(funcName.find("[with T = ") + sizeof("[with T = ") - 1);
    auto pos = split.find(';');
    split = split.substr(0, pos);
    pos = split.find("<");
    if (pos != std::string_view::npos) {
        split = split.substr(0, pos);
    }
    pos = split.rfind("::");
    if (pos != std::string_view::npos) {
        split = split.substr(pos + 2);
    }
#elif defined(_MSC_VER)
    auto split = funcName.substr(funcName.rfind("getTypeName<") + sizeof("getTypeName<") - 1);
    auto pos = split.find("<");
    if (pos != std::string_view::npos) {
        split = split.substr(0, pos);
    } else {
        pos = split.find('>');
        split = split.substr(0, pos);
    }
    pos = split.rfind("::");
    if (pos != std::string_view::npos) {
        split = split.substr(pos + 2);
    } else {
        pos = split.find(' ');
        if (pos != std::string_view::npos) {
            split = split.substr(pos + 1);
        }
    }
#else
    static_assert(
        false, 
        "You are using an unsupported compiler. Please use GCC, Clang "
        "or MSVC or switch to the rfl::Field-syntax."
    );
#endif
    // 暂时不支持 Lambda 类型
    // if (split.empty()) {
    //     return "Lambda";
    // }
    return split;
}

} // namespace HX::reflection