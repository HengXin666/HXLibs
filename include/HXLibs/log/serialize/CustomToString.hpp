#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-08-29 23:03:09
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

#include <string>

namespace HX::log {

/**
 * @brief (偏) 特化 CustomToString, 以实现自定义序列化支持
 * @tparam T 自定义序列化支持的类型
 * @tparam FormatType 支持的序列化容器
 */
template <typename T, typename FormatType>
struct CustomToString;
/*
    要求实现:

    constexpr std::string make(T const& t) {}

    template <typename Stream>
    constexpr void make(T const& t, Stream& s) {}
*/

template <typename T, typename FormatType>
constexpr bool IsCustomToStringVal = requires(
    CustomToString<T, FormatType> c, T const& t, std::string& s
) {
    { CustomToString<T, FormatType>{ std::declval<FormatType*>() } };
    { c.make(t) } -> std::same_as<std::string>;
    { c.make(t, s) } -> std::same_as<void>;
};

} // namespace HX::log