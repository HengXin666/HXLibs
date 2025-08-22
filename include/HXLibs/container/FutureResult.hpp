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

namespace HX::container {

template <typename T = void>
class FutureResult {
    struct _hx_Result {
        using _hx_DataType = container::Uninitialized<T>;

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
            return _exception == nullptr;
        }
    private:
        _hx_DataType _data;
        std::exception_ptr _exception;
        std::mutex _mtx;
        std::condition_variable _cv;
        bool _isResed;
    };
public:
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
    }
private:
    std::shared_ptr<FutureResultType> const _res;
};

} // namespace HX::container

