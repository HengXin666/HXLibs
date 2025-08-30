/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-26 18:45:38
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

#if defined(_MSC_VER)
    #define HX_NO_WARNINGS_BEGIN                                \
        __pragma(warning(push, 0))                              \
        __pragma(warning(disable: 4100 4189 4996)) // 可选: MSVC的一些常见噪声
    #define HX_NO_WARNINGS_END                                  \
        __pragma(warning(pop))

#elif defined(__clang__)
    #define HX_NO_WARNINGS_BEGIN                                \
        _Pragma("clang diagnostic push")                        \
        _Pragma("clang diagnostic ignored \"-Wall\"")           \
        _Pragma("clang diagnostic ignored \"-Wextra\"")         \
        _Pragma("clang diagnostic ignored \"-Wpedantic\"")      \
        _Pragma("clang diagnostic ignored \"-Weverything\"")   // 全关闭
    #define HX_NO_WARNINGS_END                                  \
        _Pragma("clang diagnostic pop")

#elif defined(__GNUC__)
    #define HX_NO_WARNINGS_BEGIN                                \
        _Pragma("GCC diagnostic push")                          \
        _Pragma("GCC diagnostic ignored \"-Wall\"")             \
        _Pragma("GCC diagnostic ignored \"-Wextra\"")           \
        _Pragma("GCC diagnostic ignored \"-Wpedantic\"")        \
        _Pragma("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
    #define HX_NO_WARNINGS_END                                  \
        _Pragma("GCC diagnostic pop")

#else
    #define HX_NO_WARNINGS_BEGIN
    #define HX_NO_WARNINGS_END
#endif

#if defined(__GNUC__) && !defined(__clang__)
// 仅 GCC 有 -Wmaybe-uninitialized
    #define HX_NO_MAYBE_UNINITIALIZED_WARNINGS_BEGIN            \
        _Pragma("GCC diagnostic push")                          \
        _Pragma("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
    #define HX_NO_MAYBE_UNINITIALIZED_WARNINGS_END              \
        _Pragma("GCC diagnostic pop")
#else
// MSVC / Clang 下啥也不做
    #define HX_NO_MAYBE_UNINITIALIZED_WARNINGS_BEGIN
    #define HX_NO_MAYBE_UNINITIALIZED_WARNINGS_END
#endif