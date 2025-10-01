#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-15 16:40:20
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

#include <cstdint>
#include <cassert>
#include <stdexcept>
#include <variant>
#include <string_view>

#include <HXLibs/meta/ContainerConcepts.hpp>
#include <HXLibs/container/CHashMap.hpp>
#include <HXLibs/container/UninitializedNonVoidVariant.hpp>
#include <HXLibs/reflection/MemberName.hpp>
#include <HXLibs/reflection/MemberPtr.hpp>
#include <HXLibs/reflection/deserialization/Numer.hpp>

namespace HX::reflection {

namespace internal {

// 宽松解析
inline constexpr bool LooseParsing = false;

namespace cv { // 该命名空间内容为复制粘贴

// https://github.com/Tencent/rapidjson/blob/master/include/rapidjson/reader.h
template <typename Ch = char, typename It>
inline unsigned parseUnicodeHex4(It&& it) {
    unsigned codepoint = 0;
    for (int i = 0; i < 4; i++) {
        Ch c = *it;
        codepoint <<= 4;
        codepoint += static_cast<unsigned>(c);
        if (c >= '0' && c <= '9')
            codepoint -= '0';
        else if (c >= 'A' && c <= 'F')
            codepoint -= 'A' - 10;
        else if (c >= 'a' && c <= 'f')
            codepoint -= 'a' - 10;
        else {
            throw std::runtime_error("Invalid Unicode Escape Hex");
        }
        ++it;
    }
    return codepoint;
}

// https://github.com/Tencent/rapidjson/blob/master/include/rapidjson/encodings.h
template <typename Ch = char, typename OutputStream>
inline void encodeUtf8(OutputStream& os, unsigned codepoint) {
    if (codepoint <= 0x7F)
        os.push_back(static_cast<Ch>(codepoint & 0xFF));
    else if (codepoint <= 0x7FF) {
        os.push_back(static_cast<Ch>(0xC0 | ((codepoint >> 6) & 0xFF)));
        os.push_back(static_cast<Ch>(0x80 | ((codepoint & 0x3F))));
    } else if (codepoint <= 0xFFFF) {
        os.push_back(static_cast<Ch>(0xE0 | ((codepoint >> 12) & 0xFF)));
        os.push_back(static_cast<Ch>(0x80 | ((codepoint >> 6) & 0x3F)));
        os.push_back(static_cast<Ch>(0x80 | (codepoint & 0x3F)));
    } else {
        assert(codepoint <= 0x10FFFF);
        os.push_back(static_cast<Ch>(0xF0 | ((codepoint >> 18) & 0xFF)));
        os.push_back(static_cast<Ch>(0x80 | ((codepoint >> 12) & 0x3F)));
        os.push_back(static_cast<Ch>(0x80 | ((codepoint >> 6) & 0x3F)));
        os.push_back(static_cast<Ch>(0x80 | (codepoint & 0x3F)));
    }
}

} // namespace cv

/**
 * @brief 跳过空白字符
 * @tparam It 
 * @param begin 
 * @param end 
 */
template <typename It>
void skipWhiteSpace(It&& it, It&& end) {
    // '\n'; // 10
    // '\t'; // 9
    // ' ' ; // 32
    // '\r'; // 13
    constexpr uint64_t WhiteSpaceBitMask
        = (1ULL << static_cast<uint64_t>(' '))
        | (1ULL << static_cast<uint64_t>('\t'))
        | (1ULL << static_cast<uint64_t>('\n'))
        | (1ULL << static_cast<uint64_t>('\r'));

    while (it != end) {
        if constexpr (LooseParsing) {
            if (static_cast<uint8_t>(*it) < 33) {
                ++it;
            } else {
                break;
            }
        } else {
            if (static_cast<uint8_t>(*it) < 33
                && (1ULL << static_cast<uint64_t>(*it) & WhiteSpaceBitMask)
            ) {
                ++it;
            } else {
                break;
            }
        }
    }
}

/**
 * @brief 处理转义字符
 * @tparam It 
 * @param it 
 * @param end 
 */
template <typename Str, typename It>
void handleEscapeChar(Str& str, It&& it, It&& end) {
    // 此时 *it == '\', 因此需要 ++it
    if (++it == end) [[unlikely]] {
        throw std::runtime_error{"Unexpected end of input while parsing escape sequence"};
    }
    switch (*it) {
        case 'r': str += '\r'; break;
        case 'n': str += '\n'; break;
        case 'b': str += '\b'; break;
        case 't': str += '\t'; break;
        case 'a': str += '\a'; break;
        case 'f': str += '\f'; break;
        case 'v': str += '\v'; break;
        case 'u': {
            ++it;
            // 此处显然只能 cv 了
            // https://github.com/alibaba/yalantinglibs/blob/main/include/ylt/standalone/iguana/json_reader.hpp
            if (std::distance(it, end) <= 4) [[unlikely]] {
                throw std::runtime_error(R"(Expected 4 hexadecimal digits)");
            }
            auto code_point = cv::parseUnicodeHex4(it);
            cv::encodeUtf8(str, code_point); // 内部会移动 it
            return;
        }
        default: str += *it; break;
    }
    ++it; // 下一个字符
}

/**
 * @brief 验证当前 it 开始 是否为 Cs... 字符, 如果不是则抛出异常
 * @tparam Cs 
 * @tparam It 
 * @param it 
 * @param end 
 */
template <char... Cs, typename It>
void verify(It&& it, It&& end) {
    const auto n = static_cast<std::size_t>(std::distance(it, end));
    if (n < sizeof...(Cs)) [[unlikely]] {
        throw std::runtime_error{
            "The buffer zone ended prematurely, not as expected"};
    }
    if (((*it++ != Cs) || ...)) [[unlikely]] {
        throw std::runtime_error{
            std::string{"Characters are not as expected: "}.append(
                {Cs..., '\0', sizeof...(Cs)})};
    }
}

/**
 * @brief 验证下一个字符为 {Cs...} 中的一个
 * @tparam Cs 
 * @tparam It 
 * @param it 
 * @param end 
 */
template <char... Cs, typename It>
void verifyIn(It&& it, It&& end) {
    if (it == end) [[unlikely]] {
        throw std::runtime_error{"Unexpected end of input while parsing escape sequence"};
    }
    if ((... || (*it == Cs))) [[likely]] {
        return;
    }
    [[unlikely]] throw std::runtime_error{"Characters are not as expected"};
}

/**
 * @brief 查找下一个在 {Cs...} 中的字符
 * @tparam Cs 
 * @tparam It 
 * @param it 
 * @param end 
 */
template <char... Cs, typename It>
void findNextIn(It&& it, It&& end) {
    while (it != end) [[likely]] {
        if ((... || (*it == Cs))) {
            return;
        }
        ++it;
    }
    [[unlikely]] throw std::runtime_error{"Characters are not as expected"};
}

template <typename It>
std::string_view findKey(It&& it, It&& end) {
    skipWhiteSpace(it, end);
    verify<'"'>(it, end);
    // 找左 "
    auto left = it;
    findNextIn<'"'>(it, end); // 假定 key 没有 '\', 有也会在 at(key) 时候报错
    return {left, it++};
}

template <meta::KeyValueContainer T, typename It>
auto& insertOrAssign(T& t, It&& it, It&& end) {
    if constexpr (requires (T& t) {
        t[findKey(it, end)];
    }) {
        return t[findKey(it, end)];
    } else if constexpr (requires (T& t) {
        t[typename T::key_type {findKey(it, end)}];
    }) {
        return t[typename T::key_type {findKey(it, end)}];
    } else {
        // 不支持由 std::string_view 构造 key
        static_assert(!sizeof(T), "Constructing keys from std::string_view is not supported");
    }
}

struct FromJson {
    template <typename T, typename It>
        requires(std::is_same_v<bool, T>
              || std::is_integral_v<T>
              || std::is_floating_point_v<T>)
    static void fromJson(T& t, It&& it, It&& end) {
        skipWhiteSpace(it, end);
        Numer::fromNumer(t, it, end);
    }

    template <typename T, typename It>
        requires(std::is_enum_v<T>)
    static void fromJson(T& t, It&& it, It&& end) {
        skipWhiteSpace(it, end);
        // 解析字符串
        verify<'"'>(it, end);
        Numer::fromNumer(t, it, end);
        verify<'"'>(it, end);
    }
    
    template <typename T, typename It>
        requires(meta::is_optional_v<T> || meta::is_smart_pointer_v<T>)
    static void fromJson(T& t, It&& it, It&& end) {
        skipWhiteSpace(it, end);
        // 解析指针 或者 optional
        constexpr uint8_t Mask = static_cast<uint8_t>(~' '); // 大小写掩码
        if (std::distance(it, end) < 4) [[unlikely]] {
            // 只能默认它是合法的, 然后解析键
            ;
        } else if ((*(it + 0) & Mask) == 'N'
                && (*(it + 1) & Mask) == 'U'
                && (*(it + 2) & Mask) == 'L'
                && (*(it + 3) & Mask) == 'L'
        ) {
            it += 4;
            return;
        }
        if constexpr (meta::is_optional_v<T>) {
            t = typename T::value_type{}; // 要求支持默认无参构造
            fromJson(*t, it, end);
        } else if constexpr (meta::is_smart_pointer_v<T>) {
            t = T{new typename T::element_type{}}; // 要求支持默认无参构造 并且可以被 new
            fromJson(*t, it, end);
        } else {
            // 不应该匹配到此处
            static_assert(!sizeof(T), "@todo");
        }
    }
    
    template <meta::StringType T, typename It>
    static void fromJson(T& t, It&& it, It&& end) {
        skipWhiteSpace(it, end);
        // 解析字符串
        verify<'"'>(it, end);
        while (it != end) {
            if (*it == '"') [[unlikely]] {
                ++it;
                break;
            }
            if (*it != '\\') {
                t += *it++; // 下一个字符
            } else {
                handleEscapeChar(t, it, end); // 内部会处理好 it
            }
        }
    }

    template <meta::SingleElementContainer T, typename It>
        requires(!meta::is_std_array_v<T> 
              && !meta::is_span_v<T>)
    static void fromJson(T& t, It&& it, It&& end) {
        skipWhiteSpace(it, end);
        verify<'['>(it, end);

        skipWhiteSpace(it, end);
        if (*it == ']') [[unlikely]] {
            ++it;
            return;
        }
        // 解析数组 []
        if constexpr (requires (T& t) {
            t.emplace_back();
        }) {
            while (it != end) {
                // 把内容移交
                fromJson(t.emplace_back(), it, end);
                skipWhiteSpace(it, end);
                if (*it == ']') {
                    ++it;
                    break;
                }
                verify<','>(it, end);
            }
        } else {
            // 对象没有默认无参构造, 显然不是聚合类
            static_assert(!sizeof(T), "Objects do not have a default parameterless construct");
        }
    }

    template <typename T, typename It>
        requires (meta::is_std_array_v<T> 
               || meta::is_span_v<T>)
    static void fromJson(T& t, It&& it, It&& end) {
        skipWhiteSpace(it, end);
        verify<'['>(it, end);

        skipWhiteSpace(it, end);
        if (*it == ']') [[unlikely]] {
            ++it;
            return;
        }
        // 解析数组 []
        std::size_t idx = 0;
        while (it != end) {
            if (idx >= t.size()) [[unlikely]] {
                // 数组没有足够的空间存放数据
                throw std::runtime_error{"The array doesn't have enough space for data"};
            }
            // 把内容移交
            fromJson(t[idx++], it, end);
            skipWhiteSpace(it, end);
            if (*it == ']') {
                ++it;
                break;
            }
            verify<','>(it, end);
        }
    }

    template <meta::KeyValueContainer T, typename It>
    static void fromJson(T& t, It&& it, It&& end) {
        skipWhiteSpace(it, end);
        // 解析字典 {}
        verify<'{'>(it, end);

        skipWhiteSpace(it, end);
        if (*it == '}') [[unlikely]] {
            ++it;
            return;
        }
        while (it != end) {
            auto& val = insertOrAssign(t, it, end);
            skipWhiteSpace(it, end);
            verify<':'>(it, end);
            fromJson(val, it, end);

            skipWhiteSpace(it, end);
            if (*it == '}') {
                ++it;
                break;
            }
            verify<','>(it, end);
        }
    }
    
    // 主模板 聚合类
    template <typename T, typename It>
        requires(reflection::IsReflective<T>)
    static void fromJson(T& t, It&& it, It&& end) {
        // 无需考虑根是数组的情况, 因为我们是反射库!
        
        // 找 { (去掉空白, 必然得是, 否则非法)
        skipWhiteSpace(it, end);
        verify<'{'>(it, end);
    
        skipWhiteSpace(it, end);
        // 不太可能是空的
        if (*it == '}') [[unlikely]] {
            ++it;
            return;
        }
        
        constexpr std::size_t N = membersCountVal<T>;
        if constexpr (N > 0) {
            static auto nameHash = reflection::makeNameToIdxVariantHashMap<T>();
            // 需要 name -> idx 映射
            while (it != end) {
                auto key = findKey(it, end);
                // 找 ':'
                skipWhiteSpace(it, end);
                verify<':'>(it, end);
                // 解析值 (非空白字符)
                // 需要一个运行时可以查找到第 i 个元素的偏移量的方法
                std::visit([&](auto&& idx) {
                    if constexpr (std::is_array_v<meta::remove_cvref_t<decltype(
                        *std::get<meta::remove_cvref_t<decltype(idx)>::Idx>(
                            reflection::internal::getStaticObjPtrTuple<T>())
                    )>>) {
                        // 不支持C风格数组, 至少替换成 std::array
                        static_assert(!sizeof(T), "Not supporting C-style arrays, "
                                                  "at least replace with std::array");
                    } else if constexpr (std::is_pointer_v<meta::remove_cvref_t<decltype(
                        *std::get<meta::remove_cvref_t<decltype(idx)>::Idx>(
                            reflection::internal::getStaticObjPtrTuple<T>())
                    )>>) {
                        // 不支持C风格裸指针, 至少替换成 智能指针
                        static_assert(!sizeof(T), "Does not support C-style bare pointers, "
                                                  "at least replace with smart pointers");
                    }
                    using PtrType = typename meta::remove_cvref_t<decltype(idx)>::Type *;
                    auto ptr = reinterpret_cast<PtrType>(
                        reinterpret_cast<char*>(&t) + idx.offset
                    );
                    fromJson(*ptr, it, end);
                }, nameHash.at(key));

                // 找 ',' 或者 '}'
                skipWhiteSpace(it, end);
                if (*it == '}') {
                    ++it;
                    break;
                }
                verify<','>(it, end);
            }
        }
    }
};

} // namespace internal

template <typename T>
void fromJson(T& t, std::string_view json) {
    // 仅支持 char, 而不是 wchar
    internal::FromJson::fromJson(t, json.begin(), json.end());
}

} // namespace HX::reflection

