#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-09-25 16:06:43
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

namespace HX::coroutine {

/**
 * @brief 线程循环 (实际上仅是控制器)
 */
struct ThreadLoop {
private:
    struct _hx_ThreadLoopController {
        _hx_ThreadLoopController(ThreadLoop& self) noexcept
            : _self{self}
        {
            std::unique_lock _{_self._mtx};
            ++_self._threadTaskCnt;
        }

        _hx_ThreadLoopController(_hx_ThreadLoopController const&) = delete;
        _hx_ThreadLoopController(_hx_ThreadLoopController&& that) noexcept
            : _self{that._self}
        {}

        _hx_ThreadLoopController& operator=(_hx_ThreadLoopController const&) = delete;
        _hx_ThreadLoopController& operator=(_hx_ThreadLoopController&&) noexcept {
            return *this;
        }

        void notify() noexcept {
            std::unique_lock _{_self._mtx};
            --_self._threadTaskCnt;
            _self.notify();
        }
    private:
        ThreadLoop& _self;
        
    };
public:
    ThreadLoop()
        : _mtx{}
        , _cv{}
        , _threadTaskCnt{0}
    {}

    bool isRun() noexcept {
        std::size_t cnt = 0;
        std::unique_lock lock{_mtx};
        if (_threadTaskCnt == 0) {
            return false;
        }
        cnt = _threadTaskCnt;
        _cv.wait(lock, [&] {
            return cnt > _threadTaskCnt;
        });
        return true;
    }

    auto makeThreadTask() noexcept {
        return _hx_ThreadLoopController{*this};
    }

private:
    void notify() noexcept {
        _cv.notify_one();
    }

    mutable std::mutex _mtx;
    std::condition_variable _cv;
    std::size_t _threadTaskCnt;
};

} // namespace HX::coroutine