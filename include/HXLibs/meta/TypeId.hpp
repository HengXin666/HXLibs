#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-11-29 21:59:41
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

#if 0

#include <atomic>

namespace HX::meta {

struct TypeId {
    // 适用于 类成员指针
    template <auto Vs>
    static std::size_t make() noexcept {
        static const auto id = get();
        return id;
    }

    template <typename... Ts>
    static std::size_t make() noexcept {
        static const auto id = get();
        return id;
    }
private:
    static std::size_t get() noexcept {
        static TypeId s{};
        return s._cnt.fetch_add(1, std::memory_order_relaxed);
    }
    TypeId() = default;
    TypeId& operator=(TypeId&&) = delete;
    std::atomic_size_t _cnt{0};
};

} // namespace HX::meta

#else

#include <HXLibs/reflection/MemberName.hpp>

namespace HX::meta {

namespace internal {

template <auto Vs>
struct StaticVal {
    inline static constexpr auto Val = Vs;
};

} // namespace internal

/**
 * @brief 获取编译期 ID
 */
struct TypeId {
    using IdType = void const * const;

    template <auto Vs>
    static consteval IdType make() noexcept {
        return static_cast<IdType>(&internal::StaticVal<Vs>::Val);
    }

    template <typename T>
    static consteval IdType make() noexcept {
        return static_cast<IdType>(&reflection::internal::getStaticObj<T>());
    }
};

} // namespace HX::meta

#endif