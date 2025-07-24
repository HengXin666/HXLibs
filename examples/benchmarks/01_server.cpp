#include <HXLibs/net/Api.hpp>
#include <HXLibs/log/Log.hpp>

using namespace HX;
using namespace net;

/**
 * @brief 本文件用于压力测试
 */

auto __init__ = []{
    setlocale(LC_ALL, "zh_CN.UTF-8");
    try {
        auto cwd = std::filesystem::current_path();
        log::hxLog.debug("当前工作路径是:", cwd);
        std::filesystem::current_path("../../../../static");
        log::hxLog.debug("切换到路径:", std::filesystem::current_path());
        log::hxLog.debug(std::this_thread::get_id());
    } catch (const std::filesystem::filesystem_error& e) {
        log::hxLog.error("Error:", e.what());
    }
    return 0;
}();

int main() {
    HttpServer serv{"127.0.0.1", "28205"};

    // 站点`/`, 纯连接, 测试 返回头, 以及 返回 Hello World!
    serv.addEndpoint<GET>("/", [] ENDPOINT {
        co_await res.setStatusAndContent(Status::CODE_200, "Hello World!")
                    .sendRes();
    });

    // 站点`/mp4`, 测试大文件下载量 I/O `misaka.mp4` 大小为 574.6 MB (带断点续传)
    serv.addEndpoint<GET, HEAD>("/mp4", [] ENDPOINT {
        co_await res.useRangeTransferFile(req.getRangeRequestView(), "bigFile/misaka.mp4");
    });

    // 站点`/html/1`, 测试小文件 (普通I/O)
    serv.addEndpoint<GET>("/html/1", [] ENDPOINT {
        co_await res.useRangeTransferFile(req.getRangeRequestView(), "bigFile/HXLibs.html");
    });


    serv.syncRun();
    return 0;
}