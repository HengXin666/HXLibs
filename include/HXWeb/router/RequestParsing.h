#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2024-7-28 12:26:32
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
 * */
#ifndef _HX_REQUEST_PARSING_H_
#define _HX_REQUEST_PARSING_H_

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <charconv>  // for std::from_chars
#include <cstdlib>   // for std::strtod

namespace HX { namespace web { namespace router {

/**
 * @brief 请求模版解析工具类 (配合路由使用的)
 */
struct RequestTemplateParsing {
    /**
     * @brief 将路径通配符解析成数组索引映射
     * @param path 模版路径, 如: `/home/{name}/{id}`
     * @return 数组索引映射, 如: arr[0] = 1, arr[1] = 2 , 因为 [home, {name}, {id}]
     * @warning 只能解析如`{id}`的匹配, 不能解析`**`!
     * @throw 如果解析不到如`{id}`的通配符, 则会抛出异常
     */
    static std::vector<int> getPathWildcardAnalysisArr(std::string_view path);

    /**
     * @brief 获取万用通配符前缀索引, 如: `/home/ **` -> `/home/`
     * @param path 模版路径, 如: `/home/ **`
     * @return `**`位置之前一个单位的索引
     * @warning 只能解析`**`!
     * @throw 如果解析不到如`**`的通配符, 则会抛出异常
     */
    static std::size_t getUniversalWildcardPathBeginIndex(std::string_view path);
};

/**
 * @brief  将通配符元素(字符串)转为指定类型, 转换失败返回`std::nullopt`
 * @tparam T 需要转换的类型
 * @warning 主模版不使用, 请使用偏特化.
 */
template <class T>
struct TypeInterpretation {
    /**
     * @brief 将通配符元素(字符串)转为指定类型, 转换失败返回`std::nullopt`
     */
    static std::optional<T> wildcardElementTypeConversion(std::string_view) {
        static_assert(
            false,
            "Please select the correct type"
        );
    }
};

template <>
struct TypeInterpretation<int32_t> {
    static std::optional<int32_t> wildcardElementTypeConversion(std::string_view we) {
        int32_t value;
        auto result = std::from_chars(we.data(), we.data() + we.size(), value);
        if (result.ec == std::errc() && result.ptr == we.data() + we.size()) {
            return value;
        }
        return std::nullopt;
    }
};

template <>
struct TypeInterpretation<int16_t> {
    static std::optional<int16_t> wildcardElementTypeConversion(std::string_view we) {
        int16_t value;
        auto result = std::from_chars(we.data(), we.data() + we.size(), value);
        if (result.ec == std::errc() && result.ptr == we.data() + we.size()) {
            return value;
        }
        return std::nullopt;
    }
};

template <>
struct TypeInterpretation<int64_t> {
    static std::optional<int64_t> wildcardElementTypeConversion(std::string_view we) {
        int64_t value;
        auto result = std::from_chars(we.data(), we.data() + we.size(), value);
        if (result.ec == std::errc() && result.ptr == we.data() + we.size()) {
            return value;
        }
        return std::nullopt;
    }
};

template <>
struct TypeInterpretation<u_int32_t> {
    static std::optional<u_int32_t> wildcardElementTypeConversion(std::string_view we) {
        u_int32_t value;
        auto result = std::from_chars(we.data(), we.data() + we.size(), value);
        if (result.ec == std::errc() && result.ptr == we.data() + we.size()) {
            return value;
        }
        return std::nullopt;
    }
};

template <>
struct TypeInterpretation<u_int16_t> {
    static std::optional<u_int16_t> wildcardElementTypeConversion(std::string_view we) {
        u_int16_t value;
        auto result = std::from_chars(we.data(), we.data() + we.size(), value);
        if (result.ec == std::errc() && result.ptr == we.data() + we.size()) {
            return value;
        }
        return std::nullopt;
    }
};

template <>
struct TypeInterpretation<u_int64_t> {
    static std::optional<u_int64_t> wildcardElementTypeConversion(std::string_view we) {
        u_int64_t value;
        auto result = std::from_chars(we.data(), we.data() + we.size(), value);
        if (result.ec == std::errc() && result.ptr == we.data() + we.size()) {
            return value;
        }
        return std::nullopt;
    }
};

template <>
struct TypeInterpretation<float> {
    static std::optional<float> wildcardElementTypeConversion(std::string_view we) {
        char* end;
        float value = std::strtof(we.data(), &end);
        if (end == we.data() + we.size()) {
            return value;
        }
        return std::nullopt;
    }
};

template <>
struct TypeInterpretation<double> {
    static std::optional<double> wildcardElementTypeConversion(std::string_view we) {
        char* end;
        double value = std::strtod(we.data(), &end);
        if (end == we.data() + we.size()) {
            return value;
        }
        return std::nullopt;
    }
};

template <>
struct TypeInterpretation<long double> {
    static std::optional<long double> wildcardElementTypeConversion(std::string_view we) {
        char* end;
        long double value = std::strtold(we.data(), &end);
        if (end == we.data() + we.size()) {
            return value;
        }
        return std::nullopt;
    }
};

template <>
struct TypeInterpretation<std::string> {
    static std::optional<std::string> wildcardElementTypeConversion(std::string& we) {
        return we; // 这里是非严格的模版特化, 单纯为了提高性能
    }
};

template <>
struct TypeInterpretation<std::string_view> {
    static std::optional<std::string_view> wildcardElementTypeConversion(std::string_view we) {
        return we;
    }
};

template <>
struct TypeInterpretation<bool> {
    static std::optional<bool> wildcardElementTypeConversion(std::string_view we) {
        if (we.size() == 1) {
            if (we[0] == '0')
                return false;
            if (we[0] == '1')
                return true;
        } else if (we.size() == 4) {
            if ((we[0] == 't' || we[0] == 'T') &&
                (we[1] == 'r' || we[1] == 'R') &&
                (we[2] == 'u' || we[2] == 'U') &&
                (we[3] == 'e' || we[3] == 'E'))
                return true;
        } else if (we.size() == 5) {
            if ((we[0] == 'f' || we[0] == 'F') &&
                (we[1] == 'a' || we[1] == 'A') &&
                (we[2] == 'l' || we[2] == 'L') &&
                (we[3] == 's' || we[3] == 'S') &&
                (we[4] == 'e' || we[4] == 'E'))
                return false;
        }
        return std::nullopt;
    }
};

}}} // namespace HX::web::router

#endif // _HX_REQUEST_PARSING_H_
