#include <HXLibs/net/ApiMacro.hpp>
#include <HXLibs/log/Log.hpp>

using namespace HX;
using namespace net;

/**
 * @brief 本文件用于压力测试
 */

auto hx_init = []{
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
    HttpServer serv{28205};

    // 站点`/`, 纯连接, 测试 返回头, 以及 返回 Hello World!
    serv.addEndpoint<GET>("/", [] ENDPOINT {
        co_await res.setStatusAndContent(Status::CODE_200, "Hello World!")
                    .sendRes();
    });

    // 站点`/mp4`, 测试大文件下载量 I/O `misaka.mp4` 大小为 574.6 MB (带断点续传)
    serv.addEndpoint<GET, HEAD>("/mp4/1", [] ENDPOINT {
        co_await res.useRangeTransferFile(req.getRangeRequestView(), "bigFile/misaka.mp4");
    });

    // 站点`/mp4`, 测试大文件下载量 I/O `misaka.mp4` 大小为 574.6 MB (ChunkedEncoding)
    serv.addEndpoint<GET>("/mp4/2", [] ENDPOINT {
        co_await res.useChunkedEncodingTransferFile("bigFile/misaka.mp4");
    });

    // 站点`/html/1`, 测试小文件 (普通I/O) [普通异步文件流读写]
    serv.addEndpoint<GET>("/html/1", [] ENDPOINT {
        co_await res.useRangeTransferFile(req.getRangeRequestView(), "bigFile/HXLibs.html");
    });

    // 站点`/html/2`, 测试小文件 (普通I/O) [ChunkedEncoding]
    serv.addEndpoint<GET>("/html/2", [] ENDPOINT {
        co_await res.useChunkedEncodingTransferFile("bigFile/HXLibs.html");
    });

    serv.syncRun();
    return 0;
}