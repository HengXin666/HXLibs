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

// 宏
#include <HXLibs/macro/for.hpp>
#include <HXLibs/macro/forEachPair.hpp>

/**
 * @brief 反射宏注册
 */
namespace HX::reflection { }

/// @brief 声明 __NAME__ 成员
#define __HX_REFL_STRUCT_MEMBER__(__NAME__) \
decltype(__NAME__) __NAME__;

/// @brief 声明 __NAME__ 成员
#define __HX_REFL_STRUCT_G_MEMBER__(__NAME__) \
decltype(meta::remove_cvref_t<decltype(t)>::__NAME__) __NAME__;

/// @brief 前置逗号获取成员变量
#define __HX_REFL_GET_MEMBER__(__NAME__) , t.__NAME__

/// @brief 把 ... 展开为 t.f0, t.f1, t.f2, ... 的形式
#define __HX_REFL_GET_MEMBERS__(__ONE__, ...) \
t.__ONE__ __VA_OPT__(HX_FOR(__HX_REFL_GET_MEMBER__, __VA_ARGS__))

/// @brief 计算成员个数
#define __HX_ADD_1__(x) +1

/////////////////////////////////////////////////////////////

/// @brief 声明 __NAME__ 类型名称为 __NEW_NAME__ 的成员
#define __HX_REFL_STRUCT_MEMBER_AS__(__NAME__, __NEW_NAME__) \
decltype(__NAME__) __NEW_NAME__;

/// @brief 声明 __NAME__ 类型名称为 __NEW_NAME__ 的成员
#define __HX_REFL_STRUCT_G_MEMBER_AS__(__NAME__, __NEW_NAME__) \
decltype(meta::remove_cvref_t<decltype(t)>::__NAME__) __NEW_NAME__;

/// @brief 前置逗号获取成员变量
#define __HX_REFL_GET_MEMBER_AS__(__NAME__, _) , t.__NAME__

/// @brief 把 ... 展开为 t.f0, t.f1, t.f2, ... 的形式
#define __HX_REFL_GET_MEMBERS_AS__(__ONE__, _, ...) \
t.__ONE__ __VA_OPT__(FOR_EACH_PAIR(__HX_REFL_GET_MEMBER_AS__, __VA_ARGS__))

/// @brief 计算成员个数
#define __HX_ADD_2__(x, y) +1

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
    inline static constexpr std::size_t membersCount() {                       \
        return static_cast<std::size_t>(HX_FOR(__HX_ADD_1__, __VA_ARGS__));    \
    }                                                                          \
    HX_DIAGNOSTIC_PUSH                                                         \
    HX_DIAGNOSTIC_IGNORE_CHANGES_MEANING                                       \
    inline constexpr static auto visit() {                                     \
        struct __internal__ {                                                  \
            HX_FOR(__HX_REFL_STRUCT_MEMBER__, __VA_ARGS__)                     \
        };                                                                     \
        return ::HX::reflection::internal::ReflectionVisitor<                  \
            __internal__, membersCount()>::visit();                            \
    }                                                                          \
    HX_DIAGNOSTIC_POP                                                          \
    template <typename T>                                                      \
    inline constexpr static auto visit(T& t) {                                 \
        return std::tie(__HX_REFL_GET_MEMBERS__(__VA_ARGS__));                 \
    }                                                                          \
    template <typename T>                                                      \
    inline constexpr static auto visit(T const& t) {                           \
        return std::tie(__HX_REFL_GET_MEMBERS__(__VA_ARGS__));                 \
    }

/**
 * @brief 反射类内部私有成员,
 * @param ... 需要反射的字段名称
 */
#define HX_REFL_AS(...)                                                        \
    inline static constexpr std::size_t membersCount() {                       \
        return static_cast<std::size_t>(                                       \
            FOR_EACH_PAIR(__HX_ADD_2__, __VA_ARGS__));                         \
    }                                                                          \
    HX_DIAGNOSTIC_PUSH                                                         \
    HX_DIAGNOSTIC_IGNORE_CHANGES_MEANING                                       \
    inline constexpr static auto visit() {                                     \
        struct __internal__ {                                                  \
            FOR_EACH_PAIR(__HX_REFL_STRUCT_MEMBER_AS__, __VA_ARGS__)           \
        };                                                                     \
        return ::HX::reflection::internal::ReflectionVisitor<                  \
            __internal__, membersCount()>::visit();                            \
    }                                                                          \
    HX_DIAGNOSTIC_POP                                                          \
    template <typename T>                                                      \
    inline constexpr static auto visit(T& t) {                                 \
        return std::tie(__HX_REFL_GET_MEMBERS_AS__(__VA_ARGS__));              \
    }                                                                          \
    template <typename T>                                                      \
    inline constexpr static auto visit(T const& t) {                           \
        return std::tie(__HX_REFL_GET_MEMBERS_AS__(__VA_ARGS__));              \
    }

/**
 * @brief 反射类指定共有成员, 
 * @param ... 需要反射的字段名称
 */
#define HX_REFL_G(__CLASS_NAME__, ...)                                         \
    inline namespace __hx_reflection_metadata {                                \
    [[maybe_unused]] inline static constexpr std::size_t                       \
    membersCount(__CLASS_NAME__ const&) {                                      \
        return static_cast<std::size_t>(HX_FOR(__HX_ADD_1__, __VA_ARGS__));    \
    }                                                                          \
    HX_DIAGNOSTIC_PUSH                                                         \
    HX_DIAGNOSTIC_IGNORE_CHANGES_MEANING                                       \
    [[maybe_unused]] inline constexpr static auto                              \
    visit(__CLASS_NAME__ const& t) {                                           \
        struct __internal__ {                                                  \
            HX_FOR(__HX_REFL_STRUCT_G_MEMBER__, __VA_ARGS__)                   \
        };                                                                     \
        return ::HX::reflection::internal::ReflectionVisitor<                  \
            __internal__,                                                      \
            ::HX::reflection::membersCountVal<__internal__>>::visit();         \
    }                                                                          \
    HX_DIAGNOSTIC_POP                                                          \
    template <typename T>                                                      \
    inline constexpr static auto visit(__CLASS_NAME__ const&, T& t) {          \
        return std::tie(__HX_REFL_GET_MEMBERS__(__VA_ARGS__));                 \
    }                                                                          \
    template <typename T>                                                      \
    inline constexpr static auto visit(__CLASS_NAME__ const&, T const& t) {    \
        return std::tie(__HX_REFL_GET_MEMBERS__(__VA_ARGS__));                 \
    }                                                                          \
    }

/**
 * @brief 反射类指定共有成员, 支持别名
 * @param ... 需要反射的字段名称
 */
#define HX_REFL_G_AS(__CLASS_NAME__, ...)                                      \
    inline namespace __hx_reflection_metadata {                                \
    [[maybe_unused]] inline static constexpr std::size_t                       \
    membersCount(__CLASS_NAME__ const&) {                                      \
        return static_cast<std::size_t>(                                       \
            FOR_EACH_PAIR(__HX_ADD_2__, __VA_ARGS__));                         \
    }                                                                          \
    HX_DIAGNOSTIC_PUSH                                                         \
    HX_DIAGNOSTIC_IGNORE_CHANGES_MEANING                                       \
    [[maybe_unused]] inline constexpr static auto                              \
    visit(__CLASS_NAME__ const& t) {                                           \
        struct __internal__ {                                                  \
            FOR_EACH_PAIR(__HX_REFL_STRUCT_G_MEMBER_AS__, __VA_ARGS__)         \
        };                                                                     \
        return ::HX::reflection::internal::ReflectionVisitor<                  \
            __internal__,                                                      \
            ::HX::reflection::membersCountVal<__internal__>>::visit();         \
    }                                                                          \
    HX_DIAGNOSTIC_POP                                                          \
    template <typename T>                                                      \
    inline constexpr static auto visit(__CLASS_NAME__ const&, T& t) {          \
        return std::tie(__HX_REFL_GET_MEMBERS_AS__(__VA_ARGS__));              \
    }                                                                          \
    template <typename T>                                                      \
    inline constexpr static auto visit(__CLASS_NAME__ const&, T const& t) {    \
        return std::tie(__HX_REFL_GET_MEMBERS_AS__(__VA_ARGS__));              \
    }                                                                          \
    }

#endif // !_HX_REFLECTION_MACRO_H_