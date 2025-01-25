#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2024-7-19 13:49:22
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
#ifndef _HX_STRING_UTILS_H_
#define _HX_STRING_UTILS_H_

#include <vector>
#include <string>
#include <string_view>

#include <HXprint/print.h>

namespace HX { namespace STL { namespace utils {

/**
 * @brief 字符串操作工具类
 */
struct StringUtil final {
    /**
     * @brief 将字符串按`delimiter`分割为数组
     * @tparam Str 字符串类型, 可以是`std::string`或者`std::string_view`等
     * @tparam SkipEmpty 是否跳过切割出来的空的字符串
     * @param str 需要分割的字符串
     * @param delim 分割字符串
     * @param res 待返回数组, 你可以事先在前面插入元素
     * @return std::vector<Str> 
     */
    template <typename Str, bool SkipEmpty = true>
    inline static std::vector<Str> split(
        std::string_view str,
        std::string_view delim, 
        std::vector<Str> res = std::vector<Str>{}
    ) {
        if (str.empty()) 
            return res;

        std::size_t start = 0;
        std::size_t end = str.find(delim, start);
        while (end != std::string_view::npos) {
            if constexpr (SkipEmpty) {
                auto tk = str.substr(start, end - start);
                if (tk.size()) {
                    res.emplace_back(std::move(tk));
                }
            } else {
                res.emplace_back(str.substr(start, end - start));
            }
            start = end + delim.size();
            end = str.find(delim, start);
        }

        // 添加最后一个分割的部分
        if constexpr (SkipEmpty) {
            auto tk = str.substr(start);
            if (tk.size()) {
                res.emplace_back(std::move(tk));
            }
        } else {
            res.emplace_back(str.substr(start));
        }
        return res;
    }

    /**
     * @brief 将字符串按从左到右第一个`delimiter`分割为两半
     * @param str 需要分割的字符串
     * @param delimiter 分割字符
     * @return 分割后的数组, 失败返回: `{"", ""}`
     */
    inline static std::pair<std::string, std::string> splitAtFirst(
        std::string_view str,
        std::string_view delim
    ) {
        std::pair<std::string, std::string> res;
        std::size_t pos = str.find(delim);
        if (pos != std::string_view::npos) {
            res.first = str.substr(0, pos);
            res.second = str.substr(pos + delim.size());
        } else {
            res.first = res.second = "";
        }
        return res;
    }

    /**
     * @brief 将字符串转为小写字母
     * @param str [in, out] 待处理字符串
     */
    inline static void toSmallLetter(std::string& str) {
        const std::size_t n = str.size();
        for (std::size_t i = 0; i < n; ++i)
            if ('A' <= str[i] && str[i] <= 'Z')
                str[i] ^= ' ';
    }

    /**
     * @brief 从右到左查找字符并截取后面的子字符串
     * @param str 目标字符串
     * @param delim 模版字符串
     * @return 截取后面的子字符串, 如果没有则返回: 返回原字符串
     */
    inline static std::string rfindAndTrim(std::string_view str, std::string_view delim) {
        std::size_t pos = str.rfind(delim);
        if (pos != std::string::npos) {
            return std::string {str.substr(pos + 1)};
        }
        return std::string {str}; // 如果未找到分隔符, 返回原字符串
    }
};

/**
 * @brief 时间格式工具类
 */
struct DateTimeFormat final {
    /**
     * @brief 格式化当前时间为如: `%Y-%m-%d %H:%M:%S`的格式
     * @param fmt 格式字符串
     * @return 当前时间格式化都的字符串
     */
    static std::string format(const std::string& fmt = "%Y-%m-%d %H:%M:%S");

    /**
     * @brief 格式化当前时间为如: `%Y-%m-%d %H:%M:%S`的格式, 并且带毫秒时间获取
     * @param fmt 格式字符串
     * @param msDelim 毫秒与前部分的分割符，默认是空格
     * @return 当前时间格式化都的字符串
     */
    static std::string formatWithMilli(const std::string& fmt = "%Y-%m-%d %H:%M:%S", const std::string msDelim = " ");
};

}}} // namespace HX::STL::utils

#endif // _HX_STRING_UTILS_H_
