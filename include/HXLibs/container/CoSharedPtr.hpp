#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2026-06-08 23:56:09
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

#include <new>
#include <utility>

namespace HX::container {

namespace internal {

template <typename T>
class RefCntBlock {
    std::size_t _cnt{1};
    alignas(T) unsigned char _data[sizeof(T)]{};
public:
    constexpr RefCntBlock(T data)
        : _cnt{1}
        , _data{}
    {
        ::new (static_cast<void*>(_data)) T(std::move(data));
    }

    template <typename... Args>
    constexpr RefCntBlock(std::in_place_t, Args&&... args) // make
        : _cnt{1}
        , _data{}
    {
        ::new (static_cast<void*>(_data)) T(std::forward<Args>(args)...);
    }

    constexpr RefCntBlock& operator=(RefCntBlock&&) = delete;

    std::size_t useCount() const noexcept {
        return _cnt;
    }

    void add() noexcept {
        ++_cnt;
    }

    void reduce() noexcept {
        if (--_cnt == 0) {
            delete this;
        }
    }

    constexpr T* get() noexcept {
        return std::launder(reinterpret_cast<T*>(_data));
    }

    constexpr T const* get() const noexcept {
        return std::launder(reinterpret_cast<const T*>(_data));
    }

    ~RefCntBlock() noexcept {
        get()->~T();
    }
};

} // namespace internal

/**
 * @brief 简易协程非原子计数共享智能指针 (超轻量)
 * @tparam T 
 */
template <typename T>
class CoSharedPtr {
public:
    using RefCntType = internal::RefCntBlock<T>;

    constexpr CoSharedPtr() = default;
    constexpr CoSharedPtr(std::nullptr_t) {}

    constexpr explicit CoSharedPtr(T data)
        : _ptr{new RefCntType{std::move(data)}}
    {}

    constexpr explicit CoSharedPtr(RefCntType* ptr)
        : _ptr{ptr}
    {}

    constexpr CoSharedPtr(CoSharedPtr const& that)
        : _ptr{that._ptr}
    {
        addRefCnt();
    }

    constexpr CoSharedPtr(CoSharedPtr&& that) noexcept {
        std::swap(_ptr, that._ptr);
    }

    constexpr CoSharedPtr& operator=(CoSharedPtr const& that) {
        CoSharedPtr{that}.swap(*this);
        return *this;
    }

    constexpr CoSharedPtr& operator=(CoSharedPtr&& that) noexcept {
        CoSharedPtr{std::move(that)}.swap(*this);
        return *this;
    }

    explicit constexpr operator bool() const noexcept {
        return _ptr != nullptr;
    }

    T* get() noexcept {
        return _ptr ? _ptr->get() : nullptr;
    }

    const T* get() const noexcept {
        return _ptr ? _ptr->get() : nullptr;
    }

    constexpr T& operator*() const noexcept {
        return *_ptr->get();
    }

    constexpr T* operator->() const noexcept {
        return _ptr->get();
    }

    constexpr bool operator==(CoSharedPtr const& that) const noexcept {
        return _ptr == that._ptr;
    }

    constexpr bool operator!=(CoSharedPtr const& that) const noexcept {
        return !operator==(that);
    }

    std::size_t useCount() const noexcept {
        return _ptr ? _ptr->useCount() : 0;
    }

    void reset(T data) {
        CoSharedPtr{std::move(data)}.swap(*this);
    }

    void swap(CoSharedPtr& that) noexcept {
        std::swap(_ptr, that._ptr);
    }

    constexpr ~CoSharedPtr() noexcept {
        delRefCnt();
    }
private:
    RefCntType* _ptr{};

    constexpr void addRefCnt() noexcept {
        if (_ptr) {
            _ptr->add();
        }
    }

    constexpr void delRefCnt() noexcept {
        if (_ptr) {
            _ptr->reduce();
            _ptr = nullptr;
        }
    }
};

template <typename T, typename... Args>
constexpr CoSharedPtr<T> makeCoSharedPtr(Args&&... args) {
    return CoSharedPtr<T>{new typename CoSharedPtr<T>::RefCntType{
        std::in_place, std::forward<Args>(args)...
    }};
}

} // namespace HX::container
