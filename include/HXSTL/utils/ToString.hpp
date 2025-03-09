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
#ifndef _HX_TO_STRING_H_
#define _HX_TO_STRING_H_

#include <optional>
#include <tuple>
#include <variant>
#include <span>
#include <format>
#include <cmath>

#include <HXSTL/concepts/KeyValueContainer.hpp>
#include <HXSTL/concepts/PairContainer.hpp>
#include <HXSTL/concepts/SingleElementContainer.hpp>
#include <HXSTL/concepts/StringType.hpp>
#include <HXSTL/reflection/MemberName.hpp>
#include <HXSTL/utils/NumericBaseConverter.hpp>

// 屏蔽未使用函数、变量和参数的警告
#if defined(_MSC_VER) // MSVC
    #pragma warning(push)
    #pragma warning(disable: 4505) // C4505: 未使用的局部函数
    #pragma warning(push)
    #pragma warning(disable: 4101) // C4101: 未使用的局部变量
    #pragma warning(push)
    #pragma warning(disable: 4456) // C4456: 声明隐藏了一个局部变量
#elif defined(__GNUC__) || defined(__clang__) // GCC 和 Clang
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-function"
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-variable"
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

namespace HX { namespace STL { namespace utils {

// 内部使用的命名空间啊喂!
namespace internal {

/**
 * @brief wstring -> string (UTF-16/UTF-32 -> UTF-8)
 * @param input 
 * @return std::string 
 */
inline std::string toByteString(const std::wstring& input) {
    std::string res;
    res.reserve(input.size() * 4); // 预分配最大可能空间
    for (size_t i = 0; i < input.size(); ++i) {
        char32_t codepoint;
        // 处理UTF-16代理对（Windows环境）
        if constexpr (sizeof(wchar_t) == 2) {
            char16_t high_surrogate = static_cast<char16_t>(input[i]);
            if (high_surrogate >= 0xD800 && high_surrogate <= 0xDBFF) {
                // 需要低代理项
                if (++i >= input.size()) {
                    throw std::runtime_error("Invalid UTF-16: truncated surrogate pair");
                }
                char16_t low_surrogate = static_cast<char16_t>(input[i]);
                if (low_surrogate < 0xDC00 || low_surrogate > 0xDFFF) {
                    throw std::runtime_error("Invalid UTF-16: malformed surrogate pair");
                }
                codepoint = 0x10000
                    + ((static_cast<char32_t>(high_surrogate) - 0xD800) * 0x400
                    + (static_cast<char32_t>(low_surrogate) - 0xDC00));
            } else {
                codepoint = high_surrogate;
            }
        } else { // 处理UTF-32（Linux/macOS环境）
            codepoint = static_cast<char32_t>(input[i]);
            // 验证码点有效性
            if (codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
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

// 概念: 鸭子类型: 只需要满足有一个成员函数是toString的, 即可
template <typename T>
concept ToStringClassType = requires(T t) {
    t.toString();
};

// 概念: 鸭子类型: 只需要满足有一个成员函数是toJson的, 即可
template <typename T>
concept ToJsonClassType = requires(T t) {
    t.toJson();
};

// 主模版
template <typename... Ts>
struct ToString {
    static std::string toString(Ts const&... obj) { // 禁止默认实现
        static_assert(sizeof...(Ts) == 0, "toString is not implemented for this type");
        return {};
    }
};

// toString`聚合类` (当其他模版都不适配时候, 就只能适配本模版; 
// 而如果可以适配其他模版, 则不会适配本模版, 因为有更适合的)
template <typename T>
struct ToString<T> {
    static std::string toString(T const& obj) {
        std::string res;
        constexpr std::size_t Cnt = HX::STL::reflection::membersCountVal<T>;
        static_assert(Cnt > 0, "toString is not implemented for this type");
        res.push_back('{');
        if constexpr (Cnt > 0) {
            HX::STL::reflection::forEach(const_cast<T&>(obj), [&](auto index, auto name, auto& val) {
                res.push_back('"');
                res.append(name.data(), name.size());
                res.push_back('"');

                res.push_back(':');
                auto&& str = ToString<HX::STL::utils::remove_cvref_t<decltype(val)>>::toString(val);
                res.append(str.data(), str.size());

                if (index < Cnt - 1) [[likely]] {
                    res.push_back(',');
                }
            });
        }
        res.push_back('}');
        return res;
    }

    template <typename Stream>
    static void toString(T const& obj, Stream& s) {
        constexpr std::size_t Cnt = HX::STL::reflection::membersCountVal<T>;
        static_assert(Cnt > 0, "toString is not implemented for this type");
        s.push_back('{');
        if constexpr (Cnt > 0) {
            HX::STL::reflection::forEach(const_cast<T&>(obj), [&](auto index, auto name, auto& val) {
                s.push_back('"');
                s.append(name.data(), name.size());
                s.push_back('"');

                s.push_back(':');
                ToString<HX::STL::utils::remove_cvref_t<decltype(val)>>::toString(val, s);

                if (index < Cnt - 1) [[likely]] {
                    s.push_back(',');
                }
            });
        }
        s.push_back('}');
    }
};

// ===偏特化 ===
template <>
struct ToString<std::nullptr_t> {
    static std::string toString(const std::nullptr_t&) { // 普通指针不行
        return "null";
    }

    template <typename Stream>
    static void toString(const std::nullptr_t&, Stream& s) {
        s.append("null");
    }
};

template <>
struct ToString<std::nullopt_t> {
    static std::string toString(const std::nullopt_t&) {
        return "null";
    }

    template <typename Stream>
    static void toString(const std::nullopt_t&, Stream& s) {
        s.append("null");
    }
};

template <>
struct ToString<bool> {
    static std::string toString(bool t) {
        return t ? "true" : "false";
    }

    template <typename Stream>
    static void toString(bool t, Stream& s) {
        s.append(t ? "true" : "false");
    }
};

template <>
struct ToString<std::monostate> {
    static std::string toString(const std::monostate&) {
        return "monostate";
    }

    template <typename Stream>
    static void toString(const std::monostate&, Stream& s) {
        s.append("monostate");
    }
};

// 数字类型
template <class T>
    requires (std::is_integral_v<T> || std::is_floating_point_v<T>)
struct ToString<T> {
    static std::string toString(const T& t) {
        if constexpr (std::is_floating_point_v<T>) {
            // 如果浮点数是整数 (例如 26.0), 则不显示小数部分
            if (std::floor(t) == t) {
                return std::format("{:.0f}", t);
            } else {
                return std::format("{}", t);
            }
        } else {
            return std::format("{}", t);
        }
    }

    template <typename Stream>
    static void toString(const T& t, Stream& s) {
        s.append(ToString<T>::toString(t));
    }
};

// C风格数组类型
template <typename T, std::size_t N>
struct ToString<T[N]> {
    static std::string toString(const T (&arr)[N]) {
        std::string res;
        bool once = false;
        res += '[';
        for (const auto& it : arr) {
            if (once)
                res += ',';
            else
                once = true;
            res += ToString<HX::STL::utils::remove_cvref_t<decltype(it)>>::toString(it);
        }
        res += ']';
        return res;
    }

    template <typename Stream>
    static void toString(const T (&arr)[N], Stream& s) {
        s.push_back('[');
        bool once = false;
        for (const auto& it : arr) {
            if (once)
                s.push_back(',');
            else
                once = true;
            ToString<HX::STL::utils::remove_cvref_t<decltype(it)>>::toString(it, s);
        }
        s.push_back(']');
    }
};

// span视图
template <class T>
struct ToString<std::span<T>> {
    static std::string toString(std::span<T> t) {
        std::string res;
        res += '[';
        bool once = false;
        for (const auto& it : t) {
            if (once)
                res += ',';
            else
                once = true;
            res += ToString<HX::STL::utils::remove_cvref_t<decltype(it)>>::toString(it);
        }
        res += ']';
        return res;
    }

    template <typename Stream>
    static void toString(std::span<T> t, Stream& s) {
        s.push_back('[');
        bool once = false;
        for (const auto& it : t) {
            if (once)
                s.push_back(',');
            else
                once = true;
            ToString<HX::STL::utils::remove_cvref_t<decltype(it)>>::toString(it, s);
        }
        s.push_back(']');
    }
};

// std::optional
template <typename... Ts>
struct ToString<std::optional<Ts...>> {
    static std::string toString(const std::optional<Ts...>& t) {
        std::string res;
        if (t.has_value())
            res += ToString<HX::STL::utils::remove_cvref_t<decltype(*t)>>::toString(*t);
        else
            res += ToString<std::nullopt_t>::toString(std::nullopt);
        return res;
    }

    template <typename Stream>
    static void toString(const std::optional<Ts...>& t, Stream& s) {
        if (t.has_value())
            ToString<HX::STL::utils::remove_cvref_t<decltype(*t)>>::toString(*t, s);
        else
            ToString<std::nullopt_t>::toString(std::nullopt, s);
    }
};

// std::variant 现代共用体
template <typename... Ts>
struct ToString<std::variant<Ts...>> {
    static std::string toString(const std::variant<Ts...>& t) {
        return std::visit([] (const auto& v) -> std::string { // 访问者模式
            return ToString<HX::STL::utils::remove_cvref_t<decltype(v)>>::toString(v);
        }, t);
    }

    template <typename Stream>
    static void toString(const std::variant<Ts...>& t, Stream& s) {
        std::visit([&] (const auto& v) -> void { // 访问者模式
            ToString<HX::STL::utils::remove_cvref_t<decltype(v)>>::toString(v, s);
        }, t);
    }
};

// std::pair
template <HX::STL::concepts::PairContainer Container>
struct ToString<Container> {
    static std::string toString(const Container& p) {
        std::string res;
        res += '(';
        res += ToString<HX::STL::utils::remove_cvref_t<decltype(std::get<0>(p))>>::toString(std::get<0>(p));
        res += ',';
        res += ToString<HX::STL::utils::remove_cvref_t<decltype(std::get<1>(p))>>::toString(std::get<1>(p));
        res += ')';
        return res;
    }

    template <typename Stream>
    static void toString(const Container& p, Stream& s) {
        s.push_back('(');
        ToString<HX::STL::utils::remove_cvref_t<decltype(std::get<0>(p))>>::toString(std::get<0>(p), s);
        s.push_back(',');
        ToString<HX::STL::utils::remove_cvref_t<decltype(std::get<1>(p))>>::toString(std::get<1>(p), s);
        s.push_back(')');
    }
};

// std::的常见的支持迭代器的单元素容器
template <HX::STL::concepts::SingleElementContainer Container>
struct ToString<Container> {
    static std::string toString(const Container& sc) {
        std::string res;
        res += '[';
        bool once = false;
        for (const auto& it : sc) {
            if (once)
                res += ',';
            else
                once = true;
            res += ToString<HX::STL::utils::remove_cvref_t<decltype(it)>>::toString(it);
        }
        res += ']';
        return res;
    }

    template <typename Stream>
    static void toString(const Container& sc, Stream& s) {
        s.push_back('[');
        bool once = false;
        for (const auto& it : sc) {
            if (once)
                s.push_back(',');
            else
                once = true;
            ToString<HX::STL::utils::remove_cvref_t<decltype(it)>>::toString(it, s);
        }
        s.push_back(']');
    }
};

// std::的常见的支持迭代器的键值对容器
template <HX::STL::concepts::KeyValueContainer Container>
struct ToString<Container> {
    static std::string toString(const Container& map) {
        std::string res;
        res += '{';
        bool once = false;
        for (const auto& [k, v] : map) {
            if (once)
                res += ',';
            else
                once = true;
            res += ToString<HX::STL::utils::remove_cvref_t<decltype(k)>>::toString(k);
            res += ':';
            res += ToString<HX::STL::utils::remove_cvref_t<decltype(v)>>::toString(v);
        }
        res += '}';
        return res;
    }

    template <typename Stream>
    static void toString(const Container& map, Stream& s) {
        s.push_back('{');
        bool once = false;
        for (const auto& [k, v] : map) {
            if (once)
                s.push_back(',');
            else
                once = true;
            ToString<HX::STL::utils::remove_cvref_t<decltype(k)>>::toString(k, s);
            s.push_back(':');
            ToString<HX::STL::utils::remove_cvref_t<decltype(v)>>::toString(v, s);
        }
        s.push_back('}');
    }
};

// str相关的类型
template <HX::STL::concepts::StringType ST>
struct ToString<ST> {
    static std::string toString(const ST& t) {
        std::string res;
        res += '"';
        res += t;
        res += '"';
        return res;
    }

    template <typename Stream>
    static void toString(const ST& t, Stream& s) {
        s.push_back('"');
        s.append(t);
        s.push_back('"');
    }
};

// wstr相关的类型
template <HX::STL::concepts::WStringType ST>
struct ToString<ST> {
    static std::string toString(const ST& t) {
        std::string res;
        res += '"';
        res += toByteString(t);
        res += '"';
        return res;
    }

    template <typename Stream>
    static void toString(const ST& t, Stream& s) {
        s.push_back('"');
        s.append(toByteString(t));
        s.push_back('"');
    }
};

// C风格字符串 char
template <typename T, std::size_t N>
    requires (std::same_as<T, char>)
struct ToString<T[N]> {
    static std::string toString(const T (&str)[N]) {
        return std::string{str, N - 1}; // - 1 是为了去掉 '\0'
    }

    template <typename Stream>
    static void toString(const T (&str)[N], Stream& s) {
        s.append(std::string{str, N - 1}); // - 1 是为了去掉 '\0'
    }
};

// C风格字符串 wchar_t
template <typename T, std::size_t N>
    requires (std::same_as<T, wchar_t>)
struct ToString<T[N]> {
    static std::string toString(const T (&str)[N]) {
        return toByteString(std::wstring{str, N - 1}); // - 1 是为了去掉 '\0'
    }

    template <typename Stream>
    static void toString(const T (&str)[N], Stream& s) {
        s.append(toByteString(std::wstring{str, N - 1})); // - 1 是为了去掉 '\0'
    }
};

// C风格字符串指针 char
template <typename T>
    requires (std::same_as<const T*, const char*>)
struct ToString<const T*> {
    static std::string toString(const T* const& str) {
        return std::string{str};
    }

    template <typename Stream>
    static void toString(const T* const& str, Stream& s) {
        s.append(std::string{str});
    }
};

// C风格字符串指针 wchar_t
template <typename T>
    requires (std::same_as<const T*, const wchar_t*>)
struct ToString<const T*> {
    static std::string toString(const T* const& str) {
        return toByteString(std::wstring{str});
    }

    template <typename Stream>
    static void toString(const T* const& str, Stream& s) {
        s.append(toByteString(std::wstring{str}));
    }
};

// C风格指针
template <typename T>
struct ToString<T*> {
    static std::string toString(T* const& p) {
        return HX::STL::utils::NumericBaseConverter::hexadecimalConversion(static_cast<std::size_t>(p));
    }

    template <typename Stream>
    static void toString(T* const& p, Stream& s) {
        s.append(HX::STL::utils::NumericBaseConverter::hexadecimalConversion(static_cast<std::size_t>(p)));
    }
};

// 鸭子类型: 存在`toString`成员函数
template <ToStringClassType T>
struct ToString<T> {
    static std::string toString(const T& t) {
        return t.toString();
    }
};

// 鸭子类型: 存在`toJson`成员函数
template <ToJsonClassType T>
struct ToString<T> {
    static std::string toString(const T& t) {
        return t.toJson();
    }
};

/**
 * @brief 编译器静态遍历所有tuple的成员, 并且toString
 * @tparam Ts tuple的成员类型包
 * @tparam Is tuple的成员数量
 * @param tup 需要打印的东西
 * @return std::string 
 */
template <typename... Ts, std::size_t... Is>
std::string tupleToString(
    const std::tuple<Ts...>& tup,
    std::index_sequence<Is...>
) {
    std::string res;
    res += '(';
    ((
        res += ToString<HX::STL::utils::remove_cvref_t<decltype(std::get<Is>(tup))>>::toString(std::get<Is>(tup)), 
        res += ','
    ), ...);
    res.back() = ')';
    return res;
}

template <typename Stream, typename... Ts, std::size_t... Is>
void tupleToString(
    const std::tuple<Ts...>& tup,
    Stream& s,
    std::index_sequence<Is...>
) {
    s.push_back('(');
    ((
        ToString<HX::STL::utils::remove_cvref_t<decltype(std::get<Is>(tup))>>::toString(std::get<Is>(tup), s), 
        s.push_back(',')
    ), ...);
    s.back() = ')';
}

// tuple
template <typename... Ts>
struct ToString<std::tuple<Ts...>> {
    static std::string toString(const std::tuple<Ts...>& tup) {
        return tupleToString(tup, std::make_index_sequence<sizeof...(Ts)>());
    }

    template <typename Stream>
    static void toString(const std::tuple<Ts...>& tup, Stream& s) {
        tupleToString(tup, s, std::make_index_sequence<sizeof...(Ts)>());
    }
};

} // namespace internal

/**
 * @brief toString 全部参数
 * @return std::string 
 */
template <typename T0, typename... Ts>
inline std::string toStrings(T0 const& t0, Ts const&... ts) {
    std::string res;
    res += internal::ToString<T0>::toString(t0);
    ((res += internal::ToString<Ts>::toString(ts)), ...);
    return res;
}

/**
 * @brief 将`t`转换为字符串
 * @tparam T 
 * @param t 
 * @return std::string 转化结果
 */
template <typename T>
inline std::string toString(T const& t) {
    return internal::ToString<T>::toString(t);
}

/**
 * @brief 将`t`转为字符串, 然后存入`s`中
 * @tparam T 
 * @tparam Stream 
 * @param t 
 * @param s 字符串
 */
template <typename T, typename Stream>
inline void toString(T&& t, Stream& s) {
    internal::ToString<HX::STL::utils::remove_cvref_t<T>>::toString(std::forward<T>(t), s);
}

}}} // namespace HX::STL::utils

// 恢复删除的警告
#if defined(_MSC_VER) // MSVC
    #pragma warning(pop)
    #pragma warning(pop)
    #pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__) // GCC 和 Clang
    #pragma GCC diagnostic pop
    #pragma GCC diagnostic pop
    #pragma GCC diagnostic pop
#endif

#endif // !_HX_TO_STRING_H_