#include <HXLibs/utils/FileUtils.hpp>
#include <HXLibs/log/Log.hpp>

using namespace HX;

auto __init__ = []{
    setlocale(LC_ALL, "zh_CN.UTF-8");
    try {
        auto cwd = std::filesystem::current_path();
        log::hxLog.debug("当前工作路径是:", cwd);
        std::filesystem::current_path("../../../../static");
        log::hxLog.debug("切换到路径:", std::filesystem::current_path());
    } catch (const std::filesystem::filesystem_error& e) {
        log::hxLog.error("Error:", e.what());
    }
    return 0;
}();

int main() {
    coroutine::EventLoop loop{};
    utils::AsyncFile file{loop};
    loop.sync(file.open("./index.html"));
    std::string data('1', 16);
    loop.sync(file.read(data));
    log::hxLog.info(data);
    file.syncClose();
    return 0;
}