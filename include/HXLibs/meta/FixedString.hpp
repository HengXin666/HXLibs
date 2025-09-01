#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-09-01 13:11:41
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

#include <array>
#include <string_view>

namespace HX::meta {
    
// 一个承载 char 参数包的类型
template <char... Cs>
struct CharPack {
    inline static constexpr std::size_t Size = sizeof...(Cs);
    inline static constexpr std::array<char, Size + 1> Val = {Cs..., '\0'};

    static constexpr auto view() noexcept {
        return std::string_view{Val.data(), Size};
    }
};

// 可作为非类型模板参数的固定字符串类型
template <std::size_t N>
struct FixedString {
    char data[N] {};

    // 接收字面量拷贝到 data
    constexpr FixedString(const char(&s)[N]) {
        for(std::size_t i = 0; i < N; ++i) 
            data[i] = s[i];
    }

    // 长度不含终止符
    static constexpr std::size_t literalSize() noexcept { return N; }

    static constexpr std::size_t size() noexcept { return N - 1; }

    // 编译期索引访问
    constexpr char operator[](std::size_t i) const noexcept { return data[i]; }

    // 为了结构化类型比较, 避免某些实现细节陷阱
    constexpr auto operator<=>(const FixedString&) const noexcept = default;
};

namespace internal {

// 把 FixedString<S> 编译期展开为 CharPack<S[0], S[1], ...>
template <FixedString S, std::size_t... I>
constexpr auto toCharPackImpl(std::index_sequence<I...>) -> CharPack<S[I]...>;

} // namespace internal

template <FixedString S>
using ToCharPack = decltype(internal::toCharPackImpl<S>(std::make_index_sequence<S.size()>{}));

} // namespace HX::meta