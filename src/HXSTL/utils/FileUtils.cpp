#include <HXSTL/utils/FileUtils.h>

#include <fstream>
#include <cassert>

#include <HXSTL/tools/ErrorHandlingTools.h>
#include <HXSTL/coroutine/loop/AsyncLoop.h>

using namespace std::chrono;

namespace HX { namespace STL { namespace utils {

std::string FileUtils::getFileContent(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::system_error(errno, std::generic_category());
    }
    return std::string {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void FileUtils::putFileContent(const std::string& path, std::string_view content) {
    std::ofstream file(path);
    if (!file.is_open()) {
        throw std::system_error(errno, std::generic_category());
    }
    std::copy(content.begin(), content.end(), std::ostreambuf_iterator<char>(file));
}

HX::STL::coroutine::task::Task<std::string> FileUtils::asyncGetFileContent(
    const std::string& path,
    OpenMode flags /*= OpenMode::Read*/,
    mode_t mode /*= 0644*/
) {
    int fd = HX::STL::tools::UringErrorHandlingTools::throwingError(
        co_await HX::STL::coroutine::loop::IoUringTask().prepOpenat(
            AT_FDCWD, path.c_str(), static_cast<int>(flags), mode
        )
    );
    std::string res;
    std::vector<char> buf(kBufMaxSize);
    std::size_t len = 0;
    uint64_t offset = 0;
    while ((len = static_cast<std::size_t>(
        co_await HX::STL::coroutine::loop::IoUringTask().prepRead(fd, buf, offset)
    )) == buf.size()) {
        res += std::string_view {buf.data(), buf.size()};
        offset += buf.size();
    }
    co_await HX::STL::coroutine::loop::IoUringTask().prepClose(fd);
    co_return (res.append(std::string_view {buf.data(), len}));
}

HX::STL::coroutine::task::Task<int> FileUtils::asyncPutFileContent(
    const std::string& path,
    std::string_view content,
    OpenMode flags,
    mode_t mode /*= 0644*/
) {
    int fd = HX::STL::tools::UringErrorHandlingTools::throwingError(
        co_await HX::STL::coroutine::loop::IoUringTask().prepOpenat(
            AT_FDCWD, path.c_str(), static_cast<int>(flags), mode
        )
    );
    // 无需设置 offset, 因为内核会根据 open 的 flags 来
    int res = co_await HX::STL::coroutine::loop::IoUringTask().prepWrite(
        fd, content, static_cast<std::uint64_t>(-1)
    );
    co_await HX::STL::coroutine::loop::IoUringTask().prepClose(fd);
    co_return res;
}

HX::STL::coroutine::task::Task<> FileUtils::AsyncFile::open(
    const std::string& path,
    FileUtils::OpenMode flags /*= OpenMode::Read*/,
    mode_t mode /*= 0644*/
) {
    _fd = co_await HX::STL::coroutine::loop::IoUringTask().prepOpenat(
        AT_FDCWD, path.c_str(), static_cast<int>(flags), mode
    );
}

HX::STL::coroutine::task::Task<int> FileUtils::AsyncFile::read(std::span<char> buf) {
    int len = HX::STL::tools::UringErrorHandlingTools::throwingError(
        co_await HX::STL::coroutine::loop::IoUringTask().prepRead(_fd, buf, _offset)
    );
    _offset += len;
    co_return len;
}

HX::STL::coroutine::task::Task<int> FileUtils::AsyncFile::read(std::span<char> buf, unsigned int size) {
    int len = HX::STL::tools::UringErrorHandlingTools::throwingError(
        co_await HX::STL::coroutine::loop::IoUringTask().prepRead(_fd, buf, size, _offset)
    );
    _offset += len;
    co_return len;
}

HX::STL::coroutine::task::Task<int> FileUtils::AsyncFile::write(std::span<char> buf) {
    co_return HX::STL::tools::UringErrorHandlingTools::throwingError(
        co_await HX::STL::coroutine::loop::IoUringTask().prepWrite(
            _fd, buf, static_cast<std::uint64_t>(-1)
        )
    );
}

inline static HX::STL::coroutine::task::TimerTask close(int fd) {
    // 如果这个fd被关闭, 那么会自动取消(无效化)等待队列的任务
    co_await HX::STL::coroutine::loop::IoUringTask().prepClose(fd);
}

FileUtils::AsyncFile::~AsyncFile() noexcept {
    HX::STL::coroutine::loop::AsyncLoop::getLoop().getTimerLoop().addInitiationTask(
        std::make_shared<HX::STL::coroutine::task::TimerTask>(
            close(_fd)
        )
    );
}

}}} // namespace HX::STL::utils