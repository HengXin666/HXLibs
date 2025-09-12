#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-07 21:56:41
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

#include <thread>
#include <chrono>
#include <coroutine>

#if defined (_WIN32)
#include <array>
#endif

#include <HXLibs/platform/EventLoopApi.hpp>

#include <HXLibs/container/Try.hpp>
#include <HXLibs/coroutine/task/Task.hpp>
#include <HXLibs/coroutine/task/AioTask.hpp>
#include <HXLibs/coroutine/loop/TimerLoop.hpp>
#include <HXLibs/coroutine/concepts/Awaiter.hpp>
#include <HXLibs/coroutine/awaiter/WhenAny.hpp>
#include <HXLibs/exception/ErrorHandlingTools.hpp>

namespace HX::coroutine {

#if defined(__linux__)

template <typename Rep, typename Period>
constexpr struct ::__kernel_timespec durationToKernelTimespec(
    std::chrono::duration<Rep, Period> dur
) noexcept {
    struct ::__kernel_timespec ts;
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(dur);
    auto nsecs = std::chrono::duration_cast<std::chrono::nanoseconds>(dur - secs);
    ts.tv_sec = static_cast<__kernel_time64_t>(secs.count());
    ts.tv_nsec = static_cast<__kernel_time64_t>(nsecs.count());
    return ts;
}

#elif defined (_WIN32)

inline DWORD toDwMilliseconds(std::chrono::system_clock::duration dur) {
    using namespace std::chrono;

    // 转换为毫秒（有符号 64 位整数）
    auto ms = duration_cast<milliseconds>(dur).count();

    // 如果负数或太大，就处理为 0 或 INFINITE
    if (ms <= 0)
        return 0;
    if (ms >= std::numeric_limits<DWORD>::max())
        return INFINITE;

    return static_cast<::DWORD>(ms);
}

#else
    #error "Does not support the current operating system."
#endif

namespace internal {

#if defined(__linux__)

/**
 * @brief 获取当前系统支持的最大io_uring环形队列的长度 (不要频繁调用, 这个只是测试使用的)
 * @warning 如果频繁调用会导致之前的东西没有在内核上完全释放 (说白了就是需要等会)
 * @return unsigned int 
 */
inline unsigned int getIoUringMaxSize() {
    struct ::io_uring ring;
    for (unsigned int entries = 64; entries; entries <<= 1) [[likely]] {
        int ret = ::io_uring_queue_init(entries, &ring, 0);
        if (!ret) {
            ::io_uring_queue_exit(&ring);
        } else {
            return entries >> 1;
        }
    }
    [[unlikely]] throw std::runtime_error{"IoUringMaxSize not find"}; // 找不到可用的大小
}

struct IoUring {
    explicit IoUring(unsigned int size = 1024U)
        : _ring{}
        , _numSqesPending{}
    {
        unsigned int flags = 0;
        exception::IoUringErrorHandlingTools::check(
            ::io_uring_queue_init(size, &_ring, flags)
        );
    }

    ~IoUring() noexcept {
        ::io_uring_queue_exit(&_ring);
    }

    AioTask makeAioTask() {
        // mandatory copy elision 场景
        // 编译器强制使用 RVO (返回值优化)
        // https://en.cppreference.com/w/cpp/language/copy_elision.html
        return AioTask{getSqe()};
    }

    bool isRun() const noexcept {
        return _numSqesPending;
    }

    void run(std::optional<std::chrono::system_clock::duration> timeout) {
        ::io_uring_cqe* cqe = nullptr;

        ::__kernel_timespec timespec; // 设置超时为无限阻塞
        ::__kernel_timespec* timespecPtr = nullptr;
        if (timeout.has_value()) {
            timespec = durationToKernelTimespec(*timeout);
            timespecPtr = &timespec;
        }

        // 阻塞等待内核, 返回是错误码; cqe是完成队列, 为传出参数
        int res = ::io_uring_submit_and_wait_timeout(
            &_ring, &cqe, 1, timespecPtr, nullptr);
        
        // 超时
        if (res == -ETIME) {
            // 内部直接 if (nr) { ... }, 此处调用传参 0, 毫无作用
            // ::io_uring_cq_advance(&_ring, 0); // 确保完成队列状态同步
            return;
        } else if (res < 0) [[unlikely]] { // 其他错误
            if (res == -EINTR) { // 被信号中断
                return;
            }
            throw std::system_error(-res, std::system_category());
        }

        unsigned head, numGot = 0;
        io_uring_for_each_cqe(&_ring, head, cqe) {
            ++numGot;
            if (cqe->res == -ECANCELED) { // 操作已取消 (比如超时了)
                continue;
            }
            auto* task = reinterpret_cast<AioTask*>(cqe->user_data);
            task->_res = cqe->res;
            tasks.push_back(task->_previous);
        }

        // 手动前进完成队列的头部 (相当于批量io_uring_cqe_seen)
        ::io_uring_cq_advance(&_ring, numGot);
        _numSqesPending -= static_cast<std::size_t>(numGot);
        for (const auto& it : tasks) {
            it.resume();
        }
        tasks.clear();
    }

private:
    ::io_uring_sqe* getSqe() {
        // 获取一个任务
        ::io_uring_sqe* sqe = ::io_uring_get_sqe(&_ring);
        while (!sqe) {
#if 0
            // 提交任务队列给内核 (为什么不是sqe, 因为sqe是从ring中get出来的, 故其本身就包含了sqe)
            int res = ::io_uring_submit(&_ring);
            if (res < 0) [[unlikely]] {
                if (res == -EINTR) {
                    continue;
                }
                throw std::system_error(-res, std::system_category());
            }
            // 提交了, 应该有空位, 那可以
            sqe = ::io_uring_get_sqe(&_ring);
#else
            ::io_uring_submit_and_wait(&_ring, 1); // 直接挂起等待操作系统完成了再说
            sqe = ::io_uring_get_sqe(&_ring);
            if (!sqe) {
                throw std::runtime_error("Still failed to get sqe after wait");
            }
#endif
        }
        ++_numSqesPending;
        return sqe;
    }

    ::io_uring _ring;
    std::size_t _numSqesPending; // 未完成的任务数
    std::vector<std::coroutine_handle<>> tasks; // 协程任务队列
                                                // 提取为成员, 避免频繁构造临时变量导致频繁扩容
};

#elif defined(_WIN32)

struct Iocp {
    Iocp() 
        : _iocpHandle{exception::checkWinError(::CreateIoCompletionPort(
            INVALID_HANDLE_VALUE,
            nullptr,
            0,
            0))}
        , _taskCnt{}
        , _tasks{}
    {
        platform::internal::InitWin32Api::ensure();
    }

    Iocp& operator=(Iocp&&) = delete;

    AioTask makeAioTask() {
        return {_iocpHandle, _taskCnt};
    }

    /**
     * @brief 是否还有任务在等待
     * @warning 如果您发现您卡在这里, 那么请检查是否有 fd 没有被 close!
     * @return true 还有任务
     * @return false 无任务
     */
    bool isRun() const {
        return _taskCnt._numSqesPending || _taskCnt._runingHandle.size();
    }

    void run(std::optional<std::chrono::system_clock::duration> timeout) {
/*
// 只能一次获取一个
BOOL GetQueuedCompletionStatus(
    [in]  HANDLE       CompletionPort,             // 完成端口的句柄
    [out] LPDWORD      lpNumberOfBytesTransferred, // 指针: 接收完成的I/O操作传输的字节数
    [out] PULONG_PTR   lpCompletionKey,            // 指针: 接收与完成的I/O操作关联的完成键
    [out] LPOVERLAPPED *lpOverlapped,              // 指针: 接收指向完成的I/O操作的OVERLAPPED结构的指针
    [in]  DWORD        dwMilliseconds              // 等待操作完成的超时时间, 以毫秒为单位
);

// 一次获取多个
BOOL GetQueuedCompletionStatusEx(
  HANDLE               CompletionPort,          // 完成端口的句柄
  LPOVERLAPPED_ENTRY   lpCompletionPortEntries, // 批量接收的数组
  ULONG                ulCount,                 // 最大获取个数
  PULONG               ulNumEntriesRemoved,     // 实际返回个数
  DWORD                dwMilliseconds,          // 等待操作完成的超时时间, 以毫秒为单位
  BOOL                 fAlertable               // 是否可被 APC (异步过程调用) 或 IO Completion Callback 中断
                                                // 一般写 false, 不允许!
);
*/
        std::array<::OVERLAPPED_ENTRY, 64> arr;
        ::ULONG n = 0;
        decltype(toDwMilliseconds(*timeout)) dw = INFINITE;
        if (timeout) {
            dw = toDwMilliseconds(*timeout);
        }
        bool ok = ::GetQueuedCompletionStatusEx(
            _iocpHandle,
            arr.data(),
            static_cast<::DWORD>(arr.size()),
            &n,
            dw,
            false
        );

        if (!ok) [[unlikely]] { // 超时
            return;
        }

        for (::ULONG i = 0; i < n; ++i) {
            auto ptr = arr[i];
            auto task = std::unique_ptr<AioTask::_AioIocpData>{
                reinterpret_cast<AioTask::_AioIocpData*>(ptr.lpOverlapped)
            };
            if (task->isCancel()) { // 垃圾 winApi 读过书吗老弟? 还要用 WSAGetOverlappedResult
                                    // 来获取错误? 乱搞! 这不又有内核态切换开销?
                                    // 因为我们期望的错误都是我们自己产生的, 比如我希望取消, 则 close
                                    // 所以close之前, 我们可以记录一下其状态!
                continue;
            }
            task->_self._res = ptr.dwNumberOfBytesTransferred;
            _tasks.push_back(task->_self._previous);
        }

        
        for (const auto& t : _tasks) {
            t.resume();
        }
        
        _taskCnt._numSqesPending -= static_cast<std::size_t>(n);
        _tasks.clear();
    }

    ~Iocp() noexcept {
        if (_iocpHandle) {
            ::CloseHandle(_iocpHandle);
        }
    }

private:
    ::HANDLE _iocpHandle;
    TaskCnt _taskCnt;
    std::vector<std::coroutine_handle<>> _tasks;
};

#else
    #error "Does not support the current operating system."
#endif

using EventDrive

#if defined(__linux__)
    = IoUring;
#elif defined(_WIN32)
    = Iocp;
#else
    #error "Does not support the current operating system."
#endif

} // namespace internal

/**
 * @brief 协程事件循环
 */
struct EventLoop {
    EventLoop() 
        : _eventDrive{}
        , _timerLoop{}
    {}

    /**
     * @brief 启动协程, 协程内部如果挂起, 应该调用 run() 进入事件循环, 以恢复挂起.
     * @tparam T 
     * @param mainTask 
     */
    template <CoroutineObject T>
    void start(T& mainTask) {
        static_cast<std::coroutine_handle<>>(mainTask).resume();
    }

    /**
     * @brief 同步调用协程, 如果协程内部抛出异常, 则该异常会在 sync 重新抛出
     * @warning 应该保证事件循环为空, 否则会抛出异常.
     * @tparam T 
     * @tparam Res 
     * @param mainTask 
     * @return Res 协程返回值
     */
    template <CoroutineObject T, typename Res = AwaiterReturnValue<T>>
    Res sync(T&& mainTask) {
        auto t = trySync(std::forward<T>(mainTask));
        if (!t) [[unlikely]] {
            t.rethrow();
        }
        if constexpr (!std::is_void_v<Res>) {
            return t.move();
        }
    }

    template <CoroutineObject T, typename Res = AwaiterReturnValue<T>>
    container::Try<Res> trySync(T&& mainTask) {
        if (_eventDrive.isRun()) [[unlikely]] {
            // 如果触发下面, 极有可能先前进行了协程挂起而没有恢复, 或者当前为协程语义环境
            // 需要先调用 run(), 方可调用 sync(), 以防止可能的死锁
            throw std::runtime_error{"Unexpected call"};
        }
        container::Try<Res> res;
        auto task = [&res, _mainTask = std::move(mainTask)]() mutable -> Task<> {
            try {
                if constexpr (!std::is_void_v<Res>) {
                    res.setVal(co_await _mainTask);
                } else {
                    co_await _mainTask;
                    res.setVal(container::NonVoidType<>{});
                }
            } catch (...) {
                res.setException(std::current_exception());
            }
        };
        auto coTask = task();
        start(coTask);
        run();
        return res;
    }
    
    /**
     * @brief 启动事件循环
     */
    void run() {
        for (;;) {
            auto timeout = _timerLoop.run();
            if (_eventDrive.isRun()) [[likely]] {
                _eventDrive.run(timeout);
            } else if (timeout) {
                std::this_thread::sleep_for(*timeout);
            } else [[unlikely]] {
                break;
            }
        }
    }

    /**
     * @brief 创建协程定时器
     * @return auto 
     */
    auto makeTimer() {
        return TimerLoop::makeTimer(_timerLoop);
    }

    /**
     * @brief 创建异步IO协程任务
     * @return decltype(auto) 
     */
    decltype(auto) makeAioTask() {
        return _eventDrive.makeAioTask();
    }

    /**
     * @brief 获取事件循环的底层引擎
     * @return auto& 
     */
    auto& getEventDrive() {
        return _eventDrive;
    }
private:
    internal::EventDrive _eventDrive;
    TimerLoop _timerLoop;
};

} // namespace HX::coroutine

