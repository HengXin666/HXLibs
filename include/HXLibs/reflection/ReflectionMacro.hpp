#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-25 09:52:33
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
#ifndef _HX_REFLECTION_MACRO_H_
#define _HX_REFLECTION_MACRO_H_

#include <HXLibs/reflection/MemberName.hpp>

/**
 * @brief 反射宏注册
 */
namespace HX::reflection { }

/* 宏 FOR - 辅助宏 */
#include <HXLibs/reflection/tools/ReflectionMacroImpl.hpp>

/* 计算成员个数 辅助宏 */
#if !defined(_MSC_VER) // MSVC 不能超过 127 个宏参数
#define __HX_COUNT_ARGS_IMPL__(                                                \
    f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, \
    f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, \
    f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, \
    f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61, \
    f62, f63, f64, f65, f66, f67, f68, f69, f70, f71, f72, f73, f74, f75, f76, \
    f77, f78, f79, f80, f81, f82, f83, f84, f85, f86, f87, f88, f89, f90, f91, \
    f92, f93, f94, f95, f96, f97, f98, f99, f100, f101, f102, f103, f104,      \
    f105, f106, f107, f108, f109, f110, f111, f112, f113, f114, f115, f116,    \
    f117, f118, f119, f120, f121, f122, f123, f124, f125, f126, f127, f128,    \
    f129, f130, f131, f132, f133, f134, f135, f136, f137, f138, f139, f140,    \
    f141, f142, f143, f144, f145, f146, f147, f148, f149, f150, f151, f152,    \
    f153, f154, f155, f156, f157, f158, f159, f160, f161, f162, f163, f164,    \
    f165, f166, f167, f168, f169, f170, f171, f172, f173, f174, f175, f176,    \
    f177, f178, f179, f180, f181, f182, f183, f184, f185, f186, f187, f188,    \
    f189, f190, f191, f192, f193, f194, f195, f196, f197, f198, f199, f200,    \
    f201, f202, f203, f204, f205, f206, f207, f208, f209, f210, f211, f212,    \
    f213, f214, f215, f216, f217, f218, f219, f220, f221, f222, f223, f224,    \
    f225, f226, f227, f228, f229, f230, f231, f232, f233, f234, f235, f236,    \
    f237, f238, f239, f240, f241, f242, f243, f244, f245, f246, f247, f248,    \
    f249, f250, f251, f252, f253, f254, f255, N, ...)                          \
    N

/**
 * @brief 计算成员个数
 */
#define HX_COUNT_ARGS(...)                                                     \
    __HX_COUNT_ARGS_IMPL__(                                                    \
        __VA_OPT__(, __VA_ARGS__), 255, 254, 253, 252, 251, 250, 249, 248,     \
        247, 246, 245, 244, 243, 242, 241, 240, 239, 238, 237, 236, 235, 234,  \
        233, 232, 231, 230, 229, 228, 227, 226, 225, 224, 223, 222, 221, 220,  \
        219, 218, 217, 216, 215, 214, 213, 212, 211, 210, 209, 208, 207, 206,  \
        205, 204, 203, 202, 201, 200, 199, 198, 197, 196, 195, 194, 193, 192,  \
        191, 190, 189, 188, 187, 186, 185, 184, 183, 182, 181, 180, 179, 178,  \
        177, 176, 175, 174, 173, 172, 171, 170, 169, 168, 167, 166, 165, 164,  \
        163, 162, 161, 160, 159, 158, 157, 156, 155, 154, 153, 152, 151, 150,  \
        149, 148, 147, 146, 145, 144, 143, 142, 141, 140, 139, 138, 137, 136,  \
        135, 134, 133, 132, 131, 130, 129, 128, 127, 126, 125, 124, 123, 122,  \
        121, 120, 119, 118, 117, 116, 115, 114, 113, 112, 111, 110, 109, 108,  \
        107, 106, 105, 104, 103, 102, 101, 100, 99, 98, 97, 96, 95, 94, 93,    \
        92, 91, 90, 89, 88, 87, 86, 85, 84, 83, 82, 81, 80, 79, 78, 77, 76,    \
        75, 74, 73, 72, 71, 70, 69, 68, 67, 66, 65, 64, 63, 62, 61, 60, 59,    \
        58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42,    \
        41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25,    \
        24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7,   \
        6, 5, 4, 3, 2, 1, 0)

#else
#define __HX_COUNT_ARGS_IMPL__(                                                \
    f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, \
    f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, \
    f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, \
    f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61, \
    f62, f63, f64, f65, f66, f67, f68, f69, f70, f71, f72, f73, f74, f75, f76, \
    f77, f78, f79, f80, f81, f82, f83, f84, f85, f86, f87, f88, f89, f90, f91, \
    f92, f93, f94, f95, f96, f97, f98, f99, f100, f101, f102, f103, f104,      \
    f105, f106, f107, f108, f109, f110, f111, f112, f113, f114, f115, f116,    \
    f117, f118, f119, f120, f121, f122, f123, f124, N, ...)                    \
    N

#define HX_COUNT_ARGS(...)                                                     \
    __HX_COUNT_ARGS_IMPL__(                                                    \
        __VA_OPT__(, __VA_ARGS__), 124, 123, 122, 121, 120, 119, 118, 117,     \
        116, 115, 114, 113, 112, 111, 110, 109, 108, 107, 106, 105, 104, 103,  \
        102, 101, 100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87, 86, \
        85, 84, 83, 82, 81, 80, 79, 78, 77, 76, 75, 74, 73, 72, 71, 70, 69,    \
        68, 67, 66, 65, 64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52,    \
        51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35,    \
        34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18,    \
        17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

#endif // !defined(_MSC_VER)


/* 展开宏 辅助宏 */
#define __EXPAND_IMPL__(x, y) x##y
#define __EXPAND__(x, y) __EXPAND_IMPL__(x, y)

/**
 * @brief 宏 for, 把 ... 中每一个都传入 __FUNC__ 中
 */
#define HX_PP_FOR(__FUNC__, ...) \
__EXPAND__(__HX_PP_FOR_IMPL_, HX_COUNT_ARGS(__VA_ARGS__))(__FUNC__ __VA_OPT__(, __VA_ARGS__))

/// @brief 声明 __NAME__
#define __HX_REFL_STRUCT_MEMBER__(__NAME__) \
decltype(__NAME__) __NAME__;

/// @brief 前置逗号获取成员变量
#define __HX_REFL_GET_MEMBER__(__NAME__) , t.__NAME__

/// @brief 把 ... 展开为 t.f0, t.f1, t.f2, ... 的形式
#define __HX_REFL_GET_MEMBERS__(__ONE__, ...) \
t.__ONE__ __HX_REFL_GET_MEMBER__(__VA_ARGS__)


#if defined(__clang__)
    #define HX_DIAGNOSTIC_PUSH _Pragma("clang diagnostic push")
    #if __has_warning("-Wchanges-meaning")
        #define HX_DIAGNOSTIC_IGNORE_CHANGES_MEANING _Pragma("clang diagnostic ignored \"-Wchanges-meaning\"")
    #else
        #define HX_DIAGNOSTIC_IGNORE_CHANGES_MEANING
    #endif
    #define HX_DIAGNOSTIC_POP _Pragma("clang diagnostic pop")
#elif defined(__GNUC__)
    #define HX_DIAGNOSTIC_PUSH _Pragma("GCC diagnostic push")
    #define HX_DIAGNOSTIC_IGNORE_CHANGES_MEANING _Pragma("GCC diagnostic ignored \"-Wchanges-meaning\"")
    #define HX_DIAGNOSTIC_POP _Pragma("GCC diagnostic pop")
#elif defined(_MSC_VER)
    #define HX_DIAGNOSTIC_PUSH __pragma(warning(push))
    #define HX_DIAGNOSTIC_IGNORE_CHANGES_MEANING __pragma(warning(disable: 4456)) // declaration hides previous local declaration
    #define HX_DIAGNOSTIC_POP __pragma(warning(pop))
#else
    #define HX_DIAGNOSTIC_PUSH
    #define HX_DIAGNOSTIC_IGNORE_CHANGES_MEANING
    #define HX_DIAGNOSTIC_POP
#endif

/**
 * @brief 反射类内部私有成员, 
 * @param ... 需要反射的字段名称
 */
#define HX_REFL(...)                                                           \
    HX_DIAGNOSTIC_PUSH                                                         \
    HX_DIAGNOSTIC_IGNORE_CHANGES_MEANING                                       \
    inline static constexpr std::size_t membersCount() {                       \
        return static_cast<std::size_t>(HX_COUNT_ARGS(__VA_ARGS__));           \
    }                                                                          \
    inline constexpr static auto visit() {                                     \
        struct __internal__ {                                                  \
            HX_PP_FOR(__HX_REFL_STRUCT_MEMBER__, __VA_ARGS__)                  \
        };                                                                     \
        return HX::reflection::internal::ReflectionVisitor<                    \
            __internal__, membersCount()>::visit();                            \
    }                                                                          \
    template <typename T>                                                      \
    inline constexpr static auto visit(T& t) {                                 \
        return std::tie(__HX_REFL_GET_MEMBERS__(__VA_ARGS__));                 \
    }                                                                          \
    template <typename T>                                                      \
    inline constexpr static auto visit(T const& t) {                           \
        return std::tie(__HX_REFL_GET_MEMBERS__(__VA_ARGS__));                 \
    }                                                                          \
    HX_DIAGNOSTIC_POP

#endif // !_HX_REFLECTION_MACRO_H_