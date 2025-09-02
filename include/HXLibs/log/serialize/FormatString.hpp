#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-12 11:04:09
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
#include <thread>
#include <filesystem>
#if __cplusplus < 202302L
#include <iosfwd>
#endif
#include <format>

#include <HXLibs/meta/ContainerConcepts.hpp>
#include <HXLibs/reflection/MemberName.hpp>
#include <HXLibs/reflection/EnumName.hpp>
#include <HXLibs/utils/NumericBaseConverter.hpp>
#include <HXLibs/log/serialize/CustomToString.hpp>

namespace HX::log {

namespace internal {
    
struct FormatString {
    inline static constexpr std::string_view DELIMIER          = ", ";
    inline static constexpr std::string_view ENTER             = "\n";
    inline static constexpr std::string_view PARENTHESES_LEFT  = "(";
    inline static constexpr std::string_view PARENTHESES_RIGHT = ")";
    inline static constexpr std::string_view BRACKET_LEFT      = "[";
    inline static constexpr std::string_view BRACKET_RIGHT     = "]";
    inline static constexpr std::string_view KEY_VAK_PAIR      = ": ";
    inline static constexpr std::string_view BRACE_LEFT        = "{";
    inline static constexpr std::string_view BRACE_DELIMIER    = ",\n";
    inline static constexpr std::string_view BRACE_RIGHT       = "}";
    inline static constexpr std::string_view QUOTATION_MARKS   = "\"";
    
    /**
     * @brief 辅助类
     */
    struct DepthRAII {
        constexpr DepthRAII(std::size_t& v) noexcept
            : _v{v}
        {
            ++_v;
        }

        DepthRAII& operator=(DepthRAII&&) noexcept = delete;

        constexpr ~DepthRAII() noexcept {
            --_v;
        }

        std::size_t& _v;
    };

    /**
     * @brief 前置缩进
     * @tparam Stream 
     * @param s 
     */
    template <bool isEnter = false, typename Stream>
    constexpr void addIndent(Stream& s) {
        using namespace std::string_view_literals;
        if constexpr (isEnter) {
            s.append("\n"sv);
        }
        for (std::size_t i = 0; i < _depth; ++i) {
            s.append(_indentStr);
        }
    }

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
        using namespace std::string_literals;
        s.append(t ? "true"s : "false"s);;
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
        using namespace std::string_literals;
        s.append("null"s);
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
        bool notNull = false;
        {
            if constexpr (Cnt > 0) {
                DepthRAII _{_depth};
                reflection::forEach(obj, [&] <std::size_t I> (
                    std::index_sequence<I>, auto name, auto const& val
                ) {
                    if (!notNull) {
                        res += ENTER;
                        notNull = true;
                    }
                    addIndent(res);
                    make(name, res);
                    res += KEY_VAK_PAIR;
                    make(val, res);
    
                    if constexpr (I < Cnt - 1) {
                        res += BRACE_DELIMIER;
                    }
                });
            }
        }
        if (notNull) {
            addIndent<true>(res);
        }
        res += BRACE_RIGHT;
        return res;
    }

    template <typename T, typename Stream>
        requires (reflection::IsReflective<T>)
    constexpr void make(T const& obj, Stream& s) {
        constexpr std::size_t Cnt = reflection::membersCountVal<T>;
        s.append(BRACE_LEFT);
        bool notNull = false;
        {
            if constexpr (Cnt > 0) {
                DepthRAII _{_depth};
                reflection::forEach(obj, [&] <std::size_t I> (
                    std::index_sequence<I>, auto name, auto const& val
                ) {
                    if (!notNull) {
                        s.append(ENTER);
                        notNull = true;
                    }
                    addIndent(s);
                    make(name, s);
                    s.append(KEY_VAK_PAIR);
                    make(val, s);
    
                    if constexpr (I < Cnt - 1) {
                        s.append(BRACE_DELIMIER);
                    }
                });
            }
        }
        if (notNull) {
            addIndent<true>(s);
        }
        s.append(BRACE_RIGHT);
    }

    // std常见的支持迭代器的键值对容器
    template <meta::KeyValueContainer Container>
    constexpr std::string make(const Container& map) {
        std::string res;
        res += BRACE_LEFT;
        bool notNull = false;
        {
            DepthRAII _{_depth};
            bool once = false;
            for (const auto& [k, v] : map) {
                if (!notNull) {
                    res += ENTER;
                    notNull = true;
                }
                if (once)
                    res += BRACE_DELIMIER;
                else
                    once = true;
                addIndent(res);
                make(k, res);
                res += KEY_VAK_PAIR;
                make(v, res);
            }
        }
        if (notNull) {
            addIndent<true>(res);
        }
        res += BRACE_RIGHT;
        return res;
    }

    template <meta::KeyValueContainer Container, typename Stream>
    constexpr void make(const Container& map, Stream& s) {
        s.append(BRACE_LEFT);
        bool notNull = false;
        {
            DepthRAII _{_depth};
            bool once = false;
            for (const auto& [k, v] : map) {
                if (!notNull) {
                    s.append(ENTER);
                    notNull = true;
                }
                if (once)
                    s.append(BRACE_DELIMIER);
                else
                    once = true;
                addIndent(s);
                make(k, s);
                s.append(KEY_VAK_PAIR);
                make(v, s);
            }
        }
        if (notNull) {
            addIndent<true>(s);
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

    // const char* C字符串指针
    constexpr std::string make(const char* t) {
        return {t};
    }

    template <typename Stream>
    constexpr void make(const char* t, Stream& s) {
        s.append({t});
    }

    // char[N] C字符数组
    template <std::size_t N>
    constexpr std::string make(char const (&t)[N]) {
        return {t, N - 1};
    }

    template <std::size_t N, typename Stream>
    constexpr void make(char const (&t)[N], Stream& s) {
        s.append({t, N - 1});
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
        [&] <std::size_t... Is> (std::index_sequence<Is...>) {
            ((make(std::get<Is>(tp), s), 
              static_cast<void>(Is + 1 < N ? s.append(DELIMIER) : s)
            ), ...);
        }(std::make_index_sequence<N>{});
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
        requires (IsCustomToStringVal<T, FormatString>)
    constexpr std::string make(T const& t) {
        return CustomToString<T, FormatString>{this}.make(t);
    }

    template <typename T, typename Stream>
        requires (IsCustomToStringVal<T, FormatString>)
    constexpr void make(T const& t, Stream& s) {
        CustomToString<T, FormatString>{this}.make(t, s);
    }

    std::size_t _depth = 0; // 嵌套深度
    std::string_view _indentStr{"    ", 4}; // 缩进字符串
};

} // namespace internal

template <typename... Ts>
    requires(sizeof...(Ts) > 0)
inline std::string formatString(Ts const&... ts) {
    std::string res;
    internal::FormatString fs{};
    ((res += fs.make(ts)), ...);
    return res;
}

template <typename T, typename Stream>
inline void formatString(T const& t, Stream& s) {
    internal::FormatString{}.make(t, s);
}

} // namespace HX::log

