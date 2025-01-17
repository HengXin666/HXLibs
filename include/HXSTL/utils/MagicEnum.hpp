#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2024-7-26 17:03:41
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
#ifndef _HX_MAGIC_ENUM_H_
#define _HX_MAGIC_ENUM_H_

#include <string>

// #define LOG(str)
// std::cout << __FILE__ << " -> (" << __LINE__ << ") in [" << __PRETTY_FUNCTION__ << "]: " << str << '\n';

namespace HX { namespace STL { namespace utils { namespace MagicEnum {

/// @brief 自用命名空间
namespace _ {

/// @brief 编译器递归深度: [-MAGIC_ENUM_RECURSION_DEPTH, +MAGIC_ENUM_RECURSION_DEPTH]
static constexpr int MAGIC_ENUM_RECURSION_DEPTH = 16;

template <class T, T N>
constexpr const char * _getNameByPrettyFunc() {
#if defined(_MSC_VER)
    return __FUNCSIG__;
#else
    return __PRETTY_FUNCTION__;
#endif
}

template <bool C>
struct HXEnableIf {
};

template <>
struct HXEnableIf<true> {
    using type = void;
};

template <int Begin, int End, class F>
typename HXEnableIf<Begin == End>::type _staticFor(const F& /*func*/) {
}

template <int Begin, int End, class F>
typename HXEnableIf<Begin != End>::type _staticFor(const F& func) {
    func.template call<Begin>();
    _staticFor<Begin + 1, End>(func);
}

template <class T>
struct _GetEnumNameFunctor {
    int _n;
    std::string& _str;

    _GetEnumNameFunctor(int n, std::string& str) : _n(n)
                                                 , _str(str)
    {}

    template <int I>
    constexpr void call() const {
        if (_n == I)
            _str = _getNameByPrettyFunc<T, (T)I>();
    }
};

} /// @bug 模版+函数重载如果相互调用, 请放到相同命名空间下

template <class T, int Begin, int End>
constexpr std::string getEnumName(T n) {
    std::string data;
    _::_staticFor<Begin, End + 1>(_::_GetEnumNameFunctor<T>((int)n, data));
    if (data.empty())
        return "";
#if defined(_MSC_VER)
    size_t pos = s.find(',');
    pos += 1;
    size_t pos2 = s.find('>', pos);
#else
    std::size_t pos = data.find("N = ");
    pos += 4;
    size_t pos2 = data.find_first_of(";]", pos);
#endif
    data = data.substr(pos, pos2 - pos);
    size_t pos3 = data.rfind(':');
    if (pos3 != data.npos)
        data = data.substr(pos3 + 1);
    return data;
}

/**
 * @brief 从枚举值映射到字符串
 * @tparam T 枚举类型
 * @param n 枚举值
 * @return 值对应的字符串, 找不到则为`""`
 * @warning 这个时间复杂度是`O(1)`
 */
template <class T>
constexpr std::string getEnumName(T n) {
    return getEnumName<T, -_::MAGIC_ENUM_RECURSION_DEPTH, _::MAGIC_ENUM_RECURSION_DEPTH>(n);
}

template <class T, T Begin, T End>
constexpr T nameFromEnum(const std::string& name) {
    for (int i = (int)Begin; i <= (int)End; ++i) {
        if (name == getEnumName<T>((T)i))
            return T(i);
    }
    throw;
}

/**
 * @brief 从名字得到枚举值
 * @tparam T 枚举类型
 * @param name 枚举类型::名字
 * @return 对应的值
 * @throw 如果找不到, 则会抛出异常
 * @warning 这个时间复杂度是`O(n)`
 */
template <class T>
constexpr T nameFromEnum(const std::string& name) {
    return nameFromEnum<T, (T)-_::MAGIC_ENUM_RECURSION_DEPTH, (T)_::MAGIC_ENUM_RECURSION_DEPTH>(name);
}

}}}} // namespace HX::STL::utils::MagicEnum

#endif // _HX_MAGIC_ENUM_H_
