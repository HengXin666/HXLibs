#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2024-09-07 15:43:57
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

#include <optional>
#include <tuple>
#include <variant>
#include <format>
#include <thread>
#include <filesystem>
#if __cplusplus < 202302L
#include <iosfwd>
#endif

#include <HXLibs/meta/ContainerConcepts.hpp>
#include <HXLibs/reflection/MemberName.hpp>
#include <HXLibs/reflection/EnumName.hpp>
#include <HXLibs/utils/NumericBaseConverter.hpp>
#include <HXLibs/log/serialize/CustomToString.hpp>

namespace HX::log {

// 内部使用的命名空间啊喂!
namespace internal {

/**
 * @brief wstring -> string (UTF-16/UTF-32 -> UTF-8)
 * @param input 
 * @return std::string 
 */
inline constexpr std::string toByteString(const std::wstring& input) {
    std::string res;
    res.reserve(input.size() * 4); // 预分配最大可能空间
    for (size_t i = 0; i < input.size(); ++i) {
        char32_t codepoint;
        // 处理UTF-16代理对 (Windows环境)
        if constexpr (sizeof(wchar_t) == 2) {
            char16_t high_surrogate = static_cast<char16_t>(input[i]);
            if (high_surrogate >= 0xD800 && high_surrogate <= 0xDBFF) {
                // 需要低代理项
                if (++i >= input.size()) [[unlikely]] {
                    throw std::runtime_error("Invalid UTF-16: truncated surrogate pair");
                }
                char16_t low_surrogate = static_cast<char16_t>(input[i]);
                if (low_surrogate < 0xDC00 || low_surrogate > 0xDFFF) [[unlikely]] {
                    throw std::runtime_error("Invalid UTF-16: malformed surrogate pair");
                }
                codepoint = 0x10000
                    + ((static_cast<char32_t>(high_surrogate) - 0xD800) * 0x400
                    + (static_cast<char32_t>(low_surrogate) - 0xDC00));
            } else {
                codepoint = high_surrogate;
            }
        } else { // 处理UTF-32 (Linux/macOS环境)
            codepoint = static_cast<char32_t>(input[i]);
            // 验证码点有效性
            if (codepoint > 0x10FFFF 
            || (codepoint >= 0xD800 && codepoint <= 0xDFFF)
            ) [[unlikely]] {
                throw std::runtime_error("Invalid UTF-32 code point");
            }
        }

        // 转换为UTF-8
        if (codepoint <= 0x7F) {
            res += static_cast<char>(codepoint);
        } else if (codepoint <= 0x7FF) {
            res += static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F));
            res += static_cast<char>(0x80 | (codepoint & 0x3F));
        } else if (codepoint <= 0xFFFF) {
            res += static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F));
            res += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            res += static_cast<char>(0x80 | (codepoint & 0x3F));
        } else {
            res += static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07));
            res += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
            res += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            res += static_cast<char>(0x80 | (codepoint & 0x3F));
        }
    }
    return res;
}

template <typename CharT, std::size_t N>
    requires (std::is_same_v<CharT, char>
           || std::is_same_v<CharT, wchar_t>)
struct CharArrWarp {
    /**
     * @note 因为 数组可以匹配到 数组引用 和 隐式转换到 指针. 使得模板情况下重载决议出现二义性
     * 应该使用包装类, 让数组匹配到更加具体的它, 并且显然无法匹配到指针, 这种情况下有更具体的
     * 就可以去掉二义性
     */
    using Type = CharT;
    CharT const (&arr)[N];
};

struct FormatZipString {
    inline static constexpr std::string_view DELIMIER          = ",";
    inline static constexpr std::string_view PARENTHESES_LEFT  = "(";
    inline static constexpr std::string_view PARENTHESES_RIGHT = ")";
    inline static constexpr std::string_view BRACKET_LEFT      = "[";
    inline static constexpr std::string_view BRACKET_RIGHT     = "]";
    inline static constexpr std::string_view KEY_VAK_PAIR      = ":";
    inline static constexpr std::string_view BRACE_LEFT        = "{";
    inline static constexpr std::string_view BRACE_RIGHT       = "}";
    inline static constexpr std::string_view QUOTATION_MARKS   = "\"";
    
    // 路径
    template <typename T>
        requires (std::is_same_v<T, std::filesystem::path>)
    constexpr std::string make(T const& t) {
        return t.string();
    }

    template <typename T, typename Stream>
        requires (std::is_same_v<T, std::filesystem::path>)
    constexpr void make(T const& t, Stream& s) {
        s.append(t.string());
    }

    // 线程id
    template <typename T>
        requires (std::is_same_v<T, std::thread::id>)
    std::string make(T const& t) {
#if __cplusplus >= 202302L
        return std::format("{}", t);
#else
        std::ostringstream oss;
        oss << t;
        return std::move(oss).str();
#endif // !__cplusplus >= 202302L
    }

    template <typename T, typename Stream>
        requires (std::is_same_v<T, std::thread::id>)
    void make(T const& t, Stream& s) {
#if __cplusplus >= 202302L
        return std::format("{}", t);
#else
        std::ostringstream oss;
        oss << t;
        s.append(std::move(oss).str());
#endif // !__cplusplus >= 202302L
    }

    // bool
    constexpr std::string make(bool t) {
        using namespace std::string_literals;
        return t ? "true"s : "false"s;
    }

    template <typename Stream>
    constexpr void make(bool t, Stream& s) {
        s.append(t ? "true" : "false");
    }

    // null
    template <typename NullType>
        requires (std::is_same_v<NullType, std::nullopt_t>
               || std::is_same_v<NullType, std::nullptr_t>
               || std::is_same_v<NullType, std::monostate>)
    constexpr std::string make(NullType const&) {
        using namespace std::string_literals;
        return "null"s;
    }

    template <typename NullType, typename Stream>
        requires (std::is_same_v<NullType, std::nullopt_t>
               || std::is_same_v<NullType, std::nullptr_t>
               || std::is_same_v<NullType, std::monostate>)
    constexpr void make(NullType const&, Stream& s) {
        s.append("null");
    }

    // 数字类型 NF 表示浮点数的位数 (-1表示默认选项, 使用 std::format("{}", t) 格式化)
    template <typename T, size_t NF = static_cast<size_t>(-1)>
        requires (std::is_integral_v<T> || std::is_floating_point_v<T>)
    constexpr std::string make(T const& t) {
        if constexpr (NF != static_cast<size_t>(-1)) {
            return std::format("{:.{}f}", t, NF);
        } else {
            return std::format("{}", t);
        }
    }

    template <typename T, size_t NF = static_cast<size_t>(-1), typename Stream>
        requires (std::is_integral_v<T> || std::is_floating_point_v<T>)
    constexpr void make(T const& t, Stream& s) {
        if constexpr (NF != static_cast<size_t>(-1)) {
            s.append(std::format("{:." + std::format("{}", NF) + "f}", t));
        } else {
            s.append(std::format("{}", t));
        }
    }

    // 枚举类型
    template <typename E>
        requires (std::is_enum_v<E>)
    constexpr std::string make(E const& t) {
        return make(reflection::toEnumName(t));
    }

    template <typename E, typename Stream>
        requires (std::is_enum_v<E>)
    constexpr void make(E const& t, Stream& s) {
        make(reflection::toEnumName(t), s);
    }
    
    // C风格数组
    template <typename T, std::size_t N>
        requires (!std::is_same_v<T, wchar_t>
               && !std::is_same_v<T, char>)
    constexpr std::string make(const T (&arr)[N]) {
        std::string res;
        bool once = false;
        res += BRACKET_LEFT;
        for (const auto& it : arr) {
            if (once)
                res += DELIMIER;
            else
                once = true;
            res += make(it);
        }
        res += BRACKET_RIGHT;
        return res;
    }

    template <typename T, std::size_t N, typename Stream>
        requires (!std::is_same_v<T, wchar_t>
               && !std::is_same_v<T, char>)
    constexpr void make(const T (&arr)[N], Stream& s) {
        bool once = false;
        s.append(BRACKET_LEFT);
        for (const auto& it : arr) {
            if (once)
                s.append(DELIMIER);
            else
                once = true;
            make(it, s);
        }
        s.append(BRACKET_RIGHT);
    }
    
    // std常见的支持迭代器的单元素容器
    template <meta::SingleElementContainer Container>
    constexpr std::string make(Container const& arr) {
        std::string res;
        bool once = false;
        res += BRACKET_LEFT;
        for (const auto& it : arr) {
            if (once)
                res += DELIMIER;
            else
                once = true;
            res += make(it);
        }
        res += BRACKET_RIGHT;
        return res;
    }

    template <meta::SingleElementContainer Container, typename Stream>
    constexpr void make(Container const& arr, Stream& s) {
        bool once = false;
        s.append(BRACKET_LEFT);
        for (const auto& it : arr) {
            if (once)
                s.append(DELIMIER);
            else
                once = true;
            make(it, s);
        }
        s.append(BRACKET_RIGHT);
    }

    // 聚合类
    template <typename T>
        requires (reflection::IsReflective<T>)
    constexpr std::string make(T const& obj) {
        constexpr std::size_t Cnt = reflection::membersCountVal<T>;
        std::string res;
        res += BRACE_LEFT;
        if constexpr (Cnt > 0) {
            reflection::forEach(obj, [&] <std::size_t I> (
                std::index_sequence<I>, auto name, auto const& val
            ) {
                res += make(name);
                res += KEY_VAK_PAIR;
                res.append(make(val));

                if constexpr (I < Cnt - 1) {
                    res += DELIMIER;
                }
            });
        }
        res += BRACE_RIGHT;
        return res;
    }

    template <typename T, typename Stream>
        requires (reflection::IsReflective<T>)
    constexpr void make(T const& obj, Stream& s) {
        constexpr std::size_t Cnt = reflection::membersCountVal<T>;
        s.append(BRACE_LEFT);
        if constexpr (Cnt > 0) {
            reflection::forEach(obj, [&] <std::size_t I> (
                std::index_sequence<I>, auto name, auto const& val
            ) {
                make(name, s);
                s.append(KEY_VAK_PAIR);
                make(val, s);

                if constexpr (I < Cnt - 1) {
                    s.append(DELIMIER);
                }
            });
        }
        s.append(BRACE_RIGHT);
    }

    // std常见的支持迭代器的键值对容器
    template <meta::KeyValueContainer Container>
    constexpr std::string make(const Container& map) {
        std::string res;
        res += BRACE_LEFT;
        bool once = false;
        for (const auto& [k, v] : map) {
            if (once)
                res += DELIMIER;
            else
                once = true;
            res += make(k);
            res += KEY_VAK_PAIR;
            res += make(v);
        }
        res += BRACE_RIGHT;
        return res;
    }

    template <meta::KeyValueContainer Container, typename Stream>
    constexpr void make(const Container& map, Stream& s) {
        s.append(BRACE_LEFT);
        bool once = false;
        for (const auto& [k, v] : map) {
            if (once)
                s.append(DELIMIER);
            else
                once = true;
            make(k, s);
            s.append(KEY_VAK_PAIR);
            make(v, s);
        }
        s.append(BRACE_RIGHT);
    }

    // str相关的类型
    template <meta::StringType ST>
        requires(!std::is_same_v<ST, std::filesystem::path>)
    constexpr std::string make(const ST& t) {
        std::string res;
        res += '"';
        res += t;
        res += '"';
        return res;
    }

    template <meta::StringType ST, typename Stream>
        requires(!std::is_same_v<ST, std::filesystem::path>)
    constexpr void make(const ST& t, Stream& s) {
        s.append(QUOTATION_MARKS);
        s.append(t);
        s.append(QUOTATION_MARKS);
    }

    // wstr相关类型
    template <meta::WStringType ST>
        requires(!std::is_same_v<ST, std::filesystem::path>)
    constexpr std::string make(const ST& t) {
        std::string res;
        res += '"';
        res += toByteString(t);
        res += '"';
        return res;
    }

    template <meta::WStringType ST, typename Stream>
        requires(!std::is_same_v<ST, std::filesystem::path>)
    constexpr void make(const ST& t, Stream& s) {
        s.append(QUOTATION_MARKS);
        s.append(toByteString(t));
        s.append(QUOTATION_MARKS);
    }

    // const char* C字符串指针
    constexpr std::string make(const char* t) {
        return {t};
    }

    template <typename CharT, typename Stream>
        requires (std::is_same_v<CharT, char>
               || std::is_same_v<CharT, wchar_t>)
    constexpr void make(CharT const* t, Stream& s) {
        if constexpr (std::is_same_v<CharT, char>) {
            s.append(t);
        } else {
            s.append(toByteString(t));
        }
    }

    // const wchar_t* C字符串指针
    constexpr std::string make(const wchar_t* t) {
        return toByteString(t);
    }

    // char[N] C字符数组
    template <std::size_t N>
    constexpr std::string make(char const (&t)[N]) {
        return {t, N - 1};
    }

    template <typename CharT, std::size_t N, typename Stream>
    constexpr void make(CharArrWarp<CharT, N> t, Stream& s) {
        if constexpr (std::is_same_v<typename meta::remove_cvref_t<decltype(t)>::Type, char>) {
            s.append({t.arr, N - 1});
        } else {
            s.append(toByteString({t.arr, N - 1}));
        }
    }

    // wchar_t[N] C字符数组
    template <std::size_t N>
    constexpr std::string make(wchar_t const (&t)[N]) {
        return toByteString({t, N - 1});
    }

    // 普通指针
    template <typename T>
    constexpr std::string make(T* const& p) {
        using namespace std::string_literals;
        std::string res = "0x"s;
        res += utils::NumericBaseConverter::hexadecimalConversion(
            reinterpret_cast<std::size_t>(p));
        return res;
    }

    template <typename T, typename Stream>
    constexpr void make(T* const& p, Stream& s) {
        using namespace std::string_literals;
        s.append("0x"s);
        s.append(utils::NumericBaseConverter::hexadecimalConversion(
            reinterpret_cast<std::size_t>(p)));
    }

    // std::optional
    template <typename T>
    constexpr std::string make(std::optional<T> const& opt) {
        return opt ? make(*opt) : make(std::nullopt);
    }

    template <typename T, typename Stream>
    constexpr void make(std::optional<T> const& opt, Stream& s) {
        opt ? make(*opt, s) : make(std::nullopt, s);
    }

    // std::pair
    template <typename T1, typename T2>
    constexpr std::string make(std::pair<T1, T2> const& p2) {
        std::string res;
        res += PARENTHESES_LEFT;
        res += make(std::get<0>(p2));
        res += DELIMIER;
        res += make(std::get<1>(p2));
        res += PARENTHESES_RIGHT;
        return res;
    }

    template <typename T1, typename T2, typename Stream>
    constexpr void make(std::pair<T1, T2> const& p2, Stream& s) {
        s.append(PARENTHESES_LEFT);
        make(std::get<0>(p2), s);
        s.append(DELIMIER);
        make(std::get<1>(p2), s);
        s.append(PARENTHESES_RIGHT);
    }

    // std::tuple
    template <typename... Ts>
    constexpr std::string make(std::tuple<Ts...> const& tp) {
        constexpr std::size_t N = sizeof...(Ts);
        std::string res;
        res += PARENTHESES_LEFT;
        [&] <std::size_t... Is> (std::index_sequence<Is...>) {
            ((res += make(std::get<Is>(tp)), 
              static_cast<void>(Is + 1 < N ? res += DELIMIER : res)
            ), ...);
        }(std::make_index_sequence<N>{});
        res += PARENTHESES_RIGHT;
        return res;
    }

    template <typename... Ts, typename Stream>
    constexpr void make(std::tuple<Ts...> const& tp, Stream& s) {
        constexpr std::size_t N = sizeof...(Ts);
        s.append(PARENTHESES_LEFT);
        if constexpr (N >= 1) {
            [&] <std::size_t... Is> (std::index_sequence<Is...>) {
                ((make(std::get<Is>(tp), s), s.append(DELIMIER)), ...);
            }(std::make_index_sequence<N - 1>{});
            make(std::get<N - 1>(tp), s);
        }
        s.append(PARENTHESES_RIGHT);
    }

    // std::variant
    template <typename... Ts>
    constexpr std::string make(std::variant<Ts...> const& v) {
        return std::visit([&](auto&& val) {
            return make(val);
        }, v);
    }

    template <typename... Ts, typename Stream>
    constexpr void make(std::variant<Ts...> const& v, Stream& s) {
        std::visit([&](auto&& val) {
            make(val, s);
        }, v);
    }

    // std::智能指针
    template <typename T>
        requires (meta::is_smart_pointer_v<T>)
    constexpr std::string make(T const& ptr) {
        return ptr ? make(*ptr) : make(nullptr);
    }

    template <typename T, typename Stream>
        requires (meta::is_smart_pointer_v<T>)
    constexpr void make(T const& ptr, Stream& s) {
        ptr ? make(*ptr, s) : make(nullptr, s);
    }

    // 任意自定义
    template <typename T>
        requires (IsCustomToStringVal<T, FormatZipString>)
    constexpr std::string make(T const& t) {
        return CustomToString<T, FormatZipString>{this}.make(t);
    }

    template <typename T, typename Stream>
        requires (IsCustomToStringVal<T, FormatZipString>)
    constexpr void make(T const& t, Stream& s) {
        CustomToString<T, FormatZipString>{this}.make(t, s);
    }
};

} // namespace internal

/**
 * @brief toString 全部参数
 * @return std::string 
 */
template <typename... Ts>
inline std::string toString(Ts const&... ts) {
    std::string res;
    internal::FormatZipString zipStr{};
    ((res += zipStr.make(ts)), ...);
    return res;
}

/**
 * @brief 将`t`转为字符串, 然后存入`s`中
 * @tparam T 
 * @tparam Stream 
 * @param t 
 * @param s 字符串
 */
template <typename T, typename Stream>
inline void toString(T const& t, Stream& s) {
    internal::FormatZipString{}.make(t, s);
}

} // namespace HX::log

