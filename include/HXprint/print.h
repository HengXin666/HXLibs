#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2024-7-16 13:47:40
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
#ifndef _HX_PRINT_H_
#define _HX_PRINT_H_

#include <iostream>

#ifdef _HX_DEBUG_
#include <cstdio>
#include <cstdarg>
#endif // _HX_DEBUG_

#include <HXSTL/utils/ToString.hpp>

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

namespace HX { namespace print {

namespace internal {

// 概念: 鸭子类型: 只需要满足有一个成员函数是print的, 即可
template <typename T>
concept PrintClassType = requires(T t) {
    t.print();
};

// === 仅DEBUG编译期(未发布)有的日志打印 ===

#ifdef _HX_DEBUG_
enum class LogLevel {
    LOG_ERROR,
    LOG_WARNING,
    LOG_INFO,
};

void logMessage(LogLevel level, const char* format, ...);

#define LOG_ERROR(...) logMessage(HX::print::internal::LogLevel::LOG_ERROR, __VA_ARGS__)
#define LOG_WARNING(...) logMessage(HX::print::internal::LogLevel::LOG_WARNING, __VA_ARGS__)
#define LOG_INFO(...) logMessage(HX::print::internal::LogLevel::LOG_INFO, __VA_ARGS__)

#else

#define LOG_ERROR(...)
#define LOG_WARNING(...)
#define LOG_INFO(...)

#endif // _HX_DEBUG_

template <typename... Ts>
inline void printImpl(Ts const&... ts) {
    std::cout << HX::STL::utils::toStrings(ts...);
}

template <PrintClassType PT>
inline void printImpl(PT const& t) {
    t.print();
}

} // namespace internal

/**
 * @brief 打印不带'\\n'的对象, 多个则使用 ' ' 空格分开.
 */
template <typename... Ts>
inline void print(Ts const&... ts) {
    internal::printImpl(ts...);
}

/**
 * @brief 打印带'\\n'的对象, 多个则使用 ' ' 空格分开.
 */
template <typename... Ts>
inline void println(Ts const&... ts) {
    print(ts...);
    std::cout << '\n';
}
// 注: 建议鸭子类型使用printnl!

}} // namespace HX::print

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

#endif // _HX_PRINT_H_