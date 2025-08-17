#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-08-17 18:31:08
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
#ifndef _HX_ENUM_NAME_H_
#define _HX_ENUM_NAME_H_

#include <cstdint>
#include <stdexcept>
#include <string_view>

#include <HXLibs/meta/IntegerIndex.hpp>

namespace HX::reflection {

namespace internal {

/**
 * @brief 获取枚举或者枚举类的值名称
 * @tparam EnumType 
 * @tparam Enum 
 * @return constexpr std::string_view 
 */
template <typename EnumType, EnumType Enum>
inline constexpr std::string_view getEnumName() noexcept {
#if defined(_MSC_VER)
    constexpr std::string_view funcName = __FUNCSIG__;
#else
    constexpr std::string_view funcName = __PRETTY_FUNCTION__;
#endif
#if defined(__clang__)
    auto split = funcName.substr(funcName.rfind("Enum = "));
    split = split.substr(7, split.size() - 8);
#elif defined(__GNUC__)
    auto split = funcName.substr(funcName.rfind("Enum = "));
    split = split.substr(7, split.find("; ") - 7);
#elif defined(_MSC_VER)
    auto split = funcName.substr(funcName.rfind(","));
    split = split.substr(1, split.find(">(void)") - 1);
#else
    static_assert(
        false, 
        "You are using an unsupported compiler. Please use GCC, Clang "
        "or MSVC or switch to the rfl::Field-syntax."
    );
#endif
    [&] {
        // 如果是 enum class, 那么会有 Type::name
        auto pos = split.rfind("::");
        if (pos == std::string_view::npos) {
            return;
        }
        split = split.substr(pos + 2);
    }();
    return split;
}

template <typename EnumType>
constexpr auto makeEnumRange() noexcept {
    // @todo, 通过偏特化允许用户定义范围
    // @todo, 支持 位定义的枚举, 如 0x01, 0x02, 0x04, 0x08 这种 (1 << i) 的枚举值
    if constexpr (std::is_unsigned_v<std::underlying_type_t<EnumType>>) {
        return meta::makeIntegerIndexRange<uint64_t, static_cast<uint64_t>(0), static_cast<uint64_t>(255)>{};
    } else {
        return meta::makeIntegerIndexRange<int64_t, static_cast<int64_t>(-128), static_cast<int64_t>(127)>{};
    }
}

} // namespace internal

/**
 * @brief 从枚举值获取其枚举的名称字符串
 * @tparam T 
 * @param enumVal 
 * @return constexpr std::string_view 
 */
template <typename T>
constexpr std::string_view toEnumName(T const& enumVal) noexcept {
    if constexpr (std::is_unsigned_v<std::underlying_type_t<T>>) {
        return [&] <uint64_t... Idx> (meta::IntegerIndex<uint64_t, Idx...>) {
            std::string_view res{};
            ([&]{
                if (Idx == static_cast<uint64_t>(enumVal)) {
                    res = internal::getEnumName<T, static_cast<T>(Idx)>();
                    return true;
                }
                return false;
            }() || ...);
            return res;
        }(internal::makeEnumRange<T>());
    } else {    
        return [&] <int64_t... Idx> (meta::IntegerIndex<int64_t, Idx...>) {
            std::string_view res{};
            ([&]{
                if (Idx == static_cast<int64_t>(enumVal)) {
                    res = internal::getEnumName<T, static_cast<T>(Idx)>();
                    return true;
                }
                return false;
            }() || ...);
            return res;
        }(internal::makeEnumRange<T>());
    }
}

/**
 * @brief 从枚举名称转换到对应的枚举值, 如果转换失败, 则会抛出异常
 * @tparam T 
 * @param name 
 * @return constexpr T 
 */
template <typename T>
constexpr T toEnum(std::string_view name) {
    if constexpr (std::is_unsigned_v<std::underlying_type_t<T>>) {
        return [&] <uint64_t... Idx> (meta::IntegerIndex<uint64_t, Idx...>) {
            T res;
            bool isFind = false;
            ([&]{
                if (name == internal::getEnumName<T, static_cast<T>(Idx)>()) {
                    res = static_cast<T>(Idx);
                    isFind = true;
                    return true;
                };
                return false;
            }() || ...);
            if (!isFind) [[unlikely]] {
                // 找不到对应枚举名称
                throw std::runtime_error{"not find enum name"};
            }
            return res;
        }(internal::makeEnumRange<T>());
    } else {    
        return [&] <int64_t... Idx> (meta::IntegerIndex<int64_t, Idx...>) {
            T res;
            bool isFind = false;
            ([&]{
                if (name == internal::getEnumName<T, static_cast<T>(Idx)>()) {
                    res = static_cast<T>(Idx);
                    isFind = true;
                    return true;
                };
                return false;
            }() || ...);
            if (!isFind) [[unlikely]] {
                // 找不到对应枚举名称
                throw std::runtime_error{"not find enum name"};
            }
            return res;
        }(internal::makeEnumRange<T>());
    }
}

template <typename T>
constexpr bool IsValidEnumName = false;

} // namespace HX::reflection

#endif // !_HX_ENUM_NAME_H_