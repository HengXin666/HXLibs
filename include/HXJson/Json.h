#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2024-7-16 13:51:13
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
#ifndef _HX_JSON_H_
#define _HX_JSON_H_

#include <utility>
#include <type_traits>
#include <variant>
#include <vector>
#include <unordered_map>
#include <string>
#include <string_view>
#include <optional>
#include <regex>
#include <charconv>

#include <HXSTL/utils/TypeTraits.hpp>

// 下期目标: 实现一个将Json数据转存为Json文件的Code
// 目前的转化感觉有点不优雅...

namespace HX { namespace json {

namespace internal {

struct NumerString {
    std::string num;

    explicit NumerString(std::string&& _num)
        : num(std::move(_num)) 
    {}

    std::string toString() const {
        return num;
    }

    // 由于`std::variant`中有用到 拷贝 (普通的JsonObject::get时候) 因此不能删除
    // NumerString(const NumerString&) = delete; // 禁止拷贝构造
    // NumerString& operator=(const NumerString&) = delete; // 禁止拷贝赋值

    // NumerString(NumerString&&) noexcept = default; // 默认移动构造
    // NumerString& operator=(NumerString&&) noexcept = default; // 默认移动赋值

    bool operator==(const NumerString& that) const {
        return that.num == num;
    }
};

template <class T>
std::optional<T> try_parse_num(std::string_view str) {
    T value;
    // std::from_chars 尝试将 str 转为 T(数字)类型的值, 返回值是一个 tuple<指针 , T>
    // 值得注意的是 from_chars 不识别指数外的正号(在起始位置只允许出现负号)
    // 具体请见: https://zh.cppreference.com/w/cpp/utility/from_chars
    auto res = std::from_chars(str.data(), str.data() + str.size(), value);
    if (res.ec == std::errc() && 
        res.ptr == str.data() + str.size()) { // 必需保证整个str都是数字
        return value;
    }
    return std::nullopt;
}

char unescaped_char(char c);

// 跳过末尾的空白字符 如: [1      , 2]
std::size_t skipTail(std::string_view json, std::size_t i, char ch);

} // namespace internal

struct JsonObject;

using JsonList = std::vector<JsonObject>;
using JsonDict = std::unordered_map<std::string, JsonObject>;

/**
 * @todo 需要支持将内容移动, 而不是拷贝!
 * @todo 需要支持指定解析, 如即便是数字, 也是可以 get<std::string> / get<int> / get<double>
 * @todo 建议: 上面的数字, 可以先存储为std::string? 然后get时候, 再进行实例化
 */

/**
 * @brief Json解析中间对象
 */
struct JsonObject {
    using JsonData = std::variant
    < std::nullptr_t  // null
    , bool            // true
    , internal::NumerString // index = 2 不可变! 懒存储数字信息, 使用时候再解析
    , std::string     // "hello"
    , JsonList        // [42, "hello"]
    , JsonDict        // {"hello": 985, "world": 211}
    >;

    JsonData _inner;

    explicit JsonObject() : _inner(std::nullptr_t{})
    {}

    explicit JsonObject(JsonData&& data) : _inner(std::move(data)) 
    {}

    /**
     * @brief 打印值
     */
    void print() const;

    /**
     * @brief 转化为字符串形式
     * @return std::string 
     */
    std::string toString() const;

    /**
     * @brief 判断当前类型是否为 T
     * @tparam T 
     * @return true 是
     * @return false 否
     */
    template <typename T>
    bool hasType() const noexcept {
        return std::holds_alternative<T>(_inner);
    }

    /**
     * @brief 安全的获取值
     * @tparam T 需要获取的值的类型
     * @return T 类型的拷贝
     */
    template <typename T, 
        typename = std::enable_if_t<
            HX::STL::utils::has_variant_type_v<T, JsonData>
            && !std::is_same_v<T, std::string>>>
    T get() const {
        return std::get<T>(_inner);
    }

    /**
     * @brief 获取字符串/大数字符串
     * @tparam T 字符串
     * @return T 字符串`std::string`
     */
    template <typename T, 
        typename = std::enable_if_t<std::is_same_v<T, std::string>>,
        typename = void,
        typename = void>
    T get() const {
        if (_inner.index() == 2) {
            return std::get<internal::NumerString>(_inner).num;
        }
        return std::get<std::string>(_inner);
    }

    /**
     * @brief 获取数字
     * @tparam T 需要获取的类型, 如`int`、`double`
     */
    template <typename T, 
        typename = std::enable_if_t<
            !HX::STL::utils::has_variant_type_v<T, JsonData>
            && (std::is_integral_v<T> || std::is_floating_point_v<T>)>,
        typename = void>
    T get() const {
        auto& numStr = std::get<internal::NumerString>(_inner).num;
        T num;
        auto [ptr, ec] = std::from_chars(numStr.data(), numStr.data() + numStr.size(), num);
        if (ec != std::errc()) [[unlikely]] {
            throw std::system_error(std::make_error_code(ec));
        } else if (ptr != numStr.data() + numStr.size()) [[unlikely]] { // 必需保证整个str都是数字
            throw std::runtime_error("Unable to parse all strings to numbers: " + numStr);
        }
        return num;
    }

    /**
     * @brief 获取值 (以移动的方式)
     * @tparam T 需要获取的值的类型
     * @return T 类型的移动
     */
    template <typename T, 
        typename = std::enable_if_t<
            HX::STL::utils::has_variant_type_v<T, JsonData>
            && !std::is_same_v<T, std::string>>>
    T move() {
        return std::move(std::get<T>(_inner));
    }

    template <typename T, 
        typename = std::enable_if_t<std::is_same_v<T, std::string>>,
        typename = void,
        typename = void>
    T move() {
        if (_inner.index() == 2) {
            auto&& res = std::move(std::get<internal::NumerString>(_inner));
            return std::move(res.num);
        }
        return std::move(std::get<std::string>(_inner));
    }

    /**
     * @brief 获取数字
     * @tparam T 需要获取的类型, 如`int`、`double`
     */
    template <typename T, 
        typename = std::enable_if_t<
            !HX::STL::utils::has_variant_type_v<T, JsonData>
            && (std::is_integral_v<T> || std::is_floating_point_v<T>)>,
        typename = void>
    T move() {
        return get<T>(); // 基础类型无需移动
    }

    /**
     * @warning 请保证当前是`JsonList`
     */
    auto& operator [](std::size_t index) {
        return std::get<JsonList>(_inner)[index];
    }

    /**
     * @warning 请保证当前是`JsonDict`
     * @throw 当前不是`JsonDict`
     * @throw `key`不存在
     */
    auto& operator [](const std::string& key) {
        return std::get<JsonDict>(_inner)[key];
    }

    /**
     * @brief 如果非此类型或者索引越界, 则会返回空
     */
    auto operator [](std::size_t index) const {
        auto&& res = get<JsonList>();
        if (res.size() >= index)
            return JsonObject {};
        return res[index];
    }

    /**
     * @brief 如果非此类型或者键不存在, 则会返回空
     */
    auto operator [](const std::string& key) const {
        auto&& dict = get<JsonDict>();
        if (auto it = dict.find(key); it != dict.end()) {
            return it->second;
        }
        return JsonObject {};
    }

    /**
     * @warning 请保证当前是`JsonDict`
     * @throw 当前不是`JsonDict`
     * @throw `key`不存在
     */
    const auto& at(const std::string& key) const {
        // returning reference to local temporary object [-Wreturn-stack-address] GCC (Clang)
        auto&& map = get<JsonDict>();
        return map.at(key);
    }

    bool operator==(const JsonObject& that) const {
        return that._inner == _inner;
    }
};

// 更优性能应该使用栈实现的非递归

/**
 * @brief 解析JSON字符串
 * @param json JSON字符串
 * @return JSON对象, 解析的JSON内容长度
 */
template <bool analysisKey = false>
std::pair<JsonObject, std::size_t> parse(std::string_view json) {
    using namespace std::string_literals;
    if (json.empty()) { // 如果没有内容则返回空
        return {JsonObject{std::nullptr_t{}}, 0};
    } else if (std::size_t off = json.find_first_not_of(" \n\r\t\v\f\0"); off != 0 && off != json.npos) { // 去除空行
        auto [obj, eaten] = parse<analysisKey>(json.substr(off));
        return {std::move(obj), eaten + off};
    } else if ((json[0] >= '0' && json[0] <= '9') || json[0] == '+' || json[0] == '-') { // 如果为数字
        // std::regex num_re{"[+-]?[0-9]+(\\.[0-9]*)?([eE][+-]?[0-9]+)?"};
        std::regex num_re{"[-]?[0-9]+(\\.[0-9]*)?([eE][+-]?[0-9]+)?"s}; // 一个支持识别内容是否为数字的: 1e-12, 114.514, -666
        std::cmatch match; // 匹配结果
        if (std::regex_search(json.data(), json.data() + json.size(), match, num_re)) { // re解析成功
            std::string str = match.str();
            auto size = str.size();
            // 支持识别为 int 或者 double
            // if (auto num = try_parse_num<int>(str)) {
            //     return {JsonObject{*num}, str.size()};
            // } else if (auto num = try_parse_num<long long>(str)) {
            //     return {JsonObject{*num}, str.size()};
            // } else 
            // if (auto num = try_parse_num<double>(str)) {
            //     return {JsonObject{*num}, str.size()};
            // }
            return {JsonObject{internal::NumerString{std::move(str)}}, size};
        }
    } else if (json[0] == '"') { // 识别字符串, 注意, 如果有 \", 那么 这个"是不识别的
        std::string str;
        enum {
            Raw,     // 前面不是'\'
            Escaped, // 前面是个'\'
        } phase = Raw;
        std::size_t i = 1;
        for (; i < json.size(); ++i) {
            char ch = json[i];
            if (phase == Raw) {
                if (ch == '\\') {
                    phase = Escaped;
                } else if (ch == '"') {
                    i += 1;
                    break;
                } else {
                    str += ch;
                }
            } else if (phase == Escaped) {
                str += internal::unescaped_char(ch); // 处理转义字符
                phase = Raw;
            }
        }
        return {JsonObject{std::move(str)}, i};
    } else if (json[0] == '[') { // 解析列表
        JsonList res;
        std::size_t i = 1;
        for (; i < json.size(); ) {
            if (json[i] == ']') {
                i += 1;
                break;
            }
            auto [obj, eaten] = parse(json.substr(i)); // 递归调用
            if (eaten == 0) {
                i = 0;
                break;
            }
            i += eaten;
            res.emplace_back(std::move(obj));

            i += internal::skipTail(json, i, ',');
        }
        return {JsonObject{std::move(res)}, i};
    } else if (json[0] == '{') { // 解析字典, 如果Key重复, 则使用最新的Key的Val
        JsonDict res;
        std::size_t i = 1;
        for (; i < json.size(); ) {
            if (json[i] == '}') {
                i += 1;
                break;
            }

            // 需要支持解析 不带双引号的 Key
            auto [key, keyEaten] = parse<true>(json.substr(i));
            
            if (keyEaten == 0) {
                i = 0;
                break;
            }
            i += keyEaten;
            if (!std::holds_alternative<std::string>(key._inner)) {
                i = 0;
                break;
            }

            i += internal::skipTail(json, i, ':');

            auto [val, valEaten] = parse(json.substr(i));
            if (valEaten == 0) {
                i = 0;
                break;
            }
            i += valEaten;

            res.emplace(key.move<std::string>(), std::move(val));

            i += internal::skipTail(json, i, ',');
        }
        return {JsonObject{std::move(res)}, i};
    } else if constexpr (analysisKey) { // 解析Key不带 ""
        if (std::size_t off = json.find_first_of(": \n\r\t\v\f\0"s); off != json.npos)
            return {JsonObject{std::string{json.substr(0, off)}}, off};
    } else if (json.size() > 3) { // 解析 null, false, true
        switch (json[0]) {
        case 'n':
            if (json[1] == 'u' && 
                json[2] == 'l' && 
                json[3] == 'l')
                return {JsonObject{std::nullptr_t{}}, 4};
            break;
        case 't':
            if (json[1] == 'r' && 
                json[2] == 'u' && 
                json[3] == 'e')
                return {JsonObject{true}, 4};
            break;
        case 'f':
            if (json.size() == 5 && 
                json[1] == 'a' && 
                json[2] == 'l' && 
                json[3] == 's' && 
                json[4] == 'e')
                return {JsonObject{false}, 5};
            break;
        default:
            break;
        }
    }
    return {JsonObject{std::nullptr_t{}}, 0};
}

}} // namespace HX::json

#endif // _HX_JSON_H_