#include <HXSTL/coroutine/loop/IoUringLoop.h>

#include <HXSTL/tools/ErrorHandlingTools.h>
#include <HXSTL/coroutine/task/TimerTask.h>
#include <HXSTL/coroutine/loop/AsyncLoop.h>

#include <HXSTL/utils/FileUtils.h>
#include <string.h>

namespace HX { namespace STL { namespace coroutine { namespace loop {

IoUringLoop::IoUringLoop(unsigned int entries) : _ring() {
    unsigned int flags = 0;

// 绕过操作系统的缓存, 直接将数据从用户空间读写到磁盘
#if IO_URING_DIRECT
    flags |= IORING_SETUP_IOPOLL;
#endif

    HX::STL::tools::UringErrorHandlingTools::throwingError(
        ::io_uring_queue_init(entries, &_ring, flags)
    );
}

bool IoUringLoop::run(std::optional<std::chrono::system_clock::duration> timeout) {
    ::io_uring_cqe* cqe = nullptr;

    __kernel_timespec timespec; // 设置超时为无限阻塞
    __kernel_timespec* timespecPtr = nullptr;
    if (timeout.has_value()) {
        timespecPtr = &(timespec = durationToKernelTimespec(*timeout));
    }

    // 阻塞等待内核, 返回是错误码; cqe是完成队列, 为传出参数
    int res = ::io_uring_submit_and_wait_timeout(&_ring, &cqe, 1, timespecPtr, nullptr);

    // 超时
    if (res == -ETIME) {
        ::io_uring_cq_advance(&_ring, 0); // 确保完成队列状态同步
        return false;
    } else if (res < 0) [[unlikely]] { // 其他错误
        if (res == -EINTR) { // 被信号中断
            return false;
        }
        throw std::system_error(-res, std::system_category());
    }

    unsigned head, numGot = 0;
    std::vector<std::coroutine_handle<>> tasks;
    io_uring_for_each_cqe(&_ring, head, cqe) {
        ++numGot;
        if (cqe->res < 0 
            && !(cqe->res == -ENOENT 
                || cqe->res == -EACCES 
                || cqe->res == -EAGAIN 
                || cqe->res == -ECONNRESET 
                || cqe->res == -EPIPE
            )
        ) {
            // printf("任务已取消 (%p)\n", (void *)cqe->user_data);
            printf("Critical error: %s\n", strerror(-cqe->res));

            continue;
        }
        auto* task = reinterpret_cast<IoUringTask *>(cqe->user_data);
        // printf("任务 (%p) | res = %d | !: (%p)\n", (void *)cqe->user_data, cqe->res, &task->_res);
        task->_res = cqe->res;
        tasks.push_back(task->_previous);
    }

    // 手动前进完成队列的头部 (相当于批量io_uring_cqe_seen)
    ::io_uring_cq_advance(&_ring, numGot);
    _numSqesPending -= static_cast<std::size_t>(numGot);
    for (const auto& it : tasks) {
        it.resume();
    }
    return true;
}

IoUringTask::IoUringTask() {
    _sqe = HX::STL::coroutine::loop::AsyncLoop::getLoop().getIoUringLoop().getSqe();
    ::io_uring_sqe_set_data(_sqe, this);
}

inline static HX::STL::coroutine::task::TimerTask cancel(IoUringTask* op) {
    // 如果这个fd被关闭, 那么会自动取消(无效化)等待队列的任务
    // printf("正在取消 %p\n", (void *)op);
    co_await HX::STL::coroutine::loop::IoUringTask().prepCancel(op, IORING_ASYNC_CANCEL_ALL);
    // printf("正在取消了 %p\n", (void *)op);
    co_return;
}

IoUringTask::~IoUringTask() noexcept {
    if (_cancel) {
        // printf("营长! 快取消!\n");
        HX::STL::coroutine::loop::AsyncLoop::getLoop().getTimerLoop().addInitiationTask(
            std::make_shared<HX::STL::coroutine::task::TimerTask>(
                cancel(this)
            )
        );
    } 
    // else
    // printf("~ %p\n", (void *)this);
}

// inline static HX::STL::coroutine::task::TimerTask log(long tis) {
//     printf("~\n");
//     // co_await HX::STL::utils::FileUtils::asyncPutFileContent(
//     //     "./debug.txt", 
//     //     std::to_string(tis) + "\n", 
//     //     HX::STL::utils::FileUtils::OpenMode::Append
//     // );
//     co_return;
// }

}}}} // namespace HX::STL::coroutine::loop