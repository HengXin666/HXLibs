#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2024-08-11 21:08:36
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
#ifdef _HX_RETURN_TO_PARENT_AWAITER_H__TODO__
// #ifndef _HX_RETURN_TO_PARENT_AWAITER_H_
#define _HX_RETURN_TO_PARENT_AWAITER_H_

/**
 * @file ReturnToParentAwaiter.hpp
 * @author your name (you@domain.com)
 * @brief 不知道为什么, 今天回看源码, 这里被废弃了?! 但是! 是应该的, 因为我似乎采用了其他方案?!
 * 似乎是所有权的问题, 这样搞记得是会内存泄漏还是转移失败来着?
 * 我只记得是让智能指针来获取所有权, 然后移动到其他地方了~
 * @version 0.1
 * @date 2025-05-15
 * 
 * @copyright Copyright (c) 2025
 */

#include <coroutine>

namespace HX { namespace STL { namespace coroutine { namespace awaiter {

/**
 * @brief 协程控制: 暂停则回到父级`await`继续执行; 其控制权交给`Loop`
 * @warning 禁止抛出异常! 因为控制权不在协程! 并且已经分离!
 */
template <class T, class P>
struct ReturnToParentAwaiter {
    using promise_type = P;
    explicit ReturnToParentAwaiter() = default;

    explicit ReturnToParentAwaiter(
        std::coroutine_handle<promise_type> coroutine
    ) : _coroutine(coroutine)
      , _coroutinePtr(coroutinePtr)
    {}

    bool await_ready() const noexcept { 
        return false; 
    }

    /**
     * @brief 挂起当前协程
     * @param coroutine 这个是`co_await`的协程句柄 (而不是 _coroutine)
     * @return std::coroutine_handle<promise_type> 
     */
    auto await_suspend(
        std::coroutine_handle<> coroutine
    ) const noexcept {
        _coroutine.promise()._previous = coroutine;
        HX::STL::tools::ForwardCoroutineTools::TimerLoopAddTask(coroutine);
        return false;
    }

    void await_resume() const noexcept {
        return _coroutine.promise().result();
    }

    std::coroutine_handle<promise_type> _coroutine;
};

}}}} // namespace HX::STL::coroutine::awaiter

#endif // !_HX_RETURN_TO_PARENT_AWAITER_H_