#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2024-08-31 21:12:33
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
#include <string>
#include <string_view>
#include <unordered_map>

namespace HX::net {

namespace internal {

// 将单个字符转为小写
constexpr unsigned char asciiToLower(unsigned char c) noexcept {
    return (c >= 'A' && c <= 'Z') ? static_cast<unsigned char>(c + 32) : c;
}

// 将单个字符转为小写(仅处理ASCII)
inline unsigned char toLower(unsigned char c) noexcept {
    static constexpr auto table = []() constexpr {
        std::array<unsigned char, 256> table{};
        for (std::size_t i = 0; i < 256; ++i) {
            table[i] = asciiToLower(static_cast<unsigned char>(i));
        }
        return table;
    }();
    return table[static_cast<std::size_t>(c)];
}

// 忽略大小写的hash
struct TransparentCaseInsensitiveHash {
    using is_transparent = void;

    std::size_t operator()(std::string_view sv) const noexcept {
        // FNV-1a或std::hash<string_view>都行, 这里FNV-1a性能较好
        std::size_t hash = 1469598103934665603ULL;
        for (char c : sv) {
            hash ^= asciiToLower(static_cast<unsigned char>(c));
            hash *= 1099511628211ULL;
        }
        return hash;
    }
};

// 忽略大小写比较
struct TransparentCaseInsensitiveEqual {
    using is_transparent = void;

    bool operator()(std::string_view lhs, std::string_view rhs) const noexcept {
        if (lhs.size() != rhs.size()) {
            return false;
        }
        const unsigned char* p1 = reinterpret_cast<const unsigned char*>(lhs.data());
        const unsigned char* p2 = reinterpret_cast<const unsigned char*>(rhs.data());
        for (std::size_t i = 0; i < lhs.size(); ++i) {
            if (toLower(p1[i]) != toLower(p2[i])) {
                return false;
            }
        }
        return true;
    }
};

} // namespace internal

class Http {};

enum class HttpMethod {
    NIL = 0,
    GET,
    HEAD,
    POST,
    PUT,
    TRACE,
    PATCH,
    CONNECT,
    OPTIONS,
    DEL,
};

inline constexpr auto WS = HttpMethod::GET; // WebSocket
inline constexpr auto GET = HttpMethod::GET;
inline constexpr auto POST = HttpMethod::POST;
inline constexpr auto DEL = HttpMethod::DEL;
inline constexpr auto HEAD = HttpMethod::HEAD;
inline constexpr auto PUT = HttpMethod::PUT;
inline constexpr auto CONNECT = HttpMethod::CONNECT;

inline constexpr std::string_view getMethodStringView(HttpMethod const mthd) {
    using namespace std::string_view_literals;
    switch (mthd) {
    case HttpMethod::DEL:     return "DELETE"sv;
    case HttpMethod::GET:     return "GET"sv;
    case HttpMethod::HEAD:    return "HEAD"sv;
    case HttpMethod::POST:    return "POST"sv;
    case HttpMethod::PUT:     return "PUT"sv;
    case HttpMethod::PATCH:   return "PATCH"sv;
    case HttpMethod::CONNECT: return "CONNECT"sv;
    case HttpMethod::OPTIONS: return "OPTIONS"sv;
    case HttpMethod::TRACE:   return "TRACE"sv;
    default:                  return "NIL"sv;
    }
}

inline constexpr std::string_view CRLF{"\r\n", 2};

inline constexpr std::string_view HEADER_SEPARATOR_SV   = ": ";
inline constexpr std::string_view HEADER_END_SV         = "\r\n\r\n";
inline constexpr std::string_view CONTENT_LENGTH_SV     = "content-length";
inline constexpr std::string_view CONTENT_TYPE_SV       = "content-type";
inline constexpr std::string_view TRANSFER_ENCODING_SV  = "transfer-encoding";
inline constexpr std::string_view CONNECTION_SV         = "connection";

/**
 * @brief 实现了 std::string 作为 Key, 但是可以通过std::string_view 进行查找的功能
 */
using HeaderHashMap = std::unordered_map<
    std::string, 
    std::string, 
    internal::TransparentCaseInsensitiveHash, 
    internal::TransparentCaseInsensitiveEqual
>;

/**
 * @brief 断点续传参数包
 */
struct RangeRequestView {
    std::string_view reqType; // 请求类型
    HeaderHashMap const& reqHead;  // 请求头
};

} // namespace HX::net

