#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-10 23:22:04
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

#include <mutex>
#include <condition_variable>

#include <HXLibs/container/Uninitialized.hpp>
#include <HXLibs/container/Try.hpp>

namespace HX::container {

/**
 * @brief 判断是否为 FutureResult<T> 类型
 * @tparam T 
 */
template <typename T>
bool constexpr IsFutureResultType = requires {
    typename T::FutureResultType;
};

template <typename T = void>
class FutureResult {
    struct _hx_Result {
        using _hx_DataType = Uninitialized<T>;

        _hx_Result()
            : _data{}
            , _exception{}
            , _mtx{}
            , _cv{}
            , _isResed(false)
        {}

        ~_hx_Result() noexcept = default;

        _hx_Result(const _hx_Result&) = delete;
        _hx_Result& operator=(const _hx_Result&) = delete;
        _hx_Result(_hx_Result&&) noexcept = delete;
        _hx_Result& operator=(_hx_Result&&) noexcept = delete;

        void wait() {
            std::unique_lock lck{_mtx};
            _cv.wait(lck, [this] { return _isResed; });
        }

        void ready() {
            {
                std::lock_guard _{_mtx};
                _isResed = true;
            }
            _cv.notify_all();
        }

        NonVoidType<T> data() {
            if (_exception) [[unlikely]] {
                std::rethrow_exception(_exception);
            }
            return _data.move();
        }

        void setData(NonVoidType<T>&& data) {
            _data.set(std::move(data));
            ready();
        }

        void unhandledException() noexcept {
            _exception = std::current_exception();
            ready();
        }

        bool isException() const noexcept {
            return _exception != nullptr;
        }

        std::exception_ptr getException() const noexcept {
            return _exception;
        }
    private:
        _hx_DataType _data;
        std::exception_ptr _exception;
        std::mutex _mtx;
        std::condition_variable _cv;
        bool _isResed;
    };
public:
    using ArgTryType = Try<RemoveTryWarpType<T>>; // thenTry 的 Func 的传入参数类型
    using FutureResultType = _hx_Result;

    FutureResult()
        : _res{std::make_shared<FutureResultType>()}
    {}

    FutureResult(FutureResult const&) = delete;
    FutureResult& operator=(FutureResult const&) = delete;

    FutureResult(FutureResult&&) noexcept = default;
    FutureResult& operator=(FutureResult&&) noexcept = default;

    NonVoidType<T> get() {
        wait();
        return _res->data();
    }

    std::shared_ptr<FutureResultType> getFutureResult() noexcept {
        return _res;
    }

    void wait() {
        _res->wait();
        if (_res->isException()) [[unlikely]] {
            std::rethrow_exception(_res->getException());
        }
    }

    /**
     * @brief 异步执行
     * @tparam Func 异步函数
     * @tparam Res 异步函数 返回值
     * @tparam ArgTryType 异步函数 传入参数, 应该为 Try<RemoveTryWarpType<T>>
     * @param func 
     * @param t 
     * @return 返回类型:
            - 如果异步函数返回 FutureResult<U>, 那么为 void. 此时 Func 不应该抛出异常
            - 如果异步函数返回 Try<U> 或者 U, 那么为 FutureResult<U>. 可链式调用
     * @note 为何不可返回 FutureResult<U> ?
            - 因为: 如果允许, 则变成: FutureResult<FutureResult<U>>, 此时可能会导致死锁!
     */
    template <typename Func, typename Res = std::invoke_result_t<Func, ArgTryType>>
        requires (requires (Func func, FutureResult<T>::ArgTryType t) {
            // https://github.com/HengXin666/HXLibs/issues/13
            { func(std::move(t)) } -> std::same_as<Res>;
        })
    std::conditional_t<
        IsFutureResultType<Res>,
        void,
        FutureResult<RemoveTryWarpType<Res>>
    > thenTry(Func&& func) && noexcept;
private:
    template <typename>
    friend class FutureResult;

    // @todo 日后再设计 via, 目前仅依赖 ThreadPool; 这样就够了~
    friend struct ThreadPool;
    using ThreadPoolRef = struct ThreadPool*;

    void via(ThreadPoolRef dispatch) {
        _dispatch.set(dispatch);
    }

    Uninitialized<ThreadPoolRef> _dispatch;
    std::shared_ptr<FutureResultType> const _res;
};

} // namespace HX::container

