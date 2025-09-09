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

template <typename T>
constexpr bool IsTryTypeVal = requires {
    typename T::TryType;
};

template <typename T = void>
struct Try {
    using TryType = T;

    explicit Try() 
        : _data{}
    {}

    Try(Try&&) noexcept = default;
    Try& operator=(Try&&) noexcept = default;

    template <typename U>
        requires (std::convertible_to<U, NonVoidType<T>>
              && !IsTryTypeVal<U>)
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

    /**
     * @brief 判断当前是否有值
     * @return true 有值
     * @return false 无值, 仅异常
     */
    bool isVal() const noexcept {
        return _data.index() == 0;
    }

    /**
     * @brief 判断当前是否有值
     * @return true 有值
     * @return false 无值, 仅异常
     */
    operator bool() const noexcept {
        return isVal();
    }

    /**
     * @brief 设置值
     * @tparam U 
     * @param u 
     */
    template <typename U>
    void setVal(U&& u) {
        _data.template emplace<NonVoidType<T>>(std::forward<U>(u));
    }

    /**
     * @brief 设置异常
     * @tparam U 
     * @param u 
     */
    void setException(std::exception_ptr ePtr) {
        _data.template emplace<std::exception_ptr>(ePtr);
    }

    /**
     * @brief 移动值
     * @return NonVoidType<T> 
     */
    NonVoidType<T> move() {
        return std::move(_data).template get<0>();
    }

    /**
     * @brief 获取值 [只读引用]
     * @return NonVoidType<T> const& 
     */
    NonVoidType<T> const& get() const {
        return _data.template get<0>();
    }

    /**
     * @brief 获取值 [引用]
     * @return NonVoidType<T>& 
     */
    NonVoidType<T>& get() {
        return _data.template get<0>();
    }

    /**
     * @brief 获取异常指针
     * @return std::exception_ptr 
     */
    std::exception_ptr exception() const {
        return _data.template get<1>();
    }

    /**
     * @brief 重新抛出之前捕获的异常
     */
    void rethrow() const {
        std::rethrow_exception(exception());
    }

    /**
     * @brief 获取异常原因
     * @return std::string 
     */
    std::string what() const noexcept {
        try {
            std::rethrow_exception(exception());
        } catch (std::exception const& e) {
            [[likely]] return e.what();
        } catch (...) {
            return "unknown exception type";
        }
    }

    /**
     * @brief 重置 Try 数据
     */
    void reset() noexcept {
        _data.reset();
    }
private:
    UninitializedNonVoidVariant<NonVoidType<T>, std::exception_ptr> _data;
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