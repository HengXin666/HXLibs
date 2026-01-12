#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-09-21 21:25:03
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

#include <HXLibs/reflection/MemberName.hpp>

namespace HX::reflection {

namespace internal {

struct _ptr_diff_type {
    std::ptrdiff_t val;
};

template <typename T>
struct MemberPtrType {
    template <typename U>
    constexpr MemberPtrType(U T::* memberPtr) noexcept
        : val{}
    {
        constexpr auto& t = getStaticObj<T>();
        val = reinterpret_cast<std::byte const*>(&(t.*memberPtr))
            - reinterpret_cast<std::byte const*>(&t);
    }

    // 内部使用的
    constexpr MemberPtrType(_ptr_diff_type v)
        : val{v.val}
    {}

    constexpr bool operator==(MemberPtrType const& that) const noexcept {
        return that.val == val;
    }

    std::ptrdiff_t val;
};

/**
 * @brief 类基址偏移
 * @tparam I 
 * @tparam T 
 */
template <std::size_t I, typename T>
struct SetObjIdx {
    using Type = T; // 字段类型
    inline static constexpr std::size_t Idx = I; // 字段索引
    constexpr SetObjIdx() = default;
    constexpr SetObjIdx(std::size_t _offset)
        : offset(_offset)
    {}
    std::size_t offset; // 字段基址偏移
};

template <std::size_t... Idx, typename... Ts>
constexpr auto makeVariantSetObjIdxs(std::index_sequence<Idx...>, std::tuple<Ts...>) {
    return std::variant<SetObjIdx<Idx, meta::RemoveCvRefType<Ts>>...>{};
}

} // namespace internal

/**
 * @brief 获取 字段名称字符串 到 类字段偏移量 的编译期哈希表
 * @tparam T 
 * @return constexpr auto 
 */
template <typename T>
constexpr auto makeNameToIdxVariantHashMap() {
    constexpr auto N = membersCountVal<T>;
    constexpr auto nameArr = getMembersNames<T>();
    constexpr auto& t = internal::getStaticObj<T>();
    constexpr auto tp = internal::getObjTie(t);
    using CHashMapValType = decltype(internal::makeVariantSetObjIdxs(std::make_index_sequence<N>{}, tp));
    return container::CHashMap<std::string_view, CHashMapValType, N>{
        [&] <std::size_t... Idx> (std::index_sequence<Idx...>) constexpr {
            return std::array<std::pair<std::string_view, CHashMapValType>, N>{{{
                nameArr[Idx], CHashMapValType{
                    internal::SetObjIdx<Idx, meta::RemoveCvRefType<decltype(std::get<Idx>(tp))>>{
                        static_cast<std::size_t>(
                            reinterpret_cast<std::byte*>(&std::get<Idx>(tp))
                            - reinterpret_cast<std::byte const*>(&t)
                     )}
                }}...
            }};
        }(std::make_index_sequence<N>{})
    };
}

/**
 * @brief 获取类成员指针到名称的映射
 * @tparam T 
 * @return constexpr auto 
 */
template <typename T>
inline constexpr auto makeMemberPtrToNameMap() noexcept {
    using U = meta::RemoveCvRefType<T>;
    constexpr auto Cnt = membersCountVal<U>;
    constexpr auto membersArr = getMembersNames<U>();
    constexpr auto& t = internal::getStaticObj<T>();
    constexpr auto tp = internal::getObjTie(t);
    return [&] <std::size_t... Is> (std::index_sequence<Is...>) constexpr {
        return container::CHashMap<internal::MemberPtrType<U>, std::string_view, Cnt>{
            std::array<std::pair<internal::MemberPtrType<U>, std::string_view>, Cnt>{
                std::pair<internal::MemberPtrType<U>, std::string_view>{
                    internal::MemberPtrType<U>{internal::_ptr_diff_type{
                        reinterpret_cast<std::byte*>(&std::get<Is>(tp))
                        - reinterpret_cast<std::byte const*>(&t)
                    }},
                    membersArr[Is]
                }...
            }
        };
    } (std::make_index_sequence<Cnt>{});
}

} // namespace HX::reflection

namespace HX::meta {
  
template <typename T>
struct Hash<reflection::internal::MemberPtrType<T>> {
    using Type = reflection::internal::MemberPtrType<T>;
    constexpr std::size_t operator()(Type const& val, std::size_t seed) const noexcept {
        return Hash<std::ptrdiff_t>{}(val.val, seed);
    }
};

} // namespace HX::meta