#include <HXWeb/socket/IO.h>

#include <HXSTL/coroutine/loop/AsyncLoop.h>
#include <HXSTL/coroutine/task/TimerTask.h>
#include <HXWeb/protocol/http/Request.h>
#include <HXWeb/protocol/http/Response.h>
#include <HXSTL/tools/ErrorHandlingTools.h>
#include <HXSTL/utils/FileUtils.h>

namespace HX { namespace web { namespace socket {

IO::IO(int fd) 
    : _fd(fd)
    , _recvBuf(HX::STL::utils::FileUtils::kBufMaxSize)
{}

inline static HX::STL::coroutine::task::TimerTask close(int fd) {
    // 如果这个fd被关闭, 那么会自动取消(无效化)等待队列的任务
    // printf("删除 %d {\n", fd);
    co_await HX::STL::coroutine::loop::IoUringTask().prepClose(fd);
    // printf("} // 删除 %d\n", fd);
}

IO::~IO() noexcept {
    // 添加到事件循环, 虽然略微延迟释放fd, 但是RAII呀~
    HX::STL::coroutine::loop::AsyncLoop::getLoop().getTimerLoop().addInitiationTask(
        std::make_shared<HX::STL::coroutine::task::TimerTask>(
            close(_fd)
        )
    );
}

HX::STL::coroutine::task::Task<std::optional<std::string>> IO::recvN(
    std::size_t n
) const {
    std::string s;
    s.resize(n);
    int len = std::max(
        co_await _recvSpan(s), 0
    );

    if (len) {
        co_return s;
    }
    co_return std::nullopt;
}

HX::STL::coroutine::task::Task<std::size_t> IO::recvN(
    std::span<char> buf,
    std::size_t n
) const {
    co_return static_cast<std::size_t>(std::max(
        co_await _recvSpan(std::span{buf.data(), n}), 0
    ));
}

HX::STL::coroutine::task::Task<int> IO::_recvSpan(
    std::span<char> buf
) const {
    co_return co_await HX::STL::coroutine::loop::IoUringTask().prepRecv(
        _fd, buf, 0
    );
}

HX::STL::coroutine::task::Task<> IO::_sendSpan(std::span<char> buf) const {
    std::size_t n = HX::STL::tools::UringErrorHandlingTools::throwingError(
        co_await HX::STL::coroutine::loop::IoUringTask().prepSend(_fd, buf, 0)
    ); // 已经写入的字节数

    while (true) {
        if (n == buf.size()) {
            // 全部写入啦
            _response->clear();
            break;
        }
        n = HX::STL::tools::UringErrorHandlingTools::throwingError(
            co_await HX::STL::coroutine::loop::IoUringTask().prepSend(
                _fd, buf = buf.subspan(n), 0
            )
        );
    }
}

HX::STL::coroutine::task::Task<int> IO::_pollAdd(
    unsigned int pollMask
) const {
    co_return co_await HX::STL::coroutine::loop::IoUringTask().prepPollAdd(
        _fd, pollMask
    );
}

}}} // namespace HX::web::socket