#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-01-16 19:30:27
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
#include <variant>
#include <format>

#include <HXLibs/meta/ContainerConcepts.hpp>
#include <HXLibs/reflection/MemberName.hpp>
#include <HXLibs/reflection/EnumName.hpp>
#include <HXLibs/utils/NumericBaseConverter.hpp>

namespace HX::reflection {

namespace internal {

namespace cv {

// https://github.com/Tencent/rapidjson/blob/master/include/rapidjson/encodings.h
inline unsigned char GetRange(unsigned char c) {
  static const unsigned char type[] = {
      0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
      0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
      0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
      0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
      0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
      0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
      0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
      0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
      0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
      0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
      0,    0,    0,    0,    0,    0,    0,    0,    0x10, 0x10, 0x10, 0x10,
      0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
      0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
      0x40, 0x40, 0x40, 0x40, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
      0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
      0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
      8,    8,    2,    2,    2,    2,    2,    2,    2,    2,    2,    2,
      2,    2,    2,    2,    2,    2,    2,    2,    2,    2,    2,    2,
      2,    2,    2,    2,    2,    2,    2,    2,    10,   3,    3,    3,
      3,    3,    3,    3,    3,    3,    3,    3,    3,    4,    3,    3,
      11,   6,    6,    6,    5,    8,    8,    8,    8,    8,    8,    8,
      8,    8,    8,    8,
  };
  return type[c];
}

// https://github.com/Tencent/rapidjson/blob/master/include/rapidjson/encodings.h
template <typename Ch = char, typename It>
inline bool decodeUtf8(It&& it, unsigned& codepoint) {
    auto c = *(it++);
    bool result = true;
    auto copy = [&]() constexpr {
        c = *(it++);
        codepoint = (codepoint << 6) | (static_cast<unsigned char>(c) & 0x3Fu);
    };
    auto trans = [&](unsigned mask) constexpr {
        result &= ((GetRange(static_cast<unsigned char>(c)) & mask) != 0);
    };
    auto tail = [&]() constexpr {
        copy();
        trans(0x70);
    };
    if (!(c & 0x80)) {
        codepoint = static_cast<unsigned char>(c);
        return true;
    }
    unsigned char type = GetRange(static_cast<unsigned char>(c));
    if (type >= 32) {
        codepoint = 0;
    } else {
        codepoint = (0xFFu >> type) & static_cast<unsigned char>(c);
    }
    switch (type) {
    case 2: tail(); return result;
    case 3:
        tail();
        tail();
        return result;
    case 4:
        copy();
        trans(0x50);
        tail();
        return result;
    case 5:
        copy();
        trans(0x10);
        tail();
        tail();
        return result;
    case 6:
        tail();
        tail();
        tail();
        return result;
    case 10:
        copy();
        trans(0x20);
        tail();
        return result;
    case 11:
        copy();
        trans(0x60);
        tail();
        tail();
        return result;
    default: return false;
    }
}

template <typename Stream, typename Ch>
inline void writeUnicodeToString(Ch& it, Stream& ss) {
    static const char hexDigits[16] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
    };
    unsigned codepoint = 0;
    if (!decodeUtf8(it, codepoint)) [[unlikely]] {
        throw std::runtime_error("illegal unicode character");
    }
    ss.push_back('\\');
    ss.push_back('u');

    if (codepoint <= 0xD7FF || (codepoint >= 0xE000 && codepoint <= 0xFFFF)) {
        ss.push_back(hexDigits[(codepoint >> 12) & 15]);
        ss.push_back(hexDigits[(codepoint >> 8) & 15]);
        ss.push_back(hexDigits[(codepoint >> 4) & 15]);
        ss.push_back(hexDigits[(codepoint) & 15]);
    } else {
        if (codepoint < 0x010000 || codepoint > 0x10FFFF) [[unlikely]] {
            throw std::runtime_error("illegal codepoint");
        }
        // Surrogate pair
        unsigned s = codepoint - 0x010000;
        unsigned lead = (s >> 10) + 0xD800;
        unsigned trail = (s & 0x3FF) + 0xDC00;
        ss.push_back(hexDigits[(lead >> 12) & 15]);
        ss.push_back(hexDigits[(lead >> 8) & 15]);
        ss.push_back(hexDigits[(lead >> 4) & 15]);
        ss.push_back(hexDigits[(lead) & 15]);
        ss.push_back('\\');
        ss.push_back('u');
        ss.push_back(hexDigits[(trail >> 12) & 15]);
        ss.push_back(hexDigits[(trail >> 8) & 15]);
        ss.push_back(hexDigits[(trail >> 4) & 15]);
        ss.push_back(hexDigits[(trail) & 15]);
    }
}

// https://github.com/Tencent/rapidjson/blob/master/include/rapidjson/writer.h
template <typename Ch, typename SizeType, typename Stream>
inline void writeStringWithEscape(
    const Ch* it,
    SizeType length,
    Stream& s
) {
    static const char hexDigits[16] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
    };
    static const char escape[256] = {
#define Z16 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        // 0    1    2    3    4    5    6    7    8    9    A    B    C    D    E
        // F
        'u', 'u', 'u',  'u', 'u', 'u', 'u', 'u', 'b', 't',
        'n', 'u', 'f',  'r', 'u', 'u',  // 00
        'u', 'u', 'u',  'u', 'u', 'u', 'u', 'u', 'u', 'u',
        'u', 'u', 'u',  'u', 'u', 'u',  // 10
        0,   0,   '"',  0,   0,   0,   0,   0,   0,   0,
        0,   0,   0,    0,   0,   0,  // 20
        Z16, Z16,                     // 30~4F
        0,   0,   0,    0,   0,   0,   0,   0,   0,   0,
        0,   0,   '\\', 0,   0,   0,                       // 50
        Z16, Z16, Z16,  Z16, Z16, Z16, Z16, Z16, Z16, Z16  // 60~FF
#undef Z16
  };
    auto end = it;
    std::advance(end, length);
    while (it < end) {
        if (static_cast<unsigned>(*it) >= 0x80) [[unlikely]] {
            writeUnicodeToString(it, s);
        } else if (escape[static_cast<unsigned char>(*it)]) [[unlikely]] {
            s.push_back('\\');
            s.push_back(escape[static_cast<unsigned char>(*it)]);

            if (escape[static_cast<unsigned char>(*it)] == 'u') {
                // 转义其他控制字符
                s.push_back('0');
                s.push_back('0');
                s.push_back(hexDigits[static_cast<unsigned char>(*it) >> 4]);
                s.push_back(hexDigits[static_cast<unsigned char>(*it) & 0xF]);
            }
            ++it;
        } else {
            s.push_back(*(it++));
        }
    }
}

} // namespace cv

struct ToJson {
    // template <typename T, typename S>
    // static void toJson(T const&, S&) {
    //     // json 不支持序列化该类型
    //     static_assert(false, "JSON does not support serialization of this type");
    // }

    // 聚合类
    template <typename T, typename S>
        requires (reflection::IsReflective<T>)
    static void toJson(T const& t, S& s) {
        s.push_back('{');
        constexpr auto N = reflection::membersCountVal<T>;
        reflection::forEach(
            t, 
            [&] <std::size_t I> (std::index_sequence<I>, std::string_view name, auto const& val) {
                toJson(name, s);
                s.push_back(':');
                toJson(val, s);
                if constexpr (I + 1 != N) {
                    s.push_back(',');
                }
            }
        );
        s.push_back('}');
    }

    // 布尔
    template <typename S>
    static void toJson(bool v, S& s) {
        s.append(v ? "true" : "false");
    }

    // 空
    template <typename NullType, typename S>
        requires (std::is_same_v<NullType, std::nullopt_t>
               || std::is_same_v<NullType, std::nullptr_t>
               || std::is_same_v<NullType, std::monostate>)
    static void toJson(NullType const&, S& s) {
        s.append("null");
    }

    // 数字
    template <typename T, size_t NF = static_cast<size_t>(-1), typename S>
        requires (std::is_integral_v<T> || std::is_floating_point_v<T>)
    static void toJson(T const& t, S& s) {
        if constexpr (NF != static_cast<size_t>(-1)) {
            s.append(std::format("{:." + std::format("{}", NF) + "f}", t));
        } else {
            s.append(std::format("{}", t));
        }
    }

    // 枚举
    template <typename E, typename S>
        requires (std::is_enum_v<E>)
    static void toJson(E const& t, S& s) {
        toJson(reflection::toEnumName(t), s);
    }

    // 字符串
    template <typename T, typename S>
        requires (meta::StringType<T> || meta::WStringType<T>)
    static void toJson(T const& t, S& s) {
        s.push_back('"');
        cv::writeStringWithEscape(t.data(), t.size(), s);
        s.push_back('"');
    }

    // c_arr
    template <typename T, std::size_t N, typename Stream>
    static void toJson(const T (&arr)[N], Stream& s) {
        if constexpr (std::is_same_v<T, char> || std::is_same_v<T, wchar_t>) {
            toJson(std::basic_string_view<T>{arr, N}, s);
        } else {
            toJson(std::span{arr}, s);
        }
    }

    // list
    template <meta::SingleElementContainer Container, typename Stream>
    static void toJson(Container const& arr, Stream& s) {
        s.push_back('[');
        auto it = arr.begin(), end = arr.end();
        if (it == end) {
            s.push_back(']');
            return;
        }
        while (it != end) {
            toJson(*it++, s);
            s.push_back(',');
        }
        s.back() = ']';
    }

    // pair - list (map)
    template <meta::KeyValueContainer Container, typename Stream>
    static void toJson(Container const& arr, Stream& s) {
        s.push_back('{');
        auto it = arr.begin(), end = arr.end();
        if (it == end) {
            s.push_back('}');
            return;
        }
        while (it != end) {
            auto const& [k, v] = *it++;
            toJson(k, s);
            s.push_back(':');
            toJson(v, s);
            s.push_back(',');
        }
        s.back() = '}';
    }

    // opt
    template <typename T, typename S>
        requires (meta::is_optional_v<T>)
    static void toJson(T const& t, S& s) {
        t ? toJson(*t, s) : toJson(std::nullopt, s);
    }

    // variant
    template <typename... Ts, typename Stream>
    static void toJson(std::variant<Ts...> const& v, Stream& s) {
        std::visit([&](auto&& val) {
            toJson(val, s);
        }, v);
    }

    // 智能指针
    template <typename T, typename S>
        requires (meta::is_smart_pointer_v<T>)
    static void toJson(T const& t, S& s) {
        t ? toJson(*t, s) : toJson(std::nullptr_t{}, s);
    }
};

} // namespace internal

/**
 * @brief 将obj反射为json字符串
 * @tparam Obj 
 * @tparam Stream 要求支持 `.append()` 方法
 * @param obj 
 * @param s 
 */
template <typename Obj, typename Stream>
inline void toJson(Obj const& obj, Stream& s) {
    internal::ToJson::toJson(obj, s);
}

} // namespace HX::reflection

