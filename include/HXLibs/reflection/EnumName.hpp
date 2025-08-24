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

#include <cstdint>
#include <stdexcept>
#include <string_view>

#include <HXLibs/meta/IntegerIndex.hpp>

#include <HXLibs/container/CHashMap.hpp>

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
    if constexpr (!std::is_enum_v<EnumType>) {
        // 传入的不是枚举类型
        static_assert(!sizeof(EnumType),
            "The input type is not an enumeration");
    }
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
    if (split[0] != '(') {
        // 如果是 enum class, 那么会有 Type::name; 但是小心 (xx::Yy)zz 是无名枚举
        // 因此含有 '(' 的就不处理了~
        auto pos = split.rfind("::");
        if (pos != std::string_view::npos) {
            split = split.substr(pos + 2);
        }
    }
    return split;
}

template <typename EnumType>
constexpr auto makeEnumRange() noexcept {
    // @todo, 通过偏特化允许用户定义范围
    // @todo, 支持 位定义的枚举, 如 0x01, 0x02, 0x04, 0x08 这种 (1 << i) 的枚举值
    if constexpr (std::is_unsigned_v<std::underlying_type_t<EnumType>>) {
        return meta::makeIntegerIndexRange<
            uint64_t,
            static_cast<uint64_t>(0),
            static_cast<uint64_t>(255)
        >{};
    } else {
        return meta::makeIntegerIndexRange<
            int64_t,
            static_cast<int64_t>(-128),
            static_cast<int64_t>(127)
        >{};
    }
}

} // namespace internal

/**
 * @brief 判断 EnumType {Enum} 是否为有效的枚举值
 */
template <typename EnumType, EnumType Enum>
constexpr bool isValidEnumValue = internal::getEnumName<EnumType, Enum>()[0] != '(';

/**
 * @brief 获取枚举的有效枚举值的个数
 * @tparam EnumType 
 * @return constexpr std::size_t 
 */
template <typename EnumType>
constexpr std::size_t getValidEnumValueCnt() noexcept {
    return [&] <typename NT, NT... Idx> (meta::IntegerIndex<NT, Idx...>) {
        std::size_t res = 0;
        ((res += static_cast<std::size_t>(
            isValidEnumValue<EnumType, static_cast<EnumType>(Idx)>
        )), ...);
        return res;
    }(internal::makeEnumRange<EnumType>());
}

namespace internal {

// 技巧来源:
// 实现 编译期常量 可以在 运行时 仅初始化一次
// https://stackoverflow.com/questions/62458079/static-constexpr-variables-in-a-constexpr-function
// 如果是 C++23 就直接上 constexpr static 就好了, 没有那么多 B 事情.

// 获取 EnumType -> str 的映射
template <typename EnumType>
class EnumValueMapStr {
    static constexpr auto getEnumValueMapStr() noexcept {
        constexpr std::size_t Cnt = getValidEnumValueCnt<EnumType>();
        using KV = std::pair<EnumType, std::string_view>;
        std::array<KV, Cnt> arr;
        std::size_t i = 0;
        [&] <typename NT, NT... Idx> (meta::IntegerIndex<NT, Idx...>) {
            ([&]() {
                if (isValidEnumValue<EnumType, static_cast<EnumType>(Idx)>) {
                    arr[i++] = {
                        static_cast<EnumType>(Idx),
                        internal::getEnumName<EnumType, static_cast<EnumType>(Idx)>()
                    };
                }
            }(), ...);
        }(internal::makeEnumRange<EnumType>());
        container::CHashMap<EnumType, std::string_view, Cnt> hash{arr};
        return hash;
    }
public:
    inline static constexpr auto map = getEnumValueMapStr();
    constexpr auto& operator()() noexcept { return map; }
    static constexpr auto& make() noexcept { return map; }
};


// 获取 str -> EnumType 的映射
template <typename EnumType>
class EnumStrMapValue {
    static constexpr auto getEnumStrMapValue() noexcept {
        constexpr std::size_t Cnt = getValidEnumValueCnt<EnumType>();
        using KV = std::pair<std::string_view, EnumType>;
        std::array<KV, Cnt> arr;
        std::size_t i = 0;
        [&] <typename NT, NT... Idx> (meta::IntegerIndex<NT, Idx...>) {
            ([&]() {
                if (isValidEnumValue<EnumType, static_cast<EnumType>(Idx)>) {
                    arr[i++] = {
                        internal::getEnumName<EnumType, static_cast<EnumType>(Idx)>(),
                        static_cast<EnumType>(Idx)
                    };
                }
            }(), ...);
        }(internal::makeEnumRange<EnumType>());
        container::CHashMap<std::string_view, EnumType, Cnt> hash{arr};
        return hash;
    }
public:
    inline static constexpr auto map = getEnumStrMapValue();
    constexpr auto& operator()() noexcept { return map; }
    static constexpr auto& make() noexcept { return map; }
};

} // namespace internal

/**
 * @brief 从枚举值获取其枚举的名称字符串
 * @tparam T 
 * @param enumVal 
 * @return constexpr std::string_view 
 */
template <typename T>
constexpr std::string_view toEnumName(T const& enumVal) noexcept {
#if 0
    return [&] <typename NT, NT... Idx> (meta::IntegerIndex<NT, Idx...>) {
        std::string_view res{};
        ([&]{
            if (Idx == static_cast<NT>(enumVal)) {
                res = internal::getEnumName<T, static_cast<T>(Idx)>();
                return true;
            }
            return false;
        }() || ...);
        return res;
    }(internal::makeEnumRange<T>());
#else
    constexpr auto map = internal::EnumValueMapStr<T>::map;
    if (auto it = map.find(enumVal); it != map.end()) {
        return it->second;
    }
    return "";
#endif
}

/**
 * @brief 从枚举值获取其枚举的名称字符串
 * @tparam T 
 * @param enumVal 
 * @return constexpr std::string_view 
 */
template <typename T, T EnumVal>
constexpr std::string_view toEnumName() noexcept {
    return internal::getEnumName<T, EnumVal>();
}

/**
 * @brief 从枚举名称转换到对应的枚举值, 如果转换失败, 则会抛出异常
 * @tparam T 
 * @param name 
 * @return constexpr T 
 */
template <typename T>
constexpr T toEnum(std::string_view name) {
#if 0
    return [&] <typename NT, NT... Idx> (meta::IntegerIndex<NT, Idx...>) {
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
#else
    constexpr auto map = internal::EnumStrMapValue<T>::map;
    if (auto it = map.find(name); it != map.end()) {
        return it->second;
    }
    [[unlikely]] throw std::runtime_error{"not find enum name"};
#endif
}

} // namespace HX::reflection

