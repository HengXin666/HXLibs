#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2024-08-17 15:57:06
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
#ifdef _HX_EXPECTED_H__TODO__ // @todo
// #ifndef _HX_EXPECTED_H_
#define _HX_EXPECTED_H_

/**
 * @brief 太复杂了, 时间成本太高了!, 并且在技术得不到保证的情况下, 你也不能保证有效
 * 最重要的是, 你怎么清理你调用的? 只能靠 RAII 清零局部变量, (析构函数还不能协程...)
 * 而用户, 或者开发者, 并不能，马上意识到!!!
 * @warning 这个废弃!!!
 */

namespace HX { namespace STL { namespace coroutine { namespace task {

template <class T>
struct Expected {
protected:
    // 不支持引用
    static_assert(!std::is_reference_v<T>);
    const std::error_category *_errorCatgory;

    union {
        T _val;
        int _errorCode;
    };
public:
    Expected(Expected&& that) : _errorCatgory(thas._errorCatgory) noexcept {
        if (!_errorCatgory) {
            // 在指定内存位置直接调用构造函数
            std::construct_at(std::addressof(_val), that._val);
        } else {
            _errorCode = that._errorCode;
        }
    }

    Expected(const Expected& that) : _errorCatgory(thas._errorCatgory) noexcept {
        if (!_errorCatgory) {
            // 在指定内存位置直接调用构造函数
            std::construct_at(std::addressof(_val), that._val);
        } else {
            _errorCode = that._errorCode;
        }
    }

    Expected& operator=(Expected&& that) noexcept {
        if (&that == this) [[unlikely]] {
            return *this;
        }
        if (!_errorCatgory) {
            std::destroy_at(std::addressof(_val));
        }
        _errorCatgory = nullptr;
        if (!that._errorCatgory) {
            std::construct_at(std::addressof(_val), that._val);
        } else {
            _errorCatgory = that._errorCatgory;
            _errorCode = that._errorCode;
        }
        return *this;
    }

    Expected& operator=(const Expected& that) noexcept {
        if (&that == this) [[unlikely]] {
            return *this;
        }
        if (!_errorCatgory) {
            std::destroy_at(std::addressof(_val));
        }
        _errorCatgory = nullptr;
        if (!that._errorCatgory) {
            std::construct_at(std::addressof(_val), that._val);
        } else {
            _errorCatgory = that._errorCatgory;
            _errorCode = that._errorCode;
        }
        return *this;
    }
};

}}}} // namespace HX::STL::coroutine::task

#endif // !_HX_EXPECTED_H_