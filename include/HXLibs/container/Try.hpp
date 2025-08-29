#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-08-29 14:09:41
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

#include <HXLibs/container/UninitializedNonVoidVariant.hpp>

namespace HX::container {

template <typename T = void>
struct Try {
    using TryType = T;

    Try() 
        : _data{}
    {}

    template <typename U>
    Try(U&& data)
        : Try{}
    {
        _data.template emplace<T>(std::forward<NonVoidType<U>>(data));
    }

    Try(std::exception_ptr ePtr)
        : Try{}
    {
        _data.template emplace<std::exception_ptr>(ePtr);
    }

    operator bool() const noexcept {
        return _data.index() == 0;
    }

    NonVoidType<T> move() {
        return std::move(_data).template get<0>();
    }

    NonVoidType<T> const& get() const {
        return _data.template get<0>();
    }

    std::exception_ptr exception() const {
        return _data.template get<1>();
    }

    std::string what() const noexcept {
        try {
            std::rethrow_exception(exception());
        } catch (std::exception const& e) {
            return e.what();
        }
    }
private:
    UninitializedNonVoidVariant<NonVoidType<T>, std::exception_ptr> _data;
};

template <typename T>
constexpr bool IsTryTypeVal = requires (T) {
    typename T::TryType;
};

namespace internal {

template <typename T>
struct RemoveTryWarpImpl {
    using Type = T;
};

template <typename T>
struct RemoveTryWarpImpl<Try<T>> {
    using Type = T;
};

} // namespace internal

template <typename T>
using RemoveTryWarpType = internal::RemoveTryWarpImpl<T>::Type;

} // namespace HX::container